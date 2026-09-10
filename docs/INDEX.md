# WattCurb Documentation & Reference Index

This document serves as the master registry for all specifications, research documents, architectural decisions, and verification tests across the **WattCurb** project.

All documents follow the Ref-ID naming standard:
- `REF-REQ-xxx`: Functional and Non-Functional Requirements
- `REF-RES-xxx`: Research, Prior Art, Papers, and Benchmarks
- `REF-ARCH-xxx`: Architecture, Subsystem Design, and Data Structures
- `REF-TEST-xxx`: Unit Tests, Benchmarks, and Oracle Gate Specifications

---

## 1. Requirements Registry (`docs/requirements/`)

| Ref ID | Title | Status | Related Documents |
| :--- | :--- | :--- | :--- |
| [`REF-REQ-001`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-001-hardware-power-profiling.md) | Physical Hardware Power Profiling Requirements | Draft | [`REF-RES-001`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-001-prior-art-and-hardware-telemetry.md), [`REF-RES-002`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-002-hardware-power-measurement-mechanisms.md), [`REF-RES-003`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-003-hardware-to-process-attribution.md) |
| [`REF-REQ-002`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-001-hardware-power-profiling.md#ref-req-002) | Zero-Wakeup Daemon & Singleton Execution | Draft | [`REF-ARCH-001`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-001-daemon-architecture.md) |
| [`REF-REQ-003`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-001-hardware-power-profiling.md#ref-req-003) | Automated Evaluation & Oracle Gate Testing | Draft | [`REF-TEST-001`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-001-daemon-architecture.md#ref-test-001) |
| [`REF-REQ-004`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-001-hardware-power-profiling.md#24-ref-req-004-hardware-to-process-power-attribution) | Hardware-to-Process Power Attribution | Draft | [`REF-RES-003`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-003-hardware-to-process-attribution.md) |
| [`REF-REQ-005`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-002-profiler-reporting-engine.md) | Detailed Hardware-to-Software Power Profiler & Report Generator | Approved | [`REF-ARCH-002`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-002-profiler-implementation.md) |
| [`REF-REQ-006`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-003-pgo-pmu-optimization.md) | Release Optimization, PMU/ASM Analysis & PGO Pipeline | Approved | [`REF-ARCH-003`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-003-pgo-pmu-pipeline.md) |
| [`REF-REQ-007`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-004-resident-daemon-direct-access.md) | Resident Background Daemon & Direct Kernel/Hardware Access Specification | Approved | [`REF-ARCH-004`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-004-resident-daemon-event-loop.md) |
| [`REF-REQ-008`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-005-zero-residue-release.md) | Total Elimination of Debug/Trace/Log Artifacts in Production Release | Approved | [`REF-ARCH-003`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-003-pgo-pmu-pipeline.md), [`REF-ARCH-004`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-004-resident-daemon-event-loop.md) |
| [`REF-REQ-009`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-006-cpuid-simd-optimization.md) | Compile-Time SIMD & Dynamic CPUID Hardware Specialization Specification | Approved | [`REF-ARCH-003`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-003-pgo-pmu-pipeline.md), [`REF-ARCH-004`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-004-resident-daemon-event-loop.md) |
| [`REF-REQ-010`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-007-extreme-hardware-telemetry.md) | Full-Domain Physical Hardware Power & Telemetry Probe Specification | Approved | [`REF-ARCH-002`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-002-profiler-implementation.md), [`REF-ARCH-004`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-004-resident-daemon-event-loop.md) |
| [`REF-REQ-011`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-008-process-hardware-feature-tracking.md) | Process-to-Hardware Feature Attribution & Physical Causation Tracking Engine | Approved | [`REF-ARCH-002`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-002-profiler-implementation.md), [`REF-RES-003`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-003-hardware-to-process-attribution.md) |
| [`REF-REQ-012`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-009-continuous-window-evaluation.md) | Multi-Sample Continuous Window Evaluation & Steady-State Telemetry | Approved | [`REF-ARCH-002`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-002-profiler-implementation.md), [`REF-ARCH-004`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-004-resident-daemon-event-loop.md) |
| [`REF-REQ-013`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-010-deep-process-power-tracking.md) | Deep Process Physical Telemetry (CCX, Timer Slack, PSS DRAM, WiFi CAM) | Approved | [`REF-ARCH-002`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-002-profiler-implementation.md), [`REF-ARCH-005`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-005-zero-cost-cpuid-simd.md) |
| [`REF-REQ-014`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-011-zero-overhead-scoped-profiler.md) | Zero-Overhead Scoped Subsystem Profiler & PMU Cost Probe | Approved | [`REF-REQ-008`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-005-zero-residue-release.md), [`REF-RES-005`](file:///home/jedclub/Develop/WattCurb/docs/research/PMU_BENCHMARKS.md) |
| [`REF-REQ-015`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-012-syscall-level-hardware-telemetry.md) | Syscall-Level Direct Hardware Telemetry (perf_event_open, PCIe Binary Config, AMD Zen MSR) | Approved | [`REF-REQ-007`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-004-resident-daemon-direct-access.md), [`REF-REQ-010`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-007-extreme-hardware-telemetry.md), [`REF-RES-005`](file:///home/jedclub/Develop/WattCurb/docs/research/PMU_BENCHMARKS.md) |
| [`REF-REQ-016`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-013-deep-analysis-scope.md) | Deep Analysis Scope: Multi-Dimensional Process Diagnostics & DRAM Culprits | Approved | [`REF-REQ-005`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-002-profiler-reporting-engine.md), [`REF-REQ-011`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-008-process-hardware-feature-tracking.md), [`REF-REQ-013`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-010-deep-process-power-tracking.md) |
| [`REF-REQ-017`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-014-custom-containers.md) | Custom Zero-Allocation High-Performance Containers (FixedVector, FixedString, TopKHeap) | Approved | [`REF-ARCH-006`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-006-custom-containers.md), [`REF-REQ-004`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-002-zero-allocation-procfs.md) |



---

## 2. Research & Prior Art Registry (`docs/research/`)

| Ref ID | Title | Focus Area | Date |
| :--- | :--- | :--- | :--- |
| [`REF-RES-001`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-001-prior-art-and-hardware-telemetry.md) | Prior Art, Academic Papers, and Hardware Energy Telemetry Survey | RAPL, Scaphandre, Kepler, TLP, Powertop, GPU/NVMe | 2026-09-10 |
| [`REF-RES-002`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-002-hardware-power-measurement-mechanisms.md) | Physical Hardware-Level Power Measurement Mechanisms in Modern Linux | RAPL (MSR/sysfs), Battery gas gauge, GPU hwmon/NVML, Backlight PWM, NVMe APST | 2026-09-10 |
| [`REF-RES-003`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-003-hardware-to-process-attribution.md) | Hardware-to-Process Power Attribution: Correlating Physical Energy Drain with Software Workloads | DRM fdinfo GPU telemetry, CPU execution quantum, Wakeup Tax, Storage APST penalty, WattCurb Drain Index (WDI) | 2026-09-10 |
| [`REF-RES-004`](file:///home/jedclub/Develop/WattCurb/docs/research/PGO_PMU_REPORT.md) | WattCurb PGO & PMU Hardware Performance Audit Report | Hardware PMU counters (IPC 3.64, L1D, dTLB), ASM inspection | 2026-09-10 |
| [`REF-RES-005`](file:///home/jedclub/Develop/WattCurb/docs/research/PMU_BENCHMARKS.md) | Continuous PMU Milestone Benchmark & Optimization History | Living PMU tracking history across development milestones | 2026-09-10 |
| [`REF-RES-006`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-006-syscall-level-kernel-telemetry-optimization.md) | Deep Kernel & Syscall-Level Telemetry Optimization Research | procfs / sysfs VFS syscall reduction, DRM FD pinning, kthread mask | 2026-09-10 |
| [`REF-RES-007`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-007-deep-kernel-primitives-and-simd-isa.md) | Radical Syscall Elimination: Single-Read uevent, /proc getdents64, C++23 SIMD | BAT0/uevent single read, /proc direct getdents64, Socket classifier, Fan EC decoupling | 2026-09-10 |

---

## 3. Architecture Registry (`docs/architecture/`)

| Ref ID | Title | Scope | Status |
| :--- | :--- | :--- | :--- |
| [`REF-ARCH-001`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-001-daemon-architecture.md) | High-Level Daemon Architecture & Subsystem Specification | Overall C++23 Pipeline & Oracle Gate | Draft |
| [`REF-ARCH-002`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-002-profiler-implementation.md) | Hardware Profiler & Report Generator Architecture | Profiler Engine & Attribution Implementation | Approved |
| [`REF-TEST-002`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-002-profiler-implementation.md#3-ref-test-002-oracle-gate-test-specifications) | Profiler Unit Testing & Oracle Gate Benchmarks | Unit Tests, Allocations, and Oracle Gate | Approved |
| [`REF-ARCH-003`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-003-pgo-pmu-pipeline.md) | Release Build Pipeline: PGO, Hardware PMU Telemetry & ASM Verification | PGO 2-stage compiler pipeline & ASM verification | Approved |
| [`REF-TEST-003`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-003-pgo-pmu-pipeline.md#3-ref-test-003-empirical-pmu-benchmark-verification) | Empirical PMU Benchmark Verification | PMU hardware assertions (< 0.1% dTLB miss) | Approved |
| [`REF-ARCH-004`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-004-resident-daemon-event-loop.md) | Resident Daemon Architecture & Zero-Wakeup epoll Subsystem | Singleton lock, timerfd, signalfd event loop | Approved |
| [`REF-TEST-004`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-004-resident-daemon-event-loop.md#3-ref-test-004-oracle-gate-unit-test-specifications) | Singleton Lock and Direct Sysfs Access Tests | Socket conflict test, pread persistent reading test | Approved |
| [`REF-ARCH-005`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-005-zero-cost-cpuid-simd.md) | Dynamic CPUID Feature Specialization & Zero-Cost Abstraction Pipeline | AVX2/BMI2 vector primitives, GNU IFUNC, NTTP dispatch | Approved |
| [`REF-ARCH-006`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-006-custom-containers.md) | Custom Zero-Allocation Container Architecture (FixedVector, FixedString, TopKHeap) | Cache-line aligned flat structures, TriviallyCopyable POD | Approved |
| [`REF-TEST-005`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-012-syscall-level-hardware-telemetry.md#4-oracle-gate--verification-standards) | Direct PMU Hardware Counter Verification | perf_event_open syscall 298 instructions, cycles, IPC assertion | Approved |
| [`REF-TEST-006`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-012-syscall-level-hardware-telemetry.md#4-oracle-gate--verification-standards) | PCIe Binary Config Space Decoding Verification | pread 64-byte config, Capability 0x10 Link Speed/Width assertion | Approved |
