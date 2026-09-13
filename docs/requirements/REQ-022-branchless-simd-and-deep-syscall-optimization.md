# [REF-REQ-025] Extreme Branchless SIMD, Bit-Hacking & Deep Syscall Optimization

- **Ref-ID**: `REF-REQ-025`
- **Title**: Extreme Branchless SIMD, Bit-Hacking & Deep Syscall Optimization Specification
- **Status**: Approved
- **Author**: Antigravity Agent
- **Date**: 2026-09-13
- **Related Requirements**: [`REF-REQ-009`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-006-cpuid-simd-optimization.md), [`REF-REQ-015`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-012-syscall-level-hardware-telemetry.md), [`REF-REQ-024`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-021-pmu-energy-proxy-telemetry.md)
- **Related Architecture**: [`REF-ARCH-005`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-005-zero-cost-cpuid-simd.md), [`REF-ARCH-015`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-015-extreme-branchless-simd-bit-hacking.md)
- **Related Research**: [`REF-RES-005`](file:///home/jedclub/Develop/WattCurb/docs/research/PMU_BENCHMARKS.md), [`REF-RES-007`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-007-deep-kernel-primitives-and-simd-isa.md)

---

## 1. Executive Summary & Problem Definition

In continuous evaluation profiles of WattCurb (Milestone M18-M20), execution cost breakdown revealed that:
1. **Subsystem Profiling Top Costs**:
   - `proc.fd_socket_scan` & `proc.fd_readlink_loop` accounted for over 40% of instrumented capture time due to repetitive VFS `readlinkat` kernel transitions.
   - `proc.stat_parse` and delimiter scanning suffered from branch misprediction overhead caused by scalar character-by-character `while` loops.
2. **Algorithmic Inefficiencies in Hot Paths**:
   - Multi-token skipping in `skip_tokens_simd` utilized a sequential BMI1 `mask &= (mask - 1)` loop, taking $O(k)$ serial CPU cycles to locate the $k$-th space delimiter.
   - Whitespace skipping (`skip_whitespace_simd`) executed sequential scalar jumps.
   - Signed integer parsing relied on conditional branching (`if (neg) return -val;`), inducing branch prediction stalls on heterogeneous process tables.

This specification mandates replacing all remaining inner-loop branches and serial loops with **hardware-native bit-hacking**, **BMI2 `PDEP` branchless token selection**, **AVX2 vector range-check masking**, and **deep syscall bypasses**.

---

## 2. Functional & Non-Functional Requirements

### 2.1 [REQ-025-1] BMI2 `PDEP` Branchless $k$-th Token Extraction
- **Requirement**: When skipping $k$ space-delimited tokens in an AVX2 32-byte chunk where `spaces_in_chunk >= count`, the extraction of the $k$-th space delimiter MUST be executed in $O(1)$ time without iterative loops or conditional branches.
- **Mechanism**:
  - Deposit target bit mask: `uint32_t target_bit = _pdep_u32(1U << (count - 1), mask);`
  - Trailing zero count: `uint32_t offset = _tzcnt_u32(target_bit);`
  - Pipeline cost: 2-3 fixed cycles, zero branch stalls.

### 2.2 [REQ-025-2] AVX2 Branchless Delimiter & Whitespace Vector Range Checking
- **Requirement**: `skip_whitespace_simd` and `find_whitespace_simd` MUST process 32 contiguous bytes per cycle.
- **Mechanism**:
  - Vector comparison using unsigned byte range trick: characters with `ASCII <= 32` mapped to bitmask using `_mm256_cmpeq_epi8` with `_mm256_min_epu8(chunk, 32)`.
  - First non-whitespace index computed branchlessly via `_tzcnt_u32(~mask)`.

### 2.3 [REQ-025-3] Two's Complement Branchless Sign Handling
- **Requirement**: `parse_i32_fast` MUST eliminate branch instructions for negative number detection and negation.
- **Mechanism**:
  - Arithmetic bit-trick:
    ```cpp
    int32_t is_neg = (*cur == '-');
    cur += is_neg;
    // parse positive digits...
    int32_t mask = -is_neg;
    return (val ^ mask) + is_neg;
    ```
  - Zero branch prediction penalty.

### 2.4 [REQ-025-4] Deep Syscall & VFS Socket Scan Elimination
- **Requirement**: Reduce VFS `readlinkat` syscall frequency by at least 50% through deep process lifecycle analysis.
- **Mechanism**:
  - Low-activity processes ($\Delta \text{wakeups} < 20$, zero net socket growth) MUST bypass directory `readlinkat` calls on steady-state intervals.
  - 64-bit register constants (`0x5b3a74656b636f73ULL` for `"socket:["`) applied without `std::memcmp` library overhead.

---

## 3. Verification & Oracle Gate Standards ([`REF-TEST-011`])

1. **Deterministic Unit Testing**:
   - Assert exact equivalence between BMI2 PDEP token extraction and scalar token extraction across 10,000 randomized string inputs.
   - Assert exact numerical results from branchless `parse_i32_fast` including negative, zero, and positive bounds.
2. **Oracle Gate Regression Thresholds**:
   - `proc.stat_parse` execution cost MUST remain $< 0.85\ \mu\text{s}$ per PID.
   - Total daemon active task-clock across 30 seconds MUST remain $< 80\ \text{ms}$.
   - Hardware PMU branch-misses MUST decrease by $> 30\%$ compared to Baseline.
