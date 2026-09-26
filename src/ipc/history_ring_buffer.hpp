#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace wattcurb::ipc {

constexpr const char* HISTORY_SHM_PATH = "/dev/shm/wattcurb_history.shm";

// Implements REF-REQ-059, REF-REQ-129 & REF-ARCH-035, REF-ARCH-076: 32-Byte Packed History Point
struct alignas(32) HistoryPoint {
    uint64_t timestamp_sec{0};
    uint32_t total_system_mw{0};
    uint16_t cpu_package_mw{0};
    uint16_t gpu_mw{0};
    uint16_t cpu_temp_c{0};
    uint16_t cpu_freq_mhz{0};
    uint8_t  battery_percent{0};
    uint8_t  battery_state{0};       // 0=AC, 1=Discharging, 2=Passthrough
    uint8_t  power_profile_mode{0}; // 0=Perf, 1=Balanced, 2=Save, 3=Ultra
    uint8_t  cstate_c3_percent{0};
    uint8_t  active_mitigations{0};
    uint8_t  reserved[1]{0};
    uint16_t mem_used_mb{0};        // REF-REQ-129: System memory used (MB)
    uint16_t swap_used_mb{0};       // REF-REQ-129: System swap used (MB)
    uint16_t top_proc_pss_mb{0};    // REF-REQ-129: Top process PSS (MB)
};
static_assert(sizeof(HistoryPoint) == 32, "HistoryPoint must be exactly 32 bytes");
static_assert(std::is_trivially_copyable_v<HistoryPoint>, "HistoryPoint must be TriviallyCopyable");

// Implements REF-REQ-059, REF-REQ-070 & REF-ARCH-047: 7-Day Lockless In-Memory Ring Buffer (~1.85 MiB)
struct alignas(64) HistoryRingBufferShm {
    static constexpr size_t CAPACITY = 60480; // 7 days (168 hours) at 10s intervals (604,800 / 10 = 60,480)

    uint64_t seq_version{0};     // Seqlock: Odd = writing, Even = stable
    uint32_t capacity{CAPACITY};
    uint32_t head_index{0};      // Insertion pointer [0 .. CAPACITY - 1]
    uint32_t count{0};           // Current valid entries count [0 .. CAPACITY]
    uint8_t  reserved[44]{0};    // Pad header to exactly 64 bytes

    HistoryPoint entries[CAPACITY]; // 60,480 * 32 = 1,935,360 bytes

    void append(const HistoryPoint& pt) noexcept {
        uint64_t ver = __atomic_load_n(&seq_version, __ATOMIC_RELAXED);
        __atomic_store_n(&seq_version, ver + 1, __ATOMIC_RELEASE); // Odd: writer active

        uint32_t idx = head_index;
        entries[idx] = pt;
        head_index = (idx + 1) % CAPACITY;
        if (count < CAPACITY) {
            count++;
        }

        __atomic_store_n(&seq_version, ver + 2, __ATOMIC_RELEASE); // Even: stable
    }

    // Thread-safe lockless reader (reads in chronological order: oldest to newest)
    bool read_snapshot(HistoryPoint* out_entries, uint32_t max_entries, uint32_t& out_count) const noexcept {
        if (!out_entries || max_entries == 0) return false;

        for (int retry = 0; retry < 5; ++retry) {
            uint64_t v1 = __atomic_load_n(&seq_version, __ATOMIC_ACQUIRE);
            if (v1 & 1) continue; // Writer is actively updating

            uint32_t c = std::min(count, max_entries);
            uint32_t head = head_index;
            // When wrapped, oldest element is at head_index
            uint32_t start = (count < CAPACITY) ? 0 : head;

            for (uint32_t i = 0; i < c; ++i) {
                uint32_t src_idx = (start + i) % CAPACITY;
                out_entries[i] = entries[src_idx];
            }
            out_count = c;

            uint64_t v2 = __atomic_load_n(&seq_version, __ATOMIC_ACQUIRE);
            if (v1 == v2) return true;
        }
        return false;
    }
};

static_assert(sizeof(HistoryRingBufferShm) == 64 + 60480 * 32, "HistoryRingBufferShm layout must be exactly 1935424 bytes (~1.85 MiB)");

} // namespace wattcurb::ipc
