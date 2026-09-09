# [REF-RES-004] WattCurb PGO & PMU Hardware Performance Audit Report

- **Date**: 2026-09-09 16:39:16 UTC
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
 [PASS] test_hw_isa_primitives (Core ID=2, TSC=15594582571707)
 [PASS] test_ifunc_and_nttp_dispatch (GNU IFUNC & C++23 NTTP verified)
 [PASS] test_simd_scanner
 [PASS] test_proc_stat_parsing
 [PASS] test_proc_status_parsing
 [PASS] test_proc_io_parsing
 [PASS] test_drm_fdinfo_parsing
 [PASS] test_attribution_engine
 [PASS] test_singleton_lock
 [INFO] Hardware Probe Sample Captured:
   - CPU Temp: 69.750000 C
   - CPU Cores Online: 16, Avg Freq: 2066 MHz
   - C-State POLL=2115593534us, C1=128011627764us, C2=615167047961us, C3=1813926781564us
   - GPU Power: 9.000000 W
   - GPU Busy: 4%
   - Fan RPM: 4314
   - Battery Discharging: false, AC Online: true
   - Battery Health: 94.191696%
   - NVMe Status: active, Read sectors: 212646942
 [PASS] test_persistent_hw_probe
 [ORACLE GATE] Running micro-benchmark on zero-allocation parser...
 [ORACLE GATE] 100k stat parses completed in 15026 us (0.15026 us/op)
 [ORACLE GATE PASS] Performance within extreme efficiency threshold (< 0.5 us/op)
=== ALL TESTS & ORACLE GATE PASSED SUCCESSFULLY ===
62716379;;cycles:u;19573960;77.00;;
103229184;;instructions:u;21736868;86.00;;
25893;;cache-misses:u;21222829;84.00;;
19936;;L1-dcache-load-misses:u;21803762;86.00;;
1667;;dTLB-load-misses:u;21926432;86.00;;
25268067;;branches:u;22573379;89.00;;
20732;;branch-misses:u;22471136;89.00;;
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
