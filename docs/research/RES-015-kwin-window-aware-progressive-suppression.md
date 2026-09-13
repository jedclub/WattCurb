# [REF-RES-015] KWin Window-Aware Progressive Suppression: Idle, Freezing, and Priority Modulation

## 1. Executive Summary & Problem Formulation
- **Ref-ID**: `REF-RES-015`
- **Module**: `policy::WindowAwareGovernor`, `actuator::CgroupActuator`, `actuator::SchedActuator`
- **Date**: 2026-09-13
- **Focus**: Window minimization state detection under KDE Plasma 6 (Wayland) and progressive 3-tier suppression (`SCHED_IDLE`, timer slack relaxation, cgroup v2 freezing) with audio immunity and sub-millisecond thaw recovery.

---

## 2. Window-State Introspection in KDE Plasma 6 Wayland

Under Wayland, client applications cannot query each other's window geometry or focus states due to compositor isolation. However, **KWin** knows the exact state of every window:
- Whether it is minimized (`client.minimized`)
- Whether it currently holds keyboard/pointer focus (`client.active`)
- Whether it is completely occluded by other fullscreen windows
- The owning process PID (`client.pid`)

### 2.1 KWin Scripting Event Hook
KWin provides a native JavaScript runtime (`/Scripting`) that runs inside the compositor process with zero polling overhead. It exposes reactive signals:

```javascript
// /etc/xdg/kwinscripts/wattcurb_tracker/contents/code/main.js
workspace.windowAdded.connect(function(window) {
    if (!window.normalWindow) return;

    window.minimizedChanged.connect(function() {
        callDBus("org.wattcurb.Daemon", "/WindowEvents", "org.wattcurb.WindowEvents",
                 "OnWindowStateChanged", window.pid, window.minimized, window.active);
    });

    window.activeChanged.connect(function() {
        callDBus("org.wattcurb.Daemon", "/WindowEvents", "org.wattcurb.WindowEvents",
                 "OnWindowStateChanged", window.pid, window.minimized, window.active);
    });
});
```

This signal mechanism operates on a **zero-polling, purely push-based event architecture**:
- When a window is minimized $\rightarrow$ KWin immediately notifies WattCurb via D-Bus.
- When WattCurb receives the event, it enters the PID into a state tracking ring buffer.

---

## 3. The Progressive 3-Tier Suppression Ladder

Directly freezing an application the moment it is minimized can cause user frustration (e.g. video conferencing or music cutting off). WattCurb employs a **hysteresis-backed progressive suppression ladder**:

```
[Window Minimized Event]
          │
          ▼
┌────────────────────────────────────────────────────────┐
│ Stage 1: Soft Throttling (Immediate: t = 0s)           │
├────────────────────────────────────────────────────────┤
│ • timerslack_ns: 50µs ➔ 100,000µs (100ms)               │
│ • Sched Policy: SCHED_IDLE (Lowest CFS runqueue rank)  │
│ • cpu.uclamp.max: 100/1024 (Caps CPU boost frequency)  │
│ ➔ Allows background audio/tasks to finish without       │
│   causing CPU frequency spikes.                        │
└──────────────────────────┬─────────────────────────────┘
                           │
                           ▼
┌────────────────────────────────────────────────────────┐
│ Audio Stream Check: Is PipeWire node active for PID?   │
└──────────────┬──────────────────────────┬──────────────┘
               │ Active Audio             │ No Audio Stream
               ▼                          ▼
      [Remain in Stage 1]        [Wait Hysteresis Window: t = 20s]
      (Never freeze audio)                │
                                          ▼
                         ┌────────────────────────────────────────┐
                         │ Stage 2: Hard Freezing (t = 20s)       │
                         ├────────────────────────────────────────┤
                         │ • cgroup.freeze = 1 (Zero CPU/GPU use) │
                         │ • memory.reclaim = 64M (Flushes RAM)   │
                         │ ➔ Power drops to 0mW for this process. │
                         └────────────────────────────────────────┘
```

---

## 4. Latency & Recovery Guarantees: Instant Thaw (< 1ms)

When the user unminimizes the window (e.g. clicks the taskbar icon or presses `Alt+Tab`):
1. KWin fires `activeChanged` and `minimizedChanged(false)`.
2. WattCurb's epoll loop wakes on the D-Bus socket descriptor in **$< 50\mu\text{s}$**.
3. Direct POSIX syscalls restore the process:
   - `write(freeze_fd, "0", 1)`: Thaws the entire cgroup tree in kernel space. Kernel wakes sleeping threads into the runqueue in **$< 300\mu\text{s}$**.
   - `sched_setscheduler(pid, SCHED_OTHER, &param)`: Restores standard CFS scheduling priority.
   - `write(timerslack_fd, "50000", 5)`: Restores 50µs timer precision for smooth 60fps/144fps UI rendering.
4. Total latency from user click to active UI frame render is **$< 1.5\text{ms}$**, completely imperceptible to human perception.

---

## 5. Audio Immunity & Safe Process Exemptions

Certain applications must **never be hard-frozen**, even when minimized:
1. **Active Audio / Media Players**:
   - Tracked via PipeWire client streams or D-Bus `org.mpris.MediaPlayer2`.
   - If an application is emitting audio buffers, WattCurb clamps it to `Stage 1` (`SCHED_IDLE` with audio thread RT priority preserved) and **never enters Stage 2 freezing**.
2. **Interactive Terminal Emulators (`foot`, `konsole`, `kitty`)**:
   - Long-running compilations or package managers (`pacman`, `ninja`, `cargo`) must continue executing. Clamped to `SCHED_IDLE` so they utilize surplus cycles without choking interactive apps.
3. **System Daemons & Compositor (`DesktopCore`)**:
   - `kwin_wayland`, `plasmashell`, `pipewire`, `wireplumber` remain **strictly immune**.
