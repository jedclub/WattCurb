# REQ-042: Vector SVG Cyber HUD & Direct Tray Activation

## 1. Context & Motivation
- **Ref-ID**: `REF-REQ-042`
- **Component**: `src/tray/tray_client.cpp`, `src/tray/tray_client.hpp`
- **User Feedback**: "뭔가 더 멋지겐 안됨?" (Can it look much cooler/fancier?)
- **Analysis**:
  - A plain HTML table, while clean, lacks futuristic aesthetic punch and visual depth.
  - Qt Quick and `QTextDocument` in KDE Plasma 6 fully support inline vector SVG data URIs (`<img src="data:image/svg+xml;utf8,..."/>`).
  - Leveraging inline SVG enables zero-overhead vector graphics: smooth proportional battery progress bars, dynamic process power share bars, and neon accent frames directly inside the Plasma tooltip popup.
  - Additionally, left-clicking the tray icon should directly open/toggle the high-precision KDE Dashboard (`wattcurb-dashboard`) rather than silently changing profiles.

## 2. Technical Requirements
1. **Inline Vector SVG Telemetry Elements**:
   - **Battery Gauge**: 260px wide vector bar with dynamic fill (`%2310b981`, `%23f59e0b`, `%23ef4444`) and rounded corners.
   - **Process Culprit Share Bars**: 140px wide vector bars with color-coded fills (`%23f43f5e` for #1, `%23fb923c` for #2) reflecting exact system power percentages.
   - **Neon Cyber Borders**: 3px solid neon accent bars on hardware domain cards (`#00f0ff`, `#10b981`, `#a855f7`, `#6366f1`).
   - **Live Pulsing Badge**: `[● REALTIME]` status badge.
2. **Left-Click Dashboard Launch**:
   - Update `method_activate` so left-clicking the tray icon instantly brings up or toggles the full KDE Plasma 6 Dashboard window.
3. **Zero Dynamic Heap Allocation & Benchmark Compliance**:
   - All SVG generation and fixed-point math formatted strictly into fixed stack buffers.
   - Latency remains strictly under 5 microseconds (`< 10,000 CPU cycles`).
