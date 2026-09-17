# [REF-REQ-045] Explicit Menu Activation for Matrix Dashboard

## 1. Requirement Metadata
- **Ref-ID**: `REF-REQ-045`
- **Title**: Explicit Menu Activation for High-Precision Matrix Dashboard Window
- **Module**: `tray::TrayClient`, `ui::DashboardWindow`
- **Status**: Active / Approved
- **Date**: 2026-09-15
- **Related Requirements**: [`REF-REQ-035`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-035-thinkpower-faithful-ultra-low-overhead-tray.md), [`REF-REQ-036`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-036-native-kde-dashboard-matrix.md), [`REF-REQ-042`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-042-tray-svg-cyber-hud-and-click-activation.md)

---

## 2. Background & Motivation
In [`REF-REQ-042`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-042-tray-svg-cyber-hud-and-click-activation.md), left-clicking the StatusNotifierItem (SNI) tray icon was configured to immediately fork/exec the full Qt Quick Matrix Dashboard window (`wattcurb-dashboard`).

However, user experience feedback identified that:
1. Accidental or habitual left-clicks on the tray icon (e.g. while inspecting tooltips or intending to cycle power modes) unintentionally launched the heavy Matrix Dashboard window, obstructing the desktop and interrupting workflow.
2. The precision matrix dashboard should be an intentional, on-demand analytical window rather than an invasive click response.

---

## 3. Specification

### 3.1. Non-Intrusive Left-Click Behavior
- The `Activate` method on `org.kde.StatusNotifierItem` (`TrayClient::method_activate`) shall **never** launch `wattcurb-dashboard`.
- Left-clicking the tray icon must perform one-click power profile cycling (`cycle_power_profile()`) as originally defined in [`REF-REQ-035.3`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-035-thinkpower-faithful-ultra-low-overhead-tray.md):
  $$\text{Performance} \longrightarrow \text{Balanced} \longrightarrow \text{SmartSave} \longrightarrow \text{UltraSave} \longrightarrow \text{Performance}$$
- The client shall emit `NewIcon`, `NewToolTip`, `XAyatanaNewLabel`, and `LayoutUpdated` to immediately update panel indicators with zero window popups.

### 3.2. Explicit Menu Choice for Matrix Dashboard
- Launching the Matrix Dashboard window (`wattcurb-dashboard`) is strictly restricted to explicit user selection from the context menu (`com.canonical.dbusmenu`).
- The menu item shall be labeled:
  `📈 정밀 분석 매트릭 창 열기 (Matrix Dashboard)`
- Clicking this menu item executes a clean fork/exec of `/home/jedclub/.local/bin/wattcurb-dashboard` with prior instance deduplication (`pkill -f wattcurb-dashboard 2>/dev/null`).

### 3.3. Ephemeral Lifecycle & Absolute Zero Inactive Resource Footprint
- **Zero Inactive Footprint**: When the dashboard window is closed, the process terminates completely via `QGuiApplication::exit(0)`, returning all resident memory (RSS), GPU buffers, and file descriptors to the Linux kernel.
- In the closed/inactive state:
  - **CPU Consumption**: Exactly **0.0%** (0 cycles, 0 scheduling wakeups).
  - **Memory RSS**: Exactly **0 MB** (process does not exist in memory).
  - **GPU / VRAM Consumption**: Exactly **0.0% / 0 MB**.
  - **Daemon IPC Overhead**: Exactly **0 calls** (the root daemon performs zero JSON serialization or socket writes while the dashboard is closed).
- The dashboard shall never run as a hidden background daemon or resident system service.

---

## 4. Verification & Testing
- Unit tests (`tests/test_units.cpp`) and tray operation verified.
- Left-clicking the tray icon triggers profile cycling and icon/label updates without spawning GUI processes.
- The Matrix Dashboard window launches only upon explicit menu selection.
- Process inspection (`ps aux | grep wattcurb-dashboard`) verifies 0 running processes and 0 MB memory footprint when closed.

