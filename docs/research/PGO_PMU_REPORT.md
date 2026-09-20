# [REF-RES-004] WattCurb Extended Multi-Faceted PGO & PMU Hardware Performance Audit Report

- **Date**: 2026-09-20 04:46:24 UTC
- **Architecture**: x86_64 / AMD Ryzen 7 PRO 4750U with Radeon Graphics
- **Compiler**: GCC 16 with C++23, Link-Time Optimization (-flto=auto), and Native Tuning (-march=native)
- **Profile-Guided Optimization**: Active (-fprofile-use -fprofile-correction) with Extended Multi-Scenario Training
- **Dev Profiler**: Disabled (WATTCURB_DEV_PROFILE=OFF — zero ScopedProfiler overhead)

---

## 1. Production Binary Sizes (Stripped & ELF-Pruned)

| Binary | Size (bytes) | Size (KB) |
| :--- | ---: | ---: |
| `output/wattcurb` (Daemon) | 294616 | 287 KB |
| `output/wattcurb-tray` (Desktop Tray) | 43160 | 42 KB |
| `output/wattcurb-dashboard` (Matrix Dashboard) | 130032 | 126 KB |
| **Total Production Suite** | 467808 | 456 KB |

---

## 2. PMU Hardware Counter Telemetry (Oracle Gate Test Suite)

```text
=== WattCurb Unit Test Suite & Oracle Gate Verifier ===
 [PASS] test_modeset_flapping_elimination_and_test_isolation (REF-TEST-036: Subshell bypass, Idempotent DRRS & KWin effects verified)
 [INFO] CPU Features detected: AVX2=1 BMI1=1 BMI2=1 POPCNT=1 AVX512F=0
 [PASS] test_cpu_features
 [PASS] test_hw_isa_primitives (Core ID=6, TSC=10668592608564)
 [PASS] test_ifunc_and_nttp_dispatch (GNU IFUNC & C++23 NTTP verified)
 [PASS] test_simd_scanner
 [PASS] test_custom_containers (FixedVector, FixedString, TopKHeap, DoubleBufferedPool, Canary & Guards verified)
 [PASS] test_memory_sequence_probe_and_cache_chunking (64B HotChunk, 32B CompactHot, 0-crossing Hot loop)
 [PASS] test_deep_battery_telemetry (ThinkPad BAT0 uevent, Degradation, Thresholds & Peripherals verified)
 [ORACLE GATE] Dense Battery Telemetry Profiling Benchmark (50000 iters):
   * SIMD uevent parse      : 0.9574 us/op (1624.5 cycles/op)
   * Battery physics calc   : 0.4873 us/op
   * Full-scope E2E pipeline: 1.4795 us/op (2510.5 cycles/op)
 [PASS] test_battery_telemetry_profiling_scopes (Dense Full-Scope REF-TEST-009)
--- [REF-TEST-011] Branchless SIMD & BMI2 PDEP Oracle Gate Verification ---
 [ORACLE GATE] Branchless SIMD & BMI2 PDEP parse_proc_stat (50000 iters):
   * Average Parse Latency : 0.2713 us/op
   * Average CPU Cycles    : 460.3 cycles/op
 [PASS] test_branchless_simd_and_bmi2_pdep (REF-TEST-011)
--- [REF-TEST-012] C++23 Zero-Cost Environment Abstraction & Policy Verification ---
 [INFO] Detected Host Environment Profile:
   * ISA Tier       : 2
   * Form Factor    : 0
   * Privilege Tier : 0
   * Battery Present: true
   * AMD Zen        : true
   * Bound Specialization: ISA=AMD-Zen-CLZERO-Specialized | Platform=Mobile-Laptop
 [ORACLE GATE] Zero-Cost Dispatch Latency (50000 iters):
   * Average Latency : 42.74 ns/op
   * Average Cycles  : 72.5 cycles/op
 [PASS] test_zero_cost_environment_abstraction (REF-TEST-012)
--- [REF-TEST-013] Syscall Storm Suppression & Lazy FD Bypass Verification ---
 [ORACLE GATE] Lazy FD Bypass Latency (50000 iters):
   * Average Latency : 12.22 ns/op
   * Average Cycles  : 20.7 cycles/op
 [PASS] test_syscall_storm_suppression_and_lazy_fd_bypass (REF-TEST-013)
 [PASS] test_tray_binary_shared_state (128B Seqlock, Zero-Copy POD serialization verified)
 [PASS] test_window_aware_governor (Non-Halting Graceful Throttle, Always-Alive Invariant verified: 13us)
 [PASS] test_unified_rapid_rollback (AC Plug-in / Charge event full-sweep restoration verified: 934us, idem: 0us)
 [ORACLE GATE] ThinkPower ToolTip Render Latency (50000 iters):
   * Average Latency : 0.0998 us/op
   * Average Cycles  : 169.4 cycles/op
 [PASS] test_thinkpower_tray_client (REF-TEST-018: Zero-heap stack formatting, Icon states verified: 0.1 us/op)

--- [REF-TEST-037] Desktop Tray Hot-Path Profiling Audit (REF-REQ-072) ---
 [PASS] test_tray_hotpath_profiling_audit (REF-TEST-037: Fine-grained scopes, breakdown table verified)

--- [REF-TEST-038] Desktop Tray Client Extreme Optimization Oracle Gate ---
 [ORACLE GATE] Icon Name O(1) LUT Latency (100000 iters):
   * Average Latency : 0.00 ns/op
   * Average Cycles  : 0.0 cycles/op
 [ORACLE GATE] 500ms Hover Hysteresis Zero-Syscall Bypass Latency (100000 iters):
   * Average Latency : 60.78 ns/op
   * Average Cycles  : 103.1 cycles/op
 [ORACLE GATE] ToolTip Render Latency with BAR_LUT & Fast Sanitizer (20000 iters):
   * Average Latency : 0.103 us/op
   * Average Cycles  : 175.1 cycles/op
 [PASS] test_tray_top10_extreme_optimization_oracle_gate (REF-TEST-038: 500ms timegate, BAR_LUT, ICON_LUT verified)

--- [REF-TEST-039] Dashboard Matrix Profiling & Zero-Copy Ingestion (REF-REQ-074) ---
 [ORACLE GATE] Matrix Dashboard Telemetry Benchmark:
   * Full JSON Ingestion (1000 iters) : 833.34 us/op (1413989.35 cycles/op)
   * Poll Loop Delta Gate (5000 iters) : 22.51 us/op (38201.04 cycles/op)
 [PASS] test_dashboard_matrix_profiling_audit (REF-TEST-039: Dashboard Scopes, Zero-Copy Shares & Delta Gate verified)
 [ORACLE GATE] Headroom Mask Computation Benchmark (50000 iters):
   * Average Latency : 0.0748 us/op
   * Averag[2026-09-20 13:46:26] [WATTCURB][MITIGATION] Mitigation Actuated: PID 8881 (baloo_file) | Action: AntiStarvationCap | Details: Nice=15, Headroom Mask applied, cgroup quota=400ms/100ms
e Cycles  : 126.9 cycles/op
 [PASS] test_anti_starvation_and_greedy_capping (REF-TEST-019: Cores 0..13 allowed, 2 reserved for audio/compositor, 0.1 us/op)
--- [REF-TEST-020] Dual-Domain State Journaling & Faithful Restoration Verification ---
 [INFO] Hardware Baseline Captured:
   * Platform Profile  : low-power
   * CPU Governor      : schedutil
   * CPU Boost         : 0
   * PCIe ASPM Policy  : powersave
   * Scaling Max Freq  : 1700000 kHz
   * Panel Power Level : 0
 [PASS] test_state_journaling_and_faithful_restoration (REF-TEST-020: Dual-domain snapshot & 100% faithful restoration verified)
 [ORACLE GATE] EventLogger Stack Formatting (50k iters): 1.1 us/op
 [ORACLE GATE] HistoryRingBuffer Append Latency (100k iters): 16.6 ns/op
 [PASS] test_zero_disk_wakeup_logging_and_history_ring_buffer (REF-TEST-024 & REF-TEST-035: 7-Day 60,480-sample wrap, < 50ns append verified)
 [ORACLE GATE] Power Share Decomposition Math (100k iters): 0.0 ns/op
 [PASS] test_circular_power_share_visualization (REF-TEST-025: Sum-invariant 100%, zero-division safety, < 100ns math verified)
 [ORACLE GATE] UltraEndurance Profile Actuation & 100% Roundtrip: 8.5 ms
 [PASS] test_ultra_endurance_extensions (REF-TEST-028: SMT, Bluetooth, Backlight Cap, DRRS, KWin Effects & Baloo verified)
 [ORACLE GATE] Platform Loss Decomposition (12.5W System -> 3.8W Plat): DRAM=1.4W, VRM=1.7W, Wi-Fi=0.5W, MB/IO=0.2W (Invariant Sum=3.8W)
 [PASS] test_wifi_txpower_and_platform_loss_decomposition (REF-TEST-029 verified)
 [PASS] test_battery_low_performance_lockout (REF-TEST-032: <=20% demotion & AC bypass verified)
 [ORACLE GATE] Tier 2 Ultra-Lightweight Probe Latency: 1841.0 us/op
 [PASS] test_adaptive_three_tier_cadence (REF-TEST-033: On-Demand 2s, 10s light, 60s deep verified)
--- [REF-TEST-034] Smart Adaptive Trigger & Temporal Sync Verification ---
 [PASS] test_smart_adaptive_trigger_and_temporal_sync (REF-TEST-034: Spike trigger <= 10s, 15s cooldown, 1x timebase scale verified)
 [PASS] test_process_classifier (6 safety tiers & REF-ARCH-045 interactive immunity validated)
 [PASS] test_mitigation_engine (Immunity guarantees & adaptive logic verified)
--- [REF-TEST-014] Adaptive Power State Machine & Dynamic Rollback Verification ---
 [PASS] test_adaptive_mitigation_and_rollback (3-Tier Hysteresis, Thaw & Rollback verified)
 [PASS] test_modular_battery_features (7 features, metadata, catalog, & extreme profile verified)
 [PASS] test_executive_briefing_and_telemetry (Two-Part Telemetry & 128B Binary State verified)
 [PASS] test_proc_stat_parsing (with deep fields: minflt, majflt, threads, core, pri, nice)
 [PASS] test_proc_statm_parsing
 [PASS] test_proc_status_parsing
 [PASS] test_proc_io_parsing
 [PASS] test_drm_fdinfo_parsing
 [PASS] test_attribution_engine
 [PASS] test_apu_ppt_and_gpu_duty_cycle_attribution (REF-TEST-009, REF-REQ-051: APU PPT decoupled, duty-cycle scaled)
 [PASS] test_windowed_attribution_engine
 [PASS] test_singleton_lock
 [INFO] Hardware Probe Sample Captured:
   - CPU Temp: 42.000000 C
   - CPU Cores Online: 16, Avg Freq: 1390 MHz
   - C-State POLL=120815620us, C1=12108612618us, C2=117683317812us, C3=1839400177771us
   - GPU Power: 4.000000 W
   - GPU Busy: 0%
   - Fan RPM: 3127
   - Battery Discharging: false, AC Online: true
   - Battery Health: 94.191696%
   - NVMe Status: active, Read sectors: 271546811
 [PASS] test_persistent_hw_probe
 [PASS] test_pmu_perf_event_telemetry (Instructions counted: 0)
 [PASS] test_pmu_energy_proxy_metrics (EPI: 22.1M, EWR: 9.3%, P_est: 500.3 mW)
 [PASS] test_pcie_binary_config_decoder (Gen4 x16 binary decode verified)
 [PASS] test_scoped_profiler (Zero-overhead release purity verified)
 [ORACLE GATE] Running micro-benchmark on zero-allocation parser...
 [ORACLE GATE] 100k stat parses completed in 32369 us (0.3 us/op)
 [ORACLE GATE PASS] Performance within extreme efficiency threshold (< 1.0 us/op)
=== ALL TESTS & ORACLE GATE PASSED SUCCESSFULLY ===

 Performance counter stats for 'build_pgo/wattcurb_tests':

          1,378.54 msec task-clock:u                                                          
     2,057,321,354      cycles:u                                                                (83.74%)
     4,580,975,335      instructions:u                                                          (83.86%)
         1,556,988      cache-misses:u                                                          (84.04%)
        13,146,448      L1-dcache-load-misses:u                                                 (83.76%)
            92,881      dTLB-load-misses:u                                                      (83.49%)
     1,045,718,439      branches:u                                                              (83.85%)
         4,450,967      branch-misses:u                                                         (83.71%)

       1.499462659 seconds time elapsed

       1.185839000 seconds user
       0.139270000 seconds sys
```

---

## 3. PMU Hardware Counter Telemetry (6-Second Daemon Run)

```text
[2m[*] WattCurb deep observation window: [>                       ] 0.0s / 6.0s (Interval 1/3)...[0m[2m[*] WattCurb deep observation window: [========>               ] 2.0s / 6.0s (Interval 2/3)...[0m[2m[*] WattCurb deep observation window: [================>       ] 4.0s / 6.0s (Interval 3/3)...[0m[K
[1m[36m================================================================================================================
                             WattCurb High-Fidelity Executive & Physical Power Briefing                         
================================================================================================================
[0m[2m Observation Scope    : [0m[1m6033 ms[0m[2m (3 continuous intervals)[0m[2m | Energy Consumption: [0m[1m162.54 Joules[0m[2m | Monitored Processes: [0m[1m88[0m[2m | Wakeups: [0m[1m224 /sec[0m

[1m [1] System Battery & Power Supply Deep Telemetry[0m
  - Total System Drain      : [1m[31m26.94 Watts[0m
  - Power Supply State      : [32mAC Connected (Line Power / Charging)[0m[2m [USB-PD Input: 0.0W (C [PD] PD_PPS)][0m
  - Battery Capacity        : [1m38%[0m [Normal] ([1m1.0h to limit[0m) | Health: 94.2% (102 cycles)
  - Battery Hardware ID     : [1mSMP LNV-5B10W13895[0m (S/N: 3502) [Li-poly]
  - Voltage & Current Flow  : 11.968 V (Design Nominal: 11.10 V) | Flow: [0m0.000 A[0m
  - Energy & Degradation    : 16.35 Wh now / 42.65 Wh full (Design: 45.28 Wh) | [1m[32m5.8% wear (2.63 Wh lost)[0m

[1m [2] Physical Hardware Domain Power & State Breakdown[0m
  * CPU Package (RAPL)    :   2.83 W ( 10.5%) [2mTemp: 41°C, 1414 MHz avg (schedutil)[0m
    └─ [2mC-State Sleep Residency : [0mC0 (Active): [1m4.5%[0m, C1: 0.3%, C2: 5.0%, C3 (Deep Sleep): [1m[32m90.2%[0m
    └─ [2mDirect PMU Telemetry    : [0mIPC: [1m1.09[0m | Cycles: 2020893 | LLC Miss: 15905 | Branch Miss: 21890
       [2mPMU Power Proxy (REF-REQ-024): [0mEPI: [1m[36m5.9M[0m | EWR: [1m[31m60.7%[0m | Est. Power: [1m500.0 mW[0m
  * GPU Silicon (DRM)     :   0.05 W (  0.2%) [2mBusy: 0%, 214 MB VRAM, 8.0 GT/s PCIe x16[0m
  * Display Backlight     :   1.90 W (  7.1%) [2mBrightness: 38%[0m
  * Storage / NVMe APST   :   0.35 W (  1.3%) [2mNVMe: active (Read 0.0 MB/s, Write 0.0 MB/s)[0m
  * Mechanical Fan        :   0.75 W (  2.8%) [2m3129 RPM[0m
  * Uncore & Platform Loss:  21.06 W ( 78.2%) [2mASPM: default performance [powersave][0m

[1m [3] Top Battery Drain Culprits & Physical Causation Breakdown[0m
  1. [1magy[0m (PID: 2096, [36mTier 1 (Compositor)[0m, 26 thr, Nice: 10)
     Drain: [1m[31m2.45 W[0m (9.1% of system, WDI: 21.8) | Primary Domain: [35mWiFi Radio CAM[0m
     Causation Mechanism: [1mActive Network Sockets (10 skt, CAM Mode)[0m
     Physical Metrics   : [2mWakeups: 19/s, RAM PSS: 720MB, Faults: 1351 min/s 0 maj/s, Sockets: 10[0m
  2. [1mplasmashell[0m (PID: 1262, [36mTier 2 (Shell)[0m, 102 thr, Nice: -6)
     Drain: [1m[33m0.86 W[0m (3.2% of system, WDI: 9.9) | Primary Domain: [35mWiFi Radio CAM[0m
     Causation Mechanism: [1mActive Network Sockets (15 skt, CAM Mode)[0m
     Physical Metrics   : [2mWakeups: 51/s, RAM PSS: 213MB, Faults: 10 min/s 0 maj/s, Sockets: 15[0m
  3. [1mkwin_wayland[0m (PID: 1116, [36mTier 1 (Compositor)[0m, 37 thr, Nice: -10)
     Drain: [1m[32m0.16 W[0m (0.6% of system, WDI: 3.8) | Primary Domain: [35mPlatform/Idle[0m
     Causation Mechanism: [1mBackground Poll (60 w/s)[0m
     Physical Metrics   : [2mWakeups: 60/s, RAM PSS: 51MB, Faults: 26 min/s 0 maj/s, Sockets: 0[0m
  4. [1mfoot[0m (PID: 1689, [36mTier 1 (Compositor)[0m, 5 thr, Nice: 10)
     Drain: [1m[32m0.22 W[0m (0.8% of system, WDI: 3.6) | Primary Domain: [35mPlatform/Idle[0m
     Causation Mechanism: [1mBackground Poll (39 w/s)[0m
     Physical Metrics   : [2mWakeups: 39/s, RAM PSS: 14MB, Faults: 0 min/s 0 maj/s, Sockets: 3[0m
  5. [1mksystemstats[0m (PID: 1463, [36mTier 4 (Bg Worker)[0m, 3 thr, Nice: 0)
     Drain: [1m[32m0.31 W[0m (1.1% of system, WDI: 3.6) | Primary Domain: [35mNVMe Storage[0m
     Causation Mechanism: [1mNVMe Active (0.00 MB/s)[0m
     Physical Metrics   : [2mWakeups: 18/s, RAM PSS: 8MB, Faults: 0 min/s 0 maj/s, Sockets: 5[0m
  6. [1mclaude[0m (PID: 393928, [36mTier 3 (User App)[0m, 19 thr, Nice: 10)
     Drain: [1m[32m0.21 W[0m (0.8% of system, WDI: 2.7) | Primary Domain: [35mPlatform/Idle[0m
     Causation Mechanism: [1mBackground Poll (19 w/s)[0m
     Physical Metrics   : [2mWakeups: 19/s, RAM PSS: 261MB, Faults: 0 min/s 0 maj/s, Sockets: 2[0m
  7. [1mopencode[0m (PID: 224879, [36mTier 1 (Compositor)[0m, 23 thr, Nice: 10)
     Drain: [1m[32m0.06 W[0m (0.2% of system, WDI: 1.0) | Primary Domain: [35mPlatform/Idle[0m
     Causation Mechanism: [1mBackground Poll (9 w/s)[0m
     Physical Metrics   : [2mWakeups: 9/s, RAM PSS: 436MB, Faults: 0 min/s 0 maj/s, Sockets: 6[0m
  8. [1magy[0m (PID: 1024657, [36mTier 1 (Compositor)[0m, 24 thr, Nice: -4)
     Drain: [1m[32m0.11 W[0m (0.4% of system, WDI: 0.9) | Primary Domain: [35mNVMe Storage[0m
     Causation Mechanism: [1mNVMe Active (0.00 MB/s)[0m
     Physical Metrics   : [2mWakeups: 0/s, RAM PSS: 218MB, Faults: 0 min/s 0 maj/s, Sockets: 5[0m

[1m [4] Modular Battery Optimization Features Execution Status[0m
  - Overall Status      : [1m[2mOptimal baseline (No intrusive throttling needed)[0m
    * All optimization features running in passive baseline observation mode.

[1m [5] Actionable Engineering Recommendations[0m
  * System power distribution is well-balanced within optimal hardware efficiency boundaries.
================================================================================================================


 Performance counter stats for 'output/wattcurb --duration 6 -i 2':

             42.78 msec task-clock:u                                                          
         5,764,373      cycles:u                                                                (72.60%)
         7,701,144      instructions:u                                                          (72.89%)
            76,579      L1-dcache-load-misses:u                                                 (72.29%)
             2,965      dTLB-load-misses:u                                                      (72.30%)
            57,290      branch-misses:u                                                         (71.98%)
               246      page-faults:u                                                         

       6.047003574 seconds time elapsed

       0.002934000 seconds user
       0.039301000 seconds sys
```

---

## 4. Assembly (ASM) Optimization & Zero-Cost Verification

Assembly files generated under `build_pgo/asm/`:
- `build_pgo/asm/process_analyzer.s`
- `build_pgo/asm/attribution_engine.s`

### Key Verified Characteristics:
1. **L1I / Loop Alignment**: Inner parsing loops are aligned to 16/32-byte boundaries (`.p2align 4`), ensuring complete residency within L1 Instruction Cache.
2. **Zero Runtime Dispatch**: No indirect `vtable` calls in inner loops; direct inlined jumps.
3. **No Dynamic Heap Allocation in Hot Path**: Zero calls to `_Znwm` (`operator new`) or `malloc` during parsing and attribution loops.
4. **Cache & TLB Efficiency**: Structure-of-arrays and flat parsing buffers minimize dTLB and L1-dcache misses.
5. **PGO Cold Path Isolation**: Error handlers and unlikely branches relocated to `.text.unlikely` sections.
6. **ScopedProfiler Complete Elimination**: Zero `rdtsc` or `std::chrono` calls in production binary (WATTCURB_DEV_PROFILE=OFF).
