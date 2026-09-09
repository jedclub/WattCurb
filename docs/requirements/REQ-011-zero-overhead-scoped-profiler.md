# [REF-REQ-014] Zero-Overhead Scoped Subsystem Profiler & PMU Cost Probe

- **Ref-ID**: `REF-REQ-014`
- **Module**: `src/core/scoped_profiler.hpp`, `src/hw/hardware_probe.cpp`, `src/proc/process_analyzer.cpp`, `src/policy/attribution_engine.cpp`
- **Related Requirements**: [`REF-REQ-006`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-003-pgo-pmu-optimization.md), [`REF-REQ-008`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-005-binary-hardening-symbol-stripping.md), [`REF-REQ-013`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-010-deep-process-power-tracking.md)
- **Status**: Draft / Implementation Ready

---

## 1. Executive Summary & Rationale

WattCurb strives for ultra-low power consumption and sub-0.1% host CPU overhead. To maintain this extreme standard as new telemetry domains (e.g. Zen 2 CCX tracking, atomic statm PSS, socket scanning) are added, engineers need **fine-grained microsecond and CPU-cycle-level cost breakdowns** for every measurement phase.

However, logging and instrumentation code must **never compromise production release builds** (as mandated by [`REF-REQ-008`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-005-binary-hardening-symbol-stripping.md) and `AGENTS.md` Section 11).

This specification defines:
1. A **Zero-Overhead Scoped Profiler** mechanism that automatically measures elapsed time ($\\text{ns}$) and hardware timestamp counters ($\\Delta \\text{TSC}$) across every individual metric collection and policy attribution scope.
2. A **Complete Compile-Time Stripping Guard**: When built in release mode (`-DNDEBUG` without development profiling flags), the instrumentation expands to `((void)0)` with zero runtime instructions, zero branches, zero strings, and zero memory footprint in the final binary.
3. A **PMU Hardware Subsystem Cost Probe**: Profiling exactly which kernel syscalls and userland routines constitute the primary active time during profiling passes.

---

## 2. Functional Requirements

### 2.1 Fine-Grained Instrumentation Scopes
The profiler must instrument the following key phases:
1. **Hardware Telemetry Probing (`HardwareProbe`)**:
   - `hw.battery`: Fuel gauge, charge rate, and USB-PD sysfs probing.
   - `hw.cpu_metrics`: Temperature, 16-core frequencies, and C-state residencies.
   - `hw.gpu_metrics`: AMDGPU power, busy load, core clock, VRAM usage, and temperatures.
   - `hw.nvme_metrics`: NVMe APST autonomous power state and block I/O sectors.
   - `hw.fan_metrics`: ThinkPad EC cooling fan tachometer RPM.
   - `hw.wifi_metrics`: WiFi thermal state, link rate, and power save flags.
   - `hw.backlight`: Display backlight brightness reading.
2. **Process Subsystem Inspection (`ProcessAnalyzer`)**:
   - `proc.readdir`: Reading `/proc` directory entries and integer PID filtering.
   - `proc.stat_read`: Reading `/proc/[pid]/stat` file into 64-byte stack buffer.
   - `proc.stat_parse`: SIMD & unrolled scalar parsing of 39 tokens.
   - `proc.statm`: Atomic `/proc/[pid]/statm` read and PSS/RSS page conversion.
   - `proc.timerslack`: Reading `/proc/[pid]/timerslack_ns`.
   - `proc.status`: Active process context switch parsing.
   - `proc.io`: Active process read/write bytes parsing.
   - `proc.fd_scan`: Reading `/proc/[pid]/fd` symlinks (DRM fdinfo & network socket counting).
   - `proc.lazy_skip`: Fast-path bypass for sleeping/idle processes ($\\Delta \\text{ticks} == 0$).
3. **Power Attribution & Mitigation Policy (`AttributionEngine`)**:
   - `policy.delta_accum`: Time-series hardware & process delta aggregation.
   - `policy.gpu_attr`: GPU engine and VRAM proportional power distribution.
   - `policy.waketax_attr`: C-state wakeups, timer slack penalty, and WakeTax calculation.
   - `policy.fan_attr`: Proportional thermal dissipation fan power attribution.
   - `policy.nvme_attr`: Major page faults and I/O rate APST disruption attribution.
   - `policy.wifi_attr`: Network socket holders RF CAM mode power attribution.
   - `policy.ccx_attr`: AMD Zen 2 CCX boundary migration penalty calculation.
   - `policy.wdi_ranking`: Watt Drain Index computation and top-process sorting.

---

## 3. Technical Constraints & Invariants

| Item | Development / Debug Mode | Production Release Mode (`-DNDEBUG`) |
| :--- | :--- | :--- |
| **Macro Expansion** | `WATTCURB_PROFILE_SCOPE(name)` instantiates RAII timer | Expands to `((void)0)` |
| **Execution Cost** | Minimal overhead ($\\sim 15$ cycles per scope via RDTSC) | **0 cycles (Zero-Cost Abstraction)** |
| **Binary Residue** | Profiler registry and formatting helpers present | **0 bytes** (No strings, no symbols, fully stripped) |
| **Output** | Printed via `--dev-profile` or upon process termination | Suppressed completely |

---

## 4. Verification & Acceptance Criteria

1. **Oracle Gate Regression Barrier**:
   - Enabling or disabling the scoped profiler macros must not regress the `parse_proc_stat` latency threshold ($< 0.5 \\mu s/\\text{op}$).
2. **Binary Purity Audit**:
   - Running `strings output/wattcurb | grep -i "proc\\.stat"` or `nm output/wattcurb` on the release binary must yield zero matches.
3. **Empirical Subsystem PMU Breakdown**:
   - An empirical report identifying the relative cost distribution of each subsystem must be generated and documented in `docs/research/PMU_BENCHMARKS.md`.
