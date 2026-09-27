# [REF-RES-004] WattCurb PGO & PMU Hardware Performance Audit Report

- **Date**: 2026-09-27 12:19:57 UTC
- **Architecture**: x86_64 / AMD Ryzen 7 PRO 4750U with Radeon Graphics
- **Compiler**: GCC 16 with C++23, Link-Time Optimization (-flto=auto), and Native Tuning (-march=native)
- **Profile-Guided Optimization**: Active (-fprofile-use -fprofile-correction, 33 merged .gcda counter files from 7 representative workloads)
- **Oracle Gate Measurement**: perf-measured pass runs with WATTCURB_BENCH_TOLERANCE=20 (timing thresholds only; correctness assertions unscaled). The strict run is `scripts/harness.py test` and CI.
- **Dev Profiler**: Disabled (WATTCURB_DEV_PROFILE=OFF — zero ScopedProfiler overhead)

---

## 1. Production Binary Sizes (Stripped & ELF-Pruned)

| Binary | Size (bytes) | Size (KB) |
| :--- | ---: | ---: |
| `output/wattcurb` (Daemon) | 393680 | 384 KB |
| `output/wattcurb-tray` (Desktop Tray) | 80336 | 78 KB |
| `output/wattcurb-dashboard` (Matrix Dashboard) | 261536 | 255 KB |

---

## 2. PMU Hardware Counter Telemetry (Oracle Gate Test Suite)

```text
QObject::startTimer: current thread's event dispatcher has already been destroyed
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
   * Analysis Latency (120 pts): 47.078 us
 [PASS] test_deep_battery_drain_report_oracle_gate (REF-TEST-043: Full SHM sweep, hardware decomposition, process attribution verified)
 [ORACLE GATE] Verifying Power Profile Telemetry Filtering & Comparisons (REF-TEST-050)...
 [PASS] test_power_profile_filtering_and_comparisons (REF-TEST-050: Filter & comparison matrix verified)
 [ORACLE GATE] Verifying Battery Report AC Telemetry Fallback & Retention (REF-TEST-086)...
 [PASS] test_battery_report_ac_telemetry_fallback (REF-TEST-086: AC fallback, [⚡AC] badges, and adaptive labeling verified)
 [ORACLE GATE] Verifying Kernel VM Writeback & Laptop Mode Coalescing (REF-TEST-051)...
 [PASS] test_kernel_vm_writeback_and_laptop_mode_coalescing (REF-TEST-051: VM writeback & laptop mode roundtrip verified)
 [ORACLE GATE] Verifying Ultimate UltraEndurance Full-Spectrum Power Minimization (REF-TEST-052)...
 [PASS] test_ultimate_ultra_endurance_power_minimization (REF-TEST-052: All 6 dimensions verified, Zero-Kill preserved)
 [PASS] test_modeset_flapping_elimination_and_test_isolation (REF-TEST-036: Subshell bypass, Idempotent DRRS & KWin effects verified)
 [INFO] CPU Features detected: AVX2=1 BMI1=1 BMI2=1 POPCNT=1 AVX512F=0
 [PASS] test_cpu_features
 [PASS] test_hw_isa_primitives (Core ID=14, TSC=173030582188126)
 [PASS] test_ifunc_and_nttp_dispatch (GNU IFUNC & C++23 NTTP verified)
 [PASS] test_simd_scanner
 [PASS] test_custom_containers (FixedVector, FixedString, TopKHeap, DoubleBufferedPool, Canary & Guards verified)
 [PASS] test_memory_sequence_probe_and_cache_chunking (64B HotChunk, 32B CompactHot, 0-crossing Hot loop)
 [PASS] test_deep_battery_telemetry (ThinkPad BAT0 uevent, Degradation, Thresholds & Peripherals verified)
 [ORACLE GATE] Dense Battery Telemetry Profiling Benchmark (50000 iters):
   * SIMD uevent parse      : 0.9634 us/op (1634.6 cycles/op)
   * Battery physics calc   : 0.3967 us/op
   * Full-scope E2E pipeline: 1.4926 us/op (2532.7 cycles/op)
 [PASS] test_battery_telemetry_profiling_scopes (Dense Full-Scope REF-TEST-009)
--- [REF-TEST-011] Branchless SIMD & BMI2 PDEP Oracle Gate Verification ---
 [ORACLE GATE] Branchless SIMD & BMI2 PDEP parse_proc_stat (50000 iters):
   * Average Parse Latency : 0.3182 us/op
   * Average CPU Cycles    : 540.0 cycles/op
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
   * Average Latency : 70.43 ns/op
   * Average Cycles  : 119.5 cycles/op
 [PASS] test_zero_cost_environment_abstraction (REF-TEST-012)
--- [REF-TEST-013] Syscall Storm Suppression & Lazy FD Bypass Verification ---
 [ORACLE GATE] Lazy FD Bypass Latency (50000 iters):
   * Average Latency : 14.12 ns/op
   * Average Cycles  : 24.0 cycles/op
 [PASS] test_syscall_storm_suppression_and_lazy_fd_bypass (REF-TEST-013)
 [PASS] test_tray_binary_shared_state (128B Seqlock, Zero-Copy POD serialization verified)
 [PASS] test_window_aware_governor (Non-Halting Graceful Throttle, Always-Alive Invariant verified: 0us)
 [PASS] test_window_minimized_progressive_cstate_governor (REF-TEST-082: Performance 10m, Balanced 1m, PowerSaver 0s, Audio Guard verified)
 [PASS] test_unified_rapid_rollback (AC Plug-in / Charge event full-sweep restoration verified: 4881us, idem: 0us)
 [ORACLE GATE] ThinkPower ToolTip Render Latency (50000 iters):
   * Average Latency : 0.1594 us/op
   * Average Cycles  : 270.4 cycles/op
 [PASS] test_thinkpower_tray_client (REF-TEST-018: Zero-heap stack formatting, Icon states verified: 0.2 us/op)

--- [REF-TEST-074] Effective Total Power Fallback (REF-REQ-116) ---
 [PASS] test_effective_total_power_fallback (REF-TEST-074: AC domain-sum total, DC rail preserved, 100% saturation eliminated)

--- [REF-TEST-075] User-Application Exemption from Mitigation (REF-REQ-117) ---
 [PASS] test_user_app_tree_exemption (REF-TEST-075: ChatGPT/codex/kmscon classified safe, no Tier 3 throttling in Balanced, indexers still throttleable)

--- [REF-TEST-037] Desktop Tray Hot-Path Profiling Audit (REF-REQ-072) ---
 [PASS] test_tray_hotpath_profiling_audit (REF-TEST-037: Fine-grained scopes, breakdown table verified)

--- [REF-TEST-038] Desktop Tray Client Extreme Optimization Oracle Gate ---
 [ORACLE GATE] Icon Name O(1) LUT Latency (100000 iters):
   * Average Latency : 0.00 ns/op
   * Average Cycles  : 0.0 cycles/op
 [ORACLE GATE] 500ms Hover Hysteresis Zero-Syscall Bypass Latency (100000 iters):
   * Average Latency : 73.66 ns/op
   * Average Cycles  : 125.0 cycles/op
 [ORACLE GATE] ToolTip Render Latency with BAR_LUT & Fast Sanitizer (20000 iters):
   * Average Latency : 0.117 us/op
   * Average Cycles  : 199.1 cycles/op
 [PASS] test_tray_top10_extreme_optimization_oracle_gate (REF-TEST-038: 500ms timegate, BAR_LUT, ICON_LUT verified)

--- [REF-TEST-039] Dashboard Matrix Profiling & Zero-Copy Ingestion (REF-REQ-074) ---
 [ORACLE GATE] Matrix Dashboard Telemetry Benchmark:
   * Full JSON Ingestion (1000 iters) : 1495.03 us/op (2536774.80 cycles/op)
   * Poll Loop Delta Gate (5000 iters) : 35.33 us/op (59951.14 cycles/op)
 [PASS] test_dashboard_matrix_profiling_audit (REF-TEST-039: Dashboard Scopes, Zero-Copy Shares & Delta Gate verified)

--- [REF-TEST-053] Matrix Dashboard Expanded Power Shares (11 Procs), Full Hardware Visibility & Typography Scaling (REF-REQ-089, REF-ARCH-066) ---
 [ORACLE GATE] 12-Process & Decomposed HW Share Poll Benchmark (50000 iters):
   * Poll + Decomposition Latency: 39.37 us/op (66806.32 cycles/op)
 [PASS] test_matrix_dashboard_expanded_power_shares_and_typography (REF-TEST-053: Top 11 Procs + Other, Full Hardware Visibility, QML Layout & Typography verified)

--- [REF-TEST-054] Process C-State Affinity Classification & Cyber Badge Telemetry (REF-REQ-090, REF-ARCH-067) ---
 [ORACLE GATE] Process C-State Classification Benchmark (100000 iters):
   * Heuristic Latency: 3.73 ns/op (6.31 cycles/op)
 [PASS] test_process_cstate_affinity_and_badges (REF-TEST-054: Heuristic, Table Badges, QML Layout & Hover Diagnostics verified)

--- [REF-TEST-055] Bi-Directional Power Profile Coherence & Seqlock Synchronization (REF-REQ-091, REF-ARCH-068) ---
 [ORACLE GATE] Seqlock Power Profile Coherence Benchmark (100000 iters):
   * Seqlock Update + Read Latency: 5.12 ns/op (8.68 cycles/op)
 [PASS] test_bi_directional_power_profile_coherence (REF-TEST-055: Seqlock Versioning, Ingestion & Coherence verified)

--- [REF-TEST-056] Ultimate Performance Unleash Full-Silicon Actuation Oracle Gate (REF-REQ-092, REF-ARCH-069) ---
   * Performance PM QoS C0 Clamp Active : NO (Mock/Non-root)
   * NVMe APST Zero-Latency Modified    : NO
   * CFS Sched Migration Cost Modified  : NO
   * Wi-Fi Power Save Disabled          : NO
   * Transition-Path Ordering Invariant   : VERIFIED (PowerSaver -> Performance -> Balanced)
 [ORACLE GATE] Ultimate Performance Actuation Benchmark (10000 iters):
   * PM QoS + GPU Profile Switch Latency: 13.58 ns/op (22.97 cycles/op)
 [PASS] test_ultimate_performance_unleash_actuation (REF-TEST-056: C0 Clamp, GPU 3D, APST 0, Rollback verified)

--- [REF-TEST-086] Matrix Dashboard System & Process Memory Telemetry Visualization (REF-REQ-132, REF-ARCH-079) ---
 [PASS] test_matrix_dashboard_memory_telemetry_visualization (REF-TEST-086: System RAM/Swap & Process PSS/RSS ingestion verified)

--- [REF-TEST-058] Watt-Reactive Tray Icon Rendering (REF-REQ-095, REF-ARCH-071) ---
   * Dial fill increment       : 0.0->0.5 lights x=14.4px (136px), 0.5->1.0 lights x=33.4px (122px)
   * Badge hue (R-G)           : -163.0 (green) -> 196.0 (red), source=font
 [ORACLE GATE] Procedural Icon Render (300 iters @48px):
   * Average Latency : 119.9 us/op
 [PASS] test_watt_reactive_tray_icon (REF-TEST-058: bands, ramp, frame warning, 4 distinct badges, needle deflection verified)

--- [REF-TEST-059] Audio Continuity Guarantee (REF-REQ-096) ---
   * Host cpuidle exit latency : 1 us (shallowest) .. 350 us (deepest)
   * Audio latency ceiling     : 100 us
   * PCM probe                 : 121.2 us/op, active=no, owners=0
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
   * Process identity readable (start_time=27552861 ticks)
   * DEF-2 ordering invariant verified by inspection, not by this suite
   * FeatureManager destructor releases tracked process state
 [PASS] test_audit_defect_remediation (REF-TEST-062: process identity, tracking bound, shutdown release verified)

--- [REF-TEST-068] CPU Frequency Ceiling Baseline (REF-REQ-112) ---
   - Hardware ceiling: 1700000 kHz, captured baseline: 1700000 kHz
   - Production path re-asserted the ceiling (1 invocation)
 [PASS] test_cpu_ceiling_baseline_is_hardware_max (REF-TEST-068: baseline >= cpuinfo_max_freq, sandboxed assertion writes nothing, production path re-asserts)
--- [REF-TEST-072] Load-Aware Frequency Starvation Watchdog ---
 [PASS] test_frequency_starvation_watchdog (REF-TEST-072: loaded-low trips, P-state-floor pin trips (REF-REQ-126), loaded-boost/idle clean, degenerate safe)
--- [REF-TEST-080] SMU Power-Limit Clawback Verification (REF-REQ-126) ---
 [PASS] test_smu_limit_clawback_verification (REF-TEST-080: parser reads the right row, clawback trips, accepted write and failed read do not)
--- [REF-TEST-079] SMU Raise Guardrails (REF-REQ-115.3) ---
 [PASS] test_smu_raise_is_guarded (REF-TEST-079: sandboxed raise refuses, restore is a no-op, limits internally consistent)
--- [REF-TEST-073] ThinkPad Thermal-Assist Fan Curve & SMU Limits ---
 [PASS] test_thinkpad_fan_thermal_assist_and_smu_limits (REF-TEST-073: curve monotonic/sandboxed, SMU constants ordered, all-profile wiring, Ultra-only cold stop)

--- [REF-TEST-069] Non-Halting Memory Pressure Ladder (REF-REQ-112) ---
 [PASS] test_memory_pressure_ladder (REF-TEST-069: swap-led escalation, PSI escalation, one-step hysteresis, swapless safety, tier 0-2 and focused-window exemption verified)

--- [REF-TEST-070] Memory Pressure Parsers (REF-REQ-112) ---
 [PASS] test_memory_pressure_parsers (REF-TEST-070: line-anchored meminfo fields, truncation safety, PSI avg10 extraction verified)

--- [REF-TEST-071] Performance Swap Expansion Policy (REF-REQ-113) ---
 [P[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (konsole) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 () | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767940 (bench) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
[2026-09-27 21:19:50] [WATTCURB][MITIGATION] Mitigation Actuated: PID 767943 (sandbox-probe) | Action: ACTIVE_WINDOW_C0_GUARANTEE | Details: Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp
ASS] test_performance_swap_expansion_policy (REF-TEST-071: growth thresholds, disk floor and file ceiling, release safety, Performance brake-only-when-exhausted, sandbox containment verified)

--- [REF-TEST-084] Safe 3-Tier Memory Reclamation (GC -> RAM Compression -> Disk Swap) ---
 [PASS] test_safe_three_tier_memory_reclamation (REF-TEST-084: Phase 1 GC levels, Phase 2 RAM compression immunity, Phase 3 ZRAM priority hierarchy verified)

--- [REF-TEST-085] Hardware Bus & Display Deep Power Minimization Gate ---
 [INFO] Hardware Baseline Captured:
   * eDP ABM path        : /sys/class/drm/card1-eDP-1/amdgpu/panel_power_savings
   * Baseline ABM level  : 0
   * Baseline ASPM policy: default
   * Baseline HDA ps (s) : 10
   * Baseline VM stat (s): 1
   * Ethernet PCI path   : /sys/bus/pci/devices/0000:02:00.0
 [PASS] test_hardware_bus_and_display_power_minimization (REF-TEST-085: ABM, PCIe ASPM, HDA power-save, Ethernet sleep, and VM stat interval verified)
--- [REF-TEST-041] Multilingual L10n & Auto System Locale Verification ---
 [ORACLE GATE] O(1) L10n Translation Latency (100000 iters):
   * Average Latency : 10.5 ns/op
 [PASS] test_multilingual_l10n_and_auto_system_locale (REF-TEST-041: 14 languages, 47 strings, POSIX auto-detect verified)
 [ORACLE GATE] Headroom Mask Computation Benchmark (50000 iters):
   * Average Latency : 0.0803 us/op
   * Average Cycles  : 136.3 cycles/op
 [PASS] test_anti_starvation_and_greedy_capping (REF-TEST-019: Cores 0..13 allowed, 2 reserved for audio/compositor, 0.1 us/op)
--- [REF-TEST-048] Adaptive C1/C2 Dual-Cluster Spatial Load Dispersion & Terminal Shield Verification ---
 [ORACLE GATE] Adaptive C1/C2 Cluster Benchmark (50000 iters):
   * Average Latency : 0.0991 us/op
   * Average Cycles  : 168.1 cycles/op
 [PASS] test_adaptive_c1_c2_cluster_dispersion (REF-TEST-048: C1=0..7, C2=8..15, Terminal Shield Active, 0.1 us/op)
--- [REF-TEST-049] KDE Active Window Resource Guarantee & PM QoS C0 Pinning Verification ---
 [ORACLE GATE] Active Window Guarantee Benchmark:
   * Total Iterations: 2000
   * Total Time      : 52631.7 us
   * Average Latency : 26.3159 us/op
 [PASS] test_active_window_resource_guarantee_and_c0_qos (REF-TEST-049: C0 Pinning, C1 Spatial Affinity, 26.3159 us/op)
--- [REF-TEST-063] Desktop Session Token Shell-Safety Verification ---
 [PASS] test_desktop_session_token_shell_safety (REF-TEST-063: 16 hostile socket names + 11 hostile runtime dirs rejected)
--- [REF-TEST-064] Actuation Sandbox vs Window Governor Live-Write Verification ---
 [PASS] test_actuation_sandbox_blocks_window_governor (REF-TEST-064: mock PM QoS untouched under sandbox, 4-byte pin when off)
--- [REF-TEST-083] Low-Overhead Memory Metrics SHM Persistence Verification ---
 [PASS] test_process_and_system_memory_shm_persistence (REF-TEST-083: 32B POD/HistoryPoint memory fields, zero-allocation propagation verified)
--- [REF-TEST-020] Dual-Domain State Journaling & Faithful Restoration Verification ---
 [INFO] Hardware Baseline Captured:
   * Platform Profile  : balanced
   * CPU Governor      : schedutil
   * CPU Boost         : 1
   * PCIe ASPM Policy  : default
   * Scaling Max Freq  : 1700000 kHz
   * Panel Power Level : 0
 [PASS] test_state_journaling_and_faithful_restoration (REF-TEST-020: Dual-domain snapshot & 100% faithful restoration verified)
 [ORACLE GATE] EventLogger Stack Formatting (50k iters): 1.3700 us/op
 [ORACLE GATE] HistoryRingBuffer Append Latency (100k iters): 15.7885 ns/op
 [PASS] test_zero_disk_wakeup_logging_and_history_ring_buffer (REF-TEST-024 & REF-TEST-035: 7-Day 60,480-sample wrap, < 50ns append verified)
 [ORACLE GATE] Power Share Decomposition Math (100k iters): 0.0009 ns/op
 [PASS] test_circular_power_share_visualization (REF-TEST-025: Sum-invariant 100%, zero-division safety, < 100ns math verified)
 [ORACLE GATE] UltraEndurance Profile Actuation & 100% Roundtrip: 10.1395 ms
 [PASS] test_ultra_endurance_extensions (REF-TEST-028: SMT, Bluetooth, Backlight Cap, DRRS, KWin Effects & Baloo verified)
 [ORACLE GATE] Platform Loss Decomposition (12.5W System -> 3.8W Plat): DRAM=1[2026-09-27 21:19:50] [WATTCURB][ALERT:WARN] CPU frequency starved under load: max core 0 MHz vs ceiling 1700 MHz, load1=34.64, profile=Performance; ceiling/profile were already at target, SMU re-apply refused (baseline not captured); SMU baseline STAPM 0 mW (REF-REQ-112.10, REF-REQ-115.2)
.3641W, VRM=1.6864W, Wi-Fi=0.5247W, MB/IO=0.2249W (Invariant Sum=3.8000W)
 [PASS] test_wifi_txpower_and_platform_loss_decomposition (REF-TEST-029 verified)
--- [REF-TEST-057] Deterministic Battery Threshold Demotion (REF-REQ-094) ---
 [PASS] test_battery_low_performance_lockout (REF-TEST-057: 30/20/5% one-shot demotion, AC & above-30% stability verified)
 [ORACLE GATE] Tier 2 Ultra-Lightweight Probe Latency: 3229.5215 us/op
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
   - CPU Temp: 62.000000 C
   - CPU Cores Online: 16, Avg Freq: 2170 MHz
   - C-State POLL=251714795us, C1=11421487314us, C2=443091632627us, C3=1683223195885us
   - GPU Power: 11.000000 W
   - GPU Busy: 19%
   - Fan RPM: 3831
   - Battery Discharging: false, AC Online: true
   - Battery Health: 94.191696%
   - NVMe Status: active, Read sectors: 3012972091
 [PASS] test_persistent_hw_probe
 [PASS] test_peripheral_battery_fd_lifecycle (REF-TEST-067: peripheral fd reopened, 42% Charging)
 [PASS] test_pmu_perf_event_telemetry (Instructions counted: 0)
 [PASS] test_pmu_energy_proxy_metrics (EPI: 22.0600M, EWR: 9.3382%, P_est: 500.3304 mW)
 [PASS] test_pcie_binary_config_decoder (Gen4 x16 binary decode verified)
 [PASS] test_scoped_profiler (Zero-overhead release purity verified)
 [ORACLE GATE] Running micro-benchmark on zero-allocation parser...
 [ORACLE GATE] 100k stat parses completed in 32219 us (0.3222 us/op)
 [ORACLE GATE PASS] Performance within extreme efficiency threshold (< 1.0 us/op)
=== ALL TESTS & ORACLE GATE PASSED SUCCESSFULLY ===

 Performance counter stats for '/home/jedclub/Develop/WattCurb/build_pgo/wattcurb_tests':

          4,840.26 msec task-clock:u                                                          
     7,119,302,868      cycles:u                                                                (85.76%)
     9,865,708,880      instructions:u                                                          (85.86%)
        44,755,922      cache-misses:u                                                          (85.60%)
        89,175,666      L1-dcache-load-misses:u                                                 (85.70%)
         1,699,229      dTLB-load-misses:u                                                      (85.63%)
     2,335,048,555      branches:u                                                              (85.85%)
        19,968,837      branch-misses:u                                                         (85.74%)

       6.285838597 seconds time elapsed

       3.563334000 seconds user
       1.003097000 seconds sys
```

---

## 3. PMU Hardware Counter Telemetry (6-Second Daemon Run)

```text
[2m[*] WattCurb deep observation window: [>                       ] 0.0s / 6.0s (Interval 1/3)...[0m[2m[*] WattCurb deep observation window: [========>               ] 2.0s / 6.0s (Interval 2/3)...[0m[2m[*] WattCurb deep observation window: [================>       ] 4.0s / 6.0s (Interval 3/3)...[0m[K
[1m[36m================================================================================================================
                             WattCurb High-Fidelity Executive & Physical Power Briefing                         
================================================================================================================
[0m[2m Observation Scope    : [0m[1m6178 ms[0m[2m (3 continuous intervals)[0m[2m | Energy Consumption: [0m[1m116.65 Joules[0m[2m | Monitored Processes: [0m[1m180[0m[2m | Wakeups: [0m[1m72631 /sec[0m

[1m [1] System Battery & Power Supply Deep Telemetry[0m
  - Total System Drain      : [1m[33m18.88 Watts[0m[1m[32m [AC Hardware Pass-Through Active: Zero Battery Wear][0m
  - Power Supply State      : [32mAC Connected (Line Power / Charging)[0m[2m [USB-PD Input: 15.0W (C [PD] PD_PPS)][0m
  - Battery Capacity        : [1m80%[0m [Normal] | Health: 94.2% (112 cycles)
  - Battery Hardware ID     : [1mSMP LNV-5B10W13895[0m (S/N: 3502) [Li-poly]
  - Voltage & Current Flow  : 11.873 V (Design Nominal: 11.10 V) | Flow: [0m0.000 A[0m
  - Energy & Degradation    : 33.91 Wh now / 42.65 Wh full (Design: 45.28 Wh) | [1m[32m5.8% wear (2.63 Wh lost)[0m

[1m [2] Physical Hardware Domain Power & State Breakdown[0m
  * CPU Package (RAPL)    :   9.62 W ( 50.9%) [2mTemp: 62°C, 1963 MHz avg (schedutil)[0m
    └─ [2mC-State Sleep Residency : [0mC0 (Active): [1m98.7%[0m, C1: 0.8%, C2: 0.5%, C3 (Deep Sleep): [1m[33m0.0%[0m
  * GPU Silicon (DRM)     :   2.01 W ( 10.6%) [2mBusy: 16%, 341 MB VRAM, 8.0 GT/s PCIe x16[0m
  * Display Backlight     :   1.90 W ( 10.1%) [2mBrightness: 38%[0m
  * Storage / NVMe APST   :   2.81 W ( 14.9%) [2mNVMe: active (Read 151.1 MB/s, Write 53.7 MB/s)[0m
  * Mechanical Fan        :   1.34 W (  7.1%) [2m3831 RPM[0m
  * Uncore & Platform Loss:   1.20 W (  6.4%) [2mASPM: [default] performance powersave[0m

[1m [3] Top Battery Drain Culprits & Physical Causation Breakdown[0m
  1. [1mclang++[0m (PID: 766403, [36mTier 5 (Runaway)[0m, 1 thr, Nice: -3)
     Drain: [1m[31m2.21 W[0m (11.7% of system, WDI: 400.5) | Primary Domain: [35mNVMe Storage / MajFlt[0m
     Causation Mechanism: [1mMajor Page Faults (4282 flt/s, NVMe Active)[0m
     Physical Metrics   : [2mWakeups: 7816/s, RAM PSS: 554MB, Faults: 49 min/s 4282 maj/s, Sockets: 0[0m
  2. [1mclang++[0m (PID: 766190, [36mTier 5 (Runaway)[0m, 1 thr, Nice: -3)
     Drain: [1m[31m2.18 W[0m (11.6% of system, WDI: 332.3) | Primary Domain: [35mNVMe Storage / MajFlt[0m
     Causation Mechanism: [1mMajor Page Faults (3413 flt/s, NVMe Active)[0m
     Physical Metrics   : [2mWakeups: 6457/s, RAM PSS: 386MB, Faults: 264 min/s 3413 maj/s, Sockets: 0[0m
  3. [1mclang++[0m (PID: 766217, [36mTier 5 (Runaway)[0m, 1 thr, Nice: -3)
     Drain: [1m[31m2.19 W[0m (11.6% of system, WDI: 326.5) | Primary Domain: [35mNVMe Storage / MajFlt[0m
     Causation Mechanism: [1mMajor Page Faults (3313 flt/s, NVMe Active)[0m
     Physical Metrics   : [2mWakeups: 6339/s, RAM PSS: 517MB, Faults: 260 min/s 3313 maj/s, Sockets: 0[0m
  4. [1mclang++[0m (PID: 766121, [36mTier 5 (Runaway)[0m, 1 thr, Nice: -3)
     Drain: [1m[31m2.17 W[0m (11.5% of system, WDI: 320.1) | Primary Domain: [35mNVMe Storage / MajFlt[0m
     Causation Mechanism: [1mMajor Page Faults (3282 flt/s, NVMe Active)[0m
     Physical Metrics   : [2mWakeups: 6213/s, RAM PSS: 409MB, Faults: 264 min/s 3282 maj/s, Sockets: 0[0m
  5. [1mclang++[0m (PID: 766123, [36mTier 5 (Runaway)[0m, 1 thr, Nice: -3)
     Drain: [1m[31m2.15 W[0m (11.4% of system, WDI: 289.6) | Primary Domain: [35mNVMe Storage / MajFlt[0m
     Causation Mechanism: [1mMajor Page Faults (2933 flt/s, NVMe Active)[0m
     Physical Metrics   : [2mWakeups: 5607/s, RAM PSS: 344MB, Faults: 209 min/s 2933 maj/s, Sockets: 0[0m
  6. [1mclang++[0m (PID: 766119, [36mTier 5 (Runaway)[0m, 1 thr, Nice: -3)
     Drain: [1m[31m2.16 W[0m (11.4% of system, WDI: 283.4) | Primary Domain: [35mNVMe Storage / MajFlt[0m
     Causation Mechanism: [1mMajor Page Faults (2831 flt/s, NVMe Active)[0m
     Physical Metrics   : [2mWakeups: 5481/s, RAM PSS: 411MB, Faults: 304 min/s 2831 maj/s, Sockets: 0[0m
  7. [1mclang++[0m (PID: 766111, [36mTier 5 (Runaway)[0m, 1 thr, Nice: -3)
     Drain: [1m[31m2.16 W[0m (11.4% of system, WDI: 281.1) | Primary Domain: [35mNVMe Storage / MajFlt[0m
     Causation Mechanism: [1mMajor Page Faults (2779 flt/s, NVMe Active)[0m
     Physical Metrics   : [2mWakeups: 5436/s, RAM PSS: 380MB, Faults: 312 min/s 2779 maj/s, Sockets: 0[0m
  8. [1mclang++[0m (PID: 766128, [36mTier 5 (Runaway)[0m, 1 thr, Nice: -3)
     Drain: [1m[31m2.18 W[0m (11.6% of system, WDI: 272.7) | Primary Domain: [35mNVMe Storage / MajFlt[0m
     Causation Mechanism: [1mMajor Page Faults (2615 flt/s, NVMe Active)[0m
     Physical Metrics   : [2mWakeups: 5263/s, RAM PSS: 385MB, Faults: 598 min/s 2615 maj/s, Sockets: 0[0m

[1m [4] Modular Battery Optimization Features Execution Status[0m
  - Overall Status      : [1m[2mOptimal baseline (No intrusive throttling needed)[0m
    * All optimization features running in passive baseline observation mode.

[1m [5] Actionable Engineering Recommendations[0m
  * [C-State Breaker] Wakeup frequency (72631 wakeups/sec) is impeding CPU C3 deep sleep. Enabling TimerSlackCoalescing will bundle timers.
================================================================================================================


 Performance counter stats for '/home/jedclub/Develop/WattCurb/output/wattcurb --duration 6 -i 2':

            122.22 msec task-clock:u                                                          
        16,885,610      cycles:u                                                              
         8,510,026      instructions:u                                                        
           205,876      L1-dcache-load-misses:u                                               
             8,227      dTLB-load-misses:u                                                    
           108,689      branch-misses:u                                                       
               317      page-faults:u                                                         

       6.223768133 seconds time elapsed

       0.009580000 seconds user
       0.108501000 seconds sys
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
