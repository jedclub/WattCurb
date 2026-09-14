# REQ-040: Dashboard Visual Overhaul: Large Typography, Zero Negative Space, and Real-Time Neon Telemetry Graphs

- **Requirement ID**: `REF-REQ-040`
- **Related Requirements**: `REF-REQ-036` (Matrix Dashboard), `REF-REQ-037` (btop Dense Matrix), `REF-REQ-038` (Hover Telemetry)
- **Status**: IN_PROGRESS
- **Target OS**: Linux (KDE Plasma 6 Wayland/X11, Qt 6 Quick / Canvas)

---

## 1. Executive Summary & Problem Statement

### 1.1 Problem Statement
1. **Typography Too Small**: Existing dashboard elements used microscopic 8px~9px fonts, severely impairing legibility on high-resolution displays.
2. **Excessive Empty / Negative Space**: Panels left vacant vertical areas and lacked visual cohesion, failing to achieve the dense, data-packed experience expected from terminal profilers like `btop`.
3. **Absence of Real-Time Graphs**: Users had no visual timeline to assess historical power trends, fluctuations, transient spikes, or cooling curves.

### 1.2 Mission Objectives
1. **Large High-Legibility Typography**:
   - Elevate body and tabular fonts from 8~9px to 11~13px.
   - Boost section headers and key wattage indicators to 16~24px with high-contrast neon accents.
   - Expand table row heights from 26px to 30px with bolder font weights and thicker visual indicator bars.
2. **Dense Zero-Waste Layout**:
   - Eliminate vacant gaps across hardware cards and process matrices.
   - Distribute space dynamically so that every pixel is dedicated to hardware metrics, scheduling status, or graphical telemetry.
3. **Real-Time Hardware Neon Graphs (btop Style)**:
   - **System Total Watts Timeline (Right Panel Top)**: High-resolution historical wave chart (recent 40 samples) with neon cyan glow, grid lines, and max/avg labels.
   - **CPU Subsystem Sparkline (Left Card 1)**: Real-time CPU package watts and temperature graph.
   - **Battery & Discharge Trend Sparkline (Left Card 2)**: Real-time battery power draw trend.
   - **GPU & Engine Load Sparkline (Left Card 3)**: Real-time GPU wattage and utilization graph.
4. **Zero-Overhead GPU-Accelerated Canvas**:
   - Maintain sliding window buffers (30~40 points) in C++ backend without memory reallocation.
   - Render via lightweight Qt Quick Canvas / 2D Path strokes without external dependencies.
