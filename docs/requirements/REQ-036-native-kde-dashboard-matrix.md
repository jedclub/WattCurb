# REQ-036: KDE Plasma 6 Native Power Dashboard Matrix (Zero-Delay Interactive GUI)

- **Requirement ID**: `REF-REQ-036`
- **Related Requirements**: `REF-REQ-020` (Executive Briefing), `REF-REQ-028` (128B Seqlock SHM), `REF-REQ-035` (Desktop Tray)
- **Status**: IMPLEMENTED
- **Target OS**: Linux (KDE Plasma 6 Wayland/X11, Qt 6.11+)

---

## 1. Executive Summary & Problem Definition

### 1.1 Problem Statement
1. Previously, triggering "Rescan Now" from the desktop tray imposed a multi-second blocking delay due to synchronous 5-second sampling loops and sleep operations.
2. The reporting UI was restricted to either simple tray tooltips or raw CLI console text, lacking a rich visual presentation matching KDE Plasma 6's desktop standards.

### 1.2 Mission Objectives
1. **Total Elimination of UI Delays**: Pop up the dashboard window immediately ($\le 20\text{ms}$) utilizing the daemon's hot in-memory Seqlock SHM cache without blocking the user interface.
2. **KDE Plasma 6 Native Visual Dashboard**: Render a modern, cybernetic matrix dashboard displaying real-time hardware component power breakdown, battery chemistry, top power drain culprits, and active mitigation features.
3. **Live 1-Second Streaming Telemetry**: Continually poll the lock-free 128-byte Seqlock POD every second without waking up the CPU unnecessarily, providing instantaneous live updates.

---

## 2. Detailed Technical Specifications

### REQ-036.1: Zero-Delay Dashboard Startup ($\le 50\text{ms}$)
- The dashboard binary `wattcurb-dashboard` shall mmap `/dev/shm/wattcurb_shared_state.bin` directly on startup.
- UI rendering shall begin immediately upon process launch without blocking on kernel sampling loops.

### REQ-036.2: Hardware Domain Power Matrix Card Grid
- Display dedicated metric cards for:
  - **System Drain & Battery Flow**: Real-time wattage (W), discharging/charging state, battery capacity level, time to empty/full.
  - **CPU & Platform Subsystem**: RAPL Package/DRAM drain (W), core temperature (°C), average frequency (MHz), C-State sleep residency.
  - **GPU Silicon Subsystem**: iGPU power (W), VRAM allocation, GPU core load (%), clock frequency.
  - **Display & Backlight**: Display drain (W), brightness percentage.
  - **Storage & Peripherals**: NVMe APST state (W), cooling fan RPM, WiFi/BT link states.

### REQ-036.3: Process Attribution Matrix Table
- Tabulate top power-consuming processes with:
  - Process Name & PID.
  - Safety Tier (T0 Critical ~ T5 Runaway).
  - Power drain in milliwatts (mW).
  - Physical causation mechanism (WiFi Radio CAM, C-State Breaker, CCX Migration, etc.).

### REQ-036.4: Interactive One-Click Profile & Action Controls
- Provide interactive buttons to:
  - Switch between 4 power profiles (`Performance`, `Balanced`, `Smart Save`, `Ultra Save`).
  - Trigger instant asynchronous daemon rescan.
  - Open KDE System Monitor.
