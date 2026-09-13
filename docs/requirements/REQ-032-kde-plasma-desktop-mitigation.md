# [REF-REQ-032] Safe KDE Plasma Desktop Optimization & KWin Mitigation Specification

## 1. Context & Motivation
- **Ref-ID**: `REF-REQ-032`
- **Related Requirements**: [`REF-REQ-016`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-016-two-part-telemetry-and-adaptive-mitigation.md), [`REF-REQ-031`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-028-adaptive-power-state-machine-and-actuation.md)
- **Related Research**: [`REF-RES-009`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-009-linux-desktop-sleep-and-resource-reclaim.md), [`REF-RES-014`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-014-kde-plasma-safe-desktop-optimization.md)
- **Target Subsystem**: `policy::KdeDesktopGovernor`, `actuator::KWinActuator`

WattCurb enforces a strict immunity rule protecting display compositors (`kwin_wayland`, `mutter`) from destructive process throttling or freezing. However, the desktop compositor and associated KDE subsystems account for substantial display, GPU shader, and storage power consumption. 

This specification establishes requirements for **safe, protocol-level KDE Plasma 6 desktop optimizations** that preserve 100% desktop stability, guarantee instant UI recovery, and deliver empirical power reductions on battery power.

---

## 2. Functional Requirements

### REQ-029.1: Compositor Effect Mitigation (KWin D-Bus Interface)
- Under the `BatterySaver` or `UltraSaver` profiles:
  - The daemon shall dynamically issue an `unloadEffect("blur")` call to `org.kde.KWin /Effects`.
  - The daemon shall verify whether `backgroundcontrast` is active, and if so, unload it.
- Under the `Balanced` profile or upon AC power restoration:
  - The daemon shall restore all previously loaded KWin effects to their exact pre-mitigation state using `loadEffect(...)`.
- **Latency Requirement**: The D-Bus method call must be asynchronous or non-blocking, completing within $\le 5\text{ms}$.

### REQ-029.2: Window-Aware Client Suppression
- The daemon shall integrate with KWin's window state tracking (via KWin Scripting D-Bus signal or `/KWin supportInformation` polling on profile change):
  - When a non-critical application (Tier 4 or Tier 5 GUI client, e.g. web browser, Electron chat client) has been minimized for longer than 30 seconds on battery:
    - The daemon shall execute `cgroup.freeze` on that client's cgroup.
    - The daemon shall trigger `memory.reclaim` (up to 64MB) on that client's cgroup.
  - Upon receiving an unminimize or window activation event:
    - The daemon shall immediately thaw the cgroup (`cgroup.freeze = 0`) within $\le 1\text{ms}$.

### REQ-029.3: Baloo Indexer Power Gate
- On battery transition:
  - If `baloo_file` is actively running, the daemon shall request indexer suspension via D-Bus (`org.kde.baloo /indexer suspendIndexer`) or `balooctl6 suspend`.
- On AC transition:
  - The daemon shall resume the indexer via `resumeIndexer()`.

### REQ-029.4: Dynamic Refresh Rate Scaling (DRRS)
- On supported eDP panels:
  - When switching to battery power, request a downscale to 60Hz via `org.kde.KScreen` if the current refresh rate exceeds 60Hz.
  - On AC connection, restore the panel's maximum native refresh rate (e.g. 120Hz/144Hz/165Hz).

### REQ-029.5: Animation Duration Scaling
- In `UltraSaver` mode:
  - Set `AnimationDurationFactor` to `0.0` or `0.2` in KDE configuration to suppress transient GPU animation frame bursts during window management.

---

## 3. Non-Functional & Safety Requirements

1. **Strict Compositor Immunity (Zero-Crash Guarantee)**:
   - Under no circumstances shall `kwin_wayland` or `plasmashell` be sent `SIGSTOP`, `cgroup.freeze`, or downgraded to `SCHED_IDLE`. All optimizations must use KDE's published D-Bus / config interfaces.
2. **Zero-Allocation Execution**:
   - The desktop governor's internal state tracking and rollback storage must use statically allocated structures with fixed capacities.
3. **Idempotency & Bi-Directional Rollback**:
   - Every modified setting must be recorded in an in-memory journal and restored completely upon AC reconnect, profile deactivation, or daemon termination (`SIGTERM`).
