# [REF-REQ-142] Ultra-Low-Overhead Function-Level Scoped Profiling & 30-Minute Continuous Session

## 1. Overview & Objectives

To identify hot-path optimization opportunities across WattCurb's monitoring, procfs parsing, RAPL sampling, attribution, and mitigation subsystems, the daemon must support a **high-precision, function-level scoped profiling architecture**.

This profiler must strictly adhere to the following design constraints:
1. **Zero Dynamic Allocation & O(1) Direct Slot Dispatch**: No string comparisons (`strcmp`) or linear scans during hot-path execution. Profiling call sites must resolve their registry slot statically upon first invocation.
2. **Sub-10ns Latency Overhead (TSC-Only)**: Invocations must avoid vDSO (`clock_gettime`) and syscall overhead by measuring exclusively via hardware Time Stamp Counter (`rdtsc`/`rdtscp`).
3. **Thread-Safe & Lock-Free Statistics**: Invocation counts, cumulative TSC cycles, minimum cycles, and maximum cycles must be updated lock-free using relaxed atomic primitives.
4. **30-Minute Continuous Session & Real-Time Extraction**: The daemon must accumulate fine-grained profiling data over sustained 30-minute operational sessions without memory drift or cache pollution, supporting on-demand IPC dump queries (`GET_PROFILE_REPORT`) and automatic file dumps upon session completion.

---

## 2. Quantitative Performance & Profiling Invariants

1. **Scope Entry/Exit Overhead**:
   - Entry: `read_tsc()` (~15-20 cycles, < 6 ns).
   - Exit: `read_tsc()` + O(1) index atomic relaxed updates (~25-35 cycles, < 10 ns).
   - Total overhead per instrumented scope: **< 15 ns**.
2. **Memory Footprint**:
   - Capacity: Up to 256 distinct instrumented functions/scopes.
   - Registry Memory: Fixed contiguous array (< 16 KB), fitting entirely inside L1/L2 cache.
3. **Session Duration & Reporting**:
   - Default deep profiling window: 30 minutes (1,800 seconds).
   - Output metrics: Call count, total execution time (ms), cycle share (%), average time per operation (us/op), average cycles per operation, min/max latency.

---

## 3. Scope Instrumentation Matrix

The following core functional subsystems must be comprehensively instrumented:
- `proc.*`: Process list scan (`getdents`), `/proc/[pid]/stat`, `/proc/[pid]/statm`, `/proc/[pid]/io`, `/proc/[pid]/status`, DRM fdinfo scanner.
- `hw.*`: RAPL package/core/uncore energy reads, GPU hwmon/sysfs reads, battery uevent parsing, NVMe/block power states, WiFi power management status.
- `policy.*`: Windowed energy attribution pass, Weighted Drain Index (WDI) ranking, process classification, Mitigation actuation, cgroup freezer / nice / affinity caps.
- `daemon.*`: Light probe cycle (100ms cadence), deep observation cycle (3s cadence), Seqlock shared state updates, history ring-buffer append, IPC dispatch.
