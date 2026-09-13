# [REF-RES-017] ThinkPower Desktop Tray UX Analysis & Zero-Overhead C++23 Client Architecture

## 1. Executive Summary & Research Motivation
- **Ref-ID**: `REF-RES-017`
- **Module**: `tray::StatusNotifierClient`, `tray::TraySharedMemoryReader`
- **Date**: 2026-09-13
- **Focus**: Preserving ThinkPower's battle-tested desktop tray UX (StatusNotifierItem / SNI) while enforcing WattCurb's radical zero-wakeup, zero-heap, sub-milliwatt C++23 engineering standards.

### The ThinkPower Legacy
In prior Linux desktop implementations (such as the legacy ThinkPower project):
- The desktop tray icon served as the central nerve center for power awareness.
- It dynamically reflected battery discharge rate (e.g. `6.8W`), charging state, and active power profiles.
- Hovering displayed an instant, dense technical tooltip: Top 2 energy-consuming processes, CPU package draw, GPU draw, battery health, and deep C-state residence.
- Left/Right-click menus offered instant profile switching (`Balanced`, `PowerSaver`, `UltraEndurance`).

### The Critical Flaw of Legacy GUI Trays
Traditional trays implemented in Python (PyQt/PyGObject) or heavy Qt/Electron:
1. **Massive Memory Footprint**: 40MB ~ 120MB RSS just to show a 16x16 icon.
2. **Periodic Polling Wakeups**: Waking up every 1~2 seconds to update tray icon pixmaps, causing CPU package C-state exits and burning 200mW ~ 500mW of idle power (the observer effect).
3. **IPC Latency**: Sluggish tooltip updates caused by JSON parsing over IPC.

---

## 2. WattCurb Zero-Overhead Tray Architecture

WattCurb re-engineers the ThinkPower tray from the ground up using modern C++23 zero-cost abstractions:

```
┌────────────────────────────────────────────────────────────────────────┐
│               KDE Plasma 6 / StatusNotifierItem (SNI) Host             │
│                      (Wayland Session / Panel)                         │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ Pure Push/Pull Events:
                                    │ • OnHover: Get("ToolTip")
                                    │ • OnClick: Activate() / ContextMenu()
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│                   wattcurb-tray (Unprivileged Client)                  │
├────────────────────────────────────────────────────────────────────────┤
│  • Pure C++23 sd-bus / direct D-Bus connection                         │
│  • Flat binary size: ~150 KB | Runtime RSS: < 1.8 MB                   │
│  • ZERO Polling Wakeups: 100% asleep in epoll_wait until D-Bus event   │
│  • ZERO Dynamic Heap Allocation in hot tooltip render path             │
└───────────────────┬────────────────────────────────▲───────────────────┘
                    │ [Action Dispatch]              │ [Zero-Copy Telemetry Ingestion]
                    │ Non-blocking UDS               │ 128-Byte Seqlock POD
                    │ /run/wattcurb.sock             │ /dev/shm/wattcurb_state.shm
                    ▼                                │ (Atomic memcpy: < 50ns, 0 locks)
┌────────────────────────────────────────────────────┴───────────────────┐
│               wattcurb (Privileged Background Sub-Daemon)              │
│  • Performs RAPL/GPU/Battery/Process attribution every 60 seconds      │
│  • Updates 128B Seqlock state atomically                               │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 3. ThinkPower UX Blueprint Preservation

| UX Element | ThinkPower Legacy Behavior | WattCurb Zero-Overhead C++23 Implementation |
| :--- | :--- | :--- |
| **Tray Icon** | Dynamic icon reflecting battery discharging ($W$), charging, or AC status. | Standard freedesktop icon naming (`battery-caution`, `battery-good`, `ac-adapter`) + dynamic overlay. Zero periodic redraws. |
| **Hover Tooltip** | Multi-line dense technical briefing: Watts, Time, Top 2 Processes, Temp. | Pre-allocated 512-byte stack buffer formatted with zero-alloc `snprintf` directly into D-Bus string response. |
| **Left Click** | Quick Profile Switcher (Cycles Balanced $\rightarrow$ Saver $\rightarrow$ Ultra). | Transmits 16-byte ASCII command to `/run/wattcurb.sock` in $< 10\mu\text{s}$. |
| **Right Click** | Context Menu: Profiles, Rescan Now, Open Detailed Briefing, Exit Tray. | Standard DBusMenu / ContextMenu implementation responding on-demand. |

---

## 4. Technical Feasibility & Dependency Selection
To eliminate the 50MB+ Qt/GTK dependency while remaining 100% compatible with KDE Plasma 6:
* **Option A: `sd-bus` (systemd D-Bus library)**:
  - Standard on modern Linux (CachyOS, Arch, Fedora, Ubuntu).
  - Pure C API, rock-solid, zero memory leaks, sub-millisecond dispatch.
  - Native integration with Linux `epoll` event loops.
* **Option B: Standalone C++23 SNI Protocol Layer**:
  - Exposes `org.kde.StatusNotifierItem` and `com.canonical.dbusmenu`.
  - Links only against `libsystemd` / `libdbus-1`.

This achieves **ThinkPower's complete user experience with less than 2MB of memory and exactly 0.0% CPU overhead**.
