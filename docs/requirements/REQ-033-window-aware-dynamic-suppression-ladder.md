# [REF-REQ-033] Window-Aware Dynamic Suppression Ladder Specification

## 1. Context & Scope
- **Ref-ID**: `REF-REQ-033`
- **Related Requirements**: [`REF-REQ-028`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-028-adaptive-power-state-machine-and-actuation.md), [`REF-REQ-032`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-032-kde-plasma-desktop-mitigation.md)
- **Related Research**: [`REF-RES-009`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-009-linux-desktop-sleep-and-resource-reclaim.md), [`REF-RES-015`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-015-kwin-window-aware-progressive-suppression.md)
- **Target Subsystem**: `policy::WindowAwareGovernor`, `actuator::CgroupActuator`

Users frequently minimize background applications (browsers with multiple JavaScript-heavy tabs, chat clients like Slack/Discord, Steam, development tools). Despite being completely invisible, these processes burn CPU quantum, trigger timer interrupts, and keep the CPU package from settling into deep C-states ($C6/C10$).

This requirement specifies the **Window-Aware Dynamic Suppression Ladder** which automatically applies progressive priority modulation (`SCHED_IDLE`, timer slack relaxation) and cgroup v2 freezing upon window minimization, with strict audio immunity and sub-millisecond thaw guarantees.

---

## 2. Functional Requirements

### REQ-033.1: Zero-Polling Event Ingestion from KWin
- The daemon shall expose a D-Bus endpoint `org.wattcurb.WindowEvents` receiving `OnWindowStateChanged(uint32 pid, bool minimized, bool active)` signals.
- Ingestion shall be push-based and driven by KWin Scripting, requiring **zero polling wakeups** during steady-state desktop execution.

### REQ-033.2: Immediate Stage 1 Throttling (Soft Priority Modulation)
- Upon receiving a `minimized = true` event:
  - If the process belongs to Tier 3, Tier 4, or Tier 5:
    1. Set the process's timer slack via `/proc/[pid]/timerslack_ns` to `100,000,000` (100ms).
    2. Demote scheduling policy to `SCHED_IDLE` via `sched_setscheduler(pid, SCHED_IDLE, ...)`.
    3. Clamp `cpu.uclamp.max` to `100` (out of 1024) if cgroup v2 cpu controller is available.
  - The process continues executing in background, but will never preempt active tasks or ramp up CPU frequencies.

### REQ-033.3: Audio Immunity Evaluation
- Before escalating to Stage 2 freezing:
  - The governor shall verify whether the PID holds an active audio stream (via PipeWire D-Bus or active audio playback lock).
  - If audio playback is detected:
    - **Stage 2 freezing is strictly inhibited.** The process remains in Stage 1 indefinitely while audio plays.

### REQ-033.4: Progressive Stage 2 Hard Freezing & Memory Reclamation
- If the application remains minimized for $\ge 20$ seconds without active audio:
  - Write `"1"` to `/sys/fs/cgroup/.../cgroup.freeze` for the application's slice.
  - Issue proactive memory compaction by writing `"64M"` to `/sys/fs/cgroup/.../memory.reclaim`.
  - Record the frozen state in the fixed-capacity `WindowSuppressionTable`.

### REQ-033.5: Instantaneous Stage 0 Recovery (Thaw on Activation)
- Upon receiving `minimized = false` or `active = true`:
  1. Write `"0"` to `cgroup.freeze` immediately.
  2. Restore scheduling policy to `SCHED_OTHER` (nice 0).
  3. Restore `timerslack_ns` to default (`50,000`).
- **Timing Constraint**: The entire thaw and priority restoration sequence must complete in $\le 1.0\text{ms}$.

---

## 3. Non-Functional & Safety Constraints

1. **Terminal & Compiler Protection**:
   - Terminal processes (`foot`, `konsole`, `kitty`) containing active subprocesses shall only be subject to Stage 1 (`SCHED_IDLE`) and never Stage 2 freezing, preventing long-running build interruptions.
2. **Deterministic Memory Footprint**:
   - The PID tracking table shall be statically allocated with a capacity of 64 concurrent windows (`FixedVector<WindowStateEntry, 64>`), with zero dynamic heap allocations.
3. **Self-Freeze Prevention Invariant (Anti-Deadlock Guard)**:
   - The mitigation engine and window governor shall strictly reject freezing the daemon's own PID (`getpid()`) or its parent PID (`getppid()`), completely eliminating test harness freeze deadlocks and daemon self-suspension.
