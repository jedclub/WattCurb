# [REF-RES-016] Rapid Dynamic Rollback & Charge Event Coordination Architecture

## 1. Executive Summary & Research Motivation
- **Ref-ID**: `REF-RES-016`
- **Module**: `policy::UnifiedRollbackCoordinator`, `policy::MitigationEngine`, `policy::WindowAwareGovernor`
- **Date**: 2026-09-13
- **Focus**: Sub-millisecond comprehensive restoration of all throttled processes, KDE desktop shaders, display refresh rates, and silicon power knobs upon AC connection or performance profile elevation.

### Problem Formulation
Power-saving daemons that successfully throttle hardware and background applications often fail in their exit strategy:
1. **Lax or Partial Restoration**: They restore CPU frequency but forget to restore process timer slack, leaving background tabs unresponsive.
2. **Missing Desktop State Synchronization**: They downscale display refresh rates to 60Hz or unload KWin blur shaders on battery, but fail to restore 144Hz and visual fidelity when the laptop is plugged into AC power.
3. **Asynchronous Lag**: Delayed detection of power supply events (e.g. 10-second polling) results in a sluggish user experience after plugging in the charger.

WattCurb establishes the **Rapid Zero-Residual Rollback Mandate**: The instant an AC plug-in or performance profile elevation is detected, every single applied power optimization across all hardware and software domains must be restored to baseline within $\le 5.0\text{ms}$.

---

## 2. The 4 Rollback Domains

```
┌────────────────────────────────────────────────────────────────────────┐
│             AC Plug-in Event / Performance Profile Selection           │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ Instantaneous epoll / uevent dispatch
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│           policy::UnifiedRollbackCoordinator::execute_rollback()       │
└───────┬───────────────────┬────────────────────┬───────────────────┬───┘
        │                   │                    │                   │
        ▼ Domain 1          ▼ Domain 2           ▼ Domain 3          ▼ Domain 4
┌───────────────┐   ┌───────────────┐    ┌───────────────┐   ┌───────────────┐
│ Process Run   │   │ KDE Desktop   │    │ Silicon & CPU │   │ Bus, GPU &    │
│ Queue & Timers│   │ & Compositor  │    │ Frequency     │   │ Peripherals   │
├───────────────┤   ├───────────────┤    ├───────────────┤   ├───────────────┤
│• SCHED_OTHER  │   │• KWin Shaders │    │• CPU EPP ➔    │   │• PCIe ASPM ➔  │
│  (CFS weight) │   │  (loadEffect) │    │  performance  │   │  default      │
│• timerslack ➔ │   │• DRRS ➔ 144Hz │    │• CPU Boost ➔  │   │• Backlight ➔  │
│  50µs         │   │• Baloo ➔      │    │  enabled (1)  │   │  saved level  │
│• IOPRIO ➔ BE  │   │  resume       │    │• RAPL limit ➔ │   │• WiFi PS ➔    │
│               │   │• Anim ➔ 1.0   │    │  unconstrained│   │  full power   │
└───────────────┘   └───────────────┘    └───────────────┘   └───────────────┘
```

---

## 3. Rollback Trigger Invariants

A full rollback sweep must execute deterministically upon:
1. **AC Adapter Online (`BAT0/uevent: POWER_SUPPLY_ONLINE = 1`)**:
   - Direct kernel uevent Netlink socket / sysfs persistent FD edge detection.
2. **Manual Profile Elevation**:
   - User command over Unix Domain Socket or Tray IPC requesting `Balanced` or `Performance`.
3. **Battery Charge Recovery (Hysteresis Crossing)**:
   - When charging raises battery capacity above $55\%$ (from `PowerSaver`) or above $25\%$ (from `UltraEndurance`).
4. **Daemon Clean Shutdown (`SIGTERM`, `SIGINT`)**:
   - Zero-residual exit guarantee: WattCurb must never leave the host in a throttled state upon exit.

---

## 4. Latency Budget & Lock-Free Guarantee

To achieve an imperceptible transition:
* **Detection Latency**: $< 100\mu\text{s}$ (kernel uevent).
* **Process Sweep Latency**: $< 500\mu\text{s}$ (up to 64 tracked PIDs in flat memory).
* **KDE D-Bus Signal Latency**: $< 2.0\text{ms}$ (non-blocking IPC).
* **Sysfs Hardware Knobs**: $< 1.0\text{ms}$ (direct `pwrite` to persistent FDs).
* **Total End-to-End Latency**: Strictly **$< 5.0\text{ms}$**.
