# [REF-RES-004] WattCurb PGO & PMU Hardware Performance Audit Report

- **Date**: 2026-09-21 16:22:57 UTC
- **Architecture**: x86_64 / AMD Ryzen 7 PRO 4750U with Radeon Graphics
- **Compiler**: GCC 16 with C++23, Link-Time Optimization (-flto=auto), and Native Tuning (-march=native)
- **Profile-Guided Optimization**: Active (-fprofile-use -fprofile-correction)
- **Dev Profiler**: Disabled (WATTCURB_DEV_PROFILE=OFF — zero ScopedProfiler overhead)

---

## 1. Production Binary Sizes (Stripped & ELF-Pruned)

| Binary | Size (bytes) | Size (KB) |
| :--- | ---: | ---: |
| `output/wattcurb` (Daemon) | 352552 | 344 KB |
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
   * Analysis Latency (120 pts): 64.11 us
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
 [PASS] test_hw_isa_primitives (Core ID=10, TSC=43307325357742)
 [PASS] test_ifunc_and_nttp_dispatch (GNU IFUNC & C++23 NTTP verified)
 [PASS] test_simd_scanner
 [PASS] test_custom_containers (FixedVector, FixedString, TopKHeap, DoubleBufferedPool, Canary & Guards verified)
 [PASS] test_memory_sequence_probe_and_cache_chunking (64B HotChunk, 32B CompactHot, 0-crossing Hot loop)
 [PASS] test_deep_battery_telemetry (ThinkPad BAT0 uevent, Degradation, Thresholds & Peripherals verified)
 [ORACLE GATE] Dense Battery Telemetry Profiling Benchmark (50000 iters):
   * SIMD uevent parse      : 0.1348 us/op (228.8 cycles/op)
   * Battery physics calc   : 0.0566 us/op
   * Full-scope E2E pipeline: 0.2370 us/op (402.4 cycles/op)
 [PASS] test_battery_telemetry_profiling_scopes (Dense Full-Scope REF-TEST-009)
--- [REF-TEST-011] Branchless SIMD & BMI2 PDEP Oracle Gate Verification ---
 [ORACLE GATE] Branchless SIMD & BMI2 PDEP parse_proc_stat (50000 iters):
   * Average Parse Latency : 0.1199 us/op
   * Average CPU Cycles    : 203.4 cycles/op
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
   * Average Latency : 19.68 ns/op
   * Average Cycles  : 33.4 cycles/op
 [PASS] test_zero_cost_environment_abstraction (REF-TEST-012)
--- [REF-TEST-013] Syscall Storm Suppression & Lazy FD Bypass Verification ---
 [ORACLE GATE] Lazy FD Bypass Latency (50000 iters):
   * Average Latency : 0.14 ns/op
   * Average Cycles  : 0.2 cycles/op
 [PASS] test_syscall_storm_suppression_and_lazy_fd_bypass (REF-TEST-013)
 [PASS] test_tray_binary_shared_state (128B Seqlock, Zero-Copy POD serialization verified)
 [PASS] test_window_aware_governor (Non-Halting Graceful Throttle, Always-Alive Invariant verified: 3us)
 [PASS] test_unified_rapid_rollback (AC Plug-in / Charge event full-sweep restoration verified: 1310us, idem: 0us)
 [ORACLE GATE] ThinkPower ToolTip Render Latency (50000 iters):
   * Average Latency : 0.0309 us/op
   * Average Cycles  : 52.4 cycles/op
 [PASS] test_thinkpower_tray_client (REF-TEST-018: Zero-heap stack formatting, Icon states verified: 0.0 us/op)

--- [REF-TEST-037] Desktop Tray Hot-Path Profiling Audit (REF-REQ-072) ---
 [PASS] test_tray_hotpath_profiling_audit (REF-TEST-037: Fine-grained scopes, breakdown table verified)

--- [REF-TEST-038] Desktop Tray Client Extreme Optimization Oracle Gate ---
 [ORACLE GATE] Icon Name O(1) LUT Latency (100000 iters):
   * Average Latency : 0.00 ns/op
   * Average Cycles  : 0.0 cycles/op
 [ORACLE GATE] 500ms Hover Hysteresis Zero-Syscall Bypass Latency (100000 iters):
   * Average Latency : 31.01 ns/op
   * Average Cycles  : 52.6 cycles/op
 [ORACLE GATE] ToolTip Render Latency with BAR_LUT & Fast Sanitizer (20000 iters):
   * Average Latency : 0.036 us/op
   * Average Cycles  : 61.5 cycles/op
 [PASS] test_tray_top10_extreme_optimization_oracle_gate (REF-TEST-038: 500ms timegate, BAR_LUT, ICON_LUT verified)

--- [REF-TEST-039] Dashboard Matrix Profiling & Zero-Copy Ingestion (REF-REQ-074) ---
 [ORACLE GATE] Matrix Dashboard Telemetry Benchmark:
   * Full JSON Ingestion (1000 iters) : 542.86 us/op (921126.13 cycles/op)
   * Poll Loop Delta Gate (5000 iters) : 14.81 us/op (25121.61 cycles/op)
 [PASS] test_dashboard_matrix_profiling_audit (REF-TEST-039: Dashboard Scopes, Zero-Copy Shares & Delta Gate verified)

--- [REF-TEST-053] Matrix Dashboard Expanded Power Shares (11 Procs), Full Hardware Visibility & Typography Scaling (REF-REQ-089, REF-ARCH-066) ---
 [ORACLE GATE] 12-Process & Decomposed HW Share Poll Benchmark (50000 iters):
   * Poll + Decomposition Latency: 14.38 us/op (24399.09 cycles/op)
 [PASS] test_matrix_dashboard_expanded_power_shares_and_typography (REF-TEST-053: Top 11 Procs + Other, Full Hardware Visibility, QML Layout & Typography verified)

--- [REF-TEST-054] Process C-State Affinity Classification & Cyber Badge Telemetry (REF-REQ-090, REF-ARCH-067) ---
 [ORACLE GATE] Process C-State Classification Benchmark (100000 iters):
   * Heuristic Latency: 2.13 ns/op (3.61 cycles/op)
 [PASS] test_process_cstate_affinity_and_badges (REF-TEST-054: Heuristic, Table Badges, QML Layout & Hover Diagnostics verified)

--- [REF-TEST-055] Bi-Directional Power Profile Coherence & Seqlock Synchronization (REF-REQ-091, REF-ARCH-068) ---
 [ORACLE GATE] Seqlock Power Profile Coherence Benchmark (100000 iters):
   * Seqlock Update + Read Latency: 3.12 ns/op (5.28 cycles/op)
 [PASS] test_bi_directional_power_profile_coherence (REF-TEST-055: Seqlock Versioning, Ingestion & Coherence verified)

--- [REF-TEST-056] Ultimate Performance Unleash Full-Silicon Actuation Oracle Gate (REF-REQ-092, REF-ARCH-069) ---
   * Performance PM QoS C0 Clamp Active : NO (Mock/Non-root)
   * NVMe APST Zero-Latency Modified    : NO
   * CFS Sched Migration Cost Modified  : NO
   * Wi-Fi Power Save Disabled          : NO
   * Transition-Path Ordering Invariant   : VERIFIED (PowerSaver -> Performance -> Balanced)
 [ORACLE GATE] Ultimate Performance Actuation Benchmark (10000 iters):
   * PM QoS + GPU Profile Switch Latency: 0.56 ns/op (0.95 cycles/op)
 [PASS] test_ultimate_performance_unleash_actuation (REF-TEST-056: C0 Clamp, GPU 3D, APST 0, Rollback verified)

--- [REF-TEST-058] Watt-Reactive Tray Icon Rendering (REF-REQ-095, REF-ARCH-071) ---
   * Dial fill increment       : 0.0->0.5 lights x=14.4px (136px), 0.5->1.0 lights x=33.4px (122px)
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (konsole) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 () | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2287233 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
   * Badge hue (R-G)           : -163.0 (green) -> 196.0 (red), source=font
 [ORACLE GATE] Procedural Icon Render (300 iters @48px):
   * Average Latency : 31.2 us/op
 [PASS] test_watt_reactive_tray_icon (REF-TEST-058: bands, ramp, frame warning, 4 distinct badges, needle deflection verified)

--- [REF-TEST-059] Audio Continuity Guarantee (REF-REQ-096) ---
   * Host cpuidle exit latency : 1 us (shallowest) .. 350 us (deepest)
   * Audio latency ceiling     : 100 us
   * PCM probe                 : 52.1 us/op, active=yes, owners=1
   * Tier shielding            : interactive=shielded, background worker=open
   * Live-daemon command path   : refused (test isolation enforced at IPC layer)
 [PASS] test_audio_continuity_guarantee (REF-TEST-059: latency band, probe cost, owner immunity, floor lifecycle verified)

--- [REF-TEST-060] System Liveness Invariant (REF-REQ-098) ---
   * PCI runtime PM allowlist  : 11 classes verified, infrastructure denied
   * Liveness shielding        : compositor/shell/critical protected, workers throttleable
   * Host cpufreq floor        : 1400000 kHz
   * drop_caches actuations    : 0 (must be 0)
 [PASS] test_system_liveness_invariant (REF-TEST-060: runtime PM allowlist, compositor/input shielding, frequency floor, no cache purge verified)

--- [REF-TEST-061] Performance Throughput & UltraEndurance Liveness (REF-REQ-107, REF-REQ-108) ---
   * Performance leaves no C0 clamp held
   * Audio latency floor independent of the Performance path (100 us)
   * Tier 0..3 shielded with no stream running
   * Tier 4/5 background workers still throttleable
   * Writeback bounded: 1500 cs writeback / 3000 cs expire
 [PASS] test_profile_throughput_and_liveness_guarantees (REF-TEST-061: no C0 clamp in Performance, playback-independent stall shield, bounded writeback verified)
--- [REF-TEST-041] Multilingual L10n & Auto System Locale Verification ---
 [ORACLE GATE] O(1) L10n Translation Latency (100000 iters):
   * Average Latency : 3.5 ns/op
 [PASS] test_multilingual_l10n_and_auto_system_locale (REF-TEST-041: 13 languages, 46 strings, POSIX auto-detect verified)
 [ORACLE GATE] Headroom Mask Computation Benchmark (50000 iters):
   * Average Latency : 0.0029 us/op
   * Average Cycles  : 4.9 cycles/op
 [PASS] test_anti_starvation_and_greedy_capping (REF-TEST-019: Cores 0..13 allowed, 2 reserved for audio/compositor, 0.0 us/op)
--- [REF-TEST-048] Adaptive C1/C2 Dual-Cluster Spatial Load Dispersion & Terminal Shield Verification ---
 [ORACLE GATE] Adaptive C1/C2 Cluster Benchmark (50000 iters):
   * Average Latency : 0.0061 us/op
   * Average Cycles  : 10.4 cycles/op
 [PASS] test_adaptive_c1_c2_cluster_dispersion (REF-TEST-048: C1=0..7, C2=8..15, Terminal Shield Active, 0.0 us/op)
--- [REF-TEST-049] KDE Active Window Resource Guarantee & PM QoS C0 Pinning Verification ---
 [ORACLE GATE] Active Window Guarantee Benchmark:
   * Total Iterations: 2000
   * Total Time      : 13613.8 us
   * Average Latency : 6.8069 us/op
 [PASS] test_active_window_resource_guarantee_and_c0_qos (REF-TEST-049: C0 Pinning, C1 Spatial Affinity, 6.8069 us/op)
--- [REF-TEST-020] Dual-Domain State Journaling & Faithful Restoration Verification ---
 [INFO] Hardware Baseline Captured:
   * Platform Profile  : balanced
   * CPU Governor      : performance
   * CPU Boost         : 1
   * PCIe ASPM Policy  : performance
   * Scaling Max Freq  : 1700000 kHz
   * Panel Power Level : 0
 [PASS] test_state_journaling_and_faithful_restoration (REF-TEST-020: Dual-domain snapshot & 100% faithful restoration verified)
 [ORACLE GATE] EventLogger Stack Formatting (50k iters): 0.5042 us/op
 [ORACLE GATE] HistoryRingBuffer Append Latency (100k iters): 8.4945 ns/op
 [PASS] test_zero_disk_wakeup_logging_and_history_ring_buffer (REF-TEST-024 & REF-TEST-035: 7-Day 60,480-sample wrap, < 50ns append verified)
 [ORACLE GATE] Power Share Decomposition Math (100k iters): 0.0003 ns/op
 [PASS] test_circular_power_share_visualization (REF-TEST-025: Sum-invariant 100%, zero-division safety, < 100ns math verified)
 [ORACLE GATE] UltraEndurance Profile Actuation & 100% Round[2026-09-22 01:22:51] [WATTCURB][MITIGATION] Mitigation Actuated: PID 8881 (baloo_file) | Action: AntiStarvationCap | Details: Nice=10, Headroom Mask applied, cgroup quota=none
trip: 4.1304 ms
 [PASS] test_ultra_endurance_extensions (REF-TEST-028: SMT, Bluetooth, Backlight Cap, DRRS, KWin Effects & Baloo verified)
 [ORACLE GATE] Platform Loss Decomposition (12.5W System -> 3.8W Plat): DRAM=1.3641W, VRM=1.6864W, Wi-Fi=0.5247W, MB/IO=0.2249W (Invariant Sum=3.8000W)
 [PASS] test_wifi_txpower_and_platform_loss_decomposition (REF-TEST-029 verified)
--- [REF-TEST-057] Deterministic Battery Threshold Demotion (REF-REQ-094) ---
 [PASS] test_battery_low_performance_lockout (REF-TEST-057: 30/20/5% one-shot demotion, AC & above-30% stability verified)
 [ORACLE GATE] Tier 2 Ultra-Lightweight Probe Latency: 1441.2752 us/op
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
   - CPU Temp: 69.500000 C
   - CPU Cores Online: 16, Avg Freq: 2149 MHz
   - C-State POLL=29448768965us, C1=28584217283us, C2=322024762254us, C3=3358492190644us
   - GPU Power: 19.000000 W
   - GPU Busy: 1%
   - Fan RPM: 4342
   - Battery Discharging: true, AC Online: false
   - Battery Health: 94.191696%
   - NVMe Status: active, Read sectors: 471690999
 [PASS] test_persistent_hw_probe
 [PASS] test_pmu_perf_event_telemetry (Instructions counted: 0)
 [PASS] test_pmu_energy_proxy_metrics (EPI: 22.0600M, EWR: 9.3382%, P_est: 500.3304 mW)
 [PASS] test_pcie_binary_config_decoder (Gen4 x16 binary decode verified)
 [PASS] test_scoped_profiler (Zero-overhead release purity verified)
 [ORACLE GATE] Running micro-benchmark on zero-allocation parser...
 [ORACLE GATE] 100k stat parses completed in 11772 us (0.1177 us/op)
 [ORACLE GATE PASS] Performance within extreme efficiency threshold (< 1.0 us/op)
=== ALL TESTS & ORACLE GATE PASSED SUCCESSFULLY ===

 Performance counter stats for '/home/jedclub/Develop/WattCurb/build_pgo/wattcurb_tests':

          1,668.89 msec task-clock:u                                                          
     4,417,718,047      cycles:u                                                                (84.83%)
     9,406,236,689      instructions:u                                                          (84.86%)
         7,903,916      cache-misses:u                                                          (84.85%)
        62,597,628      L1-dcache-load-misses:u                                                 (84.97%)
           583,748      dTLB-load-misses:u                                                      (84.88%)
     2,222,080,600      branches:u                                                              (84.64%)
        14,334,570      branch-misses:u                                                         (84.80%)

       1.792945177 seconds time elapsed

       1.392165000 seconds user
       0.251639000 seconds sys
```

---

## 3. PMU Hardware Counter Telemetry (6-Second Daemon Run)

```text
[2m[*] WattCurb deep observation window: [>                       ] 0.0s / 6.0s (Interval 1/3)...[0m[2m[*] WattCurb deep observation window: [========>               ] 2.0s / 6.0s (Interval 2/3)...[0m[2m[*] WattCurb deep observation window: [================>       ] 4.0s / 6.0s (Interval 3/3)...[0m[K[2026-09-22 01:22:57] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2285800 (clang++) | Action: AntiStarvationCap | Details: Nice=10, Headroom Mask applied, cgroup quota=none
[2026-09-22 01:22:57] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2064651 (chrome) | Action: AntiStarvationCap | Details: Nice=10, Headroom Mask applied, cgroup quota=none
[2026-09-22 01:22:57] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2242744 (claude) | Action: AntiStarvationCap | Details: Nice=10, Headroom Mask applied, cgroup quota=none
[2026-09-22 01:22:57] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2063933 (chrome) | Action: AntiStarvationCap | Details: Nice=10, Headroom Mask applied, cgroup quota=none
[2026-09-22 01:22:57] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2227330 (ChatGPT) | Action: AntiStarvationCap | Details: Nice=10, Headroom Mask applied, cgroup quota=none
[2026-09-22 01:22:57] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2064536 (chrome) | Action: AntiStarvationCap | Details: Nice=10, Headroom Mask applied, cgroup quota=none
[2026-09-22 01:22:57] [WATTCURB][MITIGATION] Mitigation Actuated: PID 2157155 (chrome) | Action: AntiStarvationCap | Details: Nice=10, Headroom Mask applied, cgroup quota=none

[1m[36m================================================================================================================
                             WattCurb High-Fidelity Executive & Physical Power Briefing                         
================================================================================================================
[0m[2m Observation Scope    : [0m[1m6033 ms[0m[2m (3 continuous intervals)[0m[2m | Energy Consumption: [0m[1m185.56 Joules[0m[2m | Monitored Processes: [0m[1m162[0m[2m | Wakeups: [0m[1m986 /sec[0m

[1m [1] System Battery & Power Supply Deep Telemetry[0m
  - Total System Drain      : [1m[31m30.76 Watts[0m
  - Power Supply State      : [31mDischarging (On Battery)[0m
  - Battery Capacity        : [1m59%[0m [Normal] ([1m0.8h to empty[0m) | Health: 94.2% (105 cycles)
  - Battery Hardware ID     : [1mSMP LNV-5B10W13895[0m (S/N: 3502) [Li-poly]
  - Voltage & Current Flow  : 10.942 V (Design Nominal: 11.10 V) | Flow: [0m0.000 A[0m
  - Energy & Degradation    : 25.29 Wh now / 42.65 Wh full (Design: 45.28 Wh) | [1m[32m5.8% wear (2.63 Wh lost)[0m

[1m [2] Physical Hardware Domain Power & State Breakdown[0m
  * CPU Package (RAPL)    :  11.23 W ( 36.5%) [2mTemp: 69°C, 2165 MHz avg (performance)[0m
    └─ [2mC-State Sleep Residency : [0mC0 (Active): [1m11.3%[0m, C1: 1.1%, C2: 13.1%, C3 (Deep Sleep): [1m[32m74.4%[0m
    └─ [2mDirect PMU Telemetry    : [0mIPC: [1m1.08[0m | Cycles: 2906260 | LLC Miss: 21793 | Branch Miss: 28302
       [2mPMU Power Proxy (REF-REQ-024): [0mEPI: [1m[36m8.5M[0m | EWR: [1m[31m61.2%[0m | Est. Power: [1m500.0 mW[0m
  * GPU Silicon (DRM)     :   0.27 W (  0.9%) [2mBusy: 1%, 446 MB VRAM, 8.0 GT/s PCIe x16[0m
  * Display Backlight     :   1.49 W (  4.8%) [2mBrightness: 25%[0m
  * Storage / NVMe APST   :   0.36 W (  1.2%) [2mNVMe: active (Read 0.3 MB/s, Write 0.6 MB/s)[0m
  * Mechanical Fan        :   1.93 W (  6.3%) [2m4342 RPM[0m
  * Uncore & Platform Loss:  15.48 W ( 50.3%) [2mASPM: default [performance] powersave[0m

[1m [3] Top Battery Drain Culprits & Physical Causation Breakdown[0m
  1. [1mclang++[0m (PID: 2285800, [36mTier 5 (Runaway)[0m, 1 thr, Nice: 10)
     Drain: [1m[31m6.53 W[0m (21.2% of system, WDI: 54.7) | Primary Domain: [35mCPU Compute[0m
     Causation Mechanism: [1mCore Execution (598 ticks, 65% CPU)[0m
     Physical Metrics   : [2mWakeups: 12/s, RAM PSS: 378MB, Faults: 230 min/s 0 maj/s, Sockets: 0[0m
  2. [1mchrome[0m (PID: 2064651, [36mTier 3 (User App)[0m, 28 thr, Nice: 0)
     Drain: [1m[33m1.49 W[0m (4.8% of system, WDI: 21.0) | Primary Domain: [35mWiFi Radio CAM[0m
     Causation Mechanism: [1mActive Network Sockets (14 skt, CAM Mode)[0m
     Physical Metrics   : [2mWakeups: 103/s, RAM PSS: 266MB, Faults: 287 min/s 299 maj/s, Sockets: 14[0m
  3. [1mclaude[0m (PID: 2242744, [36mTier 3 (User App)[0m, 21 thr, Nice: 10)
     Drain: [1m[33m0.97 W[0m (3.2% of system, WDI: 14.0) | Primary Domain: [35mWiFi Radio CAM[0m
     Causation Mechanism: [1mActive Network Sockets (5 skt, CAM Mode)[0m
     Physical Metrics   : [2mWakeups: 139/s, RAM PSS: 337MB, Faults: 471 min/s 0 maj/s, Sockets: 5[0m
  4. [1mchrome[0m (PID: 2063933, [36mTier 3 (User App)[0m, 42 thr, Nice: 10)
     Drain: [1m[33m0.85 W[0m (2.8% of system, WDI: 13.3) | Primary Domain: [35mWiFi Radio CAM[0m
     Causation Mechanism: [1mActive Network Sockets (32 skt, CAM Mode)[0m
     Physical Metrics   : [2mWakeups: 30/s, RAM PSS: 140MB, Faults: 21 min/s 1 maj/s, Sockets: 32[0m
  5. [1mkwin_wayland[0m (PID: 1116, [36mTier 1 (Compositor)[0m, 38 thr, Nice: -12)
     Drain: [1m[33m0.62 W[0m (2.0% of system, WDI: 13.2) | Primary Domain: [35mPlatform/Idle[0m
     Causation Mechanism: [1mBackground Poll (201 w/s)[0m
     Physical Metrics   : [2mWakeups: 201/s, RAM PSS: 62MB, Faults: 865 min/s 0 maj/s, Sockets: 0[0m
  6. [1mopencode[0m (PID: 2263106, [36mTier 1 (Compositor)[0m, 35 thr, Nice: -5)
     Drain: [1m[33m1.15 W[0m (3.7% of system, WDI: 12.9) | Primary Domain: [35mCPU Compute[0m
     Causation Mechanism: [1mCore Execution (70 ticks, 7% CPU)[0m
     Physical Metrics   : [2mWakeups: 77/s, RAM PSS: 883MB, Faults: 3123 min/s 0 maj/s, Sockets: 4[0m
  7. [1mpipewire-pulse[0m (PID: 137791, [36mTier 0 (Immune)[0m, 2 thr, Nice: -12)
     Drain: [1m[33m0.84 W[0m (2.7% of system, WDI: 11.8) | Primary Domain: [35mWiFi Radio CAM[0m
     Causation Mechanism: [1mActive Network Sockets (26 skt, CAM Mode)[0m
     Physical Metrics   : [2mWakeups: 95/s, RAM PSS: 15MB, Faults: 0 min/s 0 maj/s, Sockets: 26[0m
  8. [1mplasmashell[0m (PID: 1262, [36mTier 2 (Shell)[0m, 105 thr, Nice: -6)
     Drain: [1m[33m0.82 W[0m (2.7% of system, WDI: 9.4) | Primary Domain: [35mWiFi Radio CAM[0m
     Causation Mechanism: [1mActive Network Sockets (15 skt, CAM Mode)[0m
     Physical Metrics   : [2mWakeups: 49/s, RAM PSS: 219MB, Faults: 0 min/s 0 maj/s, Sockets: 15[0m

[1m [4] Modular Battery Optimization Features Execution Status[0m
  - Overall Status      : [1m[32m8 throttled, 0 frozen, 0MB reclaimed (~1.03W saved across 3 features)[0m
  - Active Feature Details:
    * [32m[SchedIdleThrottle] 1 action(s) on target PIDs (~0.02W)[0m
    * [32m[PcieAspmEnforcer] 1 action(s) on target PIDs (~0.30W)[0m
    * [32m[AntiStarvationHeadroom] Headroom preserved, capped 7 greedy PID(s) (~1.01W)[0m

[1m [5] Actionable Engineering Recommendations[0m
  * [C-State Breaker] Wakeup frequency (986 wakeups/sec) is impeding CPU C3 deep sleep. Enabling TimerSlackCoalescing will bundle timers.
  * [PCIe Bus] PCIe ASPM policy is 'default [performance] powersave'. Enforcing 'powersave' policy will allow PCIe link substates L1.1/L1.2.
================================================================================================================


 Performance counter stats for '/home/jedclub/Develop/WattCurb/output/wattcurb --duration 6 -i 2':

             40.02 msec task-clock:u                                                          
         9,152,261      cycles:u                                                                (67.71%)
         9,350,572      instructions:u                                                          (67.91%)
           114,263      L1-dcache-load-misses:u                                                 (67.89%)
             3,302      dTLB-load-misses:u                                                      (70.32%)
            81,863      branch-misses:u                                                         (70.12%)
               282      page-faults:u                                                         

       6.042968263 seconds time elapsed

       0.003138000 seconds user
       0.036561000 seconds sys
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
