# [REF-REQ-033] Window-Aware Non-Halting Graceful Throttle Specification

## 1. Context & Scope
- **Ref-ID**: `REF-REQ-033`
- **Related Requirements**: [`REF-REQ-028`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-028-adaptive-power-state-machine-and-actuation.md), [`REF-REQ-032`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-032-kde-plasma-desktop-mitigation.md)
- **Related Research**: [`REF-RES-009`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-009-linux-desktop-sleep-and-resource-reclaim.md), [`REF-RES-015`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-015-kwin-window-aware-progressive-suppression.md)
- **Target Subsystem**: `policy::WindowAwareGovernor`, `actuator::SchedActuator`

Users frequently minimize background applications (browsers with multiple JavaScript-heavy tabs, chat clients like Slack/Discord, Steam, development tools). 

### Core Architectural Mandate: Non-Halting Principle (Zero Extreme Freezing)
**Background applications must NEVER be completely halted or frozen (`cgroup.freeze = 1`).**  
Freezing processes causes network drops (broken WebSockets, missed chat notifications), D-Bus IPC timeouts (causing desktop stutter or KWin hangs), and audio/download corruption. 

Instead, WattCurb enforces **Graceful Idle Throttling**: processes remain 100% alive and responsive, but yield CPU execution priority to foreground tasks (`SCHED_IDLE`), coalesce timer wakeups (`timerslack_ns = 50ms`), and minimize disk I/O contention.

---

## 2. Functional Requirements

### REQ-033.1: Zero-Polling Event Ingestion from KWin
- The daemon shall expose a D-Bus endpoint `org.wattcurb.WindowEvents` receiving `OnWindowStateChanged(uint32 pid, bool minimized, bool active)` signals.
- Ingestion shall be push-based and driven by KWin Scripting, requiring **zero polling wakeups** during steady-state desktop execution.

### REQ-033.2: Non-Halting Graceful Throttling (GracefulIdleThrottled)
- Upon receiving a `minimized = true` event:
  - If the process belongs to Tier 3, Tier 4, or Tier 5:
    1. Demote scheduling policy to `SCHED_IDLE` via `sched_setscheduler(pid, SCHED_IDLE, ...)`.
       - *Guarantee*: Process executes **only when no other thread demands the CPU**, preventing any CPU starvation or UI stutter for active foreground windows.
    2. Coalesce timer slack to `50,000,000` (50ms) via `/proc/[pid]/timerslack_ns`.
       - *Guarantee*: Process continues processing network packets, timers, and WebSockets without forcing high-frequency CPU package wakeups.
    3. Lower Block I/O priority to `IOPRIO_CLASS_IDLE`.
- **Absolute Non-Halting Invariant**: The process is never halted, never enters `FROZEN` state, and never drops active network connections.

### REQ-033.3: Instantaneous Unthrottle on Window Activation
- Upon receiving `minimized = false` or `active = true`:
  1. Restore scheduling policy to `SCHED_OTHER` (nice 0).
  2. Restore `timerslack_ns` to default (`50,000` ns).
- **Latency Requirement**: Unthrottling must complete in $\le 50\mu\text{s}$ (sub-millisecond), guaranteeing instantaneous UI responsiveness without reconnect delays.

---

## 3. Non-Functional & Safety Constraints

1. **Self-Safety Invariant (Anti-Deadlock Guard)**:
   - The governor shall strictly reject tracking the daemon's own PID (`getpid()`) or its parent PID (`getppid()`), completely preventing self-throttling deadlocks.
2. **Terminal & Media App Safety**:
   - Compiling terminals (`konsole`, `foot`) and media players continue executing smoothly under `SCHED_IDLE` without unexpected drops.
3. **Deterministic Memory Footprint**:
   - The PID tracking table shall be statically allocated with a capacity of 64 concurrent windows (`FixedVector<WindowStateEntry, 64>`), with zero dynamic heap allocations.
