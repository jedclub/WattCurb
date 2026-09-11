# REF-RES-012: ThinkPower Hardware Domain Survey, Silicon Telemetry & Zero-Wakeup C++23 Re-Engineering Specification

- **Ref-ID**: `REF-RES-012`
- **Related Requirements**: [`REF-REQ-001`](../requirements/REQ-001-hardware-power-profiling.md), [`REF-REQ-007`](../requirements/REQ-004-resident-daemon-direct-access.md), [`REF-REQ-010`](../requirements/REQ-007-extreme-hardware-telemetry.md), [`REF-REQ-020`](../requirements/REQ-017-detailed-briefing-and-modular-optimization-features.md)
- **Related Architecture**: [`REF-ARCH-004`](../architecture/ARCH-004-resident-daemon-event-loop.md), [`REF-ARCH-009`](../architecture/ARCH-009-modular-optimization-feature-framework.md), [`REF-ARCH-011`](../architecture/ARCH-011-cacheline-chunking-and-bitfield-packing.md)
- **Source Project Reference**: ThinkPower (`/home/jedclub/Develop/thinkpower`, v1.1.0)
- **Status**: Approved Architectural Research Document

---

## 1. Executive Summary & Problem Formulation

The **ThinkPower** project (`/home/jedclub/Develop/thinkpower`) established an extensive, empirical catalogue of hardware silicon, motherboard, display, and bus-level power controls specifically calibrated for the author's daily-driver machine: **Lenovo ThinkPad L15 Gen 1 (AMD Ryzen 7 PRO 4750U Renoir APU, 8C/16T) running CachyOS/Arch Linux on KDE Plasma 6 Wayland**.

Through ThinkPower, whole-system battery discharge was empirically reduced from **15W ~ 18W down to 6.5W ~ 8.2W** by enforcing hardware-level constraints across 7 distinct physical domains.

However, as acknowledged in the design evaluation, **ThinkPower was developed as a feature-first prototype**:
1. **The Subshell & Fork Bomb Hazard**: The root hardware manager ([`power-profile-manager`](file:///home/jedclub/Develop/thinkpower/src/power-profile-manager)) spans 1,123 lines of Bash script. Toggling power states spawns dozens of external utility subshells (`cat`, `echo`, `grep`, `awk`, `sed`, `sudo`, `pkexec`, `rfkill`, `iw`, `wpctl`, `kscreen-doctor`, `qdbus6`, `powertop`).
2. **The "Observer Effect" Spike**: Spawning multiple processes to save power ironically triggers the CPU scheduler to boost cores to 4.1GHz at 1.35V+, causing short package spikes of 25W–40W right as the system attempts to enter low-power states.
3. **Open-Loop Static Control**: ThinkPower applies static, indiscriminate profile clamps without knowing which specific background processes or threads are draining energy.

**WattCurb's Re-Engineering Objective**:  
Extract every verified hardware domain, register, and kernel interface from ThinkPower, discard the legacy shell scaffolding, and re-engineer the entire feature matrix into **ultra-low-overhead, zero-wakeup, zero-allocation C++23 native actuators** integrated into WattCurb's closed-loop mitigation engine.

---

## 2. Complete ThinkPower Hardware Domain Taxonomy

ThinkPower touches 7 core architectural domains. The complete matrix below documents every physical subsystem, sysfs/kernel node, legacy mechanism, and required C++23 re-engineering method.

```
┌──────────────────────────────────────────────────────────────────────────────────────────────────┐
│                               THINKPOWER HARDWARE DOMAIN TAXONOMY                                │
├─────────────────────────┬───────────────────────────────────┬────────────────────────────────────┤
│ 1. CPU Silicon & SMU    │ 2. GPU & Display Subsystem        │ 3. Storage & NVMe Controller       │
│ - AMD SMU 4W STAPM Lock │ - AMDGPU OverDrive 640MHz Ceiling │ - NVMe APST / L1.2 Sub-states      │
│ - VRM Phase Shedding    │ - Dynamic DPM Clamping            │ - Linux Laptop Mode 5              │
│ - CPU Boost Cutoff      │ - AMD ABM Panel Power Savings 1-4 │ - 60s Dirty Page Flush Batching    │
│ - 1.4GHz P-State Cap    │ - Physical Backlight 20% Clamp    │ - RAM THP Defrag Interruption Stop │
│ - cpuidle `teo` Gov     │ - KWin 24FPS Frame Limiter        │ - Swappiness & Memory Compaction   │
│ - Timer Migration Stop  │ - KWin Shader Unload (`blur` etc) │ - SCSI/SATA min_power Link Policy  │
├─────────────────────────┼───────────────────────────────────┼────────────────────────────────────┤
│ 4. Motherboard & Bus    │ 5. Communications & Radios        │ 6. ThinkPad Chassis & EC           │
│ - PCIe ASPM PowerSave   │ - Wi-Fi 802.11 DTIM Power Save    │ - EC Platform Profile `low-power`  │
│ - PCIe D3hot Runtime PM │ - Wi-Fi Unconnected Radio Cut     │ - 0 RPM Fan Power Decoupling       │
│ - USB Autosuspend 100ms │ - Ethernet PHY Powerdown & No-WOL │ - Keyboard LED Backlight Dim/Off   │
│ - HD Audio Controller   │ - Bluetooth Baseband Cutoff       │ - 80% Battery Health Conservation  │
│ - Workqueue Power Eff   │ - Mic ADC Analog Circuit Mute     │ - USB-C PD Real-Time Gas Gauge     │
│ - NMI Watchdog Disable  │ - Camera USB Optical Powerdown    │                                    │
├─────────────────────────┴───────────────────────────────────┴────────────────────────────────────┤
│ 7. Background User Services & Session Telemetry                                                  │
│ - ksystemstats / kdeconnectd / ananicy-cpp SIGSTOP Suspension                                     │
│ - avahi-daemon mDNS Multicast Freeze (Wi-Fi DTIM Protection)                                     │
│ - Waydroid Android LXC Container Shutdown & waydroid0 Bridge Down                                │
│ - balooctl6 File Indexing Background Interruption                                                │
└──────────────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Deep Domain Specifications & Hardware Mechanisms

### 3.1 Domain 1: CPU Silicon & AMD SMU Co-Processor

Modern AMD Ryzen APUs (Zen 2 Renoir 4750U through Zen 4/5) contain an on-die **System Management Unit (SMU)** co-processor running proprietary firmware that arbitrates voltages, frequencies, and power distribution across Core, SOC, and GFX rails.

| Hardware Control Parameter | Sysfs / Register Interface | Legacy ThinkPower Value | Native C++23 Re-Engineering |
| :--- | :--- | :--- | :--- |
| **STAPM Limit** | SMU Co-Processor Register | 4,000 mW (Ultra) / 10,000 mW (Save) | Direct SMU Mailbox / `libryzenadj` C-API |
| **Fast PPT Limit** | SMU Co-Processor Register | 5,000 mW (Ultra) / 12,000 mW (Save) | Direct SMU Mailbox / `libryzenadj` C-API |
| **Slow PPT Limit** | SMU Co-Processor Register | 4,000 mW (Ultra) / 10,000 mW (Save) | Direct SMU Mailbox / `libryzenadj` C-API |
| **Tctl Thermal Clamp**| SMU Co-Processor Register | 60°C (Ultra) / 75°C (Save) | Direct SMU Mailbox / `libryzenadj` C-API |
| **VRM Continuous (TDC)**| Motherboard VRM Controller | 12,000 mA (Ultra) / 25,000 mA (Save)| Direct SMU Mailbox / `libryzenadj` C-API |
| **VRM Peak (EDC)** | Motherboard VRM Controller | 16,000 mA (Ultra) / 35,000 mA (Save)| Direct SMU Mailbox / `libryzenadj` C-API |
| **VRM Phase Shed (PSI0)**| Motherboard VRM Controller | 8,000 mA (Ultra) / 15,000 mA (Save) | Forces VRM Buck converter to 1 phase |
| **CPU Turbo Boost** | `/sys/devices/system/cpu/cpufreq/boost` | `0` (Disabled) / `1` (Enabled) | Direct VFS `openat` + single-byte write |
| **CPU Max Frequency** | `/sys/devices/system/cpu/cpu*/cpufreq/scaling_max_freq` | `1400000` (1.4GHz) / `1700000` | Vectorized write across 16 core nodes |
| **CPU Idle Governor** | `/sys/devices/system/cpu/cpuidle/current_governor` | `teo` (Ultra) / `menu` (Balanced) | Single string write (Timer Events Orient) |
| **C-State POLL Disable**| `/sys/devices/system/cpu/cpu*/cpuidle/state0/disable` | `1` (Disables CPU polling loop) | Vectorized write across 16 core nodes |
| **Timer Migration** | `/proc/sys/kernel/timer_migration` | `0` (Prevents cross-core wakeups) | Single byte write (`0` or `1`) |

**Key Hardware Insight**:  
Restricting VRM continuous current (TDC) to 12A and peak current (EDC) to 16A forces the motherboard's multi-phase switching regulator into low-current **Phase Shedding (PSI0)** mode. Instead of keeping 4~6 phases hot, the regulator shuts down idle phases, cutting quiescent switching losses by $> 2.0\text{W}$.

---

### 3.2 Domain 2: Graphics, Display & Wayland Compositor

The display subsystem is typically the single largest non-CPU power consumer in modern laptops (consuming up to $4.3\text{W}$ at full brightness and high refresh rates).

| Hardware Control Parameter | Sysfs / IPC Interface | Legacy ThinkPower Value | Native C++23 Re-Engineering |
| :--- | :--- | :--- | :--- |
| **AMDGPU OverDrive Clock**| `/sys/class/drm/card*/device/pp_od_clk_voltage` | `s 1 640`, `c` (Strict 640MHz Cap)| Direct file descriptor write to DRI node |
| **AMDGPU DPM Level** | `/sys/class/drm/card*/device/power_dpm_force_performance_level` | `manual` (during set) $\rightarrow$ `auto` | Direct file descriptor write |
| **AMD ABM Panel Savings** | `/sys/class/drm/card1-eDP-1/amdgpu/panel_power_savings` | `4` (Max, Ultra) / `2` (Smart Save) | Direct sysfs write (0, 1, 2, 4) |
| **Physical Brightness** | `/sys/class/backlight/*/brightness` | Clamped to 20% of `max_brightness` | Direct pread/pwrite with memory backup |
| **Dynamic Refresh (DRRS)**| KWin Wayland / `kscreen-doctor` | 48.04Hz (Ultra) / 60.06Hz (Default) | Native D-Bus call to `org.kde.KScreen` |
| **KWin Compositor FPS** | `kwriteconfig6 --file kwinrc --group Compositing --key MaxFPS 24` | 24 FPS Cap (Ultra) / Unset (60 FPS)| Native D-Bus reconfigure to `/KWin` |
| **KWin GPU Shader Unload**| `qdbus6 org.kde.KWin /Effects unloadEffect <name>` | Unloads `blur`, `scale`, `slide`, etc. | Native D-Bus method call `unloadEffect` |
| **Zero Animation Factor** | `kwriteconfig6 --file kdeglobals --group KDE AnimationDurationFactor 0` | `0` or `0.2` (Instant rendering) | Native D-Bus notify to `/KWin` |

**Key Hardware Insight**:  
In KDE Plasma 6 Wayland, background Gaussian blur (`blur` shader) requires multiple compute/render passes per frame across all translucent windows (taskbar, terminal, notifications). Dynamically unloading the `blur` effect drops AMDGPU VRAM bandwidth consumption by up to $60\%$ without closing running applications.

---

### 3.3 Domain 3: Storage, Memory & Linux VM Tuning

Modern NVMe SSDs (e.g. Samsung 980 PRO) consume $\sim 5\text{W}$ during active read/write bursts, but drop to **$5\text{mW} \sim 30\text{mW}$** in autonomous power state (APST L1.2 / PS4). However, Linux default flush intervals wake the drive every 5 seconds.

| Kernel Control Node | Default Linux Value | ThinkPower Ultra Setting | Physical Energy Impact |
| :--- | :--- | :--- | :--- |
| `/proc/sys/vm/laptop_mode` | `0` | `5` | Batches disk flushes to coincide with reads |
| `/proc/sys/vm/dirty_writeback_centisecs` | `500` (5 seconds) | `6000` (60 seconds) | Keeps NVMe in PS4 (5mW) for 1-minute blocks |
| `/proc/sys/vm/dirty_expire_centisecs` | `3000` (30 seconds) | `6000` (60 seconds) | Prevents premature expiration of dirty pages |
| `/proc/sys/vm/vfs_cache_pressure` | `100` | `50` | Preserves dentries/inodes in RAM, avoiding SSD reads |
| `/sys/kernel/mm/transparent_hugepage/enabled` | `always` | `madvise` | Prevents kernel worker from churning RAM |
| `/sys/kernel/mm/transparent_hugepage/defrag` | `always` | `never` | Eliminates synchronous memory compaction pauses |
| `/proc/sys/vm/swappiness` | `150` (CachyOS zram) | `60` | Reduces page swap thrashing on battery |

---

### 3.4 Domain 4: Motherboard, Peripheral & Communication Radios

Unused wireless radios and peripheral controllers bleed static leakage current:

1. **Wi-Fi 802.11 DTIM Power Save**:
   - Commanded via `iw dev <dev> set power_save on`.
   - Forces the wireless chipset into Dynamic Traffic Indication Map (DTIM) sleep, waking only during AP beacon intervals ($100\text{ms} \sim 300\text{ms}$), cutting radio draw by $0.4\text{W} \sim 0.65\text{W}$.
2. **Unconnected Radio Baseband Powerdown**:
   - When Wi-Fi is unconnected: `rfkill block wifi` (removes RF synthesizer power).
   - When Bluetooth has no connected devices: `rfkill block bluetooth` (cuts baseband standby draw of $\sim 0.3\text{W}$).
3. **Microphone ADC Analog Circuit Powerdown**:
   - Commanded via PipeWire `wpctl set-mute @DEFAULT_AUDIO_SOURCE@ 1`.
   - Disables the analog-to-digital converter (ADC) preamp circuit on the motherboard audio codec, saving $\sim 0.15\text{W}$.
4. **Webcam Optical Sensor Powerdown**:
   - Commanded via `/sys/bus/usb/devices/*/authorized = 0`.
   - Completely removes USB device authorization, cutting optical standby sensor current.
5. **Ethernet PHY Link Down**:
   - Unlinked Ethernet ports continue drawing $0.3\text{W} \sim 0.8\text{W}$ in auto-negotiation pulses. ThinkPower executes `ip link set <eth> down` and `ethtool -s <eth> wol d` when `carrier == 0`.
6. **PCIe ASPM & Runtime PM**:
   - `/sys/module/pcie_aspm/parameters/policy = powersupersave` enforces L1.1 and L1.2 PCIe link low-power states.
   - `/sys/bus/pci/devices/*/power/control = auto` and `autosuspend_delay_ms = 100` drops PCIe bridges into D3hot.

---

### 3.5 Domain 5: ThinkPad Embedded Controller (EC) & Chassis

The ThinkPad Embedded Controller (EC) manages hardware thermal fan curves and battery cell protection:
1. **Platform Profile Low-Power**:
   - Writing `low-power` to `/sys/firmware/acpi/platform_profile` commands the EC to adjust its internal thermal trip points.
   - Because fan electrical power scales cubically ($P \propto \text{RPM}^3$), bringing fan speed from 3,200 RPM down to 0 RPM saves **$0.8\text{W} \sim 1.7\text{W}$** directly.
2. **ThinkPad Battery Conservation Mode**:
   - Reading `/sys/class/power_supply/BAT0/charge_control_end_threshold` (typically 80%).
   - Calculates time remaining until 80% rather than 100%, preventing Li-ion voltage stress ($> 4.2\text{V}$ per cell).

---

## 4. Architectural Critique: Legacy Bash vs. Native C++23 Re-Engineering

| Architectural Dimension | Legacy ThinkPower (`power-profile-manager`) | Re-Engineered WattCurb Daemon |
| :--- | :--- | :--- |
| **Execution Engine** | 1,123-line Bash Script | Native C++23 Event Loop (`timerfd`/`epoll`) |
| **Syscall Mechanism** | Fork/exec hundreds of subshells (`cat`, `echo`, `sudo`) | Direct VFS descriptors (`openat`, `pwrite`, `ioctl`) |
| **CPU Wakeup Tax** | High ($> 50$ process spawns per transition) | **Zero-Wakeup** (Strictly event-driven) |
| **Memory Footprint** | Dozens of transient bash instances ($> 20\text{ MB}$) | **300 KB flat** static binary, zero dynamic heap |
| **Desktop IPC Protocol** | Shell-invoked CLI binaries (`qdbus6`, `kscreen-doctor`)| Native `sd-bus` / direct D-Bus socket wire calls |
| **Control Paradigm** | Static, coarse-grained 4-stage profiles | **Adaptive Closed-Loop** (Process WDI + Domain Power)|
| **Failsafe Guarantees**| Flat `.cache` text files (`prev_display_brightness`) | In-memory atomic state restoration structures |

---

## 5. WattCurb Re-Engineering Blueprint: Modular Battery Features FEAT-008 ~ FEAT-015

To absorb ThinkPower's full capabilities without violating WattCurb's zero-overhead design rules, we specify 8 new C++23 modular battery features for [`src/policy/battery_feature.cpp`](file:///home/jedclub/Develop/WattCurb/src/policy/battery_feature.cpp):

```cpp
// 1. FEAT-008: AmdSmuTdpClamper
// Directly interfaces with AMD SMU co-processor (STAPM 4W, Fast PPT 5W, VRM 12A/16A).
class AmdSmuTdpClamper : public IBatteryFeature;

// 2. FEAT-009: DisplayAbmGovernor
// Dynamically adjusts /sys/class/drm/card1-eDP-1/amdgpu/panel_power_savings (0 to 4).
class DisplayAbmGovernor : public IBatteryFeature;

// 3. FEAT-010: StorageLaptopModeBatcher
// Sets /proc/sys/vm/laptop_mode=5 and dirty_writeback_centisecs=6000 (60s NVMe sleep).
class StorageLaptopModeBatcher : public IBatteryFeature;

// 4. FEAT-011: KWinWaylandRenderGovernor
// Emits native D-Bus calls to KWin to unload blur shaders and enforce 24FPS limit.
class KWinWaylandRenderGovernor : public IBatteryFeature;

// 5. FEAT-012: AmdGpuOverDriveCap
// Writes "s 1 640\nc\n" to /sys/class/drm/card*/device/pp_od_clk_voltage (640MHz ceiling).
class AmdGpuOverDriveCap : public IBatteryFeature;

// 6. FEAT-013: ThinkPadEcPlatformProfile
// Writes "low-power" to /sys/firmware/acpi/platform_profile for 0 RPM fan decoupling.
class ThinkPadEcPlatformProfile : public IBatteryFeature;

// 7. FEAT-014: PeripheralRadioGovernor
// Manages Wi-Fi 802.11 DTIM power save and blocks idle Bluetooth baseband.
class PeripheralRadioGovernor : public IBatteryFeature;

// 8. FEAT-015: BackgroundServiceSuspender
// Sends direct SIGSTOP / SIGCONT syscalls to ksystemstats, kdeconnectd, and baloo.
class BackgroundServiceSuspender : public IBatteryFeature;
```

---

## 6. Verification & Oracle Gate Standards

1. **Zero-Subshell Invariant**:
   - Re-engineered features must **never invoke `system()` or `popen()`**. Every sysfs, procfs, and D-Bus interaction must use native POSIX system calls (`openat`, `write`, `read`, `ioctl`) or native D-Bus libraries.
2. **Zero-Allocation Hot Path**:
   - Activation and deactivation loops must operate strictly within static memory buffers (`FixedString`, `FixedVector`, stack buffers).
3. **Atomic State Backup & Restoration**:
   - All overridden settings (brightness, CPU frequencies, KWin effects) must be recorded in statically allocated memory structures and automatically restored upon AC reconnect or daemon clean exit (`SIGTERM`).

---

## 7. Document Cross-References
- Source Project: ThinkPower v1.1.0 ([`README.md`](file:///home/jedclub/Develop/thinkpower/README.md))
- Hardware Tuning Reference: [`docs/02-HARDWARE-TUNING.md`](file:///home/jedclub/Develop/thinkpower/docs/02-HARDWARE-TUNING.md)
- Extreme Power Guide: [`docs/05-EXTREME-POWER-SAVING-GUIDE.md`](file:///home/jedclub/Develop/thinkpower/docs/05-EXTREME-POWER-SAVING-GUIDE.md)
- WattCurb Modular Framework: [`REF-ARCH-009`](../architecture/ARCH-009-modular-optimization-feature-framework.md)
- WattCurb Requirements: [`REF-REQ-020`](../requirements/REQ-017-detailed-briefing-and-modular-optimization-features.md)
