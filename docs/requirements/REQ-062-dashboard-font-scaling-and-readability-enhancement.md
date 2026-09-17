# REQ-062: GUI Dashboard Font Scaling & Ergonomic Readability Enhancement

- **Ref-ID**: `REF-REQ-062`
- **Category**: GUI Dashboard Ergonomics & Visual Accessibility
- **Title**: Global Font Scaling, Proportional Grid Adaptation, and Ergonomic Readability Enhancement
- **Status**: Approved
- **Domain**: Qt6/QML, Desktop Ergonomics, Visual Accessibility, High-DPI Readability
- **Dependencies**: [`REF-REQ-036`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-036-matrix-dashboard-and-cyber-hud.md), [`REF-REQ-037`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-037-dense-btop-telemetry-payload.md), [`REF-REQ-060`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-060-circular-power-share-visualization.md), [`REF-REQ-061`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-061-ui-layout-restructuring-and-bottom-power-share-deck.md)

---

## 1. Problem Statement & Design Rationale

While the WattCurb btop-style power dashboard provided exceptional information density, its reliance on micro-typography (8px, 9px, and 10px fonts) created readability and eye-strain issues, particularly on high-resolution displays (Full HD 1080p+, QHD, and HiDPI laptop screens). Secondary labels, C-state residencies, process metrics (PID, PSS, mechanism), and hover inspection popups were challenging to read at normal desktop viewing distances.

To dramatically enhance visual accessibility, glanceability, and comfort without sacrificing the cyber-terminal aesthetic or layout balance, a comprehensive font scale-up and proportional grid expansion must be executed across all dashboard components.

---

## 2. Functional Requirements

### 2.1 Global Font Scaling Matrix (`REF-REQ-062-F1`)
1. **Accessibility Floor (Minimum 11px)**:
   - All secondary labels, C-state residencies, attribution mechanisms, process safety tiers, and donut legends are scaled up from `8px–10px` to **`11px`**.
2. **Column Headers & Sub-Headings**:
   - Hardware card sub-headings and process table column headers are scaled up from `10px–11px` to **`12px–13px`**.
3. **Primary App & Section Titles**:
   - App title, hardware card titles, and matrix section titles are scaled up to **`13px–16px`**.
4. **Primary Wattage & Telemetry Callouts**:
   - Header system power drain: scaled to **`22px`**.
   - Battery remaining percentage: scaled to **`20px`**.
   - CPU Package / GPU Live power: scaled to **`16px–18px`**.
   - Process row total power: scaled to **`12px bold`**.
5. **Hover Inspection Card (Floating Cyber Card)**:
   - All internal telemetry (PMU hardware counters, chemical battery specs, scheduler profiles, and 3-column power decomposition grid) scaled from `9px–10px` to **`11px–13px`**.

---

### 2.2 Proportional Grid & Layout Geometry Adaptation (`REF-REQ-062-F2`)
To prevent text truncation, clipping, or uncoordinated wrapping caused by larger font metrics, all containing containers and column layouts are proportionally expanded:
1. **Application Window Bounds**:
   - Default dimensions expanded from `1260x800` to **`1280x840`**.
   - Minimum window bounds expanded from `1040x680` to **`1080x720`**.
2. **Header Bar**:
   - Height increased from `42px` to **`46px`**.
3. **Hardware Left Column & Device Donut Alignment**:
   - Left column width expanded from `430px` to **`460px`**.
   - Hardware Card 1 (CPU/Memory) height expanded to **`210px`**.
   - Hardware Card 2 (Battery BAT0) height expanded to **`180px`**.
   - Lower Deck Card A (Device Share Donut) width expanded to **`460px`** to maintain 100% vertical grid alignment with the upper hardware column.
4. **Process Attribution Matrix Table**:
   - Column header row height increased from `24px` to **`28px`**.
   - Process row delegate height increased from `26px` to **`30px`**, guaranteeing generous vertical padding around 12px text.
   - Column layout widths expanded: Comm (`135px`), Safety Tier (`45px`), Total W (`80px`), Share % (`55px`), PSS (`65px`), Mechanism (`Layout.fillWidth: true`).
5. **Lower Power Share Analytics Deck**:
   - Height increased from `145px` to **`155px`**, providing comfortable padding for 11px donut legends.
6. **Bottom Control Dock**:
   - Height increased from `38px` to **`42px`**, button fonts scaled to `11px bold`.
7. **Floating Cyber Inspection Card**:
   - Process hover card expanded from `440x300` to **`490x340`**.
   - Hardware hover card expanded from `380x210` to **`420x230`**.

---

## 3. Verification & Oracle Gate Standards (`REF-TEST-027`)

1. **Zero Text Clipping & Overflow Invariant**:
   - All text components, especially fixed-width table cells and legend badges, must render completely without horizontal or vertical clipping.
2. **Strict Accessibility Floor Assertion**:
   - Zero text elements across the entire dashboard may render at less than `11px` (with the single exception of the compact `T0`–`T5` safety badge at `10px`).
3. **Continuous Grid Alignment Assertion**:
   - The boundary between Left Column and Right Column must align pixel-perfectly with the boundary between Lower Card A (Device Donut) and Lower Card B (Process Donut) at exactly `460px`.
4. **Performance & Invisible Zero-Wakeup Guarantee**:
   - Font scaling and dimension adjustments must cause zero regression in rendering latency (< 16ms frame dispatch) and maintain zero background CPU wakeups when the dashboard window is closed.
