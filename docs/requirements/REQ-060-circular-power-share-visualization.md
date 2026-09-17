# REQ-060: Circular Power Share Visualization (Device & Process Donut Charts)

- **Ref-ID**: `REF-REQ-060`
- **Category**: GUI Dashboard, Data Visualization & Power Attribution
- **Title**: Dual-Domain Circular Power Share Visualization (Hardware Devices & Top Processes)
- **Status**: Approved
- **Domain**: Qt6/QML, Canvas 2D, Power Attribution, Zero-Allocation Serialization
- **Dependencies**: [`REF-REQ-036`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-036-matrix-dashboard-and-cyber-hud.md), [`REF-REQ-037`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-037-dense-btop-telemetry-payload.md), [`REF-ARCH-027`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-027-apu-ppt-disambiguation-and-ui-acceleration.md)

---

## 1. Problem Statement & User Experience Objectives

While the Matrix Dashboard currently displays tabular process lists and hardware cards, users require an instantaneous, high-level visual breakdown of **who and what is dominating system energy consumption**.
To understand battery drain at a glance:
1. **Device-Level Attribution (장치별 전력 지분)**: How much of the total platform power is consumed by CPU vs GPU vs Display vs Storage vs Cooling vs Uncore/Platform loss?
2. **Process-Level Attribution (프로세스별 전력 지분)**: Among software workloads, which applications (and background runaway tasks) account for what percentage of active process drain?

---

## 2. Functional Requirements

### 2.1 Device Power Share Breakdown (`REF-REQ-060-D1`)
1. **Categorized Physical Hardware Slices**:
   - **CPU Package**: RAPL Package power (excluding DRAM if measured separately).
   - **GPU Silicon**: Dedicated/Integrated graphics power.
   - **Display / Backlight**: Backlight power scaled by brightness level.
   - **Storage / NVMe**: NVMe APST controller active/sleep power.
   - **Mechanical Fan**: Cooling fan acoustic/mechanical power estimate.
   - **Platform & Uncore Loss**: Remainder ($P_{\text{sys}} - \sum P_{\text{devices}}$) representing motherboard, VRM conversion losses, and SoC uncore.
2. **Mathematical Invariant**:
   $$\sum_{i=1}^{N} P_{\text{device}, i} = P_{\text{system}} \quad (\text{or } 100\%)$$
3. **Visual Representation**:
   - Donut chart with distinct cyber-palette colors (Cyan, Purple, Orange, Emerald, Blue, Gray).
   - Center text showing Total System Drain in Watts.
   - Interactive legend showing device name, power (W), and percentage (%).

### 2.2 Process Power Share Breakdown (`REF-REQ-060-D2`)
1. **Top Process Attributed Energy Slices**:
   - Top 5 processes ranked by total attributed power (Watts).
   - Sixth slice: "기타 프로세스 (Other Processes)", aggregating all remaining processes.
2. **Mathematical Invariant**:
   $$\sum_{p \in \text{Top5}} P_p + P_{\text{other}} = P_{\text{process\_total}} \quad (100\%)$$
3. **Visual Representation**:
   - Donut chart with distinct high-contrast colors.
   - Center text showing Total Attributed Process Power in Watts.
   - Interactive legend showing PID, process name (`comm`), power (W), and percentage (%).

### 2.3 Real-Time QML Rendering & Performance (`REF-REQ-060-D3`)
1. **Threaded Framebuffer Rendering**: Donut canvases must render via `Canvas.Threaded` / `Canvas.FramebufferObject` to prevent main UI thread stutters.
2. **Responsive Hover Tooltip**: Hovering over a donut arc or legend row must smoothly highlight the segment and display detailed attribution.
3. **Zero Resource Drain When Hidden**: Donut computations must only execute when the dashboard window is active/visible.

---

## 3. Verification & Oracle Gate Standards (`REF-TEST-025`)

1. **Sum-Invariant Assertion**:
   - Sum of device power shares must equal 100.0% within floating-point tolerance ($\pm 0.1\%$).
   - Sum of process power shares must equal 100.0% within floating-point tolerance ($\pm 0.1\%$).
2. **Empty / Single Process Safety**:
   - Donut computation must handle edge cases gracefully (0W total power, 0 processes) without division-by-zero or `NaN` outputs.
3. **Sub-100us Computation Gate**:
   - Backend data preparation for both donut charts must execute in **< 100 microseconds**.
