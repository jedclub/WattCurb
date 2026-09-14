# [REF-REQ-044] Absolute Zero-Kill and Non-Halting Power Management Invariant

## 1. Requirement Metadata
- **Ref-ID**: `REF-REQ-044`
- **Title**: Absolute Zero-Kill & Non-Halting Power Management Invariant
- **Module**: `policy::MitigationEngine`, `policy::FeatureManager`, `policy::ProcessClassifierDB`, `tray::TrayClient`
- **Status**: Active / Strictly Enforced
- **Date**: 2026-09-15
- **Related Requirements**: [`REF-REQ-019`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-019-process-safety-tiers-and-progressive-mitigation.md), [`REF-REQ-031`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-031-closed-loop-mitigation-engine.md), [`REF-REQ-033`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-033-window-aware-dynamic-suppression-ladder.md)
- **Related Research**: [`REF-RES-015`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-015-kwin-window-aware-progressive-suppression.md)

---

## 2. Background & Problem Statement

In previous iterations, aggressive mitigation stages included:
1. Stage 4: `cgroup.freeze = 1` (Freezing process cgroups).
2. Stage 5: `Terminate` (`SIGTERM` / `SIGKILL`).
3. External script invocation: `killall -STOP` on specific daemons.

### 2.1. Critical Hazards Identified in Practice
1. **Tree-Wide Application Freezing in Systemd User Slices**:
   In modern desktop environments (e.g. KDE Plasma 6 / systemd-oomd), background processes frequently share parent slices (`app.slice` or `user@1000.service`). Writing `1` to `cgroup.freeze` on an unresolved or shared cgroup freezes **every sibling thread and user application** residing in that slice, causing user editors, browsers, terminals, and background compile jobs to freeze simultaneously.
2. **D-Bus IPC Deadlocks & Unresponsive App Timeouts**:
   When a process is frozen into uninterruptible sleep, any synchronous D-Bus or socket communication to/from the desktop compositor, audio server (`pipewire`), or notification daemon locks up. The system then declares the application unresponsive and terminates it with `SIGABRT` or `SIGKILL`.
3. **Severe Risk of User Data Loss**:
   Killing or freezing runaway candidates or background workers destroys in-flight data, active network transfers, and open editor buffers.

---

## 3. Strict Architectural Invariants

WattCurb strictly enforces the following **Zero-Kill & Non-Halting Invariant** across all power modes (`Performance`, `Balanced`, `PowerSaver`, `UltraEndurance`):

```
┌──────────────────────────────────────────────────────────────────────────┐
│                   WattCurb Zero-Kill Safety Invariant                    │
├──────────────────────────────────────────────────────────────────────────┤
│ 1. NEVER Kill Processes (0% Terminate, 0% SIGTERM, 0% SIGKILL).          │
│ 2. NEVER Halt Processes (0% cgroup.freeze, 0% SIGSTOP).                  │
│ 3. 100% Non-Halting Graceful Throttling Only:                            │
│    • CPU Scheduler: SCHED_IDLE (runs strictly on idle CPU cycles).       │
│    • Block I/O: IOPRIO_CLASS_IDLE (prevents disk starvation).            │
│    • Timer Coalescing: timerslack_ns (50ms ~ 100ms wakeup flattening).   │
│    • Memory Management: cgroup.memory.reclaim (cold cache reclaim only). │
│ 4. Hardware-Level Power Optimization:                                    │
│    • CPU EPP: 'power' / 1.4GHz hardware cap.                             │
│    • GPU & Panel: DRRS 48Hz, PCIe ASPM powersave, soft backlight cap.    │
└──────────────────────────────────────────────────────────────────────────┘
```

### 3.1. Prohibited Actions
* **Prohibited**: Invoking `::kill(pid, SIGTERM)`, `::kill(pid, SIGKILL)`, or `::kill(pid, SIGSTOP)`.
* **Prohibited**: Writing `"1"` to `/sys/fs/cgroup/.../cgroup.freeze`. Any actuation request for `apply_cgroup_freeze(pid, true)` must be immediately rejected and redirected to `apply_sched_idle(pid)` and `apply_timer_slack(pid, 100'000'000ULL)`.
* **Prohibited**: Script-level execution of `killall -STOP` or any halting signals.

### 3.2. Permitted Non-Destructive Mitigations
1. **`SCHED_IDLE` (`sched_setscheduler`)**:
   Downgrades the process priority weight to the absolute minimum. When user-interactive tasks or desktop tasks require CPU, they immediately preempt the throttled process with 0 latency. When the system is idle, the background process continues executing smoothly without data loss.
2. **`timerslack_ns` Relaxation**:
   Coalesces high-frequency wakeups up to 100ms. Allows CPU cores to enter deeper C-states (`C2`/`C3`) while keeping network sockets, WebSockets, and file I/O completely alive.
3. **`cgroup.memory.reclaim`**:
   Requests kernel page cache compaction and cold anonymous page eviction to ZRAM without terminating or corrupting memory state.
4. **Autonomous Hardware Capping**:
   Clock rate limits (1.4GHz in `UltraEndurance`), AMD SMU TDP clamps, and PCIe runtime power management.

---

## 4. Verification & Oracle Gate Compliance

- All unit tests (`tests/test_units.cpp`) must verify:
  - `ProcessClassifierDB::classify(...).can_freeze == false` across **all** safety tiers (Tiers 0 through 5).
  - `FeatureManager::descriptor(FeatureId::CgroupFreezer).default_enabled == false`.
  - `MitigationEngine::apply_cgroup_freeze(pid, true)` returns `false` and leaves the process unhalted.
