# REF-REQ-083: Process Full Name Resolution, Command Line Extraction & Interactive Tooltip Architecture

- **Status**: Approved
- **Ref ID**: `REF-REQ-083`
- **Related Requirements**: [`REF-REQ-078`](REQ-078-battery-drain-deep-audit-report-and-window.md), [`REF-REQ-081`](REQ-081-tray-battery-report-action-and-tactile-buttons.md), [`REF-REQ-082`](REQ-082-granular-platform-loss-hardware-telemetry.md)
- **Related Architecture**: [`REF-ARCH-060`](../architecture/ARCH-060-process-full-name-cmdline-and-interactive-tooltips.md)
- **Created**: 2026-09-20
- **Category**: UI Ergonomics, Diagnostics, Process Identification

---

## 1. Background & Problem Statement

In the Battery Drain Deep Audit Report (`BatteryReportWindow.qml`), process culprits were displayed in a fixed 160px column. Because Linux kernel `comm` strings can reach 16 bytes and PID formatting `(12345)` occupies significant horizontal space, process names were frequently truncated with ellipses (`...` via `elide: Text.ElideRight`). 

For critical processes such as `Isolated Web Co` (Firefox Web Content), `baloo_file_extr` (KDE Baloo File Extractor), or `systemd-oomd`, users could not easily ascertain which exact application was responsible for battery drain without manual terminal queries. Furthermore, hovering over the truncated text provided no tooltip.

---

## 2. Requirements & Functional Specifications

### 2.1 Full Process Name & Command Line Resolution (`resolve_proc_full_info`)
1. When generating battery drain reports (`BatteryHistoryAnalyzer::analyze`), the analyzer MUST dynamically resolve the full binary name and command line arguments from `/proc/<pid>/cmdline`:
   - If `/proc/<pid>/cmdline` exists and contains binary arguments, extract the basename of `argv[0]`.
   - For script interpreters (`python3`, `node`, `bash`, `sh`, `perl`, `ruby`), inspect subsequent arguments and synthesize an informative descriptor: e.g. `python3 (harness.py)`.
   - For disambiguated browser processes, translate generic strings (e.g. `Isolated Web Co` -> `firefox (Web Content)`).
   - If the process has terminated (historical PID) or cmdline is inaccessible, fall back gracefully to the cached kernel `comm`.
2. Total execution latency for on-demand resolution across the top 12 culprits MUST NOT exceed $50\mu\text{s}$.

### 2.2 Table Column Expansion & Visual Ergonomics
1. Expand the Process Name & PID column from 160px to **200px** in both the table header and row delegate to eliminate truncation for standard process identifiers.
2. The process name text MUST dynamically highlight in Cyan (`#00d2ff`) on mouse hover.

### 2.3 Rich Interactive Cyber ToolTip Overlay
1. An interactive `ToolTip` MUST be attached to the process name cell:
   - **Trigger**: Mouse hover (`hoverEnabled: true`, cursor shape: `Qt.PointingHandCursor`).
   - **Appearance**: Dark glassmorphic background (`#0f172a`), 1.5px Cyan border, rounded corners, subtle inner glow.
   - **Content**:
     - Line 1: Monospace bold Full Name + PID in high-contrast orange.
     - Line 2: Full command-line invocation string (`cmdline`), wrapped with maximum width constraints.
     - Line 3: Hardware attribution domain & estimated drain (Wh and % share).
     - Line 4: Trigger mechanism & recommended mitigation action.
2. In the clipboard markdown export (`to_markdown()`), output the resolved full process name instead of truncated strings.
