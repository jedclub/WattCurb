# REF-ARCH-059: Granular Platform Loss Multi-Rail Telemetry Architecture

- **Status**: Implemented / Specification
- **Ref ID**: `REF-ARCH-059`
- **Related Requirements**: [`REF-REQ-082`](../requirements/REQ-082-granular-platform-loss-hardware-telemetry.md), [`REF-REQ-064`](../requirements/REQ-064-wifi-txpower-capping-and-decomposed-platform-loss.md), [`REF-REQ-078`](../requirements/REQ-078-battery-drain-deep-audit-report-and-window.md)
- **Related Research**: [`REF-RES-019`](../research/RES-019-granular-platform-loss-decomposition-and-kernel-interfaces.md)
- **Created**: 2026-09-20
- **Category**: Telemetry Architecture, Hardware Probes, Non-Allocating Analytics

---

## 1. Architectural Overview

`REF-ARCH-059` establishes the design of the **Platform Loss Decomposer** within the WattCurb daemon and analytics pipeline:

```
+-----------------------------------------------------------------------------------+
|                           HardwareProbe (Kernel Sysfs/Hwmon)                     |
+-----------------------------------------------------------------------------------+
  | BAT0 (V*I)   | AMDGPU in0/in1 | IWlwifi Temp | ThinkPad Fan | PCIe/USB States   |
  v              v                v              v              v                   v
+-----------------------------------------------------------------------------------+
|                        PlatformLossDecomposer (C++23)                             |
|                                                                                   |
|  1. VRM Loss Model: Dynamic efficiency curve eta(P_load) based on rail voltages  |
|  2. DRAM Model: Base standby + pgpgin/out memory page activity rate               |
|  3. RF Model: txpower dBm + iwlwifi die temp + power_save status                  |
|  4. Mechanical Fan: Cubic law P_fan = k * (RPM/1000)^3                            |
|  5. Bus & Interconnect: Active PCIe SerDes lanes + USB PHY state + Audio Codec    |
|  6. Conservation Projection: Scale raw sub-rail estimates to match total residual |
+-----------------------------------------------------------------------------------+
         |                                                       |
         v                                                       v
+-----------------------------+               +-------------------------------------+
|      DashboardBackend       |               |       BatteryHistoryAnalyzer        |
|  (Real-Time GUI Telemetry)  |               |    (Deep Audit Window & CSV Logs)   |
+-----------------------------+               +-------------------------------------+
```

---

## 2. Decomposed Sub-Rail Structure

A zero-allocation, trivially copyable structure represents the decomposed power snapshot:

```cpp
struct alignas(32) DecomposedPlatformLoss {
    double vrm_watts{0.0};          // DC-DC switching and conduction losses
    double dram_watts{0.0};         // LPDDR5/DDR5 refresh and bus power
    double wireless_watts{0.0};     // Wi-Fi RF PA + BT HCI + Baseband
    double fan_watts{0.0};          // ThinkPad EC blower fan mechanical drag
    double bus_io_watts{0.0};       // PCIe ASPM + USB PHYs + Audio Codec
    double motherboard_watts{0.0};  // Quiescent base (EC, RTC, PCB leakage)
    double total_residual_watts{0.0}; // Strictly equals sum of above
};
```

---

## 3. Mathematical Normalization & Precision Invariant

Raw physical sub-rail models compute physical estimates $\hat{P}_k$:
$$\hat{P}_{\text{sum}} = \hat{P}_{\text{vrm}} + \hat{P}_{\text{dram}} + \hat{P}_{\text{wifi}} + \hat{P}_{\text{fan}} + \hat{P}_{\text{bus}} + \hat{P}_{\text{mb}}$$

The conservation multiplier $\alpha$ scales each sub-rail to match the physical battery balance:
$$\alpha = \frac{P_{\text{plat\_rem}}}{\hat{P}_{\text{sum}}}$$
$$P_k = \hat{P}_k \times \alpha$$

This guarantees that:
1. No fictitious energy is invented.
2. The exact law of conservation of energy ($\sum P_k = P_{\text{plat\_rem}}$) is maintained to 6 decimal places.
3. If an individual component spikes (e.g., fan turns on to 4,500 RPM or Wi-Fi bursts transmit data), its proportional slice expands dynamically while maintaining exact physical balance.
