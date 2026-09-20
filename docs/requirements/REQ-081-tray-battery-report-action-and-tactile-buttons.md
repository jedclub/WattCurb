# REF-REQ-081: Tray Battery Report Action & Tactile Cyber Button UX

## 1. Overview & Context

- **REF-ID**: `REF-REQ-081`
- **Related Requirements**: [`REF-REQ-078`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-078-battery-drain-deep-audit-report-and-window.md), [`REF-REQ-076`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-076-multilingual-l10n-matrix-and-auto-system-locale.md), [`REF-REQ-025`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-025-desktop-tray-and-bidirectional-control.md)
- **Target Components**: StatusNotifierItem Tray Client (`wattcurb-tray`), QML Matrix Dashboard (`DashboardWindow.qml`), Standalone Battery Report Window (`BatteryReportWindow.qml`)

WattCurb provides deep historical battery drain auditing to diagnose which hardware components and runaway background processes consume energy. Users require immediate access to this report directly from the desktop tray icon without needing to navigate through the dashboard first. Furthermore, dark UI mode requires high-contrast, visually distinct button boundaries with responsive hover illumination and physical depression feedback to ensure an intuitive and tactile user experience.

---

## 2. Functional Requirements

### 2.1 Dedicated Tray Menu Action for Battery Report
- The StatusNotifierItem (SNI) DBusMenu layout MUST include a dedicated action item for opening the Battery Precision Analysis Report (`ACTION_OPEN_BATTERY_REPORT`):
  - English: `🔋 Open Battery Drain Report`
  - Korean: `🔋 배터리 정밀 분석 리포트 열기 (Battery Audit Report)`
  - Fully translated across all 13 supported languages.
- Clicking the tray menu item MUST invoke `wattcurb-dashboard --report` non-blockingly, instantly launching the standalone Deep Battery Drain Telemetry Audit window.

### 2.2 Tactile Cyber Button Component (`TactileButton`)
- All interactive buttons across `DashboardWindow.qml` and `BatteryReportWindow.qml` MUST provide clear visual demarcation and tactile feedback:
  1. **Visual Demarcation**: Distinct surface color (`#1a222e`) with clear border line (`#37475d`, width 1.0~1.5) and 6px rounded corners.
  2. **Top/Bottom 3D Bevel Highlights**: 1px subtle top highlight (`Qt.rgba(1, 1, 1, 0.25)` on hover) and dark bottom shadow (`Qt.rgba(0, 0, 0, 0.4)`) to deliver depth.
  3. **Hover Illumination**: Surface brightens to `#263345`, border lights up with domain accent color (Cyan, Green, Orange, Purple, or Red), and mouse cursor switches to `Qt.PointingHandCursor`.
  4. **Physical Click Depression (Tactile Press)**:
     - On mouse down (`down`), scale down smoothly to `0.95` (`Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutQuad } }`).
     - Background sinks to darker shade (`#0e1520`).
     - Content text translates vertically by +1px (`y: down ? 1 : 0`).
  5. **Selected/Active Profile State**: Glowing tinted accent fill (`Qt.rgba(accent.r, accent.g, accent.b, 0.32)`) with high-contrast accent border.
