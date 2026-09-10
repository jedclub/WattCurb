# [REF-RES-008] Deep Linux Process Classification & Strategic Mitigation Database

## 1. Executive Summary & Research Scope
- **Ref-ID**: `REF-RES-008`
- **Module**: `policy::MitigationEngine`, `policy::ProcessClassifierDB`
- **Date**: 2026-09-11
- **Focus**: Systematic classification of Linux processes, identification of runaway battery drainers, safety tiers, and progressive power-capping actions.

To safely throttle, freeze, or reclaim resources from background processes without destabilizing the Linux OS or degrading user responsiveness, WattCurb requires a definitive, hardware-verified **Process Knowledge Base**. This document establishes the classification taxonomy, safety rules, and mitigation strategies for standard Linux desktop and server processes.

---

## 2. Process Safety Tiers & Classification Taxonomy

Every process observed in `/proc` is categorized into one of six distinct functional tiers:

```text
+------------------------------------------------------------------------------------+
| Tier 0: Critical Kernel & Init (CRITICAL_IMMUNE) — ZERO INTERFERENCE              |
| systemd, kthreadd, kworker/*, dbus-broker, seatd, polkitd, pipewire, wireplumber   |
+------------------------------------------------------------------------------------+
| Tier 1: Desktop Compositor & Core (DESKTOP_CORE) — NEVER FREEZE OR TERMINATE       |
| kwin_wayland, kwin_x11, mutter, sway, hyprland, Xorg, wayland                      |
+------------------------------------------------------------------------------------+
| Tier 2: Desktop Shell & System Panels (DESKTOP_SHELL) — SAFE RECLAIM ONLY          |
| plasmashell, gnome-shell, xfce4-panel, krunner                                     |
+------------------------------------------------------------------------------------+
| Tier 3: User Interactive Applications (USER_INTERACTIVE) — CONDITIONAL THROTTLE    |
| chrome, firefox, kitty, alacritty, code, cursor, slack, discord, telegram-desktop  |
+------------------------------------------------------------------------------------+
| Tier 4: Background Indexers & Sync Workers (BACKGROUND_WORKER) — AGGRESSIVE IDLE   |
| baloo_file, tracker-miner-fs-3, updatedb, packagekitd, nextcloud, dropbox, rclone  |
+------------------------------------------------------------------------------------+
| Tier 5: Runaway / Orphan / Ephemeral Daemons (RUNAWAY_CANDIDATE) — FREEZE / KILL   |
| Stalled build jobs, runaway scripts, unneeded daemon workers (dirmngr, etc.)       |
+------------------------------------------------------------------------------------+
```

---

## 3. Comprehensive Process Knowledge Database

| Process Pattern | Category | Safety Tier | Max Allowed Mitigation | Reactivation Trigger | Battery Power Threat Mechanism |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `systemd`, `init` | System Init | **CRITICAL_IMMUNE** | **None (Observe Only)** | N/A | Timer wakeups / cgroup housekeeping |
| `kthreadd`, `kworker/*` | Kernel Subsystem | **CRITICAL_IMMUNE** | **None (Observe Only)** | N/A | Kernel deferred work, interrupt dispatch |
| `pipewire`, `wireplumber`| Low-Latency Audio | **CRITICAL_IMMUNE** | **None (Observe Only)** | N/A | Real-time ALSA/JACK scheduling |
| `dbus-broker`, `dbus-daemon`| IPC Bus | **CRITICAL_IMMUNE** | **None (Observe Only)** | N/A | High socket switch rates under app churn |
| `seatd`, `polkitd`, `udevd`| Auth & Devices | **CRITICAL_IMMUNE** | **None (Observe Only)** | N/A | Session access arbitration |
| `kwin_wayland`, `kwin_x11`| Wayland Compositor | **DESKTOP_CORE** | **None (Observe Only)** | User input / VSync | GPU rendering, display refresh sync |
| `mutter`, `Xorg` | Compositor / X Server| **DESKTOP_CORE** | **None (Observe Only)** | User input / VSync | Hardware display scanout |
| `plasmashell` | Desktop Shell | **DESKTOP_SHELL** | `MemoryReclaim` | Shell interaction | Widget polling, QML rendering, system tray |
| `chrome`, `firefox`, `zen` | Web Browser | **USER_INTERACTIVE** | `SCHED_IDLE`, `TimerSlack`, `MemoryReclaim`, `Freeze` | Window Focus / Mouse hover | 100% GPU VRAM, WiFi CAM sockets, wakeups |
| `kitty`, `alacritty`, `konsole`| Terminal Emulator | **USER_INTERACTIVE**| `SCHED_IDLE`, `TimerSlack`, `MemoryReclaim` | Window Focus / PTY event | GPU rendering, high wakeup rates |
| `code`, `cursor`, `zed` | IDE / Editor | **USER_INTERACTIVE** | `SCHED_IDLE`, `MemoryReclaim`, `Freeze` (unfocused)| Window Focus / File save | Language server compute, DRAM footprint |
| `slack`, `discord`, `telegram`| Electron Chat | **USER_INTERACTIVE** | `SCHED_IDLE`, `TimerSlack`, `MemoryReclaim`, `Freeze` | Notification / Focus | WiFi CAM mode radio retention, Chromium timers |
| `baloo_file`, `baloo_file_extr`| KDE File Indexer | **BACKGROUND_WORKER**| `SCHED_IDLE`, `IONICE_IDLE`, `Freeze` | AC power return | High NVMe SSD I/O, CPU compute churn |
| `tracker-miner-fs-3` | GNOME File Indexer | **BACKGROUND_WORKER**| `SCHED_IDLE`, `IONICE_IDLE`, `Freeze` | AC power return | Constant disk walks, flash storage wakeups |
| `updatedb`, `locate` | mlocate Indexer | **BACKGROUND_WORKER**| `SCHED_IDLE`, `IONICE_IDLE`, `Freeze`, `SIGSTOP` | Scheduled cron | Storage APST state disruption, high major faults |
| `packagekitd` | Package Manager Daemon| **BACKGROUND_WORKER**| `SCHED_IDLE`, `Freeze` | User package query | Network polling, unneeded C-state wakeups |
| `nextcloud`, `dropbox`, `rclone`| Cloud Sync Agent| **BACKGROUND_WORKER**| `SCHED_IDLE`, `TimerSlack`, `Freeze` | Network connection change| WiFi CAM sockets, periodic stat walks |
| `dirmngr`, `gpg-agent` | Cryptographic Worker| **RUNAWAY_CANDIDATE**| `Freeze`, `SIGTERM` (if idle > 30m) | Cryptographic request | Idle background wakeups |

---

## 4. Progressive Mitigation Action Hierarchy

Mitigation actions must strictly follow a **Progressive 5-Stage Escalation Ladder**. Lower stages must always be exhausted before considering invasive actions:

```text
[Stage 0: Observe & Classify]
  │  Compute 5-second window attribution and determine process safety tier.
  ▼
[Stage 1: Non-Intrusive Scheduler Demotion (SCHED_IDLE + ionice class 3)]
  │  Demotes CFS scheduling priority to IDLE; kernel treats core as idle for C-states.
  ▼
[Stage 2: Timer Slack Coalescing (/proc/[pid]/timerslack_ns)]
  │  Relaxes timer granularity to 100ms ~ 500ms; batches wakeups, preserving C3 deep sleep.
  ▼
[Stage 3: Proactive Memory Reclaim (cgroup v2 memory.reclaim)]
  │  Compacts inactive anon/file memory, reducing DRAM retention wattage.
  ▼
[Stage 4: Cgroup v2 Transparent Freezing (cgroup.freeze = 1)]
  │  Atomically pauses process tree; zero signals sent, completely stops CPU/GPU drain.
  ▼
[Stage 5: Conservative Graceful Termination (SIGTERM)]
     EXCLUSIVELY for runaway background indexers/orphans under Critical Battery (< 10%).
```

---

## 5. Reactivation & State Restoration Protocols

Any process placed under mitigation must be immediately restored to normal execution upon:
1. **User Focus Change**: Wayland/X11 active window focus switched to the application.
2. **IPC / Socket Activity**: Arrival of network packet or D-Bus message (via cgroup or inotify).
3. **Power Transition**: AC power plugged in (battery discharging stops).

Restoration must execute within **< 1 millisecond** by:
- Writing `0` to `cgroup.freeze`
- Resetting scheduler policy to `SCHED_OTHER` (`nice 0`)
- Restoring `timerslack_ns` to 50,000 ns.
