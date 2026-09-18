# REQ-064: Wi-Fi TxPower Capping & Decomposed Platform Loss Physical Domain Architecture

- **Status**: Approved
- **Ref ID**: `REF-REQ-064`
- **Related Architecture**: `REF-ARCH-040`
- **Related Test**: `REF-TEST-029`
- **Created**: 2026-09-18
- **Category**: Physical Hardware Power Profiling, Mitigation Actuation, UI Analytics

---

## 1. Background & Problem Statement

### 1.1 Wi-Fi RF Power Inefficiency in Ultra-Low Power Modes
In standard Linux operation, modern Wi-Fi adapters (e.g., Intel AX210 / MediaTek MT7921) operate with transmit power (`txpower`) set up to **22.00 dBm (~158 mW RF power)**. Due to power amplifier (PA) efficiency typically sitting around 15% to 20%, generating 158 mW of RF output consumes approximately **0.8W ~ 1.0W of DC battery power**.
Under good signal environments (RSSI > -70 dBm), maintaining 22 dBm is unnecessary. Completely disabling Wi-Fi disrupts network connectivity, but capping TxPower to **12.00 dBm (16 mW RF power)** maintains stable Wi-Fi connectivity while slashing front-end power draw by **0.3W ~ 0.5W**.

### 1.2 Monolithic "Platform & Loss" Obfuscation
Previously, power that was not directly attributed to CPU, GPU, Display, or NVMe was lumped into a single monolithic category: `Platform & Loss`. This prevented users from understanding the physical sources of battery drain. In reality, platform power comprises distinct physical hardware components:
1. **DRAM Memory Subsystem**: Periodic cell refresh (`tREFI`), termination voltage, and LPDDR5 command/data bus power (~0.7W ~ 0.95W).
2. **VRM Power Loss**: Switching loss, conduction resistance, and inductor thermal loss of multi-phase DC-DC buck converters (~8% ~ 10% of total system draw).
3. **Wireless (Wi-Fi / Bluetooth)**: Baseband standby, RF front-end receiver, and beacon listening (~0.3W ~ 0.5W).
4. **Motherboard & IO**: Embedded Controller (EC), I2C sensors, audio codec, and PCIe bridges (~0.1W ~ 0.25W).

---

## 2. Requirements & Functional Specifications

### 2.1 Wi-Fi Transmit Power Capping (`set_wifi_txpower_limit`)
- In `PowerProfileMode::UltraEndurance`, the daemon automatically interrogates available wireless interfaces via `/sys/class/net/*/wireless` and applies a non-intrusive transmit power cap of **1200 mBm (12.00 dBm)**:
  `iw dev <iface> set txpower limit 1200`
- The command executes asynchronously (`&`) without blocking daemon monitoring loops or epoll dispatch.
- Upon transitioning to `Performance`, `Balanced`, or `PowerSaver` mode, or upon system rollback (`rollback_all`), the driver automatically restores TxPower to automatic management:
  `iw dev <iface> set txpower auto`
- Zero-kill and non-halting invariant is strictly preserved: Wi-Fi connection remains active with zero packet loss or connection drops.

### 2.2 Constituent Decomposition of Platform & Loss
In the GUI Dashboard and telemetry backend, the remaining platform drain `plat_w = max(0, sys_w - known_w)` must be decomposed into 4 physical domains:
$$\text{est}_{VRM} = \text{sys\_w} \times 0.09$$
$$\text{est}_{DRAM} = 0.70 + \min(0.25, \text{cpu\_w} \times 0.05)$$
$$\text{est}_{WiFi} = 0.35$$
$$\text{est}_{MB} = 0.15$$

Scaling factor:
$$S = \frac{\text{plat\_w}}{\text{est}_{VRM} + \text{est}_{DRAM} + \text{est}_{WiFi} + \text{est}_{MB}}$$
$$W_{domain} = \text{est}_{domain} \times S$$

- The sum of decomposed components is strictly invariant:
  $$\sum W_{domain} \equiv \text{plat\_w}$$
- Each constituent domain is visualized independently in the Device Power Share Donut chart and legend with dedicated distinct telemetry colors.

### 2.3 Process Power Share Expansion & Detailed "Other" Labeling
- Expand the Process Power Share ranking from Top 5 to **Top 7** distinct processes with 7 dedicated high-contrast colors.
- Re-label the residual tail from generic "기타 프로세스 (Other)" to **"기타 150+ 프로세스 (Other)"**, explicitly reflecting that this slice represents the cumulative sum of over 150 kernel worker threads, browser renderer sub-processes, systemd journals, and background daemons rather than an unknown rogue process.

---

## 3. Verification & Oracle Gate Standards (`REF-TEST-029`)

1. **Wi-Fi TxPower Capping & Baseline Roundtrip**:
   - `MitigationEngine::set_wifi_txpower_limit(1200)` sets `hardware_baseline().wifi_txpower_capped == true`.
   - `MitigationEngine::restore_wifi_txpower()` clears `hardware_baseline().wifi_txpower_capped == false`.
2. **Decomposition Mathematical Invariant**:
   - For arbitrary system power inputs, verify $|(\sum W_{domain}) - \text{plat\_w}| < 10^{-9}$.
   - Verify non-zero attribution across all 4 domains under real hardware workloads.
