# [REF-RES-002] Physical Hardware-Level Power Measurement Mechanisms in Modern Linux

- **Ref-ID**: `REF-RES-002`
- **Related Requirements**: [`REF-REQ-001`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-001-hardware-power-profiling.md)
- **Related Architecture**: [`REF-ARCH-001`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-001-daemon-architecture.md)
- **Author**: WattCurb Core Engineering Team
- **Status**: Completed / Active Reference
- **Date**: 2026-09-10

---

## 1. Executive Summary

This research document provides a comprehensive technical breakdown of how modern Linux kernels (Linux 6.x+) interface with physical hardware telemetry to measure power consumption across distinct physical components: CPU Package, Cores, DRAM, Discrete & Integrated GPUs, Display/Backlight, NVMe Storage, and Battery/ACPI Power Supply. 

It evaluates measurement accuracy, register semantics, kernel exposure layers (sysfs, MSRs, perf PMU, hwmon), and sampling constraints.

---

## 2. Component 1: CPU & Memory Subsystem (Intel & AMD RAPL)

### 2.1 Technical Mechanism
Intel and AMD modern processors (Intel Sandy Bridge+ and AMD Zen 2+) incorporate **RAPL (Running Average Power Limit)**. Hardware accumulators track energy consumed based on on-die current and voltage sensors (and calibrated energy models for uncore components).

#### Hardware Energy Domains:
1. **Package Domain (`package-0`, `package-1`)**: Entire physical CPU socket (Cores, Cache, Integrated GPU, Memory Controller, System Agent).
2. **Core Domain (`core` / `pp0`)**: All CPU compute cores.
3. **Uncore Domain (`uncore`)**: Shared L3 cache, ring interconnect, memory controllers (often calculated as $P_{\text{uncore}} = P_{\text{pkg}} - P_{\text{core}} - P_{\text{dram}}$ if not exposed directly).
4. **DRAM Domain (`dram`)**: Physical DDR4/DDR5/LPDDR memory rails powered by the memory controller.
5. **Platform Domain (`psys`)**: Entire motherboard platform rails (available on select Intel mobile/laptop platforms).

### 2.2 Kernel Access Interfaces

#### A. Power Capping Subsystem (`/sys/class/powercap/intel-rapl/`)
- **Recommended Interface for WattCurb**:
  - Path: `/sys/class/powercap/intel-rapl/intel-rapl:0/energy_uj`
  - Subdomains: `/sys/class/powercap/intel-rapl/intel-rapl:0/intel-rapl:0:0/energy_uj`
- **Data Format**: 64-bit integer representing accumulated energy in **microjoules ($\mu J$)**.
- **Average Power Calculation Formula**:
  $$P_{\text{Watts}} = \frac{E(t_2) - E(t_1)}{(t_2 - t_1) \times 10^6} \quad (\text{where } E \text{ is in } \mu J \text{ and } t \text{ is in seconds})$$
- **Counter Rollover Handling**:
  The hardware register is typically a 32-bit counter with a max energy range defined in `max_energy_range_uj`. When $E(t_2) < E(t_1)$:
  $$\Delta E = (\text{max\_energy\_range\_uj} - E(t_1)) + E(t_2)$$
- **Security & Permissions**:
  Following CVE-2020-8694 (Platypus side-channel attack), modern distributions restrict `energy_uj` permissions to `0400` (root-only). WattCurb runs with `CAP_SYS_ADMIN` or root daemon privilege to read this interface without performance degradation.

#### B. Direct MSR Access (`/dev/cpu/*/msr`)
- Direct register reads via `rdmsr`:
  - `MSR_RAPL_POWER_UNIT` (`0x606`): Provides units for time, energy, and power.
  - `MSR_PKG_ENERGY_STATUS` (`0x611`): Package energy.
  - `MSR_DRAM_ENERGY_STATUS` (`0x619`): Memory energy.
- **Evaluation**: Reading `/dev/cpu/*/msr` requires loading the `msr` kernel module and opening individual file descriptors per CPU. Sysfs (`powercap`) provides virtually identical accuracy with significantly higher code safety and portability.

#### C. Perf PMU (`perf_event_open`)
- Kernel provides RAPL PMU events: `power/energy-pkg/`, `power/energy-cores/`, `power/energy-ram/`.
- Accessible via `perf_event_open` syscall with `PERF_TYPE_RAW`. Useful if high-frequency kernel-space ring-buffering is required.

---

## 3. Component 2: Battery & Total System Discharge (ACPI / Power Supply)

### 3.1 Technical Mechanism
Laptop systems employ an **ACPI Smart Battery Subsystem (SBS)** monitored by an on-board Embedded Controller (EC) via SMBus/I2C. The EC polls the battery gas gauge (e.g., Texas Instruments BQ-series) measuring shunt resistor voltage drops.

### 3.2 Kernel Access Interface (`/sys/class/power_supply/BAT*/`)
- **Key Metrics**:
  - `status`: `Discharging`, `Charging`, `Full`, `Not charging`.
  - `power_now`: Current discharge/charge rate in **microwatts ($\mu W$)**.
  - `voltage_now`: Instantaneous voltage across terminals in **microvolts ($\mu V$)**.
  - `current_now`: Instantaneous current draw in **microamperes ($\mu A$)**.
  - `energy_now` / `energy_full`: Remaining / Design capacity in **microwatt-hours ($\mu Wh$)**.
- **Fallback Calculation**:
  When `power_now` is missing (common on certain ACPI implementations):
  $$P_{\mu W} = \frac{\text{voltage\_now} \times \text{current\_now}}{10^6}$$
- **Sampling Characteristics & Physical Latency**:
  - **Crucial Engineering Reality**: Battery gas gauges do **NOT** update instantaneously. Most hardware ECs update `power_now` at $0.5\text{Hz} \sim 1\text{Hz}$ (every 1 to 2 seconds).
  - Sampling faster than $1\text{Hz}$ produces identical cached values and introduces meaningless jitter.
  - WattCurb applies a low-pass moving average or Kalman filter over battery readings to smooth out transient inductive spikes.

---

## 4. Component 3: Graphics Processing Units (GPU)

### 4.1 AMDGPU Subsystem
- **Mechanism**: AMDGPU driver exposes hardware telemetry via `hwmon` and `sysfs`.
- **Paths**:
  - `/sys/class/drm/card*/device/hwmon/hwmon*/power1_input`: Current power consumption in $\mu W$ (or `power1_average` for rolling window average).
  - `power1_label`: Often labeled `PPT` (Package Power Tracking).
  - `/sys/class/drm/card*/device/gpu_busy_percent`: Active compute load percentage ($0 \sim 100\%$).
  - `/sys/class/drm/card*/device/pp_dpm_sclk`: Current and available engine clock frequencies.
- **Physical Domain**: Reflects total power across GPU VRAM, GPU Cores (ALUs), and display engine.

### 4.2 Intel Integrated & Arc GPUs (`i915` / `xe`)
- **Paths**:
  - RAPL `intel-rapl:1` or `intel-rapl:0:1` (often designated as `core` or `gfx`).
  - Sysfs: `/sys/class/drm/card0/device/hwmon/hwmon*/power1_input` ($\mu W$).
  - Clock frequency: `/sys/class/drm/card0/gt/gt0/rps_act_freq_mhz`.

### 4.3 NVIDIA Discrete GPUs
- **Mechanism**: Proprietary driver does not fully map to standard Linux `hwmon` sysfs.
- **Direct C-API**: **NVML (`libnvidia-ml.so`)**:
  - `nvmlDeviceGetPowerUsage(device, &power_mw)`: Returns real-time power draw in milliwatts ($mW$) with $\pm 5\%$ accuracy.
  - `nvmlDeviceGetUtilizationRates(device, &utilization)`: GPU and memory controller load.

---

## 5. Component 4: Display & Backlight

### 5.1 Technical Mechanism
Display backlights are among the highest energy consumers in portable computers ($2\text{W} \sim 8\text{W}$, frequently exceeding the idle CPU package draw!).
- Backlight brightness is regulated via PWM (Pulse-Width Modulation) by the GPU display engine or platform EC.

### 5.2 Kernel Access Interface
- **Path**: `/sys/class/backlight/<device>/`
  - `brightness`: Current PWM level.
  - `max_brightness`: Maximum scaling factor (e.g., $255$ or $65535$).
- **Power Correlation Model**:
  Display power follows a predictable affine curve:
  $$P_{\text{display}}(b) = P_{\text{panel\_static}} + P_{\text{backlight\_max}} \times \left( \frac{b}{\text{max\_brightness}} \right)^\gamma$$
  where $\gamma \approx 1.0 \sim 1.4$ depending on PWM linearity and panel technology (IPS vs OLED).

---

## 6. Component 5: NVMe Storage & APST

### 6.1 Technical Mechanism
Modern NVMe solid-state drives implement **APST (Autonomous Power State Transitions)** defined in the NVMe specification. Drives offer distinct power states:
- **Operational States (PS0 - PS2)**: Active read/write, consuming $3\text{W} \sim 8\text{W}$.
- **Non-Operational Low-Power States (PS3 - PS4)**: Low power standby, consuming $5\text{mW} \sim 50\text{mW}$ (transition latency: $500\mu s \sim 5000\mu s$).

### 6.2 Kernel Exposure & Power Impact
- The Linux kernel driver `nvme_core` autonomously manages state transitions based on latency tolerance.
- **The Power Problem**: Constant background disk I/O (e.g. continuous micro-writes by logging daemons or indexing bots) prevents the drive from remaining in PS3/PS4, keeping the controller in high-power states indefinitely.
- **Kernel Telemetry**:
  - `/sys/class/block/nvme*n1/stat`: Read/write/discard tick metrics.
  - `/proc/[pid]/io`: Per-process I/O byte counts and system call counts.

---

## 7. Component 6: PCIe ASPM & Wireless Networking

### 7.1 PCIe Active State Power Management (ASPM)
- **Mechanisms**:
  - **L0s**: Fast resume standby (< 100 ns).
  - **L1 / L1 Substates (L1.1, L1.2)**: High-latency deep power saving where PCIe transceivers turn off completely ($0.1\text{mW}$ level).
- **Interface**: `/sys/module/pcie_aspm/parameters/policy` (`default`, `performance`, `powersave`, `powersupersave`).

### 7.2 Wireless (Wi-Fi / 802.11) Power Save
- High-frequency beacon listening and active wakeups prevent wireless cards from entering IEEE 802.11 Power Save Mode (PSM).
- **Control Interface**: `nl80211` Netlink interface or `iw dev <wlan> get power_save`.

---

## 8. Summary of Hardware Measurement Vectors for WattCurb

| Hardware Component | Measurement Metric | Kernel Interface | Unit / Resolution | Privilege Required |
| :--- | :--- | :--- | :--- | :--- |
| **CPU Package** | Energy Accumulator | `/sys/class/powercap/intel-rapl/intel-rapl:0/energy_uj` | $\mu J$ (1 ms update) | Root / `CAP_SYS_ADMIN` |
| **CPU Cores** | Energy Accumulator | `intel-rapl:0:0/energy_uj` | $\mu J$ | Root / `CAP_SYS_ADMIN` |
| **DRAM Memory** | Energy Accumulator | `intel-rapl:0:2/energy_uj` (if available) | $\mu J$ | Root / `CAP_SYS_ADMIN` |
| **Total Battery** | Instantaneous Power | `/sys/class/power_supply/BAT*/power_now` | $\mu W$ (1 s update) | Unprivileged (0444) |
| **AMD GPU** | Package Power (PPT) | `/sys/class/drm/card*/device/hwmon/*/power1_input` | $\mu W$ | Unprivileged (0444) |
| **Intel GPU** | Instantaneous Power | `/sys/class/drm/card0/device/hwmon/*/power1_input` | $\mu W$ | Unprivileged (0444) |
| **NVIDIA GPU** | Instantaneous Power | NVML `nvmlDeviceGetPowerUsage` | $mW$ | Unprivileged |
| **Display Panel** | Brightness Level | `/sys/class/backlight/*/brightness` | Raw integer ($0 \sim \text{max}$) | Unprivileged (0444) |
| **NVMe Drive** | Active I/O time | `/sys/class/block/nvme*n1/stat` | Milliseconds spent | Unprivileged (0444) |
