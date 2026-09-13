# [REF-REQ-031] Adaptive Power State Machine & Dynamic Kernel Actuation Specification

## 1. Executive Summary & Problem Definition

In prior iterations, WattCurb possessed granular hardware telemetry and process attribution mechanisms, along with initial mitigation prototypes. However, actuations lacked **state-machine orchestration, bidirectional rollback (un-mitigation/thawing), hardware domain scaling (ASPM/EPP/Display/GPU), and user-controllable power profile modes**.

**Specification Mandate**:
WattCurb must implement a **Closed-Loop Adaptive Mitigation Engine** governed by a deterministic 3-tier Power Profile State Machine (`Balanced`, `PowerSaver`, `UltraEndurance`). The engine must perform both process-level mitigations (`SCHED_IDLE`, timer slack, memory reclaim, cgroup v2 freeze) and hardware-level actuations (PCIe ASPM, CPU EPP, GPU DPM, Panel cap), while guaranteeing **complete bidirectional rollback** when returning to AC power or higher battery states.

---

## 2. Power Profile State Machine Specifications

### 2.1 Three-Tier Operational Profiles
```
                    Battery <= 50%               Battery < 20%
   [ Balanced ]  ─────────────────>  [ PowerSaver ]  ──────────────>  [ UltraEndurance ]
   (AC / > 50%)  <─────────────────  (20% ~ 50%)    <──────────────  (Critical < 20%)
                    Battery > 55%                Battery >= 25%
                    or AC Connected              or AC Connected
```

- **[REQ-31.1] Tier 0: Balanced Mode (Default / AC / Battery > 50%)**:
  - Focus: Maintain 100% desktop interactive responsiveness.
  - Mitigations: Monitor all processes; actuate only severe runaway candidates (Tier 5 with WDI > 15.0).
  - Hardware: Default CPU governor/EPP (`balance_performance`), PCIe ASPM `default`, GPU DPM `auto`.
- **[REQ-31.2] Tier 1: PowerSaver Mode (Battery 20% ~ 50%)**:
  - Focus: Maximize battery runtime without perceivable desktop UI degradation.
  - Mitigations: Background workers (Tier 4) throttled to `SCHED_IDLE` + idle I/O priority; uncoordinated timers relaxed to 100ms; inactive background memory reclaimed (> 100MB).
  - Hardware: PCIe ASPM forced to `powersave`; CPU EPP set to `balance_power`.
- **[REQ-31.3] Tier 2: UltraEndurance Mode (Battery < 20%)**:
  - Focus: Emergency battery preservation to prevent unexpected host shutdown.
  - Mitigations: Heavy background workers (Tier 4 with WDI > 6.0) frozen via cgroup v2 freeze; display brightness soft-capped to 50%; aggressive memory reclaim.
  - Hardware: CPU EPP set to `power`; GPU DPM set to `low`.

### 2.2 Hysteresis & Anti-Flapping Guards
- **[REQ-31.4] Hysteresis Thresholds**:
  - PowerSaver $\rightarrow$ UltraEndurance trigger: `< 20.0%`.
  - UltraEndurance $\rightarrow$ PowerSaver recovery: `>= 25.0%`.
  - Balanced $\rightarrow$ PowerSaver trigger: `<= 50.0%`.
  - PowerSaver $\rightarrow$ Balanced recovery: `> 55.0%` or `is_ac_online == true`.
  - Prevents rapid oscillation/flapping at battery percentage boundaries.

---

## 3. Bidirectional Actuation & Rollback Guarantees

### 3.1 Immunity Invariants
- **[REQ-31.5] Absolute Immunity**:
  - Tier 0 (`CriticalImmune`: `systemd`, `dbus`, `pipewire`, `wireplumber`, kernel threads) and Tier 1 (`DesktopCore`: `kwin_wayland`, `mutter`, `Xorg`, display server) are **strictly immune** from all throttling, freezing, and priority modifications under all profiles.

### 3.2 Dynamic Rollback (Un-mitigation / Thawing)
- **[REQ-31.6] Reverse Actuation on State Recovery**:
  - When transitioning from `UltraEndurance` to `PowerSaver` or `Balanced`, all frozen processes must be unfrozen (`cgroup.freeze = 0`).
  - When transitioning to `Balanced` (or AC reconnected), all `SCHED_IDLE` throttled processes must be restored to `SCHED_OTHER` with standard CFS dynamic weighting.
  - Timer slack must be restored to kernel default (50,000 ns).

### 3.3 Hardware Domain Actuations
- **[REQ-31.7] PCIe ASPM Enforcement**:
  - Write `powersave` to `/sys/module/pcie_aspm/parameters/policy` under battery; restore previous policy under AC.
- **[REQ-31.8] Display Panel Soft-Cap**:
  - When battery `< 20%` and brightness exceeds 50%, gently cap brightness via `/sys/class/backlight/*/brightness`.

---

## 4. Verification & Oracle Gate Standards (`REF-TEST-014`)

1. **State Machine Transition Test**:
   - Verify transitions: Balanced $\rightarrow$ PowerSaver $\rightarrow$ UltraEndurance $\rightarrow$ Balanced on AC.
   - Validate hysteresis boundary dampening.
2. **Rollback Integrity Test**:
   - Verify that all frozen PIDs are successfully thawed upon AC connection.
   - Verify that throttled PIDs return to `SCHED_OTHER`.
3. **Zero-Allocation Invariant**:
   - Entire evaluation, state machine dispatch, and rollback loop must execute with **zero dynamic heap allocations**.
