# REF-ARCH-055: Deep Battery Drain Analytics & Standalone Report Architecture

## 1. Architectural Topology Overview
This document specifies the architectural topology of the **Deep Battery Drain Analytics Engine** and the **Standalone Report Window (`BatteryReportWindow`)**, implementing [`REF-REQ-078`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-078-deep-battery-drain-report-window.md).

```
+-----------------------------------------------------------------------------------------+
|                                     Root Daemon                                         |
|  +---------------------------+       10s Cadence        +----------------------------+  |
|  | HardwareProbe / RAPL /    | -----------------------> | HistoryRingBufferShm       |  |
|  | Battery Sysfs Sensors     |                          | (/dev/shm/wattcurb_history)|  |
|  +---------------------------+                          +----------------------------+  |
|                                                                        |                |
+------------------------------------------------------------------------|----------------+
                                                                         | Lockless Seqlock
                                                                         v
+-----------------------------------------------------------------------------------------+
|                                 BatteryHistoryAnalyzer                                  |
|  +-----------------------------------------------------------------------------------+  |
|  | - Single-pass contiguous sweep across up to 60,480 samples (~1.85 MiB)            |  |
|  | - Filter Discharging Segments (battery_state == 1)                                |  |
|  | - Numerical Trapezoidal Integration: E_total = \sum (W * 10s) / 3600               |  |
|  | - Physical Hardware Decomposition (CPU, GPU, Display, NVMe, Platform/WiFi)        |  |
|  | - Correlation with Top Process Attributed Power Telemetry                         |  |
|  | - Deep Sleep C3+ Residency Audit & Energy Waste Ratio (EWR) Evaluation            |  |
|  +-----------------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------------+
                                         |
                                         | In-Memory Summary Structs & Q_PROPERTY Models
                                         v
+-----------------------------------------------------------------------------------------+
|                              DashboardBackend (Qt6 Bridge)                              |
|  +-------------------------------------+   +-----------------------------------------+  |
|  | batteryReportSummary (QVariantMap)  |   | batteryReportProcessCulprits (QList)    |  |
|  | batteryReportHardwareShares (QList) |   | batteryReportTimeline (QList)           |  |
|  +-------------------------------------+   +-----------------------------------------+  |
+-----------------------------------------------------------------------------------------+
                    |                                                    |
                    v (show / invoke)                                    v (show / invoke)
+----------------------------------------+           +------------------------------------+
|    Main DashboardWindow.qml            |           |   Standalone BatteryReportWindow   |
|   [⚡ 배터리 정밀 분석 리포트 Button]   | --------> |   (Dedicated Floating Window)      |
|   [Battery Card Deep Analysis Link]    |           |   - Executive Summary Cards        |
|                                        |           |   - Hardware Domain Breakdown Bars |
|                                        |           |   - Top Drain Culprits Table       |
|                                        |           |   - Diagnostic Recommendations     |
+----------------------------------------+           +------------------------------------+
```

---

## 2. Core Components

### 2.1 `BatteryHistoryAnalyzer` (`src/report/battery_history_analyzer.hpp` & `.cpp`)
The core analytics engine operates on a lockless snapshot of `HistoryRingBufferShm`:
- **`analyze_history(const HistoryPoint* points, size_t count, const std::vector<ProcessAttributedPower>& procs)`**:
  - Linear scan across contiguous memory: cache friendly, zero cache-miss thrashing.
  - Accumulates:
    $$E_{\text{total\_wh}} = \sum_{i \in \text{discharging}} \frac{P_{\text{sys\_mw}}[i]}{1000.0} \times \frac{10}{3600}$$
    $$E_{\text{cpu\_wh}} = \sum_{i \in \text{discharging}} \frac{P_{\text{cpu\_mw}}[i]}{1000.0} \times \frac{10}{3600}$$
    $$E_{\text{gpu\_wh}} = \sum_{i \in \text{discharging}} \frac{P_{\text{gpu\_mw}}[i]}{1000.0} \times \frac{10}{3600}$$
    $$E_{\text{display\_wh}} = \sum_{i \in \text{discharging}} P_{\text{display\_w}}[i] \times \frac{10}{3600}$$
    $$E_{\text{nvme\_wh}} = \sum_{i \in \text{discharging}} P_{\text{nvme\_w}}[i] \times \frac{10}{3600}$$
    $$E_{\text{platform\_wh}} = E_{\text{total\_wh}} - (E_{\text{cpu\_wh}} + E_{\text{gpu\_wh}} + E_{\text{display\_wh}} + E_{\text{nvme\_wh}})$$
- **Process Culprit Attribution**:
  - Ingests cached daemon process telemetry.
  - Combines process instantaneous attribution shares with cumulative discharge energy to compute estimated drain energy ($Wh$), $W_{avg}$, and ranks processes by impact.
- **Diagnostics Generation**:
  - Checks if deep sleep residency (C3+ percentage) $< 60\% \implies$ flags "Excessive Wakeup Interrupts".
  - Checks if GPU silicon is active during background tasks $\implies$ flags "Unthrottled GPU Compositor/Browser Loop".
  - Proposes mitigation steps.

### 2.2 Dashboard C++ Bridge (`DashboardBackend`)
- Exposes:
  - `batteryReportSummary`: Map containing:
    - `totalDischargeWh`, `totalDischargeMah`, `totalDischargeJoules`
    - `dischargeDurationSec`, `dischargeDurationStr`
    - `avgDischargeWatts`, `peakDischargeWatts`, `peakDischargeTime`
    - `c3PercentAvg`, `samplesAnalyzed`, `batteryDropPercent`
    - `primaryCulpritName`, `primaryCulpritDomain`, `recommendationText`
  - `batteryReportHardwareShares`: List of domain share maps (`name`, `icon`, `wh`, `watts`, `percent`, `color`, `tip`).
  - `batteryReportProcessCulprits`: List of process culprit maps (`rank`, `pid`, `comm`, `domain`, `wh`, `watts`, `sharePercent`, `wdiScore`, `mechanism`, `action`).
  - `openBatteryReport()`: Instantiates and reveals the standalone window.
  - `exportBatteryReportText()`: Generates a complete diagnostic report in markdown format and copies it to the system clipboard.

### 2.3 Standalone QML Architecture (`BatteryReportWindow.qml`)
- Configured as an independent `Window` component:
  - `flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowMinMaxButtonsHint | Qt.WindowCloseButtonHint`
  - Dimensions: default $1120 \times 740$, resizable down to $960 \times 640$.
  - Includes a visual scrollable layout with 3 main sections:
    1. **Header & Executive Summary**: Period, total $Wh$, duration, peak draw, and 4 KPI cards.
    2. **Hardware Domain Breakdown Grid**: Visual bar meters comparing CPU, GPU, Display, Storage, and Platform loss.
    3. **Top Drain Culprits Table**: Rich table displaying processes with hover tooltips detailing exact causation mechanisms.
    4. **Diagnostic Recommendations Box**: Plain-text summary with actionable optimization buttons.

---

## 3. Performance & Memory Efficiency Verification
- Analysis computation time: $\le 5\text{ ms}$ for 60,480 points.
- Working set allocation: $< 2\text{ MiB}$ stack/arena buffer during analysis; zero ongoing background memory allocations.
- Qt6 UI bindings: Computed on-demand when the report is requested or refreshed, ensuring zero overhead during normal background operation.
