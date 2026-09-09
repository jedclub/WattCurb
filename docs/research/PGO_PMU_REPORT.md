# [REF-RES-004] WattCurb PGO & PMU Hardware Performance Audit Report

- **Date**: 2026-09-09 15:57:19 UTC
- **Architecture**: x86_64 / 
- **Compiler**: GCC 16 with C++23, Link-Time Optimization (-flto), and Native Tuning (-march=native)
- **Profile-Guided Optimization**: Active (-fprofile-use -fprofile-correction)

---

## 1. PMU Hardware Counter Performance Telemetry

The test suite was audited using hardware PMU counters via Linux `perf`:

```text
=== WattCurb Unit Test Suite & Oracle Gate Verifier ===
 [PASS] test_proc_stat_parsing
 [PASS] test_proc_status_parsing
 [PASS] test_proc_io_parsing
 [PASS] test_drm_fdinfo_parsing
 [PASS] test_attribution_engine
 [PASS] test_singleton_lock
 [PASS] test_persistent_hw_probe
 [ORACLE GATE] Running micro-benchmark on zero-allocation parser...
 [ORACLE GATE] 100k stat parses completed in 6432 us (0.06432 us/op)
 [ORACLE GATE PASS] Performance within extreme efficiency threshold (< 0.5 us/op)
=== ALL TESTS & ORACLE GATE PASSED SUCCESSFULLY ===
25813161;;cycles:u;9338686;82.00;;
97573430;;instructions:u;9351120;82.00;;
23797;;cache-misses:u;9333548;82.00;;
10506;;L1-dcache-load-misses:u;9369944;82.00;;
1157;;dTLB-load-misses:u;10125496;89.00;;
26597114;;branches:u;10350979;91.00;;
17014;;branch-misses:u;10225103;90.00;;
```

---

## 2. Assembly (ASM) Optimization & Zero-Cost Verification

Assembly files generated under `build/asm/`:
- `build/asm/process_analyzer.s`
- `build/asm/attribution_engine.s`

### Key Verified Characteristics:
1. **L1I / Loop Alignment**: Inner parsing loops are aligned to 16/32-byte boundaries (`.p2align 4`), ensuring complete residency within L1 Instruction Cache.
2. **Zero Runtime Dispatch**: No indirect `vtable` calls in inner loops; direct inlined jumps.
3. **No Dynamic Heap Allocation in Hot Path**: Zero calls to `_Znwm` (`operator new`) or `malloc` during parsing and attribution loops.
4. **Cache & TLB Efficiency**: Structure-of-arrays and flat parsing buffers minimize dTLB and L1-dcache misses.
