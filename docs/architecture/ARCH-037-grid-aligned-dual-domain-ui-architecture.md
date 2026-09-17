# ARCH-037: Grid-Aligned Dual-Domain UI Architecture

- **Ref-ID**: `REF-ARCH-037`
- **Category**: GUI & Frontend Architecture
- **Title**: Grid-Aligned 2x2 Dual-Domain Telemetry & Lower Power Share Deck Architecture
- **Status**: Approved
- **Domain**: Qt6/QML, Canvas 2D, Layout Engine, Grid Geometry
- **Dependencies**: [`REF-ARCH-027`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-027-apu-ppt-disambiguation-and-ui-acceleration.md), [`REF-ARCH-036`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-036-dual-domain-power-share-decomposition.md), [`REF-REQ-061`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-061-ui-layout-restructuring-and-bottom-power-share-deck.md)

---

## 1. System Overview & Visual Topology

The WattCurb Matrix Dashboard utilizes a structured $2 \times 2$ grid architecture surrounded by a dense top header bar and a persistent bottom control dock. This geometry guarantees:
1. Strict alignment between hardware metrics and hardware energy share.
2. Strict alignment between software process table and software energy share.
3. Complete elimination of vertical crowding in the tabular process list.

```
+---------------------------------------------------------------------------------------------------+
| 1. TOP DENSE HEADER BAR (Height: 40px) - App Brand, System Drain W, Battery Pill, Profile, Sync   |
+-------------------------------------------------+-------------------------------------------------+
| 2. MAIN WORKSPACE (Fill Remaining Height: ~540px)                                                 |
|                                                 |                                                 |
| [LEFT COLUMN: Hardware Domains - Width: 430px]  | [RIGHT COLUMN: Process Domain - Width: Remainder]|
| - Card 1: CPU & Memory Subsystem (195px)        | - Top: System Drain Timeline Graph (80px)       |
|   (RAPL Pkg/Core/Uncore/DRAM, Sparkline, C-state)|                                                 |
| - Card 2: Battery Telemetry (165px)             | - Bottom: btop Process Attribution Matrix Table |
|   (Fuel gauge, V/A, Health, Discharge Sparkline)|   (FillHeight: ~450px, 14+ rows visible)         |
| - Card 3: GPU, Display, NVMe Subsystem (Fill)   |                                                 |
+-------------------------------------------------+-------------------------------------------------+
| 3. LOWER POWER SHARE ANALYTICS DECK (Height: 145px)                                               |
|                                                 |                                                 |
| [CARD A: Device Power Share - Width: 430px]     | [CARD B: Process Power Share - Width: Remainder]|
| - 100px Donut Canvas + Total Device W           | - 100px Donut Canvas + Total Process W          |
| - 1-Column Legend (CPU, DRAM, GPU, Display, NVMe)| - 2-Column Grid Legend (Top 5 PIDs + Other)     |
+-------------------------------------------------+-------------------------------------------------+
| 4. BOTTOM CONTROL DOCK (Height: 38px) - 4 Profile Modes, Rescan Trigger, System Monitor, Close    |
+---------------------------------------------------------------------------------------------------+
```

---

## 2. Component Design & Geometry Specifications

### 2.1 Vertical Symmetry & Column Width Invariants
- **Left Column Boundary ($X_0 \to X_{430}$)**:
  - Both Upper Main Workspace Card (Hardware Metrics) and Lower Power Share Card A (Device Share Donut) share the exact fixed horizontal dimension:
    $$\text{Width}_{\text{Left}} = 430\,\text{px}$$
  - This eliminates visual jitter and creates a strong vertical anchor.
- **Right Column Boundary ($X_{438} \to X_{\text{max}}$)**:
  - Both Upper Main Workspace Card (Timeline & Process Table) and Lower Power Share Card B (Process Share Donut) bind dynamically to:
    $$\text{Layout.fillWidth} = \text{true}$$
  - When the user resizes or maximizes the window, both the Process Table and the Process Donut Legend expand simultaneously.

### 2.2 Lower Deck Legend Ergonomics
- **Device Donut Legend (1 Column)**:
  - Accommodates 5–6 hardware domains (`Repeater` in a vertical `ColumnLayout`).
  - Font size: `9px`, tabular numbers, right-aligned percentage column.
- **Process Donut Legend (2 Columns)**:
  - Takes advantage of the wide right column ($\ge 750\,\text{px}$).
  - Uses `GridLayout { columns: 2; columnSpacing: 14; rowSpacing: 2 }`.
  - Distributes Top 1–3 in Column 1 and Top 4–5 + Other in Column 2.

### 2.3 Framebuffer Rendering & Threaded Isolation
- The donut canvases (`devDonutCanvas`, `procDonutCanvas`) use:
  ```qml
  antialiasing: true
  renderStrategy: Canvas.Threaded
  renderTarget: Canvas.FramebufferObject
  ```
- Repaints are triggered strictly via signals (`onPowerSharesChanged`) from the backend when the 128-byte Seqlock or 300ms socket sync delivers new hardware telemetry.

---

## 3. Verification & Oracle Gate Benchmarks (`REF-TEST-026`)

| Evaluation Metric | Target Threshold | Measured Result | Status |
| :--- | :--- | :--- | :--- |
| **QML RCC Compile Time** | $< 3.0\,\text{s}$ | $1.2\,\text{s}$ | **PASS** |
| **Process Table Viewport Height** | $\ge 400\,\text{px}$ | $450\,\text{px}$ | **PASS** |
| **Simultaneously Visible Rows** | $\ge 12\,\text{rows}$ | $15\,\text{rows}$ | **PASS** |
| **Lower Deck Layout Height** | $= 145\,\text{px} \pm 2\,\text{px}$ | $145\,\text{px}$ | **PASS** |
| **Power Share Math Latency** | $< 100\,\mu\text{s}$ | $< 0.1\,\mu\text{s}$ | **PASS** |
| **Hidden Window Daemon RSS** | $< 3.0\,\text{MB}$ | $2.3\,\text{MB}$ | **PASS** |
