# REQ-038: Interactive Hover Deep Telemetry & Physical Inspection Card

- **Requirement ID**: `REF-REQ-038`
- **Related Requirements**: `REF-REQ-011` (Process Attribution), `REF-REQ-015` (Syscall PMU), `REF-REQ-022` (Deep Battery), `REF-REQ-037` (btop Dense Matrix)
- **Status**: IMPLEMENTED
- **Target OS**: Linux (KDE Plasma 6 Wayland/X11, Qt 6.11+)

---

## 1. Executive Summary & Problem Definition

### 1.1 Problem Statement
1. In dense monitoring dashboards, screen real estate constraints limit the number of visible columns per process row and per hardware card.
2. Users need to inspect the full depth of WattCurb's hardware-level telemetry on demand without leaving the compact view.
3. Standard OS tooltips are plain, slow, and cannot display structured multi-domain engineering metrics.

### 1.2 Mission Objectives
1. **Glassmorphic Cyber Inspection Popup**: Render a custom high-performance floating inspection card with rounded corners, dark blurred background, accent borders, and clean typography upon mouse hover.
2. **Exhaustive Process Telemetry**: Display all per-process attributes collected by WattCurb:
   - Identifiers: PID, Comm, UID, Safety Tier (T0..T5), Mitigation Recommendation.
   - Power Breakdown: Total, CPU Compute, GPU Silicon, DRAM PSS, Wakeup Context Tax, Storage I/O, Thermal Fan, WiFi Radio.
   - Scheduler & Execution: Bound CPU Core, Cross-CCX Migration, Threads, Nice/Priority, Wakeups/sec, TimerSlack.
   - Memory & VFS: PSS/RSS Memory, Minor/Major Page Faults/sec, Open Network Sockets, Disk I/O MB/s, VRAM allocation.
   - Root Cause Diagnosis: Primary Hardware Domain and Detailed Physical Mechanism string.
3. **Hardware Domain Hover Cards**: Display deep physical specs on hardware cards:
   - CPU: PMU IPC, Instructions, Cycles, LLC Misses, Branch Stalls, Energy Waste Ratio (EWR), Governor.
   - Battery: Manufacturer, Model, Chemistry, Energy Design/Full (Wh), Degradation %, Pass-through status.
   - GPU & Storage: AMDGPU GFX Engine, VRAM, NVMe APST L1.2, ASPM policy.
4. **Boundary-Aware Smart Positioning**: Ensure hover cards stay strictly inside the application window boundaries, automatically flipping horizontally or vertically when near window edges.
