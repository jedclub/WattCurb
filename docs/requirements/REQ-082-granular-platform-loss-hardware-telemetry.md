# REF-REQ-082: Granular Platform Loss Hardware Telemetry & Multi-Rail Decomposition

- **Status**: Approved
- **Ref ID**: `REF-REQ-082`
- **Related Requirements**: [`REF-REQ-001`](REQ-001-hardware-power-profiling.md), [`REF-REQ-010`](REQ-007-extreme-hardware-telemetry.md), [`REF-REQ-064`](REQ-064-wifi-txpower-capping-and-decomposed-platform-loss.md), [`REF-REQ-078`](REQ-078-battery-drain-deep-audit-report-and-window.md)
- **Related Research**: [`REF-RES-019`](../research/RES-019-granular-platform-loss-decomposition-and-kernel-interfaces.md)
- **Related Architecture**: [`REF-ARCH-059`](../architecture/ARCH-059-granular-platform-loss-telemetry-architecture.md)
- **Created**: 2026-09-20
- **Category**: Physical Hardware Telemetry, Platform Power Decomposition, Battery Diagnostics

---

## 1. Context & Motivation

In previous implementations (`REF-REQ-064`), the daemon decomposed platform residual power using static heuristic equations:
$$\text{est}_{VRM} = \text{sys\_w} \times 0.09, \quad \text{est}_{DRAM} = 0.70 + \min(0.25, \text{cpu\_w} \times 0.05), \quad \text{est}_{WiFi} = 0.35, \quad \text{est}_{MB} = 0.15$$

While effective as a primary UI estimation, modern power diagnosis demands **direct physical correlation with kernel hardware telemetry**. Users must know whether platform battery loss is caused by an active cooling fan, Wi-Fi transmit bursts, an un-suspended PCIe bridge, an active audio codec, or heavy DRAM page swapping.

`REF-REQ-082` establishes the requirement for a granular multi-rail physical decomposition engine that ingests live kernel telemetry across 7 distinct physical domains.

---

## 2. Functional Requirements

### 2.1 Multi-Rail Physical Telemetry Ingestion
The telemetry engine MUST capture live hardware parameters for:
1. **VRM DC-DC Conversion Efficiency**: Dynamic efficiency $\eta(P_{\text{load}})$ derived from total delivered power and live rail voltages (`amdgpu in0/in1`).
2. **DRAM Memory Subsystem**: Background standby ($350\text{mW} \sim 500\text{mW}$) plus dynamic energy from page traffic (`/proc/vmstat` pgpgin/out) and VDDNB memory rail voltage.
3. **Wireless & RF Subsystem**: Wi-Fi operational state, `iwlwifi` die temperature (`hwmon8/temp1_input`), configured `txpower`, and `power_save` status.
4. **Cooling Fan Mechanical Loss**: ThinkPad EC RPM (`hwmon3/fan1_input`) modeled with cubic aerodynamic law: $P_{\text{fan}} = k \cdot (\text{RPM}/1000)^3$.
5. **PCIe Interconnect & ASPM**: Active (D0) vs suspended (D3) device counts from `/sys/bus/pci/devices/*/power/runtime_status` and binary config space link speeds.
6. **USB Host & Peripherals**: Active vs suspended USB endpoints (`/sys/bus/usb/devices/*/power/runtime_status`) and USB-C PD telemetry.
7. **Audio Codec Subsystem**: ALSA runtime status (`/sys/class/sound/card*/power/runtime_status` and `/proc/asound/card*/pcm*/sub*/status`).
8. **Motherboard Quiescent Base**: Calibrated static load representing EC, RTC, and PCB leakage ($150\text{mW} \sim 200\text{mW}$).

### 2.2 Mathematical Decomposition & Invariant Conservation
Given total system power $P_{\text{sys}}$ and directly measured primary domains ($P_{\text{cpu}}, P_{\text{gpu}}, P_{\text{disp}}, P_{\text{nvme}}$):
$$P_{\text{plat\_rem}} = \max\left(0.05, P_{\text{sys}} - (P_{\text{cpu}} + P_{\text{gpu}} + P_{\text{disp}} + P_{\text{nvme}})\right)$$

The sum of decomposed sub-rails MUST strictly conserve total platform power:
$$\sum_{k=1}^{7} P_{\text{sub\_k}} \equiv P_{\text{plat\_rem}}$$

### 2.3 Non-Allocating Execution Invariant
All kernel telemetry readings, buffer scans, and algebraic projections MUST execute without heap allocation (`malloc`/`new`), conforming to `AGENTS.md` Section 9.

---

## 3. UI & Report Integration

1. **Real-Time Dashboard (`DashboardBackend`)**:
   - The device share donut chart and list MUST reflect decomposed sub-rails (DRAM, VRM, Wireless, Fan, PCIe/USB/Audio) whenever $P_{\text{plat\_rem}} > 0.05\text{W}$.
2. **Battery Drain Deep Audit Report (`BatteryReportWindow`)**:
   - The report MUST break down historical platform energy into physical categories with respective energy consumption ($Wh$), average power ($W$), and percentage share ($\%$).
