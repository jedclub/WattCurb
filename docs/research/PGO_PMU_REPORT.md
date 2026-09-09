# [REF-RES-004] WattCurb PGO & PMU Hardware Performance Audit Report

- **Date**: 2026-09-09 16:14:56 UTC
- **Architecture**: x86_64 / 
- **Compiler**: GCC 16 with C++23, Link-Time Optimization (-flto), and Native Tuning (-march=native)
- **Profile-Guided Optimization**: Active (-fprofile-use -fprofile-correction)

---

## 1. PMU Hardware Counter Performance Telemetry

The test suite was audited using hardware PMU counters via Linux `perf`:

```text
=== WattCurb Unit Test Suite & Oracle Gate Verifier ===
 [INFO] CPU Features detected: AVX2=1 BMI1=1 BMI2=1 POPCNT=1 AVX512F=0
 [PASS] test_cpu_features
 [PASS] test_hw_isa_primitives (Core ID=12, TSC=13115962011890)
 [PASS] test_simd_scanner
 [PASS] test_proc_stat_parsing
 [PASS] test_proc_status_parsing
 [PASS] test_proc_io_parsing
 [PASS] test_drm_fdinfo_parsing
 [PASS] test_attribution_engine
 [PASS] test_singleton_lock
 [PASS] test_persistent_hw_probe
 [ORACLE GATE] Running micro-benchmark on zero-allocation parser...
 [ORACLE GATE] 100k stat parses completed in 10374 us (0.10374 us/op)
 [ORACLE GATE PASS] Performance within extreme efficiency threshold (< 0.5 us/op)
=== ALL TESTS & ORACLE GATE PASSED SUCCESSFULLY ===
33618769;;cycles:u;11074790;84.00;;
106848639;;instructions:u;11085321;84.00;;
30630;;cache-misses:u;11089319;84.00;;
14072;;L1-dcache-load-misses:u;11070602;84.00;;
1220;;dTLB-load-misses:u;11072296;84.00;;
25666602;;branches:u;11657396;89.00;;
17165;;branch-misses:u;11431782;87.00;;
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
