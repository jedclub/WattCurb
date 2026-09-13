# REQ-036: Production Deployment, Autostart & ThinkPower Migration Specification

## 1. Executive Summary & Objective

- **REF-ID**: `REF-REQ-036`
- **Related Requirements**: [`REF-REQ-001`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-001-singleton-and-ipc.md), [`REF-REQ-007`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-007-binary-shared-state-and-ipc.md), [`REF-REQ-035`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-035-thinkpower-faithful-ultra-low-overhead-tray.md)
- **Primary Objective**:
  Migrate the host environment from legacy ThinkPower (`power-tray.service`) to WattCurb's dual-binary architecture (`wattcurb` background daemon + `wattcurb-tray` SNI client). Establish permanent systemd user service registration and XDG autostart integration for boot-time automated activation.

---

## 2. ThinkPower Decommissioning Requirements

1. **Service Suspension & Disabling**:
   - Stop `power-tray.service` via `systemctl --user stop power-tray.service`.
   - Disable automatic boot activation via `systemctl --user disable power-tray.service`.
   - Neutralize `~/.config/autostart/power-tray.desktop` by archiving to `.disabled` state.
2. **Zero-Residual Verification**:
   - Ensure `power-tray` and related legacy scripts are completely purged from active process memory (`PID` dead, memory reclaimed).

---

## 3. WattCurb Dual-Binary Deployment Requirements

1. **Target Installation Paths**:
   - Daemonic backend binary: `~/.local/bin/wattcurb`
   - SNI Desktop tray client binary: `~/.local/bin/wattcurb-tray`
   - Both binaries must be compiled with production `-O3`, `-flto`, `-DNDEBUG`, stripped (`strip --strip-all`), and hardened.
2. **Service Topology & Dependency Ordering**:
   - **`wattcurb.service` (Background Daemon)**:
     - `Type=simple`, `ExecStart=%h/.local/bin/wattcurb --daemon`
     - `Restart=on-failure`, `RestartSec=3`
     - Part of `graphical-session.target`.
   - **`wattcurb-tray.service` (Desktop SNI Client)**:
     - `Type=simple`, `ExecStart=%h/.local/bin/wattcurb-tray`
     - `After=graphical-session.target wattcurb.service`
     - `PartOf=graphical-session.target`
     - `Restart=on-failure`, `RestartSec=2`
3. **XDG Autostart Compliance**:
   - Create `~/.config/autostart/wattcurb-tray.desktop` referencing `%h/.local/bin/wattcurb-tray`.
   - Ensure compatibility with KDE Plasma 6 Wayland session autostart phases.

---

## 4. Verification & Validation Metrics

1. **Process Tree & Resource Validation**:
   - `wattcurb`: RSS < 2.0 MB, CPU < 0.1%.
   - `wattcurb-tray`: RSS < 1.5 MB, CPU = 0.00% (epoll idle).
2. **D-Bus SNI Registration**:
   - StatusNotifierItem registered on session bus and visible in KDE system tray.
