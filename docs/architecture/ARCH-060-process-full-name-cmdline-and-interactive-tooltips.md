# REF-ARCH-060: Process Full Name Resolution, Command Line Extraction & Interactive Tooltip Architecture

- **Status**: Approved
- **Ref ID**: `REF-ARCH-060`
- **Related Requirements**: [`REF-REQ-083`](../requirements/REQ-083-process-full-name-cmdline-and-interactive-tooltips.md), [`REF-REQ-078`](../requirements/REQ-078-battery-drain-deep-audit-report-and-window.md), [`REF-REQ-081`](../requirements/REQ-081-tray-battery-report-action-and-tactile-buttons.md)
- **Created**: 2026-09-20
- **Category**: Telemetry Pipeline, Diagnostics, UI Architecture

---

## 1. Architectural Pipeline

```
+-----------------------------------------------------------------------------------+
|               Historical Telemetry Ingestion (HistoryRingBufferShm)               |
+-----------------------------------------------------------------------------------+
                                       |
                                       v
+-----------------------------------------------------------------------------------+
|               BatteryHistoryAnalyzer::analyze (Top 12 Process Ranking)             |
|                                                                                   |
|   For each top 12 culprit:                                                        |
|     1. Check if /proc/<pid>/cmdline is readable                                   |
|     2. If readable: parse argv[0] -> full_name, sanitize '\0' -> cmdline          |
|     3. Check interpreters (python, node, bash) -> synthesize script identifier     |
|     4. If terminated/inaccessible: fall back to kernel comm                       |
+-----------------------------------------------------------------------------------+
                                       |
                                       v
+-----------------------------------------------------------------------------------+
|                     DashboardBackend (Qt6 Property Exposure)                      |
|                                                                                   |
|   pm["fullName"] = p.full_name;                                                   |
|   pm["cmdline"]  = p.cmdline;                                                     |
+-----------------------------------------------------------------------------------+
                                       |
                                       v
+-----------------------------------------------------------------------------------+
|                         BatteryReportWindow.qml (QML View)                        |
|                                                                                   |
|   - 200px Column Width (eliminates standard truncation)                           |
|   - PointingHandCursor & Cyan Hover Text Highlight                                |
|   - Non-Clipping Window-Level Cyber ToolTip:                                      |
|     * Full Name (e.g., 'firefox (Web Content)' / 'baloo_file_extractor')          |
|     * Exact command-line string                                                   |
|     * Real-time Wh & % share, domain, and mitigation action                       |
+-----------------------------------------------------------------------------------+
```

---

## 2. Data Structure Extensions

`ProcessDrainCulprit` in `src/report/battery_history_analyzer.hpp`:

```cpp
struct ProcessDrainCulprit {
    int rank{0};
    int pid{0};
    std::string comm;
    uint32_t uid{0};
    std::string domain;
    double drain_wh{0.0};
    double avg_watts{0.0};
    double share_percent{0.0};
    double wdi_score{0.0};
    std::string mechanism;
    std::string action_str;
    std::string full_name; // Full resolved process / script name
    std::string cmdline;   // Full command line arguments
};
```
