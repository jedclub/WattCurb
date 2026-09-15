# REQ-050: Instant Hover Probe & Dense Monospace Cyber HUD Specification

## 1. Overview & Problem Statement

### 1.1 The Issue
Users observed that mouse-hovering over the WattCurb tray indicator failed to reflect real-time hardware telemetry updates. When CPU or GPU loads were induced, the displayed tooltip numbers appeared frozen on stale data from minutes earlier, and the tooltip styling lacked dense progressive information.

### 1.2 Root-Cause Forensic Analysis
1. **Excessive Daemon Sampling Period (60.0s)**:
   - `wattcurb.service` was launched with default `--period 60.0`, updating shared memory only once every 60 seconds.
2. **KDE Plasma ToolTip Caching & Missing Change Signals**:
   - The D-Bus StatusNotifierItem host (KDE Plasma) caches the rendered ToolTip property and never re-queries the daemon unless a `NewToolTip` signal is emitted.
   - The tray event loop only emitted `NewToolTip` on coarse 200mW deltas, leaving Plasma showing stale cached tooltips even after load spikes.
3. **Absence of On-Demand Live Probing**:
   - `property_get_tooltip()` read whatever snapshot was sitting in shared memory rather than sampling active kernel sensors at the exact instant of hover.

---

## 2. Functional Requirements (`REF-REQ-050`)

### 2.1 Sub-5µs Instant On-Demand Sensor Probe
1. Upon receiving a D-Bus `ToolTip` property request (`property_get_tooltip()`), `TrayClient` immediately triggers `probe_live_sensors_on_hover()`:
   - **Battery Telemetry**: Real-time power drain, capacity, and charge/discharge status parsed directly from `/sys/class/power_supply/BAT0/uevent` (or BAT1).
   - **CPU Thermal Sensor**: Real-time package temperature read from `/sys/class/thermal/thermal_zone0/temp`.
   - **CPU Core Frequency**: Real-time active clock read from `/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq`.
2. All probing executes with zero dynamic heap allocation in < 5.0µs, guaranteeing instantaneous live feedback without latency.

### 2.2 KDE Plasma 6 ToolTip Line Limit & Dense Monospace 6-Line Cyber HUD
1. **KDE Plasma 6 `maximumLineCount: 8` Constraint**:
   - Forensic analysis of `/usr/lib/qt6/qml/org/kde/plasma/core/DefaultToolTip.qml` revealed that KDE Plasma 6's native tooltip component strictly enforces `maximumLineCount: 8`.
   - Tooltips exceeding 8 lines (or lines that soft-wrap due to excessive width) have trailing content abruptly truncated by Kirigami/QtQuick.
2. **6-Line Zero-Wrap Cyber HUD Architecture**:
   - To guarantee 100% telemetry visibility without truncation or soft line-wrapping, the tooltip is compressed into an exact 6-line layout (< 60 chars per line):
     - **Line 1 (HUD Header)**: `⚡ WATTCURB CYBER HUD ● LIVE | ±X.X W`
     - **Line 2 (Battery Domain)**: `• 배터리: XX% [████░░░░] (충전/방전/AC 직결 · 수명 XX% · XXXX RPM)`
     - **Line 3 (CPU Computation)**: `• CPU연산: XX.X W (XX%) [███░░░░░] X.XX GHz XX°C`
     - **Line 4 (GPU & Platform IO)**: `• GPU/IO : X.X W GPU | X.X W IO | XX% C3슬립`
     - **Line 5 (Top Culprits)**: `• 톱소비: #1 process X.X W | #2 process X.X W`
     - **Line 6 (Governor & Audio RT)**: `• 모드/RT: Performance (4.1G 언락) | PipeWire RT(-12)`
3. Compact typography container:
   `<div style="font-family: 'JetBrains Mono', 'Hack', monospace; font-size: 11px; line-height: 1.25;"><font size="2">`

### 2.3 1-Second Reactive Daemon & Tray Event Synchronization
1. `wattcurb.service` default sampling period reduced to 3.0s (window: 1.0s) for continuous live daemon telemetry.
2. `wattcurb-tray` event loop timeout reduced to 1.0s, with sensitive change detection (50mW delta or `seq_version` step) triggering instant `NewToolTip` and `XAyatanaNewLabel` emissions to force immediate desktop cache invalidation.

---

## 3. Verification & Oracle Gate Standards

1. **Unit Test Verification (`tests/test_units.cpp`)**:
   - `test_thinkpower_tray_client()` verifies < 5.0µs render latency, zero heap allocations, monospace container formatting, and multi-domain HUD field consistency.
2. **Live Telemetry Verification**:
   - Tooltip D-Bus query returns live CPU frequency, temperature, and battery power matching real-time `/sys` readings.
