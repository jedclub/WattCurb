# REQ-061: UI Layout Restructuring - Dedicated Bottom Power Share Analytics & Independent Domain Matrices

- **Ref-ID**: `REF-REQ-061`
- **Category**: GUI Dashboard & Ergonomics Layout Architecture
- **Title**: Decoupled Dual-Domain Matrix Tables & Grid-Aligned Bottom Power Share Analytics Deck
- **Status**: Approved
- **Domain**: Qt6/QML, Desktop Ergonomics, Canvas 2D, Power Attribution Deck
- **Dependencies**: [`REF-REQ-036`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-036-matrix-dashboard-and-cyber-hud.md), [`REF-REQ-037`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-037-dense-btop-telemetry-payload.md), [`REF-REQ-060`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-060-circular-power-share-visualization.md), [`REF-ARCH-036`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-036-dual-domain-power-share-decomposition.md)

---

## 1. Problem Statement & Design Rationale

In the previous iteration ([`REF-REQ-060`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-060-circular-power-share-visualization.md)), the newly introduced circular power share (donut) graphs were placed directly between the top Timeline Waveform and the Top Process Attribution Matrix Table. This caused two primary ergonomic and layout deficiencies:
1. **Vertical Crowding & Process Matrix Squishing**: The tabular process list was compressed to under 200px in height, allowing only 4–5 processes to be visible simultaneously without continuous vertical scrolling.
2. **Domain Inversion**: Hardware device power shares and software process power shares were co-located inside the right software-focused column, destroying the strict spatial separation between the **Hardware Domain (Left)** and the **Process Domain (Right)**.

To restore visual clarity, maximum information density, and intuitive ergonomics, the circular share graphs must be repositioned to a dedicated **Bottom Analytics Deck**, while restoring both the **Device List / Hardware Cards** and the **Process List / Attribution Table** to independent, full-height workspaces.

---

## 2. Functional Requirements

### 2.1 Spatial Decoupling & Independent Domain Matrices (`REF-REQ-061-D1`)
1. **Independent Hardware Domain (Left Column, Width: 430px)**:
   - Full dedicated vertical height dedicated to hardware cards:
     - Card 1: CPU & Memory Subsystem (RAPL Package, Core, Uncore, DRAM, C-States, Fan RPM, Real-time Sparkline).
     - Card 2: Battery & Electrical Telemetry (BAT0 fuel gauge, voltage, current, cycles, wear, Discharge Sparkline).
     - Card 3: GPU Silicon, Display Brightness & NVMe Storage Subsystem (APST states, I/O rates, Load Sparkline).
2. **Independent Process Domain (Right Column, Fill Remainder)**:
   - Full dedicated vertical height dedicated to software power attribution:
     - Top Waveform: System Total Power Drain Timeline (Height: 80px).
     - Process Attribution Matrix Table: Full remaining height (~450px+), rendering 14–16 top processes simultaneously without squishing or obstruction.

### 2.2 Grid-Aligned Lower Power Share Analytics Deck (`REF-REQ-061-D2`)
1. **Positioning**: Located immediately below the Main Workspace and directly above the Bottom Control Dock.
2. **Grid Alignment & Proportions**:
   - **Card A (Hardware Device Share Donut)**:
     - Fixed Width: 430px (strictly aligned with the Left Hardware Column above).
     - Contains: 100px Donut Canvas, Center Text (Total Device Watts), and 1-column interactive legend (CPU Core, Uncore, DRAM, GPU, Display, Storage).
   - **Card B (Process Share Donut)**:
     - Width: `Layout.fillWidth: true` (strictly aligned with the Right Process Column above).
     - Contains: 100px Donut Canvas, Center Text (Total Process Watts), and a spacious **2-Column Grid Legend** (`GridLayout`, columns: 2) utilizing the wide horizontal real estate to cleanly display Top 5 processes + Other processes.
3. **Fixed Deck Height**: Constrained to `145px` to maintain ample room for the main matrices while ensuring zero text truncation.

---

## 3. Verification & Oracle Gate Standards (`REF-TEST-026`)

1. **Compilation & Resource Integrity**:
   - QML syntax, Canvas binding, and signal connections (`onPowerSharesChanged`, `onHistoryChanged`) must compile cleanly with `rcc` and produce zero runtime warnings.
2. **Ergonomic Headroom & Process Table Visibility**:
   - On a standard 1260x800 dashboard window, the Process Attribution ListView must exhibit a vertical viewport of at least **400px**, displaying at least 14 process rows simultaneously.
3. **Zero Resource Overhead Invariant**:
   - Deck rendering must strictly observe the invisible window freeze rule (0% CPU, 0 wakeups when hidden).
