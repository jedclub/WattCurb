#pragma once

#include "core/types.hpp"
#include <atomic>
#include <cstdint>
#include <cstring>

namespace wattcurb::ipc {

// REF-REQ-028, REF-ARCH-018: 32-byte POD Culprit Record
struct SharedCulprit {
    char     comm[16]{};
    int32_t  pid{0};
    uint32_t drain_mw{0};
    uint8_t  domain_id{0};
    uint8_t  tier{0};
    uint8_t  padding[6]{0};
};
static_assert(sizeof(SharedCulprit) == 32, "SharedCulprit must be exactly 32 bytes");

// REF-REQ-028, REF-ARCH-018: 128-byte (2 Cacheline) Seqlock POD Shared State
struct alignas(64) WattCurbSharedState {
    // Cacheline 0: Seqlock Header & Electrical Telemetry (64 Bytes)
    uint64_t seq_version{0};             // Odd: Writer active, Even: Stable data
    uint32_t system_drain_mw{0};
    uint8_t  battery_percent{0};
    uint8_t  battery_state{0};            // 0: AC, 1: Discharging, 2: AC Passthrough
    uint16_t time_to_empty_min{0};
    uint32_t active_mitigations{0};       // Bitmask of active features
    uint32_t cpu_drain_mw{0};
    uint32_t gpu_drain_mw{0};
    uint32_t wakeups_per_sec{0};
    uint16_t cpu_temp_c{0};
    uint16_t fan_rpm{0};
    uint8_t  cstate_c3_percent{0};
    uint8_t  battery_health_percent{0};
    uint8_t  power_profile_mode{0};        // 0: Balanced, 1: PowerSaver, 2: UltraEndurance (REF-REQ-031)
    uint16_t cpu_freq_mhz{0};              // Realtime average CPU clock in MHz
    uint8_t  reserved0[19]{0};

    // Cacheline 1: Top Dominant Energy Culprits
    SharedCulprit culprits[2];            // 2 * 32 = 64 Bytes -> Total struct = 128 Bytes exactly

    // Seqlock Atomic Write Helper (Daemon Provider)
    void update_from_report(const AnalysisReportData& r) noexcept {
        uint64_t ver = __atomic_load_n(&seq_version, __ATOMIC_RELAXED);
        __atomic_store_n(&seq_version, ver + 1, __ATOMIC_RELEASE); // Mark writer busy (odd)

        system_drain_mw = static_cast<uint32_t>(r.hardware.total_system_watts * 1000.0);
        battery_percent = static_cast<uint8_t>(std::clamp(static_cast<int>(r.hardware.battery_capacity_percent), 0, 100));
        battery_state = r.hardware.is_ac_passthrough ? 2 : (r.hardware.is_battery_discharging ? 1 : 0);
        time_to_empty_min = static_cast<uint16_t>(std::clamp(static_cast<int>(r.hardware.battery_remaining_hours_to_empty * 60.0), 0, 65535));
        active_mitigations = static_cast<uint32_t>(r.mitigation_status.feature_summary_count);

        cpu_drain_mw = static_cast<uint32_t>(r.hardware.cpu_package_watts * 1000.0);
        gpu_drain_mw = static_cast<uint32_t>(r.hardware.gpu_watts * 1000.0);
        wakeups_per_sec = static_cast<uint32_t>(r.total_system_wakeups_per_sec);
        cpu_temp_c = static_cast<uint16_t>(std::clamp(static_cast<int>(r.hardware.cpu_temp_c), 0, 200));
        cpu_freq_mhz = static_cast<uint16_t>(r.hardware.cpu_freq_avg_mhz);
        fan_rpm = static_cast<uint16_t>(std::clamp(r.hardware.fan_rpm, 0u, 20000u));
        cstate_c3_percent = static_cast<uint8_t>(std::clamp(static_cast<int>(r.hardware.cstate_c3_deep_percent), 0, 100));
        battery_health_percent = static_cast<uint8_t>(std::clamp(static_cast<int>(r.hardware.battery_health_percent), 0, 100));
        power_profile_mode = static_cast<uint8_t>(r.mitigation_status.current_profile);

        // Copy top 2 culprits
        size_t n = std::min(size_t{2}, r.top_processes.size());
        for (size_t i = 0; i < n; ++i) {
            const auto& p = r.top_processes[i];
            std::strncpy(culprits[i].comm, p.comm.c_str(), sizeof(culprits[i].comm) - 1);
            culprits[i].comm[sizeof(culprits[i].comm) - 1] = '\0';
            culprits[i].pid = p.pid;
            culprits[i].drain_mw = static_cast<uint32_t>(p.total_attributed_watts * 1000.0);
            culprits[i].tier = static_cast<uint8_t>(p.safety_tier);
            culprits[i].domain_id = 0;
        }

        __atomic_store_n(&seq_version, ver + 2, __ATOMIC_RELEASE); // Mark writer stable (even)
    }

    // Seqlock Atomic Profile Mode Update Helper (Daemon Provider - REF-REQ-091, REF-ARCH-068)
    void update_profile_mode(uint8_t mode) noexcept {
        uint64_t ver = __atomic_load_n(&seq_version, __ATOMIC_RELAXED);
        __atomic_store_n(&seq_version, ver + 1, __ATOMIC_RELEASE); // Mark writer busy (odd)
        power_profile_mode = mode;
        __atomic_store_n(&seq_version, ver + 2, __ATOMIC_RELEASE); // Mark writer stable (even)
    }

    // Seqlock Atomic Read Helper (Tray Consumer, 0-lock, 0-allocation)
    bool read_atomic(WattCurbSharedState& out) const noexcept {
        uint64_t v1 = 0, v2 = 0;
        int retries = 0;
        do {
            if (++retries > 100) return false;
            v1 = __atomic_load_n(&seq_version, __ATOMIC_ACQUIRE);
            if (v1 & 1) continue; // Writer is currently modifying
            std::memcpy(&out, this, sizeof(WattCurbSharedState));
            v2 = __atomic_load_n(&seq_version, __ATOMIC_ACQUIRE);
        } while ((v1 & 1) != 0 || v1 != v2);
        return true;
    }
};
static_assert(sizeof(WattCurbSharedState) == 128, "WattCurbSharedState must be exactly 128 bytes (2 cache lines)");
static_assert(std::is_trivially_copyable_v<WattCurbSharedState>, "WattCurbSharedState must be trivially copyable for memcpy and shm");

constexpr const char* SHARED_STATE_SHM_PATH = "/dev/shm/wattcurb_state.shm";
constexpr const char* CONTROL_SOCKET_PATH   = "/run/wattcurb.sock";

} // namespace wattcurb::ipc
