# [REF-REQ-034] Rapid Charge & Profile Restoration Engine Specification

## 1. Context & Motivation
- **Ref-ID**: `REF-REQ-034`
- **Related Requirements**: [`REF-REQ-028`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-028-adaptive-power-state-machine-and-actuation.md), [`REF-REQ-032`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-032-kde-plasma-desktop-mitigation.md), [`REF-REQ-033`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-033-window-aware-dynamic-suppression-ladder.md)
- **Related Research**: [`REF-RES-016`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-016-rapid-dynamic-rollback-and-charge-coordination.md)
- **Target Subsystem**: `policy::UnifiedRollbackCoordinator`

Whenever power saving mitigations are active, the system's hardware and background applications operate under constrained parameters. However, when the user connects AC power, charges the battery, or manually elevates performance, the host must **instantaneously and completely return to its unconstrained baseline**.

This specification defines the functional and non-functional requirements for the **Unified Rapid Rollback Engine**.

---

## 2. Functional Requirements

### REQ-034.1: Multi-Domain Full-Sweep Rollback
Upon receiving a rollback trigger, the daemon shall execute a unified, 4-domain rollback sweep:
1. **Process Domain**:
   - Every PID demoted to `SCHED_IDLE` by either `MitigationEngine` or `WindowAwareGovernor` shall be immediately restored to `SCHED_OTHER` (nice 0).
   - Every PID whose `timerslack_ns` was extended shall have its timer slack restored to `50,000` ns ($50\mu\text{s}$).
   - Every PID whose block I/O priority was modified shall have it restored to Best-Effort (IOPRIO_CLASS_BE, priority 4).
2. **KDE Desktop Domain**:
   - Re-load all dynamically unloaded KWin shaders (e.g. `blur`, `backgroundcontrast`) via `loadEffect`.
   - Restore native high refresh rate (e.g. 120Hz/144Hz/165Hz) via `org.kde.KScreen`.
   - Resume `baloo_file` indexer execution via `resumeIndexer`.
   - Restore default UI animation duration factor.
3. **Silicon & Frequency Domain**:
   - Restore CPU Energy Performance Preference (`energy_performance_preference`) to `balance_performance` (or `performance` if requested).
   - Re-enable CPU Turbo Boost (`/sys/devices/system/cpu/cpufreq/boost = 1`).
   - Remove any temporary RAPL package power caps.
4. **Bus & Peripheral Domain**:
   - Restore PCIe ASPM policy to `default` or `performance`.
   - Restore display backlight brightness to the exact pre-mitigation level.
   - Restore WiFi power save mode to standard AC performance.

### REQ-034.2: Trigger Invariants
The unified rollback sweep shall execute upon any of the following events:
1. **AC Power Connection**: Transition from battery discharging to AC online (`on_battery == false`).
2. **Profile Elevation**: Manual user command or IPC request transitioning profile to `Balanced` or `Performance`.
3. **Battery Hysteresis Recovery**: Battery level rising above upper threshold ($>55\%$ from PowerSaver, $>25\%$ from UltraEndurance).
4. **Daemon Termination**: Reception of `SIGTERM` or `SIGINT` (Zero-Residual Exit).

---

## 3. Non-Functional Performance & Safety Constraints

1. **Sub-5ms Execution Latency**:
   - The entire 4-domain rollback sequence must complete in $\le 5.0\text{ms}$ from trigger detection.
2. **Zero-Residual Guarantee**:
   - No tracked process or sysfs node may remain in a throttled or altered state once the sweep completes.
3. **Idempotency**:
   - Consecutive or redundant rollback triggers shall execute as a no-op without incurring redundant syscalls or D-Bus message spam.
