# REQ-039: Desktop Tray Icon Rich-Text Hover Telemetry Card

- **Requirement ID**: `REF-REQ-039`
- **Related Requirements**: `REF-REQ-035` (SNI Desktop Tray), `REF-REQ-038` (Hover Telemetry), `REF-ARCH-025` (Tray Architecture)
- **Status**: IMPLEMENTED
- **Target OS**: Linux (KDE Plasma 6 Wayland/X11, FreeDesktop StatusNotifierItem)

---

## 1. Executive Summary & Problem Statement

### 1.1 Problem Statement
1. The default StatusNotifierItem (SNI) tooltip on the desktop taskbar panel only provided 5 lines of plain, unstyled ASCII text.
2. Users hovering over the tray icon could not immediately distinguish physical power domains or quickly evaluate battery health and real-time culprit processes.
3. The formatting lacked visual hierarchy, color coding, and Korean localized system status.

### 1.2 Mission Objectives
1. **KDE Plasma Rich-Text HTML Card**: Format the SNI `ToolTip` description into structured Qt Rich-Text HTML with vibrant cyber styling, headers, dividers (`<hr>`), and bold color-coded telemetry values.
2. **Exhaustive Core Telemetry Scope**:
   - Title: System drain wattage ($W$) and battery charging/discharging state with high contrast emoji indicators (`⚡`).
   - Battery: Percentage, charging state, health retention (%), and localized remaining runtime projection (hours/minutes).
   - Electrical & Power Breakdown: Real-time total system drain ($W$), CPU package power ($W$), core temp (°C), cooling fan RPM, GPU silicon power ($W$), and C3 Deep Sleep residency (%).
   - Wakeup Overhead: System-wide wakeups per second.
   - Real-Time Dominant Culprits: Top 1 and Top 2 runaway processes with attributed wattage ($W$), PID, and safety tier ($T0..T5$).
   - Operational Mode: Active power profile name (Performance / Balanced / SmartSave / UltraSave) and count of active mitigation gates.
3. **Zero-Overhead Stack Formatting**:
   - Maintain pure stack-based string formatting (`std::snprintf` into a 2048-byte stack buffer) without introducing dynamic heap allocations or D-Bus latency.
