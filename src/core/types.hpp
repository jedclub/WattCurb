#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace wattcurb {

// Implements REF-REQ-001, REF-REQ-010 & REF-ARCH-002
struct HardwareSample {
    std::chrono::steady_clock::time_point timestamp{};

    // 1. Power Supply & Battery Gas Gauge (REF-REQ-010 Sec 2.1)
    std::optional<uint64_t> battery_power_uw;
    std::optional<uint64_t> battery_voltage_uv;
    std::optional<int64_t> battery_current_ua;
    std::optional<uint64_t> battery_energy_now_uwh;
    std::optional<uint64_t> battery_energy_full_uwh;
    std::optional<uint64_t> battery_energy_full_design_uwh;
    std::optional<uint32_t> battery_cycle_count;
    std::optional<uint32_t> battery_capacity_percent;
    bool is_discharging{false};
    bool is_ac_online{false};

    // USB-C Power Delivery Input
    std::optional<uint64_t> usbc_pd_voltage_uv;
    std::optional<uint64_t> usbc_pd_current_ua;
    bool usbc_pd_online{false};

    // 2. CPU & Platform Subsystem (REF-REQ-010 Sec 2.2)
    std::optional<uint64_t> rapl_package_uj;
    std::optional<uint64_t> rapl_core_uj;
    std::optional<uint64_t> rapl_dram_uj;
    std::optional<int32_t> cpu_temp_mdeg; // k10temp / coretemp
    uint32_t cpu_freq_avg_khz{0};
    uint32_t cpu_freq_min_khz{0};
    uint32_t cpu_freq_max_khz{0};
    uint32_t cpu_cores_online{0};
    std::array<uint64_t, 4> cstate_time_us{}; // Aggregate POLL, C1, C2, C3
    std::array<char, 16> cpu_governor{};

    // 3. Graphics Processing Unit (AMDGPU / DRM) (REF-REQ-010 Sec 2.3)
    std::optional<uint64_t> gpu_power_uw; // Package Power Tracking (PPT)
    std::optional<uint32_t> gpu_busy_percent;
    std::optional<uint64_t> gpu_freq_hz;
    std::optional<int32_t> gpu_temp_mdeg;
    std::optional<uint32_t> gpu_vddgfx_mv;
    std::optional<uint32_t> gpu_vddsoc_mv;
    std::optional<uint64_t> gpu_vram_used_bytes;
    std::optional<uint64_t> gpu_vram_total_bytes;
    std::array<char, 32> gpu_pcie_link_speed{};
    std::optional<uint32_t> gpu_pcie_link_width;

    // 4. Storage Subsystem (NVMe SSD) (REF-REQ-010 Sec 2.4)
    bool nvme_active{false};
    std::optional<int32_t> nvme_temp_composite_mdeg;
    std::optional<int32_t> nvme_temp_sensor1_mdeg;
    uint64_t disk_read_sectors{0};
    uint64_t disk_write_sectors{0};
    uint64_t disk_io_ticks_ms{0};

    // 5. Thermal & Mechanical Chassis (ThinkPad EC) (REF-REQ-010 Sec 2.5)
    std::optional<uint32_t> fan_rpm;
    std::optional<uint32_t> fan_pwm;
    std::optional<int32_t> chassis_temp_mdeg;
    std::optional<uint32_t> kbdlight_level;
    std::optional<bool> bluetooth_enabled;

    // 6. Display Subsystem (REF-REQ-010 Sec 2.6)
    std::optional<uint32_t> backlight_brightness;
    std::optional<uint32_t> backlight_max_brightness;

    // 7. Bus & Wireless Peripherals (REF-REQ-010 Sec 2.7)
    bool wifi_active{false};
    std::optional<int32_t> wifi_temp_mdeg;
    std::array<char, 32> aspm_policy{};

    // 8. Syscall-Level Direct Hardware Telemetry (REF-REQ-015)
    uint64_t pmu_instructions{0};
    uint64_t pmu_cycles{0};
    double pmu_ipc{0.0};
    uint64_t pmu_llc_misses{0};
    std::optional<uint32_t> cpu_core_vid_mv; // Silicon Core Voltage (mV) via MSR / hwmon
    uint8_t pcie_link_speed_gen{0};          // Binary Config Space: PCIe Gen (1-5)
    uint8_t pcie_link_width_lanes{0};        // Binary Config Space: Negotiated Width (1-16)
};

// Zero-Allocation Fixed-Capacity Process Comm (REF-REQ-009, REF-ARCH-005)
// TASK_COMM_LEN in Linux kernel is strictly 16 bytes.
// Making ProcessSample TriviallyCopyable unlocks SIMD vector memmove/sort.
struct ProcessComm {
    std::array<char, 16> data{};

    ProcessComm() noexcept = default;
    ProcessComm(std::string_view sv) noexcept {
        size_t len = std::min(sv.size(), size_t{15});
        std::memcpy(data.data(), sv.data(), len);
        data[len] = '\0';
    }
    ProcessComm(const char* s) noexcept {
        if (!s) return;
        size_t len = std::min(std::strlen(s), size_t{15});
        std::memcpy(data.data(), s, len);
        data[len] = '\0';
    }

    [[nodiscard]] const char* c_str() const noexcept { return data.data(); }
    [[nodiscard]] std::string_view view() const noexcept { return std::string_view(data.data()); }
    [[nodiscard]] size_t size() const noexcept { return std::strlen(data.data()); }
    [[nodiscard]] bool empty() const noexcept { return data[0] == '\0'; }

    [[nodiscard]] std::string substr(size_t pos = 0, size_t count = std::string_view::npos) const {
        return std::string(view().substr(pos, count));
    }

    bool operator==(std::string_view sv) const noexcept { return view() == sv; }
    bool operator==(const char* s) const noexcept { return std::strcmp(data.data(), s) == 0; }
    bool operator==(const ProcessComm& o) const noexcept { return data == o.data; }

    friend std::ostream& operator<<(std::ostream& os, const ProcessComm& pc) {
        return os << pc.data.data();
    }
};

// Implements REF-REQ-004, REF-REQ-011 & REF-ARCH-002
// TriviallyCopyable POD: Zero-allocation, L1D-cache aligned
struct ProcessSample {
    int32_t pid{0};
    int32_t ppid{0};
    ProcessComm comm{};
    uint32_t uid{0};
    uint64_t utime_ticks{0};
    uint64_t stime_ticks{0};
    uint64_t voluntary_ctxt_switches{0};
    uint64_t nonvoluntary_ctxt_switches{0};
    uint64_t read_bytes{0};
    uint64_t write_bytes{0};
    uint64_t io_syscalls{0};
    uint64_t drm_engine_gfx_ns{0};
    uint64_t drm_engine_compute_ns{0};
    uint64_t drm_engine_dec_ns{0};
    uint64_t drm_engine_enc_ns{0};
    uint64_t drm_vram_kib{0};

    // Deep Process Physical Telemetry (REF-REQ-013)
    int32_t cpu_core{-1};
    uint32_t num_threads{1};
    uint64_t minflt{0};
    uint64_t majflt{0};
    uint64_t pss_kib{0};
    uint64_t rss_kib{0};
    uint64_t timerslack_ns{50000};
    uint32_t open_sockets{0};
};

static_assert(std::is_trivially_copyable_v<ProcessSample>, "ProcessSample must be TriviallyCopyable for SIMD acceleration");


// Implements REF-REQ-001, REF-REQ-010 & REF-RES-002
struct HardwarePowerBreakdown {
    // Hardware Domain Power (Watts)
    double total_system_watts{0.0};
    double cpu_package_watts{0.0};
    double gpu_watts{0.0};
    double display_watts{0.0};
    double fan_estimated_watts{0.0};
    double storage_estimated_watts{0.0};
    double uncore_and_platform_watts{0.0};

    // Operational States
    bool is_battery_discharging{false};
    bool is_ac_online{false};
    bool has_direct_rapl{false};

    // Battery Health & Charging
    double battery_health_percent{0.0};
    double battery_remaining_hours{0.0};
    uint32_t battery_cycle_count{0};
    uint32_t battery_capacity_percent{0};
    double usbc_input_watts{0.0};
    bool usbc_online{false};

    // CPU & Platform Telemetry
    double cpu_temp_c{0.0};
    double cpu_freq_avg_mhz{0.0};
    double cpu_freq_min_mhz{0.0};
    double cpu_freq_max_mhz{0.0};
    std::string cpu_governor;
    double cstate_c0_active_percent{0.0};
    double cstate_c1_percent{0.0};
    double cstate_c2_percent{0.0};
    double cstate_c3_deep_percent{0.0};

    // GPU Telemetry
    uint32_t gpu_busy_percent{0};
    double gpu_freq_mhz{0.0};
    double gpu_temp_c{0.0};
    double gpu_vddgfx_v{0.0};
    double gpu_vddsoc_v{0.0};
    double gpu_vram_used_mb{0.0};
    double gpu_vram_total_mb{0.0};
    std::string gpu_pcie_link;

    // Storage Telemetry
    std::string nvme_status;
    double nvme_temp_c{0.0};
    double disk_read_mb_per_sec{0.0};
    double disk_write_mb_per_sec{0.0};

    // Chassis & Cooling
    uint32_t fan_rpm{0};
    uint32_t kbdlight_level{0};
    bool bluetooth_enabled{false};

    // Display & Wireless
    double display_brightness_percent{0.0};
    std::string wifi_status;
    double wifi_temp_c{0.0};
    std::string aspm_policy;

    // Direct Syscall Hardware Telemetry (REF-REQ-015)
    uint64_t pmu_instructions{0};
    uint64_t pmu_cycles{0};
    double pmu_ipc{0.0};
    uint64_t pmu_llc_misses{0};
    std::optional<uint32_t> cpu_core_vid_mv;
    uint8_t pcie_link_speed_gen{0};
    uint8_t pcie_link_width_lanes{0};
};

// Implements REF-REQ-004, REF-REQ-011 & REF-RES-003
struct ProcessAttributedPower {
    int32_t pid{0};
    ProcessComm comm{};
    uint32_t uid{0};
    double cpu_watts{0.0};
    double gpu_watts{0.0};
    double io_watts{0.0};
    double wakeup_tax_watts{0.0};
    double fan_attributed_watts{0.0};
    double total_attributed_watts{0.0};
    double wdi_score{0.0}; // WattCurb Drain Index
    uint64_t wakeups_per_sec{0};
    uint64_t vram_kib{0};
    double disk_io_mb_per_sec{0.0};
    bool is_runaway_candidate{false};

    // Deep Process Physical Telemetry (REF-REQ-013)
    int32_t cpu_core{-1};
    uint32_t num_threads{1};
    bool cross_ccx_migration{false};
    uint64_t timerslack_ns{50000};
    uint64_t pss_kib{0};
    uint64_t minflt_per_sec{0};
    uint64_t majflt_per_sec{0};
    uint32_t open_sockets{0};
    double wifi_attributed_watts{0.0};
    double dram_attributed_watts{0.0};

    std::string primary_hw_domain;  // e.g. "GPU Silicon", "CPU C-State Wakeup", "CPU Compute", "NVMe Storage"
    std::string hardware_mechanism; // e.g. "AMDGPU GFX Engine (455MB VRAM, 98% GPU)"
};


// Implements REF-REQ-011 (Hardware Domain Direct Attribution)
struct ProcessDomainShare {
    int32_t pid{0};
    ProcessComm comm{};
    double watts{0.0};
    double share_percent{0.0};
    std::string detail;
};

struct DomainCulprit {
    std::string domain_name;
    double domain_total_watts{0.0};
    std::vector<ProcessDomainShare> top_culprits;
};

// Implements REF-REQ-005, REF-REQ-011, REF-REQ-012 & REF-ARCH-002
struct AnalysisReportData {
    std::chrono::milliseconds sample_duration{0};
    HardwarePowerBreakdown hardware;
    std::vector<ProcessAttributedPower> top_processes;
    std::vector<DomainCulprit> domain_culprits;
    size_t total_monitored_processes{0};
    uint64_t total_system_wakeups_per_sec{0};
    size_t sample_count{1};
    double total_energy_joules{0.0};
    bool is_short_window{false};
};


} // namespace wattcurb
