# [REF-REQ-016] Deep Analysis Scope & Multi-Dimensional Process Diagnostics Specification

- **Ref-ID**: `REF-REQ-016`
- **Title**: Deep Analysis Scope: Multi-Dimensional Process Physical Diagnostics, Micro-Scope Instrumentations, and Memory/DRAM Domain Culprits
- **Status**: Approved
- **Authors**: Antigravity Architecture Team
- **Related Requirements**: [`REF-REQ-005`](./REQ-002-profiler-reporting-engine.md), [`REF-REQ-011`](./REQ-008-process-hardware-feature-tracking.md), [`REF-REQ-013`](./REQ-010-deep-process-power-tracking.md), [`REF-REQ-014`](./REQ-011-zero-overhead-scoped-profiler.md)
- **Related Architecture**: [`REF-ARCH-002`](../architecture/ARCH-002-profiler-implementation.md), [`REF-ARCH-005`](../architecture/ARCH-005-zero-cost-cpuid-simd.md)
- **Related Benchmarks**: [`REF-RES-005`](../research/PMU_BENCHMARKS.md) (Milestone M11)

---

## 1. Executive Summary & Objective

To provide exhaustive forensic transparency into system power drain without degrading zero-overhead execution guarantees, this specification expands WattCurb's analysis scope across three dimensions:
1. **Multi-Dimensional Process Physical Diagnostics**: Enriches per-process attribution with active worker thread counts (`num_threads`), memory page fault velocity (`minflt/s`, `majflt/s`), direct storage throughput (`disk_io_mb_per_sec`), scheduling priority/nice (`nice`), and individual DRAM retention power (`dram_watts`).
2. **Hardware Domain Culprit Expansion**: Introduces dedicated domain causal attribution for the **Memory & DRAM Subsystem** (PSS DRAM retention & bus churn) and **Core Thrashing / CCX Migration**.
3. **Fine-Grained Subsystem Micro-Scope Profiling**: Subdivides inner daemon loops into high-precision micro-scopes, tracking sub-microsecond latencies across VFS operations, parsing units, and policy attribution passes.

---

## 2. Functional Requirements

### 2.1 Process Diagnostics Metrics (REF-REQ-016.1)
The process inspection pipeline must extract and compute the following telemetry metrics with zero heap allocation:
- **`num_threads`**: Number of active kernel task contexts per process, indicating parallel core wakeups.
- **`majflt_per_sec` & `minflt_per_sec`**: Page fault velocity directly correlating with memory bus contention and storage block reads.
- **`disk_io_mb_per_sec`**: Cumulative read and write data rates ($(\Delta \text{read\_bytes} + \Delta \text{write\_bytes}) / \Delta t$) preventing storage APST idle sleep.
- **`nice` & `priority`**: Task scheduling niceness ($-20$ to $+19$), distinguishing high-priority interactive tasks from background runaway workloads.
- **`dram_attributed_watts`**: Physical DRAM power attributed to process PSS footprint ($P_{\text{DRAM}} \times (\text{PSS} / \text{Total\_DRAM})$).

### 2.2 Hardware Domain Culprit Expansion (REF-REQ-016.2)
The causal attribution engine must group and rank processes into six distinct physical hardware domains:
1. **GPU Silicon (AMDGPU / DRM Engine & VRAM)**
2. **CPU C-State Sleep Breakers (Preventing C3 Deep Sleep)**
3. **Mechanical Cooling Fan (Thermally Induced Dissipation)**
4. **Storage & NVMe Subsystem (APST Disrupters & I/O Rates)**
5. **WiFi Wireless Transceiver (Active Sockets in CAM Mode)**
6. **Memory & DRAM Subsystem (PSS Retention & Memory Bus Thrashing)**

### 2.3 Fine-Grained Subsystem Micro-Scopes (REF-REQ-016.3)
The developmental scoped profiler (`ScopedProfiler`) must track isolated micro-scopes:
- `proc.root_getdents`: Kernel directory streaming of root `/proc`.
- `proc.kthread_filter`: Binary search filtering of kernel threads.
- `proc.stat_read` & `proc.stat_parse`: Sysfs VFS read and AVX2 SIMD token parsing of `/stat`.
- `proc.status_read_parse`: Parsing of `/status` (UID, context switches).
- `proc.io_read_parse`: Parsing of `/io` (read/write bytes, syscalls).
- `proc.statm_read_parse`: Parsing of `/statm` (PSS, RSS pages).
- `proc.timerslack_read`: Caching of `timerslack_ns`.
- `proc.fd_socket_scan`: Directory scan of `/fd`.
- `proc.fd_readlink_loop`: Symlink evaluation and socket counting.
- `proc.fd_drm_fdinfo`: Extraction of GPU engine statistics.
- `policy.attr_pass1`: Domain calculation passes (GPU, CPU, Fan, WiFi, Storage, DRAM).
- `policy.wdi_ranking`: WattCurb Drain Index sorting and normalization.

---

## 3. Data Structure Specifications

```cpp
struct ProcessSample {
    // ... existing fields ...
    int32_t nice{0};
    int32_t priority{0};
};

struct ProcessAttributedPower {
    // ... existing fields ...
    int32_t nice{0};
    int32_t priority{0};
    double dram_attributed_watts{0.0};
    double disk_io_mb_per_sec{0.0};
    uint64_t minflt_per_sec{0};
    uint64_t majflt_per_sec{0};
};
```

---

## 4. Verification & Oracle Gate Acceptance Criteria

1. **Zero-Allocation Invariant**: `ProcessSample` and `ProcessAttributedPower` must remain trivially copyable POD structures.
2. **Dashboard Fidelity**: The rendered terminal table must cleanly display Thread counts, Page Faults, Disk I/O, DRAM watts, and PSS memory alongside existing WDI and core metrics.
3. **Overhead Invariant**: Total daemon active CPU consumption must remain $< 0.03\%$ on the 30-second benchmark baseline (`--duration 30 -i 2`).
