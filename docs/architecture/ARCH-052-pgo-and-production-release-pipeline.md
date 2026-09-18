# [REF-ARCH-052] Multi-Target PGO Pipeline & Zero-Cost Production Release Architecture

## 1. Architectural Intent & Scope
This document specifies the end-to-end Profile-Guided Optimization (PGO), Link-Time Optimization (LTO), and ELF metadata pruning architecture that produces WattCurb's production release suite: `wattcurb`, `wattcurb-tray`, and `wattcurb-dashboard`.

- **Architecture Identifier**: `REF-ARCH-052`
- **Related Requirement**: `REF-REQ-075`
- **Verification Gate**: `REF-TEST-040`

---

## 2. PGO Pipeline Topology & State Transitions

```
+-----------------------------------------------------------------------------------+
| Stage 1: PGO Instrumentation Build (-fprofile-generate, -O3, -flto=auto)          |
| Targets: wattcurb, wattcurb-tray, wattcurb-dashboard, wattcurb_tests             |
+-----------------------------------------------------------------------------------+
                                         │
                                         ▼
+-----------------------------------------------------------------------------------+
| Stage 2: Representative Operational Training Phase                                |
| 1. wattcurb_tests (SIMD tokenization, RAPL, procfs, math invariant benchmarks)    |
| 2. wattcurb CLI sweeps (--features, --interval 1 --top 30, --briefing, -X)        |
| 3. Emits *.gcda branch probabilities & basic block weights into build directory   |
+-----------------------------------------------------------------------------------+
                                         │
                                         ▼
+-----------------------------------------------------------------------------------+
| Stage 3: PGO Feedback Re-compilation & Pure Zero-Cost Release Build               |
| -fprofile-use -fprofile-correction -flto=auto -march=native                       |
| -DWATTCURB_DEV_PROFILE=OFF (Eliminates ScopedProfiler overhead entirely)          |
| -DNDEBUG -fvisibility=hidden -fvisibility-inlines-hidden -fno-rtti -fno-exceptions|
+-----------------------------------------------------------------------------------+
                                         │
                                         ▼
+-----------------------------------------------------------------------------------+
| Stage 4: ELF Section Pruning, Stripping & Production Staging into output/         |
| strip --strip-all --remove-section=.note.* --remove-section=.comment              |
| Targets: output/wattcurb, output/wattcurb-tray, output/wattcurb-dashboard         |
+-----------------------------------------------------------------------------------+
                                         │
                                         ▼
+-----------------------------------------------------------------------------------+
| Stage 5: Hardware PMU Performance Audit & Intel-Syntax Assembly Verification      |
| perf stat analysis (instructions, cycles, IPC, cache-misses, dTLB)                |
| Assembly audit: build/asm/process_analyzer.s, build/asm/attribution_engine.s      |
+-----------------------------------------------------------------------------------+
```

---

## 3. Key Compiler Directives & Optimizations

### 3.1 Code Placement & Branch Prediction
Profile-guided feedback reorganizes code layout within binary text sections:
- **Hot Basic Blocks**: Placed contiguously to maximize L1 Instruction Cache (L1I) hits and utilize the CPU's branch target buffer (BTB) effectively.
- **Cold Paths & Error Handlers**: Relocated out-of-line to `.text.unlikely` sections, preventing instruction cache pollution.

### 3.2 Total Elimination of Development Residue
In the final release compilation (`WATTCURB_ENABLE_DEV_PROFILER=OFF`), `ScopedProfiler` macro expansion reduces to `((void)0)`:
```cpp
#if defined(WATTCURB_DEV_PROFILE)
#define WATTCURB_PROFILE_SCOPE(name) ::wattcurb::core::ScopedProfiler _prof_##__LINE__(name)
#else
#define WATTCURB_PROFILE_SCOPE(name) ((void)0)
#endif
```
This guarantees:
- 0 bytes of diagnostic string literals in `.rodata`.
- 0 cycles spent on `std::chrono` or `rdtsc` timestamping during runtime loops.
- Complete compiler inlining of inner functions without function call frame overhead.

### 3.3 Aggressive ELF Metadata Pruning
Standard release binaries contain unnecessary sections that add bloat and cache pressure:
- `.note.gnu.build-id`, `.note.ABI-tag`, `.note.gnu.property`: Stripped.
- `.comment`: Stripped.
- `.sframe`, `.eh_frame`, `.eh_frame_hdr`: Exception handling frames stripped (WattCurb core does not throw exceptions).

---

## 4. Empirical Evaluation Protocol (`REF-TEST-040`)
1. All three binaries in `output/` must be stripped and have valid executable ELF headers.
2. Hardware PMU counter verification must demonstrate steady IPC > 1.0 on compute loops and sub-0.1% dTLB misses.
3. Report generated in `docs/research/PGO_PMU_REPORT.md` and recorded in `docs/research/PMU_BENCHMARKS.md` as Milestone M34.
