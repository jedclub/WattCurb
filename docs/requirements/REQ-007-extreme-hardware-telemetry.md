# [REF-REQ-010] Full-Domain Physical Hardware Power & Telemetry Probe Specification

- **Ref-ID**: `REF-REQ-010`
- **Related Requirements**: [`REF-REQ-001`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-001-hardware-power-profiling.md), [`REF-REQ-005`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-002-profiler-reporting-engine.md), [`REF-REQ-007`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-004-resident-daemon-direct-access.md)
- **Related Architecture**: [`REF-ARCH-002`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-002-profiler-implementation.md), [`REF-ARCH-004`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-004-resident-daemon-event-loop.md)
- **Related Research**: [`REF-RES-002`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-002-hardware-power-measurement-mechanisms.md), [`REF-RES-005`](file:///home/jedclub/Develop/WattCurb/docs/research/PMU_BENCHMARKS.md)
- **Status**: Approved

---

## 1. Executive Summary & Objective

Power profiling in WattCurb must not treat host hardware as a monolithic entity. Power drain is physically dispersed across heterogeneous silicon domains, power delivery rails, thermal cooling apparatus, and storage controllers.

This specification mandates that WattCurb captures the **complete physical hardware telemetry state** across all available sensor nodes in modern Linux systems with **sub-millisecond sampling latency** and **zero runtime heap allocations**.

---

## 2. Full-Domain Hardware Telemetry Matrix

WattCurb must monitor seven distinct hardware domains:

### 2.1 Power Supply & Battery Gas Gauge
- **Real-Time Power**: `/sys/class/power_supply/BAT0/power_now` ($\mu W$)
- **DC Voltage & Current**: `voltage_now` ($\mu V$), `current_now` ($\mu A$)
- **Battery Health**: $\text{Health} = \frac{\text{energy\_full}}{\text{energy\_full\_design}} \times 100\%$
- **Cycle Count & Remaining Energy**: `cycle_count`, `energy_now` ($\mu Wh$)
- **USB-C Power Delivery Input**: `ucsi-source-psy-USBC*:00*/voltage_now`, `current_now`

### 2.2 CPU & Platform Subsystem (AMD Zen / Intel Core)
- **Package Core Temperature**: `/sys/class/hwmon/hwmon5/temp1_input` (k10temp Tctl/Tdie millidegrees C)
- **C-State Sleep Residency**: `/sys/devices/system/cpu/cpu*/cpuidle/state*/time`
  - Quantifies microsecond deltas across `POLL`, `C1`, `C2`, `C3` to evaluate sleep disruption.
- **Core Frequency Distribution**: `/sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq` across all logical threads.
- **Scaling Governor**: `scaling_governor` (`schedutil`, `powersave`, `performance`)

### 2.3 Graphics Processing Unit (AMDGPU / DRM)
- **Package Power Tracking (PPT)**: `hwmon/hwmon4/power1_input` ($\mu W$)
- **Engine Activity**: `/sys/class/drm/card*/device/gpu_busy_percent` (0~100%)
- **Memory Allocation**: `mem_info_vram_used` vs `mem_info_vram_total`, `mem_info_gtt_used`
- **PCIe Interface State**: `current_link_speed` (e.g. 8.0 GT/s Gen3) and `current_link_width` (x16, x8, x4)
- **Core & SOC Voltages**: `hwmon4/in0_input`, `in1_input` ($\mu V$)
- **Edge Temperature**: `hwmon4/temp1_input` (millidegrees C)

### 2.4 Storage Subsystem (NVMe SSD)
- **Autonomous Power State (APST)**: `/sys/class/nvme/nvme*/device/power/runtime_status` (`active` vs `suspended`)
- **Multi-Sensor Temperatures**: `hwmon2/temp1_input`, `temp2_input`, `temp3_input` (Controller & NAND flash)
- **Throughput & Active Time**: `/proc/diskstats` (sectors read/written, I/O active milliseconds)

### 2.5 Thermal & Mechanical Chassis (Embedded Controller)
- **Cooling Fan Speed**: `hwmon3/fan1_input` (RPM)
  - Fan power draw ($P_{fan} \propto \text{RPM}^3$) accounts for up to 2~3 W under load.
- **Chassis Temperatures**: `temp1_input` ~ `temp8_input` (Palmrest, motherboard, exhaust)

### 2.6 Display & Keyboard Illumination
- **Display Backlight**: `/sys/class/backlight/*/actual_brightness` vs `max_brightness`
- **Keyboard Illumination**: `/proc/acpi/ibm/kbdlight` (Level 0, 1, 2)

### 2.7 Bus & Wireless Peripherals
- **PCIe ASPM Policy**: `/sys/module/pcie_aspm/parameters/policy` (`default`, `powersave`, `powersupersave`)
- **Wireless Interface (WiFi)**: `/sys/class/net/wlan0/device/power/runtime_status`, power_save state, and chip temperature (`hwmon8/temp1_input`)

---

## 3. Ultra-Low-Overhead Access Directives

1. **Persistent File Descriptors**:
   All static sysfs nodes must be opened once during `HardwareProbe` bootstrap.
2. **Zero-Offset `pread`**:
   Values are sampled via `pread(fd, stack_buf, sizeof(stack_buf), 0)`, eliminating VFS directory tree traversal.
3. **Zero Heap Allocation**:
   All numbers are parsed directly from fixed stack buffers using `std::from_chars`.
4. **Sampling Latency Budget**:
   Total telemetry capture across all 40+ hardware parameters must execute in $< 0.5\text{ ms}$.
