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
| **M1: Target (Phase 2 Extreme Diet)** | Lazy Deep Inspection (only active PIDs), `char comm[16]` zero-heap POD, selective DRM tracking | **< 8.0 ms** | **< 4.0 M** | **> 3.0** | **< 0.5%** | **< 0.005%** | **< 3.0 MB** | **< 3.0 mW** | 🎯 Target Milestone |

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
