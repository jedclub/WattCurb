# [REF-RES-004] WattCurb PGO & PMU Hardware Performance Audit Report

- **Date**: 2026-09-22 06:44:32 UTC
- **Architecture**: x86_64 / AMD Ryzen 7 PRO 4750U with Radeon Graphics
- **Compiler**: GCC 16 with C++23, Link-Time Optimization (-flto=auto), and Native Tuning (-march=native)
- **Profile-Guided Optimization**: Active (-fprofile-use -fprofile-correction, 29 merged .gcda counter files from 7 representative workloads)
- **Oracle Gate Measurement**: perf-measured pass runs with WATTCURB_BENCH_TOLERANCE=20 (timing thresholds only; correctness assertions unscaled). The strict run is `scripts/harness.py test` and CI.
- **Dev Profiler**: Disabled (WATTCURB_DEV_PROFILE=OFF — zero ScopedProfiler overhead)

---

## 1. Production Binary Sizes (Stripped & ELF-Pruned)

| Binary | Size (bytes) | Size (KB) |
| :--- | ---: | ---: |
| `output/wattcurb` (Daemon) | 377032 | 368 KB |
| `output/wattcurb-tray` (Desktop Tray) | 100744 | 98 KB |
| `output/wattcurb-dashboard` (Matrix Dashboard) | 261360 | 255 KB |

---

## 2. PMU Hardware Counter Telemetry (Oracle Gate Test Suite)

```text
=== WattCurb Unit Test Suite & Oracle Gate Verifier ===
 [ORACLE GATE] Verifying Token-Minimization Harness (REF-REQ-079)...
 [PASS] test_token_minimization_harness_integrity (REF-TEST-044: Harness isolation verified)
 [ORACLE GATE] Verifying Package Autostart & Installer Integrity (REF-REQ-080)...
 [PASS] test_package_autostart_and_installer_integrity (REF-TEST-045: Autostart & GUI launch verified)
 [ORACLE GATE] Verifying Tray Battery Report Action & Tactile Buttons (REF-REQ-081)...
 [PASS] test_tray_report_action_and_tactile_button_integrity (REF-TEST-046: Tray action & tactile UX verified)
 [ORACLE GATE] Verifying Process Full Name & Interactive Tooltip Integrity (REF-REQ-083)...
 [PASS] test_process_full_name_and_interactive_tooltips (REF-TEST-047: Full name & cyber tooltips verified)
 [ORACLE GATE] Running Deep Battery Drain History Analytics Suite...
   * Analysis Latency (120 pts): 52.598 us
 [PASS] test_deep_battery_drain_report_oracle_gate (REF-TEST-043: Full SHM sweep, hardware decomposition, process attribution verified)
 [ORACLE GATE] Verifying Power Profile Telemetry Filtering & Comparisons (REF-TEST-050)...
 [PASS] test_power_profile_filtering_and_comparisons (REF-TEST-050: Filter & comparison matrix verified)
 [ORACLE GATE] Verifying Kernel VM Writeback & Laptop Mode Coalescing (REF-TEST-051)...
 [PASS] test_kernel_vm_writeback_and_laptop_mode_coalescing (REF-TEST-051: VM writeback & laptop mode roundtrip verified)
 [ORACLE GATE] Verifying Ultimate UltraEndurance Full-Spectrum Power Minimization (REF-TEST-052)...
 [PASS] test_ultimate_ultra_endurance_power_minimization (REF-TEST-052: All 6 dimensions verified, Zero-Kill preserved)
 [PASS] test_modeset_flapping_elimination_and_test_isolation (REF-TEST-036: Subshell bypass, Idempotent DRRS & KWin effects verified)
 [INFO] CPU Features detected: AVX2=1 BMI1=1 BMI2=1 POPCNT=1 AVX512F=0
 [PASS] test_cpu_features
 [PASS] test_hw_isa_primitives (Core ID=2, TSC=7252172346661)
 [PASS] test_ifunc_and_nttp_dispatch (GNU IFUNC & C++23 NTTP verified)
 [PASS] test_simd_scanner
 [PASS] test_custom_containers (FixedVector, FixedString, TopKHeap, DoubleBufferedPool, Canary & Guards verified)
 [PASS] test_memory_sequence_probe_and_cache_chunking (64B HotChunk, 32B CompactHot, 0-crossing Hot loop)
 [PASS] test_deep_battery_telemetry (ThinkPad BAT0 uevent, Degradation, Thresholds & Peripherals verified)
 [ORACLE GATE] Dense Battery Telemetry Profiling Benchmark (50000 iters):
   * SIMD uevent parse      : 0.5484 us/op (930.5 cycles/op)
   * Battery physics calc   : 0.2446 us/op
   * Full-scope E2E pipeline: 0.9077 us/op (1540.2 cycles/op)
 [PASS] test_battery_telemetry_profiling_scopes (Dense Full-Scope REF-TEST-009)
--- [REF-TEST-011] Branchless SIMD & BMI2 PDEP Oracle Gate Verification ---
 [ORACLE GATE] Branchless SIMD & BMI2 PDEP parse_proc_stat (50000 iters):
   * Average Parse Latency : 0.1900 us/op
   * Average CPU Cycles    : 322.3 cycles/op
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
   * Average Latency : 28.99 ns/op
   * Average Cycles  : 49.2 cycles/op
 [PASS] test_zero_cost_environment_abstraction (REF-TEST-012)
--- [REF-TEST-013] Syscall Storm Suppression & Lazy FD Bypass Verification ---
 [ORACLE GATE] Lazy FD Bypass Latency (50000 iters):
   * Average Latency : 7.83 ns/op
   * Average Cycles  : 13.3 cycles/op
 [PASS] test_syscall_storm_suppression_and_lazy_fd_bypass (REF-TEST-013)
 [PASS] test_tray_binary_shared_state (128B Seqlock, Zero-Copy POD serialization verified)
 [PASS] test_window_aware_governor (Non-Halting Graceful Throttle, Always-Alive Invariant verified: 4us)
 [PASS] test_unified_rapid_rollback (AC Plug-in / Charge event full-sweep restoration verified: 2533us, idem: 0us)
 [ORACLE GATE] ThinkPower ToolTip Render Latency (50000 iters):
   * Average Latency : 0.0666 us/op
   * Average Cycles  : 113.1 cycles/op
 [PASS] test_thinkpower_tray_client (REF-TEST-018: Zero-heap stack formatting, Icon states verified: 0.1 us/op)

--- [REF-TEST-037] Desktop Tray Hot-Path Profiling Audit (REF-REQ-072) ---
 [PASS] test_tray_hotpath_profiling_audit (REF-TEST-037: Fine-grained scopes, breakdown table verified)

--- [REF-TEST-038] Desktop Tray Client Extreme Optimization Oracle Gate ---
 [ORACLE GATE] Icon Name O(1) LUT Latency (100000 iters):
   * Average Latency : 0.00 ns/op
   * Average Cycles  : 0.0 cycles/op
 [ORACLE GATE] 500ms Hover Hysteresis Zero-Syscall Bypass Latency (100000 iters):
   * Average Latency : 41.25 ns/op
   * Average Cycles  : 70.0 cycles/op
 [ORACLE GATE] ToolTip Render Latency with BAR_LUT & Fast Sanitizer (20000 iters):
   * Average Latency : 0.065 us/op
   * Average Cycles  : 110.3 cycles/op
 [PASS] test_tray_top10_extreme_optimization_oracle_gate (REF-TEST-038: 500ms timegate, BAR_LUT, ICON_LUT verified)

--- [REF-TEST-039] Dashboard Matrix Profiling & Zero-Copy Ingestion (REF-REQ-074) ---
 [ORACLE GATE] Matrix Dashboard Telemetry Benchmark:
   * Full JSON Ingestion (1000 iters) : 712.17 us/op (1208435.96 cycles/op)
   * Poll Loop Delta Gate (5000 iters) : 18.80 us/op (31897.24 cycles/op)
 [PASS] test_dashboard_matrix_profiling_audit (REF-TEST-039: Dashboard Scopes, Zero-Copy Shares & Delta Gate verified)

--- [REF-TEST-053] Matrix Dashboard Expanded Power Shares (11 Procs), Full Hardware Visibility & Typography Scaling (REF-REQ-089, REF-ARCH-066) ---
 [ORACLE GATE] 12-Process & Decomposed HW Share Poll Benchmark (50000 iters):
   * Poll + Decomposition Latency: 19.57 us/op (33202.77 cycles/op)
 [PASS] test_matrix_dashboard_expanded_power_shares_and_typography (REF-TEST-053: Top 11 Procs + Other, Full Hardware Visibility, QML Layout & Typography verified)

--- [REF-TEST-054] Process C-State Affinity Classification & Cyber Badge Telemetry (REF-REQ-090, REF-ARCH-067) ---
 [ORACLE GATE] Process C-State Classification Benchmark (100000 iters):
   * Heuristic Latency: 3.33 ns/op (5.64 cycles/op)
 [PASS] test_process_cstate_affinity_and_badges (REF-TEST-054: Heuristic, Table Badges, QML Layout & Hover Diagnostics verified)

--- [REF-TEST-055] Bi-Directional Power Profile Coherence & Seqlock Synchronization (REF-REQ-091, REF-ARCH-068) ---
 [ORACLE GATE] Seqlock Power Profile Coherence Benchmark (100000 iters):
   * Seqlock Update + Read Latency: 4.07 ns/op (6.90 cycles/op)
 [PASS] test_bi_directional_power_profile_coherence (REF-TEST-055: Seqlock Versioning, Ingestion & Coherence verified)

--- [REF-TEST-056] Ultimate Performance Unleash Full-Silicon Actuation Oracle Gate (REF-REQ-092, REF-ARCH-069) ---
   * Performance PM QoS C0 Clamp Active : NO (Mock/Non-root)
   * NVMe APST Zero-Latency Modified    : NO
   * CFS Sched Migration Cost Modified  : NO
   * Wi-Fi Power Save Disabled          : NO
   * Transition-Path Ordering Invariant   : VERIFIED (PowerSaver -> Performance -> Balanced)
 [ORACLE GATE] Ultimate Performance Actuation Benchmark (10000 iters):
   * PM QoS + GPU Profile Switch Latency: 6.43 ns/op (10.90 cycles/op)
 [PASS] test_ultimate_performance_unleash_actuation (REF-TEST-056: C0 Clamp, GPU 3D, APST 0, Rollback verified)

--- [REF-TEST-058] Watt-Reactive Tray Icon Rendering (REF-REQ-095, REF-ARCH-071) ---
   * Dial fill increment       : 0.0->0.5 lights x=14.4px (136px), 0.5->1.0 lights x=33.4px (122px)
wattcurb_tests: /home/jedclub/Develop/WattCurb/tests/test_units.cpp:4399: void test::test_profile_throughput_and_liveness_guarantees(): assertion `declined && "REQ-109.1: platform_profile write declined while a competitor runs"' 실패.
/home/jedclub/Develop/WattCurb/build_pgo/wattcurb_tests: 중지됨

 Performance counter stats for '/home/jedclub/Develop/WattCurb/build_pgo/wattcurb_tests':

          2,177.83 msec task-clock:u                                                          
     4,527,259,186      cycles:u                                                                (28.61%)
     9,219,268,520      instructions:u                                                          (28.64%)
         9,336,381      cache-misses:u                                                          (28.67%)
        53,090,915      L1-dcache-load-misses:u                                                 (28.65%)
           514,473      dTLB-load-misses:u                                                      (28.53%)
     2,186,604,434      branches:u                                                              (28.55%)
        12,881,249      branch-misses:u                                                         (28.55%)

       2.580645508 seconds time elapsed

       1.888462000 seconds user
       0.250242000 seconds sys
```

---

## 3. PMU Hardware Counter Telemetry (6-Second Daemon Run)

```text
[2m[*] WattCurb deep observation window: [>                       ] 0.0s / 6.0s (Interval 1/3)...[0m[2m[*] WattCurb deep observation window: [========>               ] 2.0s / 6.0s (Interval 2/3)...[0m[2m[*] WattCurb deep observation window: [================>       ] 4.0s / 6.0s (Interval 3/3)...[0m[K
[1m[36m================================================================================================================
                             WattCurb High-Fidelity Executive & Physical Power Briefing                         
================================================================================================================
[0m[2m Observation Scope    : [0m[1m6921 ms[0m[2m (3 continuous intervals)[0m[2m | Energy Consumption: [0m[1m162.55 Joules[0m[2m | Monitored Processes: [0m[1m197[0m[2m | Wakeups: [0m[1m1835 /sec[0m

[1m [1] System Battery & Power Supply Deep Telemetry[0m
  - Total System Drain      : [1m[33m23.49 Watts[0m
  - Power Supply State      : [32mAC Connected (Line Power / Charging)[0m[2m [USB-PD Input: 0.0W (C [PD] PD_PPS)][0m
  - Battery Capacity        : [1m70%[0m [Normal] ([1m0.5h to limit[0m) | Health: 94.2% (107 cycles)
  - Battery Hardware ID     : [1mSMP LNV-5B10W13895[0m (S/N: 3502) [Li-poly]
  - Voltage & Current Flow  : 12.169 V (Design Nominal: 11.10 V) | Flow: [0m0.000 A[0m
  - Energy & Degradation    : 29.86 Wh now / 42.65 Wh full (Design: 45.28 Wh) | [1m[32m5.8% wear (2.63 Wh lost)[0m

[1m [2] Physical Hardware Domain Power & State Breakdown[0m
  * CPU Package (RAPL)    :   4.21 W ( 17.9%) [2mTemp: 69°C, 2433 MHz avg (performance)[0m
    └─ [2mC-State Sleep Residency : [0mC0 (Active): [1m19.3%[0m, C1: 6.3%, C2: 27.8%, C3 (Deep Sleep): [1m[33m46.6%[0m
  * GPU Silicon (DRM)     :   6.42 W ( 27.3%) [2mBusy: 59%, 473 MB VRAM, 8.0 GT/s PCIe x16[0m
  * Display Backlight     :   1.49 W (  6.3%) [2mBrightness: 25%[0m
  * Storage / NVMe APST   :   0.38 W (  1.6%) [2mNVMe: active (Read 2.6 MB/s, Write 0.1 MB/s)[0m
  * Mechanical Fan        :   1.91 W (  8.1%) [2m4326 RPM[0m
  * Uncore & Platform Loss:   9.08 W ( 38.7%) [2mASPM: default [performance] powersave[0m

[1m [3] Top Battery Drain Culprits & Physical Causation Breakdown[0m
  1. [1mChatGPT[0m (PID: 2724838, [36mTier 5 (Runaway)[0m, 25 thr, Nice: -8)
     Drain: [1m[31m4.09 W[0m (17.4% of system, WDI: 55.2) | Primary Domain: [35mWiFi Radio CAM[0m
     Causation Mechanism: [1mActive Network Sockets (20 skt, CAM Mode)[0m
     Physical Metrics   : [2mWakeups: 277/s, RAM PSS: 67MB, Faults: 684 min/s 0 maj/s, Sockets: 20[0m
  2. [1mchrome[0m (PID: 2743115, [36mTier 3 (User App)[0m, 39 thr, Nice: -8)
     Drain: [1m[31m2.23 W[0m (9.5% of system, WDI: 31.2) | Primary Domain: [35mWiFi Radio CAM[0m
     Causation Mechanism: [1mActive Network Sockets (28 skt, CAM Mode)[0m
     Physical Metrics   : [2mWakeups: 104/s, RAM PSS: 92MB, Faults: 30 min/s 5 maj/s, Sockets: 28[0m
  3. [1mbun[0m (PID: 2854698, [36mTier 5 (Runaway)[0m, 15 thr, Nice: 16)
     Drain: [1m[31m2.36 W[0m (10.1% of system, WDI: 29.9) | Primary Domain: [35mCPU Compute[0m
     Causation Mechanism: [1mCore Execution (745 ticks, 48% CPU)[0m
     Physical Metrics   : [2mWakeups: 251/s, RAM PSS: 596MB, Faults: 12467 min/s 0 maj/s, Sockets: 0[0m
  4. [1mkwin_wayland[0m (PID: 1116, [36mTier 1 (Compositor)[0m, 41 thr, Nice: -10)
     Drain: [1m[32m0.47 W[0m (2.0% of system, WDI: 14.8) | Primary Domain: [35mCPU C-State Wakeup[0m
     Causation Mechanism: [1mC3 Sleep Breaker (272 wakeups/s)[0m
     Physical Metrics   : [2mWakeups: 272/s, RAM PSS: 47MB, Faults: 5 min/s 0 maj/s, Sockets: 0[0m
  5. [1mChatGPT[0m (PID: 2724967, [36mTier 5 (Runaway)[0m, 23 thr, Nice: 0)
     Drain: [1m[33m1.01 W[0m (4.3% of system, WDI: 13.8) | Primary Domain: [35mWiFi Radio CAM[0m
     Causation Mechanism: [1mActive Network Sockets (14 skt, CAM Mode)[0m
     Physical Metrics   : [2mWakeups: 118/s, RAM PSS: 434MB, Faults: 846 min/s 0 maj/s, Sockets: 14[0m
  6. [1mchrome[0m (PID: 2743277, [36mTier 3 (User App)[0m, 25 thr, Nice: 0)
     Drain: [1m[33m0.77 W[0m (3.3% of system, WDI: 12.8) | Primary Domain: [35mWiFi Radio CAM[0m
     Causation Mechanism: [1mActive Network Sockets (11 skt, CAM Mode)[0m
     Physical Metrics   : [2mWakeups: 50/s, RAM PSS: 126MB, Faults: 73 min/s 6 maj/s, Sockets: 11[0m
  7. [1mChatGPT[0m (PID: 2724773, [36mTier 5 (Runaway)[0m, 56 thr, Nice: -8)
     Drain: [1m[33m0.87 W[0m (3.7% of system, WDI: 11.7) | Primary Domain: [35mWiFi Radio CAM[0m
     Causation Mechanism: [1mActive Network Sockets (32 skt, CAM Mode)[0m
     Physical Metrics   : [2mWakeups: 85/s, RAM PSS: 234MB, Faults: 752 min/s 0 maj/s, Sockets: 32[0m
  8. [1mchrome[0m (PID: 2743050, [36mTier 3 (User App)[0m, 42 thr, Nice: -8)
     Drain: [1m[33m0.85 W[0m (3.6% of system, WDI: 11.7) | Primary Domain: [35mWiFi Radio CAM[0m
     Causation Mechanism: [1mActive Network Sockets (32 skt, CAM Mode)[0m
     Physical Metrics   : [2mWakeups: 89/s, RAM PSS: 145MB, Faults: 12 min/s 0 maj/s, Sockets: 32[0m

[1m [4] Modular Battery Optimization Features Execution Status[0m
  - Overall Status      : [1m[2mOptimal baseline (No intrusive throttling needed)[0m
    * All optimization features running in passive baseline observation mode.

[1m [5] Actionable Engineering Recommendations[0m
  * [C-State Breaker] Wakeup frequency (1835 wakeups/sec) is impeding CPU C3 deep sleep. Enabling TimerSlackCoalescing will bundle timers.
================================================================================================================


 Performance counter stats for '/home/jedclub/Develop/WattCurb/output/wattcurb --duration 6 -i 2':

            933.05 msec task-clock:u                                                          
        25,481,526      cycles:u                                                                (39.83%)
        18,642,388      instructions:u                                                          (40.02%)
           385,071      L1-dcache-load-misses:u                                                 (40.06%)
             5,257      dTLB-load-misses:u                                                      (40.07%)
           492,383      branch-misses:u                                                         (40.01%)
               304      page-faults:u                                                         

       6.937571806 seconds time elapsed

       0.021417000 seconds user
       0.886066000 seconds sys
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
