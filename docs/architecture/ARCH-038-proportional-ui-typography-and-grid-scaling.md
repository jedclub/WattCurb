# ARCH-038: Proportional UI Typography & Grid Scaling Architecture

- **Ref-ID**: `REF-ARCH-038`
- **Category**: GUI & Frontend Architecture
- **Title**: Proportional UI Typography, Minimum Accessibility Floor & High-DPI Grid Architecture
- **Status**: Approved
- **Domain**: Qt6/QML, Typography Hierarchy, High-DPI Ergonomics, Grid Alignment
- **Dependencies**: [`REF-ARCH-037`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-037-grid-aligned-dual-domain-ui-architecture.md), [`REF-REQ-062`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-062-dashboard-font-scaling-and-readability-enhancement.md)

---

## 1. System Overview & Typographic Hierarchy

The WattCurb Matrix Dashboard utilizes an expanded btop cyber-terminal typographic system designed to reconcile high information density with effortless legibility at standard laptop/desktop viewing distances.

To prevent eyestrain on modern displays (FHD 1080p, 2K/QHD, and 4K HiDPI), the typographic scale enforces a strict **`11px` accessibility floor** for all secondary data points while expanding primary callouts up to `22px`:

```
+-------------------------------------------------------------------------------------------------------------+
| 1. HEADER BAR (46px)                                                                                        |
| App Brand (16px) | Total Drain (22px bold) | Battery Badge (12px) | Profile Pill (12px) | SYNC Rate (11px)  |
+------------------------------------------------------+------------------------------------------------------+
| 2. MAIN WORKSPACE (Fill Remaining Height: ~555px)                                                           |
|                                                      |                                                      |
| [LEFT COLUMN: Hardware Domains - Width: 460px]       | [RIGHT COLUMN: Process Domain - Width: Remainder]    |
| - Card 1: CPU/Memory (210px)                         | - Top: System Drain Timeline Graph (85px)            |
|   Title: 13px | Live: 18px | Sub: 12px | Met: 11px   |   Title: 12px | Live/Peak: 12px                      |
| - Card 2: Battery Telemetry (180px)                  | - Bottom: btop Process Attribution Matrix Table      |
|   Title: 13px | Remainder: 20px | Sub: 12px | Met: 11px|   Header: 12px (28px height)                         |
| - Card 3: GPU, Display, NVMe Subsystem (Fill)        |   Rows: Comm: 12px bold, Total W: 12px bold,         |
|   Title: 13px | Live: 14-16px | Sub: 11px            |         PSS: 12px, Mechanism: 12px (30px row height) |
+------------------------------------------------------+------------------------------------------------------+
| 3. LOWER POWER SHARE ANALYTICS DECK (Height: 155px)                                                         |
|                                                      |                                                      |
| [CARD A: Device Power Share - Width: 460px]          | [CARD B: Process Power Share - Width: Remainder]     |
| - 100px Donut Canvas + Total Device W (14px)         | - 100px Donut Canvas + Total Process W (14px)        |
| - 1-Column Legend: 11px                              | - 2-Column Grid Legend: 11px                         |
+------------------------------------------------------+------------------------------------------------------+
| 4. BOTTOM CONTROL DOCK (Height: 42px) - 4 Profile Mode Buttons (11px bold), Actions (11px), Close (12px)     |
+-------------------------------------------------------------------------------------------------------------+
```

---

## 2. Structural Grid & Component Dimensions

### 2.1 Vertical Symmetry Invariant ($460\,\text{px}$)
- To accommodate increased font sizes across all hardware metrics (C-state residencies, PMU hardware counters, and ThinkPad electrical values) without awkward horizontal clipping or line wrapping, the hardware column width is expanded:
  $$\text{Width}_{\text{Left}} = 460\,\text{px}$$
- Both the upper hardware column and lower Card A (Device Power Share Donut) are clamped to `460px`, preserving strict $2 \times 2$ vertical grid alignment.

### 2.2 Row Delegate & Spacing Proportions
- **Process Table Delegate Height**: Expanded from `26px` to **`30px`**, guaranteeing $9\,\text{px}$ of vertical breathing room around $12\,\text{px}$ text elements.
- **Process Table Column Widths**:
  - `comm` (Process Name): expanded to `135px`.
  - `tier` (Safety Tier Badge): `45px` (badge: 24x18, font 10px).
  - `totalWatts` (Total Drain): `80px` (font: 12px bold monospace).
  - `ratioPercent` (Energy Contribution): `55px` (font: 11px monospace).
  - `pssMb` (Proportional Set Size): `65px` (font: 12px monospace).
  - `mechanism` (Attribution Logic): `Layout.fillWidth: true` (font: 12px).
- **Lower Deck Height**: Expanded from `145px` to **`155px`**, ensuring the 2-column process legend has adequate margin and padding.

### 2.3 Floating Cyber Inspection Card Adaptation
- **Process Hover Card**:
  - Dimensions: expanded from `440x300` to **`490x340`**.
  - Section headers: `11px bold`.
  - Telemetry grid (CPU, GPU, DRAM, WakeTax, I/O, Fan): `11px monospace`.
  - Scheduler and VFS profiles: `11px`.
- **Hardware Hover Card**:
  - Dimensions: expanded from `380x210` to **`420x230`**.
  - PMU hardware counters and battery chemical parameters: `11px monospace`.
  - Title bars: `12px bold`.

---

## 3. Verification & Performance Invariants

1. **Rendering Latency**:
   - Frame dispatch with full typography remains strictly sub-16ms ($60\text{Hz}$ v-sync compliance).
2. **Invisible Window Zero-Wakeup Invariant**:
   - When the dashboard window is closed, QML timer updates, Canvas redraws, and tooltip animations cease completely, maintaining $0\%$ daemon CPU usage and zero wakeups.
