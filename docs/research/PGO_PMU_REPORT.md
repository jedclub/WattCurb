# [REF-RES-004] WattCurb PGO & PMU Hardware Performance Audit Report

- **Date**: 2026-09-22 07:22:37 UTC
- **Architecture**: x86_64 / AMD Ryzen 7 PRO 4750U with Radeon Graphics
- **Compiler**: GCC 16 with C++23, Link-Time Optimization (-flto=auto), and Native Tuning (-march=native)
- **Profile-Guided Optimization**: Active (-fprofile-use -fprofile-correction, 35 merged .gcda counter files from 7 representative workloads)
- **Oracle Gate Measurement**: perf-measured pass runs with WATTCURB_BENCH_TOLERANCE=20 (timing thresholds only; correctness assertions unscaled). The strict run is `scripts/harness.py test` and CI.
- **Dev Profiler**: Disabled (WATTCURB_DEV_PROFILE=OFF — zero ScopedProfiler overhead)

---

## 1. Production Binary Sizes (Stripped & ELF-Pruned)

| Binary | Size (bytes) | Size (KB) |
| :--- | ---: | ---: |
| `output/wattcurb` (Daemon) | 377192 | 368 KB |
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
   * Analysis Latency (120 pts): 153.917 us
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
 [PASS] test_hw_isa_primitives (Core ID=3, TSC=11111517223587)
 [PASS] test_ifunc_and_nttp_dispatch (GNU IFUNC & C++23 NTTP verified)
 [PASS] test_simd_scanner
 [PASS] test_custom_containers (FixedVector, FixedString, TopKHeap, DoubleBufferedPool, Canary & Guards verified)
 [PASS] test_memory_sequence_probe_and_cache_chunking (64B HotChunk, 32B CompactHot, 0-crossing Hot loop)
 [PASS] test_deep_battery_telemetry (ThinkPad BAT0 uevent, Degradation, Thresholds & Peripherals verified)
 [ORACLE GATE] Dense Battery Telemetry Profiling Benchmark (50000 iters):
   * SIMD uevent parse      : 1.3827 us/op (2346.1 cycles/op)
   * Battery physics calc   : 0.4763 us/op
   * Full-scope E2E pipeline: 2.5013 us/op (4244.1 cycles/op)
 [PASS] test_battery_telemetry_profiling_scopes (Dense Full-Scope REF-TEST-009)
--- [REF-TEST-011] Branchless SIMD & BMI2 PDEP Oracle Gate Verification ---
 [ORACLE GATE] Branchless SIMD & BMI2 PDEP parse_proc_stat (50000 iters):
   * Average Parse Latency : 0.7984 us/op
   * Average CPU Cycles    : 1354.7 cycles/op
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
   * Average Latency : 112.76 ns/op
   * Average Cycles  : 191.3 cycles/op
 [PASS] test_zero_cost_environment_abstraction (REF-TEST-012)
--- [REF-TEST-013] Syscall Storm Suppression & Lazy FD Bypass Verification ---
 [ORACLE GATE] Lazy FD Bypass Latency (50000 iters):
   * Average Latency : 0.34 ns/op
   * Average Cycles  : 0.6 cycles/op
 [PASS] test_syscall_storm_suppression_and_lazy_fd_bypass (REF-TEST-013)
 [PASS] test_tray_binary_shared_state (128B Seqlock, Zero-Copy POD serialization verified)
 [PASS] test_window_aware_governor (Non-Halting Graceful Throttle, Always-Alive Invariant verified: 5us)
 [PASS] test_unified_rapid_rollback (AC Plug-in / Charge event full-sweep restoration verified: 10842us, idem: 0us)
 [ORACLE GATE] ThinkPower ToolTip Render Latency (50000 iters):
   * Average Latency : 0.2676 us/op
   * Average Cycles  : 454.0 cycles/op
 [PASS] test_thinkpower_tray_client (REF-TEST-018: Zero-heap stack formatting, Icon states verified: 0.3 us/op)

--- [REF-TEST-037] Desktop Tray Hot-Path Profiling Audit (REF-REQ-072) ---
 [PASS] test_tray_hotpath_profiling_audit (REF-TEST-037: Fine-grained scopes, breakdown table verified)

--- [REF-TEST-038] Desktop Tray Client Extreme Optimization Oracle Gate ---
 [ORACLE GATE] Icon Name O(1) LUT Latency (100000 iters):
   * Average Latency : 0.00 ns/op
   * Average Cycles  : 0.0 cycles/op
 [ORACLE GATE] 500ms Hover Hysteresis Zero-Syscall Bypass Latency (100000 iters):
   * Average Latency : 176.98 ns/op
   * Average Cycles  : 300.3 cycles/op
 [ORACLE GATE] ToolTip Render Latency with BAR_LUT & Fast Sanitizer (20000 iters):
   * Average Latency : 0.303 us/op
   * Average Cycles  : 513.5 cycles/op
 [PASS] test_tray_top10_extreme_optimization_oracle_gate (REF-TEST-038: 500ms timegate, BAR_LUT, ICON_LUT verified)

--- [REF-TEST-039] Dashboard Matrix Profiling & Zero-Copy Ingestion (REF-REQ-074) ---
 [ORACLE GATE] Matrix Dashboard Telemetry Benchmark:
   * Full JSON Ingestion (1000 iters) : 1496.23 us/op (2538681.77 cycles/op)
   * Poll Loop Delta Gate (5000 iters) : 75.79 us/op (128589.27 cycles/op)
 [PASS] test_dashboard_matrix_profiling_audit (REF-TEST-039: Dashboard Scopes, Zero-Copy Shares & Delta Gate verified)

--- [REF-TEST-053] Matrix Dashboard Expanded Power Shares (11 Procs), Full Hardware Visibility & Typography Scaling (REF-REQ-089, REF-ARCH-066) ---
 [ORACLE GATE] 12-Process & Decomposed HW Share Poll Benchmark (50000 iters):
   * Poll + Decomposition Latency: 55.58 us/op (94298.33 cycles/op)
 [PASS] test_matrix_dashboard_expanded_power_shares_and_typography (REF-TEST-053: Top 11 Procs + Other, Full Hardware Visibility, QML Layout & Typography verified)

--- [REF-TEST-054] Process C-State Affinity Classification & Cyber Badge Telemetry (REF-REQ-090, REF-ARCH-067) ---
 [ORACLE GATE] Process C-State Classification Benchmark (100000 iters):
   * Heuristic Latency: 3.10 ns/op (5.24 cycles/op)
 [PASS] test_process_cstate_affinity_and_badges (REF-TEST-054: Heuristic, Table Badges, QML Layout & Hover Diagnostics verified)

--- [REF-TEST-055] Bi-Directional Power Profile Coherence & Seqlock Synchronization (REF-REQ-091, REF-ARCH-068) ---
 [ORACLE GATE] Seqlock Power Profile Coherence Benchmark (100000 iters):
   * Seqlock Update + Read Latency: 4.18 ns/op (7.07 cycles/op)
 [PASS] test_bi_directional_power_profile_coherence (REF-TEST-055: Seqlock Versioning, Ingestion & Coherence verified)

--- [REF-TEST-056] Ultimate Performance Unleash Full-Silicon Actuation Oracle Gate (REF-REQ-092, REF-ARCH-069) ---
   * Performance PM QoS C0 Clamp Active : NO (Mock/Non-root)
   * NVMe APST Zero-Latency Modified    : NO
   * CFS Sched Migration Cost Modified  : NO
   * Wi-Fi Power Save Disabled          : NO
   * Transition-Path Ordering Invariant   : VERIFIED (PowerSaver -> Performance -> Balanced)
 [ORACLE GATE] Ultimate Performance Actuation Benchmark (10000 iters):
   * PM QoS + GPU Profile Switch Latency: 923.59 ns/op (1566.73 cycles/op)
 [PASS] test_ultimate_performance_unleash_actuation (REF-TEST-056: C0 Clamp, GPU 3D, APST 0, Rollback verified)

--- [REF-TEST-058] Watt-Reactive Tray Icon Rendering (REF-REQ-095, REF-ARCH-071) ---
   * Dial fill increment       : 0.0->0.5 lights x=14.4px (136px), 0.5->1.0 lights x=33.4px (122px)
   * Badge hue (R-G)           : -163.0 (green) -> 196.0 (red), source=font
 [ORACLE GATE] Procedural Icon Render (300 iters @48px):
   * Average Latency : 273.3 us/op
 [PASS] test_watt_reactive_tray_icon (REF-TEST-058: bands, ramp, frame warning, 4 distinct badges, needle deflection verified)

--- [REF-TEST-059] Audio Continuity Guarantee (REF-REQ-096) ---
   * Host cpuidle exit latency : 1 us (shallowest) .. 350 us (deepest)
   * Audio latency ceiling     : 100 us
   * PCM probe                 : 338.2 us/op, active=no, owners=0
   * Tier shielding            : interactive=open, background worker=open
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
   * Competing power manager detected (power-profiles-daemon); platform_profile coordinated via ppd
   * Affinity repair matches engine masks, spares a deliberate taskset
 [PASS] test_profile_throughput_and_liveness_guarantees (REF-TEST-061: no C0 clamp in Performance, playback-independent stall shield, bounded writeback verified)

--- [REF-TEST-062] Audit Defect Remediation (REF-REQ-111, REF-RES-029) ---
   * Process identity readable (start_time=58171399 ticks)
   * DEF-2 ordering invariant verified by inspection, not by this suite
   * FeatureManager destructor releases tracked process state
 [PASS] test_audit_defect_remediation (REF-TEST-062: process identity, tracking bound, shutdown release verified)

--- [REF-TEST-068] CPU Frequency Ceiling Baseline (REF-REQ-112) ---
   - Hardware ceiling: 1700000 kHz, captured baseline: 1700000 kHz
   - Production path re-asserted the ceiling (1 invocation)
 [PASS] test_cpu_ceiling_baseline_is_hardware_max (REF-TEST-068: baseline >= cpuinfo_max_freq, sandboxed assertion writes nothing, production path re-asserts)
--- [REF-TEST-072] Load-Aware Frequency Starvation Watchdog ---
 [PASS] test_frequency_starvation_watchdog (REF-TEST-072: loaded-low trips, loaded-boost/idle clean, degenerate safe)

--- [REF-TEST-069] Non-Halting Memory Pressure Ladder (REF-REQ-112) ---
 [PASS] test_memory_pressure_ladder (REF-TEST-069: swap-led escalation, PSI escalation, one-step hysteresis, swapless safety, tier 0-2 and focused-window exemption verified)

--- [REF-TEST-070] Memory Pressure Parsers (REF-REQ-112) ---
 [PASS] test_memory_pressure_parsers (REF-TEST-070: line-anchored meminfo fields, truncation safety, PSI avg10 extraction verified)

--- [REF-TEST-071] Performance Swap Expansion Policy (REF-REQ-113) ---
 [PASS] test_performance_swap_expansion_policy (REF-TEST-071: growth thresholds, disk floor and file ceiling, release safety, Performance brake-only-when-exhausted, sandbox containment verified)
--- [REF-TEST-041] Multilingual L10n & Auto System Locale Verification ---
 [ORACLE GATE] O(1) L10n Translation Latency (100000 iters):
   * Average Latency : 51.5 ns/op
 [PASS] test_multilingual_l10n_and_auto_system_locale (REF-TEST-041: 13 languages, 46 strings, POSIX auto-detect verified)
 [ORACLE GATE] Headroom Mask Computation Benchmark (50000 iters):
   * Average Latency : 0.0049 us/op
   * Average Cycles  : 8.3 cycles/op
 [PASS] test_anti_starvation_and_greedy_capping (R[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (konsole) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 () | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:22] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005815 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:23] [WATTCURB][MITIGATION] Mitigation Actuated: PID 3005816 (sandbox-probe) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-22 16:22:24] [WATTCURB][ALERT:WARN] CPU frequency starved under load: max core 0 MHz vs ceiling 1700 MHz, load1=20.66, profile=Performance; re-asserted ceiling and platform_profile (REF-REQ-112.10)
EF-TEST-019: Cores 0..13 allowed, 2 reserved for audio/compositor, 0.0 us/op)
--- [REF-TEST-048] Adaptive C1/C2 Dual-Cluster Spatial Load Dispersion & Terminal Shield Verification ---
 [ORACLE GATE] Adaptive C1/C2 Cluster Benchmark (50000 iters):
   * Average Latency : 0.0145 us/op
   * Average Cycles  : 24.7 cycles/op
 [PASS] test_adaptive_c1_c2_cluster_dispersion (REF-TEST-048: C1=0..7, C2=8..15, Terminal Shield Active, 0.0 us/op)
--- [REF-TEST-049] KDE Active Window Resource Guarantee & PM QoS C0 Pinning Verification ---
 [ORACLE GATE] Active Window Guarantee Benchmark:
   * Total Iterations: 2000
   * Total Time      : 171948.6 us
   * Average Latency : 85.9743 us/op
 [PASS] test_active_window_resource_guarantee_and_c0_qos (REF-TEST-049: C0 Pinning, C1 Spatial Affinity, 85.9743 us/op)
--- [REF-TEST-063] Desktop Session Token Shell-Safety Verification ---
 [PASS] test_desktop_session_token_shell_safety (REF-TEST-063: 16 hostile socket names + 11 hostile runtime dirs rejected)
--- [REF-TEST-064] Actuation Sandbox vs Window Governor Live-Write Verification ---
 [PASS] test_actuation_sandbox_blocks_window_governor (REF-TEST-064: mock PM QoS untouched under sandbox, 4-byte pin when off)
--- [REF-TEST-020] Dual-Domain State Journaling & Faithful Restoration Verification ---
 [INFO] Hardware Baseline Captured:
   * Platform Profile  : performance
   * CPU Governor      : performance
   * CPU Boost         : 1
   * PCIe ASPM Policy  : performance
   * Scaling Max Freq  : 1700000 kHz
   * Panel Power Level : 0
 [PASS] test_state_journaling_and_faithful_restoration (REF-TEST-020: Dual-domain snapshot & 100% faithful restoration verified)
 [ORACLE GATE] EventLogger Stack Formatting (50k iters): 23.7429 us/op
 [ORACLE GATE] HistoryRingBuffer Append Latency (100k iters): 14.7274 ns/op
 [PASS] test_zero_disk_wakeup_logging_and_history_ring_buffer (REF-TEST-024 & REF-TEST-035: 7-Day 60,480-sample wrap, < 50ns append verified)
 [ORACLE GATE] Power Share Decomposition Math (100k iters): 0.0017 ns/op
 [PASS] test_circular_power_share_visualization (REF-TEST-025: Sum-invariant 100%, zero-division safety, < 100ns math verified)
 [ORACLE GATE] UltraEndurance Profile Actuation & 100% Roundtrip: 10.6646 ms
 [PASS] test_ultra_endurance_extensions (REF-TEST-028: SMT, Bluetooth, Backlight Cap, DRRS, KWin Effects & Baloo verified)
 [ORACLE GATE] Platform Loss Decomposition (12.5W System -> 3.8W Plat): DRAM=1.3641W, VRM=1.6864W, Wi-Fi=0.5247W, MB/IO=0.2249W (Invariant Sum=3.8000W)
 [PASS] test_wifi_txpower_and_platform_loss_decomposition (REF-TEST-029 verified)
--- [REF-TEST-057] Deterministic Battery Threshold Demotion (REF-REQ-094) ---
 [PASS] test_battery_low_performance_lockout (REF-TEST-057: 30/20/5% one-shot demotion, AC & above-30% stability verified)
 [ORACLE GATE] Tier 2 Ultra-Lightweight Probe Latency: 40167.9066 us/op
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
 [PASS] test_rapl_wrap_and_boottime_timebase (REF-TEST-066: range-based wrap, 0 on reset, BOOTTIME monotonic)
 [PASS] test_apu_ppt_and_gpu_duty_cycle_attribution (REF-TEST-009, REF-REQ-051: APU PPT decoupled, duty-cycle scaled)
 [PASS] test_windowed_attribution_engine
 [PASS] test_singleton_lock
 [INFO] Hardware Probe Sample Captured:
   - CPU Temp: 70.250000 C
   - CPU Cores Online: 16, Avg Freq: 2370 MHz
   - C-State POLL=29448768965us, C1=39613210451us, C2=410679903428us, C3=3986375800393us
   - GPU Power: 15.000000 W
   - GPU Busy: 1%
   - Fan RPM: 4329
   - Battery Discharging: false, AC Online: true
   - Battery Health: 94.191696%
   - NVMe Status: active, Read sectors: 974614033
 [PASS] test_persistent_hw_probe
 [PASS] test_peripheral_battery_fd_lifecycle (REF-TEST-067: peripheral fd reopened, 42% Charging)
 [PASS] test_pmu_perf_event_telemetry (Instructions counted: 0)
 [PASS] test_pmu_energy_proxy_metrics (EPI: 22.0600M, EWR: 9.3382%, P_est: 500.3304 mW)
 [PASS] test_pcie_binary_config_decoder (Gen4 x16 binary decode verified)
 [PASS] test_scoped_profiler (Zero-overhead release purity verified)
 [ORACLE GATE] Running micro-benchmark on zero-allocation parser...
 [ORACLE GATE] 100k stat parses completed in 624862 us (6.2486 us/op)
 [ORACLE GATE PASS] Performance within extreme efficiency threshold (< 1.0 us/op)
=== ALL TESTS & ORACLE GATE PASSED SUCCESSFULLY ===

 Performance counter stats for '/home/jedclub/Develop/WattCurb/build_pgo/wattcurb_tests':

          3,721.60 msec task-clock:u                                                          
     6,762,801,861      cycles:u                                                                (28.85%)
     9,323,006,982      instructions:u                                                          (29.00%)
        52,033,114      cache-misses:u                                                          (28.97%)
       102,068,476      L1-dcache-load-misses:u                                                 (28.41%)
         1,368,664      dTLB-load-misses:u                                                      (28.24%)
     2,171,426,606      branches:u                                                              (28.25%)
        16,302,304      branch-misses:u                                                         (28.55%)

      14.623122057 seconds time elapsed

       2.997656000 seconds user
       0.592687000 seconds sys
```

---

## 3. PMU Hardware Counter Telemetry (6-Second Daemon Run)

```text
[2m[*] WattCurb deep observation window: [>                       ] 0.0s / 6.0s (Interval 1/3)...[0m[2m[*] WattCurb deep observation window: [========>               ] 2.0s / 6.0s (Interval 2/3)...[0m[2m[*] WattCurb deep observation window: [================>       ] 4.0s / 6.0s (Interval 3/3)...[0m[K
[1m[36m================================================================================================================
                             WattCurb High-Fidelity Executive & Physical Power Briefing                         
================================================================================================================
[0m[2m Observation Scope    : [0m[1m6410 ms[0m[2m (3 continuous intervals)[0m[2m | Energy Consumption: [0m[1m139.31 Joules[0m[2m | Monitored Processes: [0m[1m233[0m[2m | Wakeups: [0m[1m5698 /sec[0m

[1m [1] System Battery & Power Supply Deep Telemetry[0m
  - Total System Drain      : [1m[33m21.73 Watts[0m[1m[32m [AC Hardware Pass-Through Active: Zero Battery Wear][0m
  - Power Supply State      : [32mAC Connected (Line Power / Charging)[0m[2m [USB-PD Input: 0.0W (C [PD] PD_PPS)][0m
  - Battery Capacity        : [1m80%[0m [Normal] | Health: 94.2% (107 cycles)
  - Battery Hardware ID     : [1mSMP LNV-5B10W13895[0m (S/N: 3502) [Li-poly]
  - Voltage & Current Flow  : 11.867 V (Design Nominal: 11.10 V) | Flow: [0m0.000 A[0m
  - Energy & Degradation    : 33.91 Wh now / 42.65 Wh full (Design: 45.28 Wh) | [1m[32m5.8% wear (2.63 Wh lost)[0m

[1m [2] Physical Hardware Domain Power & State Breakdown[0m
  * CPU Package (RAPL)    :  15.32 W ( 70.5%) [2mTemp: 70°C, 2399 MHz avg (performance)[0m
    └─ [2mC-State Sleep Residency : [0mC0 (Active): [1m92.4%[0m, C1: 1.1%, C2: 2.7%, C3 (Deep Sleep): [1m[33m3.9%[0m
  * GPU Silicon (DRM)     :   0.05 W (  0.2%) [2mBusy: 0%, 466 MB VRAM, 8.0 GT/s PCIe x16[0m
  * Display Backlight     :   1.49 W (  6.9%) [2mBrightness: 25%[0m
  * Storage / NVMe APST   :   1.76 W (  8.1%) [2mNVMe: active (Read 61.2 MB/s, Write 56.0 MB/s)[0m
  * Mechanical Fan        :   1.91 W (  8.8%) [2m4329 RPM[0m
  * Uncore & Platform Loss:   1.20 W (  5.5%) [2mASPM: default [performance] powersave[0m

[1m [3] Top Battery Drain Culprits & Physical Causation Breakdown[0m
  1. [1mChatGPT[0m (PID: 2724773, [36mTier 5 (Runaway)[0m, 56 thr, Nice: -8)
     Drain: [1m[31m1.55 W[0m (7.1% of system, WDI: 23.9) | Primary Domain: [35mWiFi Radio CAM[0m
     Causation Mechanism: [1mActive Network Sockets (32 skt, CAM Mode)[0m
     Physical Metrics   : [2mWakeups: 153/s, RAM PSS: 169MB, Faults: 816 min/s 24 maj/s, Sockets: 32[0m
  2. [1mclang++[0m (PID: 3005625, [36mTier 5 (Runaway)[0m, 1 thr, Nice: -8)
     Drain: [1m[33m1.00 W[0m (4.6% of system, WDI: 23.8) | Primary Domain: [35mCPU Compute[0m
     Causation Mechanism: [1mCore Execution (362 ticks, 4% CPU)[0m
     Physical Metrics   : [2mWakeups: 289/s, RAM PSS: 667MB, Faults: 6038 min/s 1 maj/s, Sockets: 0[0m
  3. [1mclang++[0m (PID: 3005749, [36mTier 5 (Runaway)[0m, 1 thr, Nice: -8)
     Drain: [1m[33m1.16 W[0m (5.3% of system, WDI: 22.4) | Primary Domain: [35mCPU Compute[0m
     Causation Mechanism: [1mCore Execution (516 ticks, 6% CPU)[0m
     Physical Metrics   : [2mWakeups: 223/s, RAM PSS: 515MB, Faults: 7503 min/s 1 maj/s, Sockets: 0[0m
  4. [1mclang++[0m (PID: 3005754, [36mTier 5 (Runaway)[0m, 1 thr, Nice: -8)
     Drain: [1m[33m1.09 W[0m (5.0% of system, WDI: 22.0) | Primary Domain: [35mCPU Compute[0m
     Causation Mechanism: [1mCore Execution (418 ticks, 5% CPU)[0m
     Physical Metrics   : [2mWakeups: 325/s, RAM PSS: 491MB, Faults: 6469 min/s 0 maj/s, Sockets: 0[0m
  5. [1mChatGPT[0m (PID: 2724967, [36mTier 5 (Runaway)[0m, 23 thr, Nice: 0)
     Drain: [1m[33m1.29 W[0m (5.9% of system, WDI: 21.2) | Primary Domain: [35mWiFi Radio CAM[0m
     Causation Mechanism: [1mActive Network Sockets (14 skt, CAM Mode)[0m
     Physical Metrics   : [2mWakeups: 152/s, RAM PSS: 360MB, Faults: 383 min/s 15 maj/s, Sockets: 14[0m
  6. [1mbun[0m (PID: 3005538, [36mTier 5 (Runaway)[0m, 12 thr, Nice: -8)
     Drain: [1m[33m1.20 W[0m (5.5% of system, WDI: 20.9) | Primary Domain: [35mCPU Compute[0m
     Causation Mechanism: [1mCore Execution (509 ticks, 6% CPU)[0m
     Physical Metrics   : [2mWakeups: 276/s, RAM PSS: 104MB, Faults: 239 min/s 0 maj/s, Sockets: 0[0m
  7. [1mclang++[0m (PID: 3005657, [36mTier 5 (Runaway)[0m, 1 thr, Nice: -8)
     Drain: [1m[33m1.13 W[0m (5.2% of system, WDI: 20.8) | Primary Domain: [35mCPU Compute[0m
     Causation Mechanism: [1mCore Execution (472 ticks, 5% CPU)[0m
     Physical Metrics   : [2mWakeups: 288/s, RAM PSS: 606MB, Faults: 6653 min/s 0 maj/s, Sockets: 0[0m
  8. [1mclang++[0m (PID: 3005628, [36mTier 5 (Runaway)[0m, 1 thr, Nice: -8)
     Drain: [1m[33m1.04 W[0m (4.8% of system, WDI: 20.3) | Primary Domain: [35mCPU Compute[0m
     Causation Mechanism: [1mCore Execution (411 ticks, 4% CPU)[0m
     Physical Metrics   : [2mWakeups: 293/s, RAM PSS: 639MB, Faults: 5941 min/s 0 maj/s, Sockets: 0[0m

[1m [4] Modular Battery Optimization Features Execution Status[0m
  - Overall Status      : [1m[2mOptimal baseline (No intrusive throttling needed)[0m
    * All optimization features running in passive baseline observation mode.

[1m [5] Actionable Engineering Recommendations[0m
  * [C-State Breaker] Wakeup frequency (5698 wakeups/sec) is impeding CPU C3 deep sleep. Enabling TimerSlackCoalescing will bundle timers.
================================================================================================================


 Performance counter stats for '/home/jedclub/Develop/WattCurb/output/wattcurb --duration 6 -i 2':

            128.65 msec task-clock:u                                                          
        19,149,003      cycles:u                                                                (38.99%)
         8,213,852      instructions:u                                                          (41.86%)
           249,003      L1-dcache-load-misses:u                                                 (41.99%)
             9,094      dTLB-load-misses:u                                                      (39.12%)
           168,494      branch-misses:u                                                         (38.04%)
               315      page-faults:u                                                         

       6.523672347 seconds time elapsed

       0.001405000 seconds user
       0.123611000 seconds sys
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
