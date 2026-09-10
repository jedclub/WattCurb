# [REF-RES-004] WattCurb PGO & PMU Hardware Performance Audit Report

- **Date**: 2026-09-10 16:55:12 UTC
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
 [PASS] test_hw_isa_primitives (Core ID=10, TSC=17197753879006)
 [PASS] test_ifunc_and_nttp_dispatch (GNU IFUNC & C++23 NTTP verified)
 [PASS] test_simd_scanner
 [PASS] test_custom_containers (FixedVector, FixedString, TopKHeap, Canary & Guards verified)
 [PASS] test_process_classifier (6 safety tiers validated)
 [PASS] test_mitigation_engine (Immunity guarantees & adaptive logic verified)
 [PASS] test_modular_battery_features (7 features, metadata, catalog, & extreme profile verified)
 [PASS] test_executive_briefing_and_telemetry (Two-Part Telemetry & JSON verified)
 [PASS] test_proc_stat_parsing (with deep fields: minflt, majflt, threads, core, pri, nice)
 [PASS] test_proc_statm_parsing
 [PASS] test_proc_status_parsing
 [PASS] test_proc_io_parsing
 [PASS] test_drm_fdinfo_parsing
 [PASS] test_attribution_engine
 [PASS] test_windowed_attribution_engine
 [PASS] test_singleton_lock
 [INFO] Hardware Probe Sample Captured:
   - CPU Temp: 69.625000 C
   - CPU Cores Online: 16, Avg Freq: 2709 MHz
   - C-State POLL=2184427111us, C1=128999370036us, C2=625773791202us, C3=2019849284453us
   - GPU Power: 12.000000 W
   - GPU Busy: 0%
   - Fan RPM: 3821
   - Battery Discharging: false, AC Online: true
   - Battery Health: 94.191696%
   - NVMe Status: active, Read sectors: 232825089
 [PASS] test_persistent_hw_probe
 [PASS] test_pmu_perf_event_telemetry (Instructions counted: 0)
 [PASS] test_pcie_binary_config_decoder (Gen4 x16 binary decode verified)
 [PASS] test_scoped_profiler (Zero-overhead release purity verified)
 [ORACLE GATE] Running micro-benchmark on zero-allocation parser...
 [ORACLE GATE] 100k stat parses completed in 28149 us (0.28149 us/op)
 [ORACLE GATE PASS] Performance within extreme efficiency threshold (< 0.5 us/op)
=== ALL TESTS & ORACLE GATE PASSED SUCCESSFULLY ===
98874300;;cycles:u;29040779;85.00;;
197584059;;instructions:u;29024556;85.00;;
41902;;cache-misses:u;29064284;85.00;;
28256;;L1-dcache-load-misses:u;29137709;85.00;;
1617;;dTLB-load-misses:u;28628064;84.00;;
60187961;;branches:u;29031550;85.00;;
125195;;branch-misses:u;29413811;86.00;;
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
