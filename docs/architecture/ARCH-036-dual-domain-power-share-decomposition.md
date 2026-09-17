# ARCH-036: Dual-Domain Power Share Decomposition & QML Threaded Donut Pipeline

- **Ref-ID**: `REF-ARCH-036`
- **Category**: Architecture Design & UI Visualization
- **Status**: Approved
- **Domain**: Qt6/QML, Canvas 2D, Data Modeling, Realtime Telemetry
- **Dependencies**: [`REF-REQ-060`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-060-circular-power-share-visualization.md), [`REF-ARCH-027`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-027-apu-ppt-disambiguation-and-ui-acceleration.md), [`REF-ARCH-032`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-032-ping-pong-double-buffer-retention-and-matrix-telemetry.md)
- **Related Tests**: [`REF-TEST-025`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-060-circular-power-share-visualization.md#3-verification--oracle-gate-standards-ref-test-025)

---

## 1. Architectural Architecture & Data Flow

```
+-----------------------------------------------------------------------------------------+
|                                  DashboardBackend (C++)                                 |
+-----------------------------------------------------------------------------------------+
       |                                                                   |
       | Compute Device Shares                                             | Compute Process Shares
       v                                                                   v
+-----------------------------------------+            +------------------------------------+
| QVariantList devicePowerShares          |            | QVariantList processPowerShares    |
|  - CPU Subsystem (W, %, Color)          |            |  - Top 1: agy (W, %, Color)        |
|  - GPU Silicon (W, %, Color)            |            |  - Top 2: kwin (W, %, Color)       |
|  - Display / Backlight (W, %, Color)    |            |  - Top 3: chrome (W, %, Color)     |
|  - NVMe Storage (W, %, Color)           |            |  - Top 4: claude (W, %, Color)     |
|  - Cooling Fan (W, %, Color)            |            |  - Top 5: code (W, %, Color)       |
|  - Platform & Uncore Loss (W, %, Color) |            |  - Other Tasks (W, %, Color)       |
+-----------------------------------------+            +------------------------------------+
       |                                                                   |
       v (NOTIFY telemetryChanged)                                         v (NOTIFY processListChanged)
+-----------------------------------------------------------------------------------------+
|                           QML Dual-Donut Power Share Panel                              |
|   +------------------------------------+   +------------------------------------+       |
|   |   💻 장치별 전력 지분 (Device)     |   |   🚀 프로세스별 전력 지분 (Process)|       |
|   |  [Canvas Donut]  [Itemized Legend] |   |  [Canvas Donut]  [Itemized Legend] |       |
|   +------------------------------------+   +------------------------------------+       |
+-----------------------------------------------------------------------------------------+
```

---

## 2. Data Structures & Normalization Algorithms

### 2.1 Device Share Decomposition
Given the total system drain $P_{\text{sys}}$ and component readings:
1. $P_{\text{cpu}} = \text{cpu\_package\_w}$
2. $P_{\text{gpu}} = \text{gpu\_w}$
3. $P_{\text{disp}} = \text{display\_w} > 0 ? \text{display\_w} : 1.8$
4. $P_{\text{nvme}} = \text{nvme\_w} > 0 ? \text{nvme\_w} : 0.8$
5. $P_{\text{fan}} = \text{fan\_rpm} > 0 ? (\text{fan\_rpm} / 4000.0) \times 0.9 : 0.0$
6. Known subtotal $P_{\text{known}} = P_{\text{cpu}} + P_{\text{gpu}} + P_{\text{disp}} + P_{\text{nvme}} + P_{\text{fan}}$
7. If $P_{\text{sys}} > P_{\text{known}}$, $P_{\text{plat}} = P_{\text{sys}} - P_{\text{known}}$. Otherwise $P_{\text{sys}} = P_{\text{known}}$ and $P_{\text{plat}} = 0.0$.
8. Percentage calculation for each device: $\text{pct}_i = (P_i / P_{\text{sys}}) \times 100\%$.

### 2.2 Process Share Decomposition
Given the list of attributed processes from the attribution engine:
1. $P_{\text{proc\_total}} = \sum_{p \in \text{processes}} P_p$
2. If $P_{\text{proc\_total}} < 0.001$, default to $1.0\text{ W}$ placeholder to avoid zero-division.
3. Extract top 5 processes by attributed watts.
4. $P_{\text{top5}} = \sum_{i=1}^{5} P_{p, i}$
5. $P_{\text{other}} = \max(0.0, P_{\text{proc\_total}} - P_{\text{top5}})$
6. Percentage calculation for each slice: $\text{pct}_i = (P_i / P_{\text{proc\_total}}) \times 100\%$.

---

## 3. QML Threaded Donut Rendering Component

Each donut chart is drawn using a custom 2D Canvas:
- **Inner Radius**: $R_{\text{in}} = 36\text{px}$
- **Outer Radius**: $R_{\text{out}} = 54\text{px}$ (expands to $58\text{px}$ on hover)
- **Angles**: For each slice $i$, sweep angle $\Delta \theta_i = (\text{pct}_i / 100.0) \times 2\pi$
- **Center Text**: Bold readout of Total Drain (W) with sub-label ("SYSTEM" or "PROCESSES")
- **Legend Layout**: Vertical column with colored square indicators, name, Watts, and percentage aligned in monospace font.
