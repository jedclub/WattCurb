# [REF-REQ-006] Release Optimization, PMU/ASM Analysis & PGO Pipeline

- **Ref-ID**: `REF-REQ-006`
- **Related Architecture**: [`REF-ARCH-003`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-003-pgo-pmu-pipeline.md)
- **Status**: Approved / Implementation Complete

---

## 1. Objectives

Establish rigorous compiler and hardware-level performance engineering standards for all release artifacts of WattCurb. The daemon must minimize execution cycles and CPU sleep-state disruption to the absolute theoretical minimum through:
1. Hardware PMU counter verification (L1D, L1I, dTLB, branch prediction).
2. Assembly (ASM) structural inspection.
3. Automated Profile-Guided Optimization (PGO) with Link-Time Optimization (LTO).

---

## 2. Requirements Specification

### 2.1 Hardware PMU Performance Standards
- **L1 Data Cache (L1D) Miss Ratio**: Must remain $< 1.5\%$ during hot parsing iterations.
- **dTLB Miss Ratio**: Must remain $< 0.1\%$ (zero page faulting or TLB churn during monitoring).
- **Instructions Per Cycle (IPC)**: Must maintain $\ge 2.0$ on modern x86-64 out-of-order cores.
- **Branch Prediction Miss Ratio**: Must remain $< 2.0\%$.

### 2.2 Assembly (ASM) Audit Standards
- All critical loops in `ProcessAnalyzer` and `AttributionEngine` must be inlined with zero dynamic heap allocations (`operator new` / `malloc`).
- Loops must be aligned to 16/32-byte cache line boundaries (`.p2align 4`).
- Compiler must generate native vector instructions (AVX2/FMA) for batch arithmetic.

### 2.3 Automated Profile-Guided Optimization (PGO) Pipeline
- Support 2-stage build:
  1. Instrumentation build with `-fprofile-generate`.
  2. Training workload capturing typical system procfs and DRM workloads.
  3. Optimized feedback build with `-fprofile-use`, `-fprofile-correction`, and `-flto=auto`.
