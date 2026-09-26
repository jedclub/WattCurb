# REF-RES-034: Hardware Bus & Display Deep Power Minimization Research

- **Ref-ID**: `REF-RES-034`
- **Domain**: Silicon & Bus Power Optimization (Display ABM, PCIe ASPM, Realtek PHY, HDA Codec, Kernel VM)
- **Status**: Approved & Evaluated
- **Date**: 2026-09-26

---

## 1. Executive Summary & Problem Definition

While CPU core throttling and RAM compaction substantially reduce active processing power, modern laptop platforms suffer from persistent **un-throttled platform bus & peripheral baseline leakages** that survive software idle states:
1. **eDP Backlight Fixed Current**: Traditional LED backlights draw fixed high current regardless of displayed pixel luminance.
2. **PCIe Sub-state Inactivity**: Linux default ASPM policy (`[default]`) leaves high-speed serial links (L0) running at full clock frequency, burning 0.8W~1.5W even when no transactions are in flight.
3. **Unplugged Ethernet PHY Carrier Search**: Realtek RTL8111 Gigabit PHY chips continuously transmit energy-detection pulses on disconnected RJ-45 jacks (`NO-CARRIER`), consuming 300mW~600mW.
4. **HDA Audio DAC Hangover**: `snd_hda_intel.power_save=10` keeps analog headphone/speaker power amplifiers energized for 10 full seconds after audio playback ceases.
5. **1-Second CPU Timer Jitter (`vm.stat_interval=1`)**: The kernel periodically awakens all idle CPU cores every 1,000ms to calculate memory statistics, sabotaging Package C6/C10 state residency.

---

## 2. Kernel & Hardware Interface Survey

### 2.1 AMD eDP Adaptive Backlight Modulation (ABM)
- **Interface**: `/sys/class/drm/card*-eDP-*/amdgpu/panel_power_savings`
- **Mechanism**: Dynamic pixel contrast expansion combined with PWM backlight current reduction.
- **Values**:
  - `0`: Disabled (Standard fixed brightness).
  - `1`: Low (Minimal luminance compression, ~10% backlight reduction).
  - `2`: Medium / Balanced (Imperceptible to human eye, ~18-22% backlight power reduction).
  - `3`: High (Noticeable contrast boost, ~28-35% backlight power reduction).
  - `4`: Extreme (Maximum power saving for critical battery states).
- **Physical Impact**: Direct reduction of DC-DC backlight boost converter current on 12V/19V rails.

### 2.2 PCIe Active State Power Management (ASPM)
- **Interface**: `/sys/module/pcie_aspm/parameters/policy`
- **Options**: `default`, `performance`, `powersave`, `powersupersave`
- **Safety Boundary**: Must not force broken ASPM on unstable non-compliant hardware; standard `powersave` activates L1.1 and L1.2 PCIe low-power link states without interface re-enumeration latency.

### 2.3 Realtek Ethernet PHY & Safe PCI Runtime PM
- **Interface**: `/sys/class/net/<iface>/carrier` & `/sys/bus/pci/devices/<bdf>/power/control`
- **Invariant**:
  - Never set `ip link set <iface> down` (which prevents hardware carrier sense when a physical cable is connected).
  - When `carrier == 0`, switch the PCI controller's `power/control` to `auto` and enable Energy Efficient Ethernet (EEE). The `r8169` kernel driver wakes via hardware interrupt within 1ms upon cable connection.
  - Blacklist: NVMe controller (`01:00.0`), GPU display engine (`06:00.0`), and CPU host bridges (`00:18.*`) are strictly immune from arbitrary runtime PM changes.

### 2.4 Intel HDA Audio Codec Power Saving
- **Interface**: `/sys/module/snd_hda_intel/parameters/power_save`
- **Value**: Shortened from default `10` to `1` second on battery.
- **Safety**: Synchronized with `REF-REQ-096` Audio Continuity Guarantee; active PipeWire audio streams maintain PM QoS and prevent premature sleep while media is playing.

### 2.5 Kernel VM Stat Interval
- **Interface**: `/proc/sys/vm/stat_interval`
- **Mechanism**: Frequency in seconds at which the kernel calculates zone and node VM stats.
- **Tuning**: Relaxed from `1` to `10` on battery to eliminate uncoordinated CPU tick wakeups across all CPU cores.

---

## 3. Physical Power Reduction Estimates

| Optimization Vector | Target Hardware | Idle Baseline | Optimized Baseline | Net Savings |
| :--- | :--- | :--- | :--- | :---: |
| **AMD Display ABM (Level 2/3)** | 1080p/1200p eDP Panel | ~2.5W @ 60% br | ~1.8W @ 60% br | **~700 mW** |
| **PCIe ASPM `powersave`** | SoC PCIe Root Complex & PHYs | L0/L0s Active | L1.1/L1.2 Substates | **~600 mW** |
| **Unplugged Ethernet Sleep** | Realtek RTL8111 PHY & UART | 350mW PHY Ping | D3hot Substate | **~300 mW** |
| **HDA Codec 1s Power-Save** | Realtek ALC257 DAC/Amp | 10s D0 Active | 1s D3hot Transition| **~200 mW** |
| **`vm.stat_interval=10`** | CPU All-Core Tick Res. | 60 wakeups/min | 6 wakeups/min | **~150 mW** |
| **Total Cumulative Power Reduction** | Platform Broad Spectrum | — | — | **~1.95W ~ 2.5W** |
