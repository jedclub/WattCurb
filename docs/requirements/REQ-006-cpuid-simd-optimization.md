# [REF-REQ-009] Compile-Time SIMD & Dynamic CPUID Hardware Specialization Specification

- **Ref-ID**: `REF-REQ-009`
- **Related Requirements**: [`REF-REQ-006`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-003-pgo-pmu-optimization.md), [`REF-REQ-007`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-004-resident-daemon-direct-access.md), [`REF-REQ-008`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-005-zero-residue-release.md)
- **Related Architecture**: [`REF-ARCH-003`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-003-pgo-pmu-pipeline.md), [`REF-ARCH-004`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-004-resident-daemon-event-loop.md)
- **Related Research**: [`REF-RES-004`](file:///home/jedclub/Develop/WattCurb/docs/research/PGO_PMU_REPORT.md), [`REF-RES-005`](file:///home/jedclub/Develop/WattCurb/docs/research/PMU_BENCHMARKS.md)
- **Status**: Approved

---

## 1. Executive Summary & Hardware Context

WattCurb runs continuously as a low-power background daemon. To minimize daemon active time ($T_{active}$) and allow the CPU to quickly return to deep C-states ($C_6/C_7/C_8$), CPU active cycles per profiling pass must be minimized through maximum instruction-level parallelism (ILP) and hardware vectorization.

Modern x86-64 processors (such as the host AMD Ryzen 7 Zen 2 architecture) provide powerful SIMD execution units capable of processing 256 bits (32 bytes or 8 single-precision floats / 4 double-precision floats / integers) per cycle. Furthermore, Bit Manipulation Extensions (BMI1/BMI2) provide single-cycle bit search and extraction instructions (`tzcnt`, `lzcnt`, `popcnt`).

This specification mandates that:
1. Production code aggressively utilizes compile-time SIMD vectorization and CPU-specific microarchitectural instructions.
2. At daemon startup, CPUID capabilities are inspected to bind the fastest available hardware execution path with **zero per-iteration branch penalty**.

---

## 2. Technical Requirements & Implementation Directives

### 2.1 Compile-Time Microarchitecture Specialization
- Production builds must target native microarchitecture (`-march=native` or minimum baseline `x86-64-v3` enabling AVX2, FMA, BMI1, BMI2).
- Data alignment: All hot buffers (such as sysfs read buffers and process table arrays) must be aligned to 32 bytes (`alignas(32)`) or 64 bytes (`alignas(64)`) to enable uninhibited 256-bit AVX2 aligned loads (`_mm256_load_si256`).

### 2.2 SIMD Delimiter Scanning & Numeric Parsing
- In hot text parsing paths (e.g. scanning `/proc/[pid]/stat`, `/proc/[pid]/status`, `/proc/[pid]/io`, `/proc/[pid]/fdinfo/*`):
  - Scanning for spaces, newlines, colons, or parentheses must be accelerated using 256-bit SIMD intrinsics (`_mm256_cmpeq_epi8` and `_mm256_movemask_epi8`).
  - Trailing bit search using `_tzcnt_u32` skips up to 32 bytes in 1~2 CPU clock cycles, achieving an order of magnitude faster scanning than scalar character-by-character loops.

### 2.3 Startup CPUID Feature Detection & Zero-Overhead Dispatch
- At initialization, the daemon invokes CPUID or `__builtin_cpu_supports`:
  ```cpp
  struct CpuFeatures {
      bool has_avx2{false};
      bool has_bmi2{false};
      bool has_avx512f{false};
      bool has_popcnt{false};
  };
  ```
- Dispatch to specialized functions is resolved **once** at bootstrap time:
  - Using function pointers initialized during startup prior to the epoll event loop, or
  - Using GNU IFUNC (`__attribute__((ifunc(...)))`) resolved during dynamic linking.
- **Zero-Wakeup & Zero-Branch Rule**: Steady-state monitoring loops must never evaluate CPU feature flags inside the hot loop.

### 2.4 Cache Prefetching
- In multi-process scanning loops, `__builtin_prefetch(ptr, 0, 1)` must be issued on upcoming process data structures or buffer addresses to eliminate memory latency stalls.

---

## 3. Verification & Oracle Gate Benchmarks

1. **Assembly (ASM) Inspection**:
   - Compiling with `-S -fverbose-asm` must verify the presence of `vmovdqa`, `vpcmpeqb`, `vpmovmskb`, and `vtzcnt` in inner parsing functions.
2. **Oracle Gate SIMD Benchmark**:
   - Unit tests must measure SIMD scanning vs scalar scanning and verify $> 3.0\times$ throughput speedup on 100k buffer scans.
3. **PMU IPC Telemetry**:
   - Sustained Instructions Per Cycle (IPC) must remain $> 3.0$ during parsing workloads.
