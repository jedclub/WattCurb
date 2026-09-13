# [REF-REQ-028] Desktop Tray System & Bi-Directional Daemon Coordination Specification

- **Ref-ID**: `REF-REQ-028`
- **Related Architecture**: [`REF-ARCH-018`](../architecture/ARCH-018-desktop-tray-and-daemon-coordination.md)
- **Status**: Approved / Specification

---

## 1. Executive Summary & Philosophy

WattCurb's mission is to minimize battery drain without creating an observer effect. Integrating a graphical desktop tray icon must never compromise this mission.
- **Privilege Separation**: The background daemon runs with root privileges (`CAP_SYS_NICE`, `CAP_SYS_ADMIN`) to enforce cgroups v2, MSR, and ASPM controls. The desktop tray icon runs as an unprivileged user in the Wayland/X11 session.
- **Zero-Wakeup & Zero-Copy Principle**: The tray icon must not poll the daemon. Telemetry must be exchanged via a 128-byte cacheline-aligned POSIX Shared Memory segment (`/dev/shm/wattcurb_state`) using lock-free Seqlock synchronization.
- **Bi-Directional Command Control**: User interactions in the tray menu (e.g., toggling Extreme Eco Mode or whitelisting runaway processes) are conveyed to the daemon asynchronously over a non-blocking Unix Domain Socket (`/run/wattcurb.sock`).

---

## 2. Functional Requirements

### 2.1 Telemetry State Broadcasting (REF-REQ-028-1)
- The daemon must update the shared memory structure at the end of each continuous observation window.
- The shared state structure (`WattCurbSharedState`) must be TriviallyCopyable POD, padded and aligned to 64 bytes (`alignas(64)`), containing:
  1. `seq_version`: 64-bit sequence counter for lock-free reader synchronization.
  2. `system_drain_mw`: Instantaneous total system power in milliwatts ($mW$).
  3. `battery_percent`: Integer battery capacity percentage ($0 \dots 100$).
  4. `battery_state`: Power supply state (`0: AC Passthrough`, `1: Discharging`, `2: Charging`).
  5. `time_to_empty_min`: Projected battery runtime in minutes.
  6. `active_mitigations`: Bitmask of currently active mitigation features.
  7. `top_culprits[3]`: Top 3 power-draining processes with `comm`, `pid`, `drain_mw`, and physical domain ID.

### 2.2 Bi-Directional Action Dispatching (REF-REQ-028-2)
- The daemon's `DaemonRunner` must monitor the control socket using `epoll` without periodic polling.
- The tray client sends one-line ASCII commands:
  - `SET_FEATURE <feature_name> <on|off>` (e.g., `SET_FEATURE sched_idle on`)
  - `THROTTLE <pid>` / `UNTHROTTLE <pid>`
  - `WHITELIST <comm|pid>`
  - `TRIGGER_RECLAIM <target_mb>`
- The daemon validates privilege and executes commands asynchronously via `MitigationEngine`.

### 2.3 Desktop Protocol Interoperability (REF-REQ-028-3)
- The tray client must implement the `org.kde.StatusNotifierItem` D-Bus interface (compatible with KDE Plasma, GNOME via AppIndicator, Sway/Waybar, and Hyprland).
- The tray binary (`wattcurb-tray`) must remain decoupled from the daemon package, ensuring headless server deployments carry zero GUI or D-Bus dependencies.
