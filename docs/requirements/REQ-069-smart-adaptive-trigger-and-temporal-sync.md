# [REF-REQ-069] Smart Adaptive Trigger, Early Telemetry Sweep & Baseline Synchronization Specification

## 1. Executive Summary & Problem Definition

Under [`REF-REQ-068`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-068-adaptive-three-tier-telemetry-cadence.md), WattCurb introduced an Adaptive 3-Tier Cadence (Interactive 2s, Background 10s light probe, 60s deep sweep). However, empirical analysis ([`REF-RES-018`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-018-empirical-log-analysis-and-mitigation-patterns.md)) revealed two critical structural limitations in the background monitoring loop:

1. **Attribution Timebase Desynchronization (6x CPU Overestimation)**:
   - In [`daemon_runner.cpp`](file:///home/jedclub/Develop/WattCurb/src/core/daemon_runner.cpp), `hw_prev_` was mutated on every 10-second light probe, whereas `proc_pool_` was only swapped on 60-second deep observation sweeps.
   - When the 60-second deep sweep executed, `compute_attribution` calculated `delta_sec ≈ 10.0` from `hw_prev_`, while `proc1` vs `proc2` contained **60 seconds** of accumulated CPU ticks.
   - Dividing 60 seconds of process execution ticks by a 10-second denominator artificially multiplied calculated process CPU usage by **600%**, causing interactive tools (`foot`, `fcitx5`, `opencode`) to falsely appear as runaway tasks and trigger 3,336 unwanted mitigation events.

2. **Detection Latency on Power Spikes (Up to 60s Delay)**:
   - During steady-state background monitoring, if an unconstrained process begins draining battery power, the 10-second light probe detects elevated hardware wattage but ignores process enumeration (`0 proc traversal`).
   - The runaway workload continues unthrottled for up to 60 seconds before attribution and mitigation are evaluated.

This specification formalizes **Option A: Smart Adaptive Trigger** with **Decoupled Baseline Synchronization**.

---

## 2. Functional Requirements

### 2.1 Decoupled Dual-Domain Baselines (`REF-REQ-069-1`)
- The daemon must maintain two independent hardware baseline samples:
  1. `hw_light_prev_`: Updated exclusively during 10-second light probe cycles for sub-50µs hardware-only power estimation.
  2. `hw_deep_prev_`: Maintained in lockstep with `proc_pool_.current()`. Updated strictly after a full deep observation sweep has completed.
- The timebase `delta_sec` in `compute_attribution` must precisely match the accumulation window of the process snapshot pair (`proc_deep_prev` to `proc_cur`), completely eliminating the 6x tick distortion.

### 2.2 Smart Adaptive Power-Spike Detection (`REF-REQ-069-2`)
- During the 10-second light probe, after computing hardware power:
  - The probe must evaluate whether system silicon power exceeds dynamic baseline thresholds:
    - **Condition A (CPU Package Spike)**: `cpu_package_watts >= 12.0 W` (or $\ge 10.0$ W while discharging on battery).
    - **Condition B (System Power Spike)**: `total_system_watts >= 20.0 W` while discharging on battery.
    - **Condition C (Rapid Transient Jump)**: `total_system_watts - last_system_watts >= 8.0 W`.
- When any condition evaluates to `true`, the probe triggers an **Early Deep Telemetry Sweep**.

### 2.3 Early Deep Telemetry Sweep & Window Realignment (`REF-REQ-069-3`)
- Upon detecting a power spike:
  - The daemon immediately executes `process_deep_observation_cycle()` without waiting for the remaining 60-second cycle.
  - The background tick counter `bg_tick_count_` resets to `0`, realigning subsequent 60-second sweeps relative to the early sweep.
  - An event alert is logged to system journal: `"ADAPTIVE: Power spike detected (X W) -> Triggered early deep sweep"`.

### 2.4 Hysteresis Cooldown & Anti-Flapping Guard (`REF-REQ-069-4`)
- Early sweeps must be throttled by an anti-storm cooldown window: at most one early deep sweep may be triggered every 15 seconds, preventing cascaded full-process traversals if power remains elevated.

---

## 3. Verification & Oracle Gate Standards (`REF-TEST-034`)

1. **Temporal Sync Invariant**:
   - For any deep observation cycle, $|(t_{hw2} - t_{hw1}) - (t_{proc2} - t_{proc1})| < 0.15\text{s}$.
   - Process CPU usage calculation must not exceed total physical core capacity ($N_{\text{cores}} \times 100\%$).
2. **Spike Trigger Latency Assertion**:
   - When synthetic power step load (e.g., 25W) is injected during background mode, full attribution and mitigation evaluation must trigger within $\le 10.0\text{s}$ (down from 60.0s).
3. **Overhead & Memory Bounds**:
   - Quiescent background execution remains $\le 0.05\%$ CPU with zero heap allocation.
