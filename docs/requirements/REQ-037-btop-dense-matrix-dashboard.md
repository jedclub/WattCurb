# REQ-037: High-Density btop-Style Power & Process Matrix Dashboard

- **Requirement ID**: `REF-REQ-037`
- **Related Requirements**: `REF-REQ-010` (Hardware Subsystem), `REF-REQ-011` (Process Attribution), `REF-REQ-028` (128B Seqlock SHM), `REF-REQ-036` (Native KDE Dashboard)
- **Status**: IMPLEMENTED
- **Target OS**: Linux (KDE Plasma 6 Wayland/X11, Qt 6.11+)

---

## 1. Executive Summary & Problem Definition

### 1.1 Problem Statement
1. The initial dashboard UI displayed high-level summary cards but lacked information density (too much whitespace), unlike command-line dense monitors such as `btop` or `htop`.
2. Process attribution was restricted to only the top 2 culprits, preventing users from seeing comprehensive per-process power breakdowns (CPU vs GPU vs DRAM vs I/O vs Wakeup tax, PSS memory, Safety Tiers).
3. Detailed physical hardware domain breakdowns (CPU core/uncore/DRAM, C-States, battery voltage/current/cycles, NVMe throughput) needed richer visualization.
4. Text clipping and layout boundary overflow must be strictly avoided across all display resolutions and font scales.

### 1.2 Mission Objectives
1. **High-Density btop-Style Layout**: Maximize screen utility with modular, tightly-packed panels displaying comprehensive hardware and process telemetry without wasted whitespace.
2. **Detailed Process Power Matrix Table**: Display top 8–10 energy-consuming processes with granular attribution breakdown (PID, Comm, Total Watts/mW, Visual Ratio Bar, CPU W, GPU W, DRAM W, Wakeup Tax, PSS MB, Safety Tier T0–T5, Primary Domain).
3. **Hardware Domain Matrix**: Full component breakdown including:
   - CPU: Package W, Core W, Uncore W, DRAM W, Core Temp (°C), Freq (MHz), C-States C0/C1/C2/C3 residency bars, Fan RPM.
   - Battery & Power: Real-time Wattage, Voltage (V), Current (A), Energy (Wh), Health (%), Cycle Count, Time to empty/full.
   - GPU & Display: iGPU W, Engine Load %, Backlight W, Brightness %, Adaptive Refresh.
   - Storage & Peripherals: NVMe APST status, Read/Write throughput, WiFi CAM mode.
4. **Zero Layout Overflow & Guaranteed Text Containment**: Strict bounding, ellipsis (`Text.ElideRight`), clipping, and responsive geometry to prevent any label or value from overflowing containers.
5. **Ultra-Low Overhead Daemon Querying**: Fetch full telemetry via non-blocking UNIX datagram socket (`FULL_TELEMETRY`), backed by 0ns 128B Seqlock SHM fallback.

---

## 2. Technical Architecture

### 2.1 Telemetry Protocol (`FULL_TELEMETRY`)
- Command: `FULL_TELEMETRY\n` sent over `/run/wattcurb.sock`.
- Response: Compact JSON packet generated directly from `cached_report_` in daemon memory.
- Contains:
  - System summary (watts, battery %, state, time to empty, voltage, current, cycles, health).
  - Hardware breakdown (CPU package, cores, uncore, dram, temp, freq, C-states C0..C3, fan rpm, GPU watts, display watts, nvme watts, read/write MB/s).
  - Top 10 processes with exact attribution metrics.

### 2.2 Dashboard Backend (`DashboardBackend`)
- Exposes `ProcessTableModel` (QAbstractListModel or QVariantList) to QML.
- Periodically queries `FULL_TELEMETRY` every 1000ms.
- Fallback to 128B Seqlock SHM if socket times out.
