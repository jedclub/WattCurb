# REF-REQ-089: Matrix Dashboard Expanded Power Shares, Full Hardware Domain Visibility & Typography Enhancement

- **Document ID**: `REF-REQ-089`
- **Related Requirements**: [`REF-REQ-060`](REQ-060-circular-power-share-visualization.md), [`REF-REQ-061`](REQ-061-ui-layout-restructuring-and-bottom-power-share-deck.md), [`REF-REQ-062`](REQ-062-dashboard-font-scaling-and-readability-enhancement.md), [`REF-REQ-074`](REQ-074-dashboard-matrix-hotpath-profiling-and-optimization.md)
- **Related Architecture**: [`REF-ARCH-066`](../architecture/ARCH-066-matrix-dashboard-two-column-legend-and-font-scaling.md)
- **Status**: Approved
- **Date**: 2026-09-21

---

## 1. Objective & Scope

In the WattCurb btop-Style Power & Hardware Matrix Dashboard (`DashboardWindow.qml`), the lower analytics deck and global UI typography must be upgraded to address information clipping, space under-utilization, and legibility on high-DPI modern desktop environments:
1. **Full Hardware Power Share Visibility (`REF-REQ-089-F01`)**: All physical constituent hardware domains (CPU, GPU, Display, Storage/NVMe, Fan, DRAM, VRM, Wi-Fi, Motherboard & IO) must be 100% visible in the Hardware Power Share deck (Card A) without clipping or hidden overflow.
2. **Top 11 Process Power Share Expansion (`REF-REQ-089-F02`)**: Expand the Process Power Share deck (Card B) from Top 7 to Top 11 culprit processes (+ 1 Other category = 12 items) to utilize previously empty right-hand layout space.
3. **Comprehensive Typography & Visual Ergonomics Upgrade (`REF-REQ-089-F03`)**: Enlarge font sizes across the matrix dashboard (from 10~11px up to 12~14px), expand row delegate heights, increase deck height, and improve legibility without text clipping.

---

## 2. Functional Requirements

### 2.1 Full Hardware Domain Visibility in Card A (`REF-REQ-089-F01`)
- Card A's legend must transition from a single-column vertical layout to a 2-column `GridLayout` (`columns: 2`, `columnSpacing: 10`, `rowSpacing: 3`).
- All 9 hardware categories (CPU Package, GPU Silicon, Display, Storage NVMe, Cooling Fan, DRAM Memory, VRM Power Loss, Wireless Wi-Fi, Motherboard & IO) must be rendered in parallel columns with zero clipping.
- Compact, clear localized string formatting ensures names do not overflow their designated column bounds.

### 2.2 Top 11 Process Energy Attribution in Card B (`REF-REQ-089-F02`)
- The C++ backend `DashboardBackend::update_power_shares()` must extract `std::min<size_t>(11, cached_proc_summaries_.size())` (expanding by 4 items from the prior 7-item cap).
- Card B's palette must supply 11 distinct, high-contrast cyber terminal hex colors plus an aggregate Slate Gray for "Other Processes".
- In Card B, the 2-column grid (`columns: 2`, `columnSpacing: 16`, `rowSpacing: 3`) accommodates 6 rows per column, filling the horizontal space cleanly.

### 2.3 Comprehensive Typography & Deck Height Enhancement (`REF-REQ-089-F03`)
- Enlarge Lower Power Share Analytics Deck preferred height from `155px` to `190px`.
- Increase default application window dimensions from `1280x840` to `1280x880` (minimum `1080x740`).
- Scale up typography systematically across the dashboard:
  - Header titles and metric values: +1~2px (e.g. 13px -> 15px, 14px -> 16px, 22px -> 24px).
  - Process table column headers and row delegates: +1px (11px/12px -> 12px/13px, delegate height 30px -> 33px).
  - Left column hardware cards: details text, C-state labels, battery voltages, and GPU loads: +1px (11px/12px -> 12px/13px).
  - Bottom control buttons: height 30px -> 32px, font 11px -> 12px.

---

## 3. Non-Functional & Oracle Gate Invariants (`REF-TEST-053`)

1. **Zero Clipping Invariant**: No text item in Card A or Card B may be clipped or pushed outside the panel viewport.
2. **Zero-Allocation Hot Path**: Expanding process list slice in `DashboardBackend` must preserve zero heap allocation invariants during steady-state polling.
3. **Responsive Scaling Invariant**: The window must resize smoothly down to 1080x740 and up to 4K UHD without layout breaks.
