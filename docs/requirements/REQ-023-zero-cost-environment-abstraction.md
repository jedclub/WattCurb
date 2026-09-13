# [REF-REQ-026] C++23 Zero-Cost Environment & Hardware Abstraction Specification

- **Ref-ID**: `REF-REQ-026`
- **Title**: C++23 Zero-Cost Environment & Hardware Abstraction Specification
- **Status**: Approved
- **Author**: Antigravity Agent
- **Date**: 2026-09-13
- **Related Requirements**: [`REF-REQ-006`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-003-pgo-pmu-optimization.md), [`REF-REQ-009`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-006-cpuid-simd-optimization.md), [`REF-REQ-025`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-022-branchless-simd-and-deep-syscall-optimization.md)
- **Related Architecture**: [`REF-ARCH-005`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-005-zero-cost-cpuid-simd.md), [`REF-ARCH-006`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-006-portable-binary-zero-overhead-dispatch.md), [`REF-ARCH-016`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-016-zero-cost-environment-dispatch.md)
- **Related Research**: [`REF-RES-005`](file:///home/jedclub/Develop/WattCurb/docs/research/PMU_BENCHMARKS.md), [`REF-RES-010`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-010-cpp23-vs-rust-empirical-benchmark.md)

---

## 1. Executive Summary & Problem Definition

In production heterogeneous Linux deployments, host runtime environments exhibit wide divergence across three independent hardware axes:
1. **Instruction Set Architecture (ISA) Tier**: Legacy x86-64-v2 (SSE4.2), modern x86-64-v3 (AVX2/BMI1/BMI2), AMD Zen (CLZERO/RDPID/Multi-CCX), or x86-64-v4 (AVX-512).
2. **Platform Form-Factor**: Battery-powered Mobile Laptop, AC-only Desktop Workstation, or Virtualized Container/Cloud VM (headless, no hardware sensors).
3. **Privilege & Telemetry Level**: Direct MSR/PMU kernel capabilities (`CAP_SYS_ADMIN`), standard sysfs read access, or restricted unprivileged sandboxes.

### The Anti-Pattern
Checking these environmental parameters with conditional `if` statements inside hot monitoring loops creates continuous Branch Target Buffer (BTB) pressure and instruction pipeline flushes, degrading IPC and wasting battery power.

### The Mandate
This specification mandates a **C++23 Zero-Cost Environment Abstraction Framework**. Divergent execution paths must be resolved **strictly once during process bootstrap**. Within inner monitoring loops, all environmental variations MUST be resolved via compile-time mechanisms (`if constexpr`, C++23 Concepts, Non-Type Template Parameters) or immutable function pointer jump tables, guaranteeing **zero runtime branch overhead ($0\ \text{cost}$)** across all user machines.

---

## 2. Functional & Non-Functional Requirements

### 2.1 [REQ-026-1] C++23 Concept-Driven Policy Interface
- **Requirement**: Hardware ISA routines and Platform probing policies MUST be constrained using modern C++23 Concepts (`concept` and `requires` clauses).
- **Constraints**:
  - `CpuIsaPolicyConcept`: Enforces static, zero-overhead primitive methods (`skip_whitespace`, `skip_tokens`, `clear_cacheline_64`, `read_core_id`, `read_tsc`).
  - `PlatformPolicyConcept`: Enforces static environmental queries (`has_battery()`, `supports_zen_ccx()`, `supports_pmu()`, `probe_battery(...)`).
  - No `virtual` function tables (vtables) or dynamic heap allocations permitted in policy instances.

### 2.2 [REQ-026-2] Once-per-Process Environment Bootstrap Detection
- **Requirement**: The daemon MUST interrogate host CPUID, `/sys/class/power_supply`, and kernel privileges during process startup (`bootstrap`) into an immutable `EnvironmentProfile` record.
- **Axes Classified**:
  1. `CpuIsaTier`: `GenericV2`, `Avx2Bmi2`, `ZenClzero`, `Avx512`
  2. `PlatformFormFactor`: `MobileLaptop`, `DesktopWorkstation`, `VirtualHeadless`
  3. `PrivilegeTier`: `KernelDirectPMU`, `StandardSysfs`, `RestrictedSandbox`

### 2.3 [REQ-026-3] Zero-Cost Outer-Loop Dispatcher
- **Requirement**: The outer monitoring loop MUST be dispatched via C++23 Generic Functor / NTTP dispatch.
- **Behavior**:
  - In native builds (`-march=native`), the compiler resolves all policies at compile-time directly to `NativeHostPolicy` with 100% inlining.
  - In portable builds, a single `switch` executed at startup binds execution to the optimal specialization. Inside the loop, all branches evaluate as compile-time constants (`if constexpr`).

### 2.4 [REQ-026-4] Graceful Degradation & Sensor Pruning
- **Requirement**: On systems lacking specific hardware (e.g., Desktops without batteries, VMs without RAPL/hwmon), probing routines for absent domains MUST be statically elided or replaced with empty no-op routines, performing **zero VFS `open` or `read` syscall attempts**.

---

## 3. Verification & Oracle Gate Standards ([`REF-TEST-012`])

1. **Policy Specialization Assertions**:
   - Verify that `GenericV2Policy`, `Avx2Bmi2Policy`, and `ZenClzeroPolicy` produce identical mathematical results on string parsing and token extraction.
   - Verify that `DesktopWorkstationPolicy` bypasses battery sysfs querying with 0 syscalls.
   - Verify that `VirtualHeadlessPolicy` gracefully populates `HardwareSample` without errors.
2. **Oracle Gate Regression Thresholds**:
   - Zero-cost dispatch invocation overhead MUST be $< 2.0\ \text{ns}$ (indistinguishable from direct inlined call).
   - Hot monitoring loop task-clock MUST show zero degradation compared to Milestone M21.
