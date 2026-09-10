# [REF-REQ-020] Detailed Executive Briefing & Modular Battery Optimization Features

- **Ref-ID**: `REF-REQ-020`
- **Related Requirements**: [`REF-REQ-001`](file:///home/jedclub/Develop/WattCurb/docs/requirements/power_profiler.md), [`REF-REQ-010`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-007-full-hardware-domain-attribution.md), [`REF-REQ-019`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-016-two-part-telemetry-and-adaptive-mitigation.md)
- **Related Architecture**: [`REF-ARCH-008`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-008-two-part-telemetry-and-mitigation-engine.md), [`REF-ARCH-009`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-009-modular-optimization-feature-framework.md)
- **Related Research**: [`REF-RES-008`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-008-deep-process-classification-and-mitigation-db.md), [`REF-RES-009`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-009-linux-desktop-sleep-and-resource-reclaim.md)
- **Status**: Approved Specification

---

## 1. Executive Summary & Core Motivation

To maximize the practical diagnostic and operational utility of **WattCurb**, two fundamental telemetry and mitigation paradigms must be refined:

1. **High-Fidelity Extended Executive Briefing**:
   - When a user explicitly requests an executive briefing (`--briefing`, `-b`), the output must not be an oversimplified summary. It must deliver an exhaustive, highly detailed physical and causation breakdown.
   - The sampling duration must be sufficiently long (default: **10.0 seconds**, configurable) to filter transient CPU burst noise and capture true steady-state energy consumption ($J$), C-state sleep residencies, GPU power draws, NVMe APST states, and process hardware causation mechanisms.

2. **Daemon Mode: Zero-String Pure In-Memory Struct Analysis Pipeline**:
   - In daemon mode (sleeping for 55 seconds, observing for 5 seconds every minute), the monitoring loop must **NEVER** perform eager string formatting, stream allocations, or text serialization during routine background cycles.
   - Telemetry must be captured and retained purely as compact, trivially copyable C++23 structural memory fields for programmatic analysis and automated mitigation.
   - Text rendering and JSON formatting must only execute **on-demand** upon receiving an explicit client IPC query or signal.

3. **Modular Battery Optimization Feature Specification**:
   - System mitigations and power reductions must not exist as hardcoded or monolithic blocks of code.
   - Every optimization technique must be formally specified and implemented as an independent, modular **Battery Optimization Feature Unit** with its own toggle, state machine, safety constraints, telemetry, and rollback capabilities.

---

## 2. Functional Requirements

### 2.1 Detailed Executive Briefing Specification
When invoked in briefing mode, the daemon or standalone binary must present:
1. **System & Battery Telemetry**:
   - Total system drain ($W$) with precision color scaling.
   - Exact battery discharge rate, capacity %, remaining hours, full design vs remaining energy ($Wh$), cycle count, and temperature.
   - CPU package RAPL power, core average/min/max frequencies, governor, core active vs sleep C-state residency breakdown ($C0, C1, C2, C3/deep$).
   - GPU Silicon telemetry: busy %, GFX/Compute/Decoder engine activity, VRAM allocated ($MB$), PCIe link speed/width, GPU temperature and voltages.
   - Display backlight power ($W$) and brightness %.
   - Storage / NVMe power state and disk throughput ($MB/s$).
   - Thermal fan RPM and platform uncore losses.
2. **Top Battery Drain Culprits & Physical Causation**:
   - Top processes ranked by WattCurb Drain Index (WDI).
   - Display PID, Comm, Safety Tier, Priority/Nice, Thread count, Minflt/Majflt rates, Memory PSS, Open sockets.
   - Specific physical causation mechanism (e.g. "Zen CCX Migration L3 Thrash", "WiFi Radio CAM Mode Active Sockets", "Aggressive Timer Slack Breaking C3 Sleep").
3. **Feature-by-Feature Mitigation Status**:
   - Detailed breakdown per active battery optimization feature (e.g. SCHED_IDLE, TimerSlack, MemoryReclaim, CgroupFreezer).
   - Display target process PIDs, actions taken, and individual estimated power savings ($W$).
4. **Actionable Recommendations**:
   - Tailored engineering recommendations based on empirical thresholds.

### 2.2 Default Observation Window Duration
- Single-shot / Briefing window: Default **10.0 seconds** (sufficiently long to isolate steady-state power).
- Daemon mode: **60.0 seconds period** with **5.0 seconds observation window** (55s deep sleep).

---

## 3. Modular Battery Optimization Feature Catalog

Each feature is identified by a unique `FeatureId`, possesses strict safety criteria, and is toggled via daemon configuration:

| Feature ID | Feature Name | Target Domain | Kernel Mechanism | Aggressiveness Tier | Safety Criteria |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `FEAT-001` | **`SchedIdleThrottle`** | CPU / Scheduler | `sched_setscheduler(SCHED_IDLE)` + `ioprio_set(IDLE)` | Conservative+ | Never apply to Tier 0 (Critical) or Tier 1 (DesktopCore) |
| `FEAT-002` | **`TimerSlackCoalescing`** | CPU / C-States | `/proc/[pid]/timerslack_ns` $\to$ 100ms ~ 500ms | Moderate+ | Applied to background apps breaking NO_HZ sleep |
| `FEAT-003` | **`ProactiveMemoryReclaim`** | DRAM / Refresh | `/sys/fs/cgroup/<path>/memory.reclaim` | Moderate+ | Reclaims inactive anon/file pages to zram without waking target |
| `FEAT-004` | **`CgroupFreezer`** | All Subsystems | `/sys/fs/cgroup/<path>/cgroup.freeze` $\to$ 1 | Progressive (< 20% batt) | Strictly restricted to Tier 4 (BgWorker) and Tier 5 (Runaway) |
| `FEAT-005` | **`ZenCcxAffinityPinning`** | CPU L3 / Infinity Fabric | `sched_setaffinity` pin to single CCX | Progressive | Pin threads thrashing cross-CCX cache lines |
| `FEAT-006` | **`DisplayBacklightFloor`** | Display Panel | `/sys/class/backlight/` cap recommendation | Advisory | Suggests brightness capping when battery < 30% |
| `FEAT-007` | **`PcieAspmEnforcer`** | PCIe Links | `/sys/module/pcie_aspm/parameters/policy` | Conservative | Verifies ASPM `powersave` policy on battery |

---

## 4. Verification & Oracle Gate Guardrails

1. **Zero-String Daemon Loop**:
   - Compiling the daemon loop must prove zero dynamic string allocations during background 60s/5s execution.
2. **Feature Independence**:
   - Disabling any individual feature must leave all remaining features functioning deterministically.
3. **Execution Latency Budget**:
   - Feature evaluation across all 2048 potential process snapshots must complete in $< 1.0$ ms.
