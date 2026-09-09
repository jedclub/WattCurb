# [REF-RES-005] Continuous PMU Milestone Benchmark & Optimization History

- **Ref-ID**: `REF-RES-005`
- **Related Requirements**: [`REF-REQ-006`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-003-pgo-pmu-optimization.md), [`REF-REQ-007`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-004-resident-daemon-direct-access.md)
- **Related Architecture**: [`REF-ARCH-003`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-003-pgo-pmu-pipeline.md), [`REF-ARCH-004`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-004-resident-daemon-event-loop.md)
- **Status**: Active Living Document

---

## 1. Purpose & LLM Operational Directives

This document tracks historical PMU (Performance Monitoring Unit) hardware benchmarks across development milestones. 

**Directive for LLM / AI Agents**:
- Before starting any major feature or refactor, review the most recent milestone metrics in this document.
- Use these metrics as empirical guardrails.
- Any change that degrades active CPU time, increases RSS memory, or introduces heap allocations must trigger an automated optimization cycle until performance matches or exceeds previous milestone benchmarks.

---

## 2. Milestone Benchmark Progression

| Milestone | Description | Active CPU Time (Task-Clock) | CPU Cycles | IPC | L1D Miss Rate | dTLB Miss Rate | Peak RSS | Est. Daemon Overhead (mW) | Evaluation Status |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **M0: Prototype Baseline** | Initial 2s profiler (Full 600-PID scan, full fd readlink scan, `std::string` comm) | 81.22 ms | 30.1 M | 1.62 | 0.78% | 0.008% | 12.18 MB | 44.6 mW | ⚠️ Unsatisfactory (High syscall tax) |
| **M1-A: SIMD & Stack Buffer Diet** | Zero-heap stack buffer, SIMD whitespace/token scanner, kernel thread kthread skip | 85.11 ms (User: 10.1ms) | 23.8 M | 1.65 | 0.62% (-35% misses) | 0.007% (-16% misses) | 10.4 MB | 41.2 mW | 🟢 Instruction (-19%) & Cache Miss (-35%) Cut |
| **M1-B: Lazy Deep Inspection** | Skip status/io/fd for idle PIDs, binary search delta tracking, DRM UID filter | 52.80 ms (User: 5.09ms) | 19.2 M | 1.51 | 0.56% (-42% misses) | 0.006% (-30% misses) | 9.8 MB | 26.1 mW | 🚀 User CPU 5.09ms, Cycles -36%, Instructions -40% |
| **M1: Target (Phase 2 Full Diet)** | Full Zero-Heap POD `ProcessSample`, openat directory caching | **< 8.0 ms** | **< 4.0 M** | **> 3.0** | **< 0.5%** | **< 0.005%** | **< 3.0 MB** | **< 3.0 mW** | 🎯 Target Milestone |

---

## 3. Detailed Milestone Reports

### Milestone M0: Initial Hardware Profiler Prototype
- **Date**: 2026-09-10
- **Configuration**: 2.083s observation window, ~600 monitored processes, PGO + LTO enabled.
- **Hardware PMU Counter Telemetry**:
  - `task-clock`: 81.22 ms (User: 11.8ms, Sys: 69.1ms)
  - `cycles`: 30,099,510
  - `instructions`: 48,829,998
  - `L1-dcache-load-misses`: 383,271
  - `dTLB-load-misses`: 4,153
  - `branch-misses`: 119,084 (1.05%)
  - `page-faults`: 206
  - `Peak RSS`: 12.18 MB (12,476 KB)
- **Analysis & Bottlenecks Identified**:
  1. 85% of time (69.1ms) spent in kernel syscalls (`sys`).
  2. Over 18,000 `readlink` calls scanning `/proc/[pid]/fd` for processes that do not use GPU.
  3. Reading `stat`, `status`, and `io` for 570 idle processes with zero tick delta.
  4. Memory RSS elevated by `std::string` heap allocations per process sample.
- **Action Items for Next Milestone (M1)**:
  - Implement Lazy Deep Inspection: skip `status`, `io`, and `fd/` for idle PIDs ($\Delta \text{ticks} == 0$).
  - Convert `ProcessSample` to fixed stack buffer (`std::array<char, 16> comm`).
  - Restrict DRM `fdinfo` inspection to known graphical/active processes.

---

### Milestone M1-A: Zero-Allocation Stack Buffer & SIMD Acceleration
- **Date**: 2026-09-10
- **Configuration**: 2.051s observation window, 446 monitored processes, PGO + LTO + AVX2/BMI2 active.
- **Hardware PMU Counter Telemetry**:
  - `task-clock`: 85.11 ms (User: 10.1ms, Sys: 74.8ms)
  - `cycles`: 23,824,837 (-20.8% reduction from M0: 30,099,510)
  - `instructions`: 39,401,223 (-19.3% reduction from M0: 48,829,998)
  - `L1-dcache-load-misses`: 248,026 (-35.3% reduction from M0: 383,271)
  - `dTLB-load-misses`: 3,475 (-16.3% reduction from M0: 4,153)
  - `Binary Size`: 72,488 bytes (-14.5% reduction from 84,776 bytes)
- **Key Breakthroughs Achieved**:
  1. Complete removal of `std::string` heap allocations in `read_small_file_buf` using 64-byte aligned stack buffers.
  2. AVX2 SIMD whitespace and token jumping in `parse_proc_stat`, saving 9.4 million instructions per 2-second pass.
  3. Kernel thread (`ppid == 2`) filtering, preventing hundreds of invalid `readlink` syscalls.
---

### Milestone M1-B: Lazy Deep Inspection & 5.09ms User CPU Breakthrough
- **Date**: 2026-09-10
- **Configuration**: 2.036s observation window, 442 monitored processes, PGO + LTO + AVX2 + Lazy Deep Inspection active.
- **Hardware PMU Counter Telemetry**:
  - `task-clock`: 52.80 ms (User: 5.09ms, Sys: 47.9ms)
  - `cycles`: 19,251,785 (-36.0% reduction from M0: 30,099,510)
  - `instructions`: 29,056,237 (-40.5% reduction from M0: 48,829,998)
  - `L1-dcache-load-misses`: 221,519 (-42.2% reduction from M0: 383,271)
  - `dTLB-load-misses`: 2,928 (-29.5% reduction from M0: 4,153)
  - `page-faults`: 204
  - `Binary Size`: 76,584 bytes (74.7 KB)
- **Key Breakthroughs Achieved**:
  1. **Lazy Deep Inspection**: Snapshot 2 compares against previous ticks via $O(\log N)$ binary search. 414 sleeping processes completely skip reading `status`, `io`, and `fd/`, eliminating thousands of kernel syscalls.
  2. **User CPU Active Time Cut in Half**: Pure user space task time dropped from 11.8ms to **5.09ms**.
  3. **Instruction Count Slashed by 40%**: Instructions per pass dropped from 48.8M to 29.0M.
  4. **L1D Cache Misses Dropped by 42%**: Only 221k misses across 2 seconds.
- **Next Target (M1 Full)**:
  - Phase 3: Zero-Wakeup Automated Process Mitigation Engine (`cgroup.freeze` and `SCHED_IDLE`).
