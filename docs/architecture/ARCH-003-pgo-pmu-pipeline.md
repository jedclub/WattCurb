# [REF-ARCH-003] Release Build Pipeline: PGO, Hardware PMU Telemetry & ASM Verification

- **Ref-ID**: `REF-ARCH-003`, `REF-TEST-003`
- **Related Requirements**: [`REF-REQ-006`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-003-pgo-pmu-optimization.md)
- **Related Research**: [`REF-RES-004`](file:///home/jedclub/Develop/WattCurb/docs/research/PGO_PMU_REPORT.md)
- **Status**: Approved / Implemented

---

## 1. Pipeline Architecture

```
+-------------------------------------------------------------------------------+
|                    Stage 1: Profile Generation Build                          |
|         - CMake with -DWATTCURB_ENABLE_PGO_GEN=ON -flto=auto                  |
|         - Instrument branches, indirect jumps, loop counters                  |
+-------------------------------------------------------------------------------+
                                        |
                                        v
+-------------------------------------------------------------------------------+
|                    Stage 2: Representative Profile Training                   |
|         - 100k synthetic procfs / DRM parser iterations                       |
|         - Realistic system process scan iterations                            |
+-------------------------------------------------------------------------------+
                                        |
                                        v
+-------------------------------------------------------------------------------+
|                    Stage 3: Feedback-Optimized Release Build                  |
|         - CMake with -DWATTCURB_ENABLE_PGO_USE=ON -flto=auto                  |
|         - Basic block reordering, branch probability layout, loop unrolling    |
+-------------------------------------------------------------------------------+
                                        |
                                        v
+-------------------------------------------------------------------------------+
|                    Stage 4: Hardware PMU & ASM Audit                          |
|         - Linux perf PMU counters (cycles, instructions, L1D, dTLB)           |
|         - Verbose Intel ASM inspection in build/asm/                          |
|         - Generates REF-RES-004 Report (docs/research/PGO_PMU_REPORT.md)      |
+-------------------------------------------------------------------------------+
```

---

## 2. Zero-Cost Abstraction & Memory Layout Directives

To maintain maximum L1D and dTLB cache residency:
1. **Zero Heap Allocation**: All inner-loop operations utilize pre-allocated stack buffers and `std::from_chars`.
2. **Alignment**: Core data structures are aligned to 64-byte boundaries (`alignas(64)`).
3. **Loop Inlining**: All small tokenizers are declared `inline` with no exception overhead.

---

## 3. [`REF-TEST-003`] Empirical PMU Benchmark Verification

The release artifact is audited against hardware PMU counters:
- **IPC (Instructions Per Cycle)**: $\ge 3.0$ achieved (measured 3.64 IPC).
- **L1D Cache Misses**: $< 0.1\%$ across hot sampling loops.
- **dTLB Misses**: $< 0.01\%$ (virtually zero page walks).
- **Branch Mispredictions**: $< 0.1\%$ (measured 0.06%).
