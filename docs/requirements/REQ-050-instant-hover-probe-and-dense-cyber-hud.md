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

### 2.2 Fixed-Prefix Progressive Bar & `<nobr>` Zero-Wrap Cyber HUD
1. **Fixed-Column Progressive Bar Alignment**:
   - To eliminate erratic horizontal jumping caused by varying numerical string lengths (e.g. `5.0 W (26%)` vs `14.2 W (100%)`), the progressive bar is placed immediately after a fixed-width 4-character label (`BAT `, `CPU `, `GPU `, `TOP `, `SYS `).
   - The progressive bar (`[████░░░░]`) is anchored strictly at column 5 across all telemetry rows, with numerical metrics and secondary details positioned cleanly after the bar.
2. **KDE Plasma `<nobr>` Anti-Wrap Guarantee**:
   - Every row is wrapped in `<nobr>...</nobr>` tags, preventing Qt/Kirigami's `Text.Wrap` engine from soft line-breaking.
   - Total character length per line is restricted to under 38 characters to ensure comfortable rendering even within narrow system tray popups.
3. **6-Line Layout Architecture**:
   - **Line 1 (HUD Header)**: `⚡ WATTCURB CYBER HUD ● LIVE | ±X.X W`
   - **Line 2 (Battery Domain)**: `BAT [██████░░] XX% · 충전 중/방전/완충 직결 · XXXXrpm`
   - **Line 3 (CPU Computation)**: `CPU [██░░░░░░] XX% · X.X W · X.XXGHz XX°C`
   - **Line 4 (GPU & Platform IO)**: `GPU [████░░░░] X.X W · C3 XX% · IO X.X W`
   - **Line 5 (Top Culprits)**: `TOP #1_comm X.X W · #2_comm X.X W`
   - **Line 6 (Governor & Audio RT)**: `SYS Profile_Name | PipeWire RT(-12)`
4. Compact typography container:
   `<div style="font-family: 'JetBrains Mono', 'Hack', monospace; font-size: 11px; line-height: 1.35;"><font size="2">`

### 2.3 1-Second Reactive Daemon & Tray Event Synchronization
1. `wattcurb.service` default sampling period reduced to 3.0s (window: 1.0s) for continuous live daemon telemetry.
2. `wattcurb-tray` event loop timeout reduced to 1.0s, with sensitive change detection (50mW delta or `seq_version` step) triggering instant `NewToolTip` and `XAyatanaNewLabel` emissions to force immediate desktop cache invalidation.

---

## 3. Verification & Oracle Gate Standards

1. **Unit Test Verification (`tests/test_units.cpp`)**:
   - `test_thinkpower_tray_client()` verifies < 5.0µs render latency, zero heap allocations, monospace container formatting, and multi-domain HUD field consistency.
2. **Live Telemetry Verification**:
   - Tooltip D-Bus query returns live CPU frequency, temperature, and battery power matching real-time `/sys` readings.
