# [REF-ARCH-047] 7-Day (60,480-Slot, ~1.85MB) In-Memory Ring-Buffer Architecture

## 1. Architectural Overview

This architecture implements the 7-day in-memory telemetry history ring buffer specified in [`REF-REQ-070`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-070-7-day-telemetry-history-ring-buffer.md).

```
+-----------------------------------------------------------------------------+
|               Linux RAM Shared Memory: /dev/shm/wattcurb_history.shm        |
|               Total Allocated Size: Exactly 1,935,424 Bytes (~1.85 MiB)     |
+-----------------------------------------------------------------------------+
| Header (64 Bytes, alignas(64))                                              |
| - seq_version: uint64_t (Seqlock Writer Counter)                            |
| - capacity   : uint32_t = 60,480                                            |
| - head_index : uint32_t [0 .. 60,479]                                       |
| - count      : uint32_t [0 .. 60,480]                                       |
| - reserved   : uint8_t[44]                                                  |
+-----------------------------------------------------------------------------+
| Ring Buffer Array (60,480 x 32 Bytes = 1,935,360 Bytes)                     |
| - entries[0]       : HistoryPoint (32 bytes) [Oldest or Active]            |
| - entries[1]       : HistoryPoint (32 bytes)                                |
|   ...                                                                       |
| - entries[60,479]  : HistoryPoint (32 bytes)                                |
+-----------------------------------------------------------------------------+
```

---

## 2. Structural Layout in `src/ipc/history_ring_buffer.hpp`

```cpp
namespace wattcurb::ipc {

constexpr const char* HISTORY_SHM_PATH = "/dev/shm/wattcurb_history.shm";

// Implements REF-REQ-059: 32-Byte Packed History Point
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
    uint8_t  reserved[7]{0};
};
static_assert(sizeof(HistoryPoint) == 32, "HistoryPoint must be exactly 32 bytes");
static_assert(std::is_trivially_copyable_v<HistoryPoint>, "HistoryPoint must be TriviallyCopyable");

// Implements REF-REQ-070 & REF-ARCH-047: 7-Day In-Memory Ring Buffer (~1.85 MiB)
struct alignas(64) HistoryRingBufferShm {
    static constexpr size_t CAPACITY = 60480; // 7 days at 10s intervals

    uint64_t seq_version{0};     // Seqlock: Odd = writing, Even = stable
    uint32_t capacity{CAPACITY};
    uint32_t head_index{0};      // Insertion pointer [0 .. CAPACITY - 1]
    uint32_t count{0};           // Current valid entries count [0 .. CAPACITY]
    uint8_t  reserved[44]{0};    // Pad header to exactly 64 bytes

    HistoryPoint entries[CAPACITY]; // 60,480 * 32 = 1,935,360 bytes
...
};

static_assert(sizeof(HistoryRingBufferShm) == 64 + 60480 * 32, "HistoryRingBufferShm layout must be 1935424 bytes");

} // namespace wattcurb::ipc
```

---

## 3. Dynamic Version Migration in `DaemonRunner::setup_history_shm`

To ensure seamless transitions across binary upgrades without manual `/dev/shm` deletion:
1. Attempt `open(HISTORY_SHM_PATH, O_RDWR | O_CLOEXEC)`.
2. Inspect `fstat(fd, &st)`.
3. If `st.st_size != sizeof(HistoryRingBufferShm)`, close and `unlink(HISTORY_SHM_PATH)` immediately.
4. Re-open with `O_CREAT` and `ftruncate` to the exact 1,935,424 byte size.
5. Initialize the Seqlock header with `capacity = CAPACITY`.
