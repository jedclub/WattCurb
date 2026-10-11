# [REF-ARCH-089] Scoped Profiler TSC Registry & 30-Minute Telemetry Topology

## 1. Architectural Topology

```
┌────────────────────────────────────────────────────────────────────────┐
│                        Function Under Test                             │
│  WATTCURB_PROFILE_SCOPE("proc.stat_parse");                            │
│    │                                                                   │
│    ├── First Call: ScopedProfilerRegistry::register_entry("name")       │
│    │   └── Returns static const size_t slot_idx (Cached forever)       │
│    │                                                                   │
│    ├── Scope Entry (Constructor): uint64_t start = read_tsc();        │
│    └── Scope Exit (Destructor):                                        │
│          uint64_t end = read_tsc();                                    │
│          ScopedProfilerRegistry::record_fast(slot_idx, end - start);   │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ O(1) Atomic Array Index
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│               ScopedProfilerRegistry (Fixed Array, 256 Slots)          │
│                                                                        │
│  Slot 0: "daemon.light_probe_cycle"   calls=18000  cycles=... min/max  │
│  Slot 1: "proc.stat_parse"            calls=540000 cycles=... min/max  │
│  Slot 2: "hw.cpu_rapl_temp"           calls=18000  cycles=... min/max  │
│  ...                                                                   │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
                                    ├── IPC: GET_PROFILE_REPORT
                                    │     └── Real-time ASCII Breakdown
                                    │
                                    └── 30-Min Session Completion / Exit
                                          └── Dump to tmp/wattcurb_profile_30min.log
```

---

## 2. Lock-Free Cache-Aligned ProfileEntry Structure

```cpp
struct alignas(64) ProfileEntry {
    const char* name{nullptr};
    std::atomic<uint64_t> invocations{0};
    std::atomic<uint64_t> total_tsc{0};
    std::atomic<uint64_t> min_tsc{UINT64_MAX};
    std::atomic<uint64_t> max_tsc{0};
};
```
- Each entry is aligned to 64 bytes to eliminate false sharing across threads.
- Invocations and cumulative cycles use `std::memory_order_relaxed`, which maps directly to `lock add` or single atomic registers on x86-64 without full memory barriers.

---

## 3. O(1) Macro Mechanism

```cpp
#define WATTCURB_PROFILE_SCOPE(name) \
    static const size_t _prof_idx_##__LINE__ = \
        ::wattcurb::core::ScopedProfilerRegistry::instance().register_entry(name); \
    ::wattcurb::core::ScopedProfiler _prof_scope_##__LINE__(_prof_idx_##__LINE__)
```
By binding `static const size_t` to the call site's line, C++ guarantees thread-safe one-time initialization, entirely eliminating hash lookups or `strcmp` in steady state.
