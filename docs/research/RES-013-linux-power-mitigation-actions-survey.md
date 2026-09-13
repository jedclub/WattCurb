# [REF-RES-013] Linux Power Mitigation Actions & Open Source Landscape Survey

## 1. Executive Summary & Research Motivation

WattCurb has achieved sub-microsecond physical hardware telemetry (RAPL, Battery gas gauge, GPU hwmon, Display, NVMe, Fan) and zero-allocation process attribution (CPU time, wakeups, GPU VRAM/engine, I/O ticks, WDI score). 

However, **telemetry and attribution are only diagnostic primitives**. To fulfill WattCurb's mission of maximizing host battery life and enforcing sub-milliwatt daemon overhead, the daemon must possess an **actionable catalog of real-world mitigation vectors**.

This research surveys:
1. The modern Linux kernel actuation mechanisms (cgroups v2, CPU schedulers, sysfs power knobs, VFS writeback, runtime PM).
2. Existing open-source power management projects (TLP, auto-cpufreq, power-profiles-daemon, ananicy-cpp, powertop, systemd-oomd).
3. Mobile and embedded power architectures (Android EAS/uclamp/Doze, ChromeOS/SteamOS TDP & refresh rate scaling).
4. Concrete functional dimensions and specific actuation primitives suitable for WattCurb's zero-wakeup C++23 architecture.

---

## 2. Deep Comparative Survey: Open-Source Power & Process Projects

| Project | Primary Focus | Strengths | Critical Limitations vs. WattCurb |
| :--- | :--- | :--- | :--- |
| **TLP** | Static hardware sysfs tuning on AC/Battery events | Most exhaustive catalog of sysfs knobs (PCIe ASPM, USB autosuspend, SATA ALPM, WiFi PS, Audio power_save, ThinkPad charge thresholds). | **Completely blind to running processes.** Cannot detect or mitigate runaway background drain. Script-based, lacks dynamic closed-loop telemetry. |
| **auto-cpufreq** | CPU governor & EPP switching based on load average | Automatically switches between `powersave` and `performance` based on CPU usage and thermals. | **Polling wakeups & high daemon overhead** (Python). Only adjusts global CPU clock; cannot throttle or freeze specific background culprits. |
| **power-profiles-daemon (PPD)** | Desktop (GNOME/KDE) platform profile integration | Standard D-Bus API for ACPI `platform_profile` (`balanced`, `power-saver`, `performance`). | Very coarse-grained. No process-level awareness, no peripheral hardware scaling (PCIe/USB/Storage/WiFi). |
| **ananicy-cpp** | Process responsiveness & scheduler auto-nice | Excellent community process rule database; sets `nice`, `SCHED_IDLE`, `ionice`, and CPU core affinity (P/E cores, X3D). | **Designed for gaming/responsiveness, not energy conservation.** Lacks battery awareness, RAPL/GPU attribution, and hardware power scaling. |
| **powertop** | Interactive diagnosis & `--auto-tune` batch script | Accurate C-state and wakeup diagnostic counters; one-click sysfs tuning script. | Interactive tool, not a daemon. Auto-tune is static and brute-force; lacks adaptive process mitigation or runtime rollback. |
| **SteamOS / Gamescope** | Handheld gaming power optimization | RAPL TDP limiting (3W~15W), GPU clock limits, Dynamic Refresh Rate Switching (60Hz $\rightarrow$ 40Hz). | Tailored exclusively for fullscreen gaming handhelds; not suitable as a general-purpose Linux background daemon. |
| **Chrome Energy Saver / Android Doze** | Background tab & app energy suspension | Freezes inactive JS execution, coalesces network/timers, aggressively reclaims memory. | Confined to a single browser or OS container; cannot govern system-wide Linux native background daemons. |

---

## 3. The 4 Functional Dimensions of Linux Actuation

Through extensive kernel interface investigation, WattCurb categorizes all possible mitigation actions into **4 distinct functional dimensions**:

```
                                  ┌─────────────────────────────────────────┐
                                  │      WattCurb Actuation Taxonomy        │
                                  └────────────────────┬────────────────────┘
                                                       │
         ┌─────────────────────────┬───────────────────┴─────────────────────┬─────────────────────────┐
         ▼                         ▼                                         ▼                         ▼
┌──────────────────┐      ┌──────────────────┐                      ┌──────────────────┐      ┌──────────────────┐
│   Dimension 1    │      │   Dimension 2    │                      │   Dimension 3    │      │   Dimension 4    │
│  Process Micro-  │      │  Non-Destructive │                      │ Hardware Domain  │      │  Kernel Wakeup & │
│   Scheduling     │      │ Resource Reclaim │                      │ Dynamic Scaling  │      │  VFS Suppression │
└────────┬─────────┘      └────────┬─────────┘                      └────────┬─────────┘      └────────┬─────────┘
         │                         │                                         │                         │
  • SCHED_IDLE              • cgroup.freeze (D-State)                 • CPU EPP / Boost         • NMI Watchdog (0)
  • timerslack_ns           • memory.reclaim                          • RAPL Package TDP        • dirty_writeback
  • CPU Affinity (E-Core)   • madvise(MADV_PAGEOUT)                   • GPU DPM / ABM           • timer_migration
  • cgroup uclamp.max       • ZRAM Swap Compression                   • PCIe ASPM / RPM         • Audio & WiFi PS
```

---

### Dimension 1: Process Micro-Scheduling & Throttling

When a background process (Tier 4) or runaway candidate (Tier 5) drains power, terminating it (`SIGKILL`) risks user data loss. Instead, micro-scheduling forces the process to yield hardware resources:

1. **`SCHED_IDLE` Policy Enforcement**:
   - `sched_setscheduler(pid, SCHED_IDLE, &sp)`: Assigns the absolute lowest scheduling weight. The process runs **only when the CPU would otherwise be 100% idle**.
   - Result: Interactive desktop tasks suffer zero latency, and background threads cannot prevent the CPU from entering deep C-states when other tasks sleep.
2. **Timer Slack Relaxation (Timer Coalescing)**:
   - Linux processes wake up frequently due to fine-grained timers (`epoll_wait`, `select`, `nanosleep`).
   - Writing `100000000` (100ms) or `1000000000` (1s) to `/proc/[pid]/timerslack_ns` coalesces timer interrupts, allowing the CPU package to remain in C6/C8 deep sleep states up to 10x longer.
3. **Core Pinning & CCX Confinement (Cache-Thrashing Suppression)**:
   - On multi-CCX AMD processors (e.g. Zen 3/4/5) and Intel Hybrid architectures (P-cores vs E-cores):
     - Background workers bouncing across CCX domains cause cross-L3 cache invalidations and wake up multiple physical core complexes.
     - Using `sched_setaffinity(pid, &mask)` to lock background workers to a single core or E-core cluster confines thermal load and allows untouched cores to power down completely.
4. **Utilization Clamping (`uclamp.max`)**:
   - Linux 5.11+ cgroup v2 controller `cpu.uclamp.max`: Clamps the task's utilization signal perceived by the `schedutil` governor.
   - Setting `cpu.uclamp.max = 100` (out of 1024) ensures that even if a background thread consumes 100% of a core, the CPU frequency governor **will not boost the clock frequency or core voltage**, capping dynamic power consumption ($P = C \cdot V^2 \cdot f$).

---

### Dimension 2: Non-Destructive Freezing & Memory Reclamation

1. **cgroup v2 Transparent Freezing (`cgroup.freeze`)**:
   - Traditional `kill(pid, SIGSTOP)` can be caught, causes ptrace issues, and breaks signal handling.
   - Writing `1` to `/sys/fs/cgroup/<path>/cgroup.freeze` moves all threads in the cgroup into the kernel's `FROZEN` state.
   - Advantages: 100% CPU/GPU consumption drops to zero; instant zero-overhead resumption when writing `0` upon user focus or AC connection.
2. **Proactive Memory Reclamation (`memory.reclaim`)**:
   - Linux 5.19+ feature `/sys/fs/cgroup/<path>/memory.reclaim`.
   - Writing `bytes` (e.g. `67108864` for 64MB) forces the kernel to reclaim clean file-backed pages and page-out idle anonymous memory to ZRAM/swap.
   - Advantages: Reduces the active memory footprint, allowing DRAM modules to stay in Self-Refresh low-power states longer.

---

### Dimension 3: Physical Hardware Domain Dynamic Scaling

Hardware knobs directly reduce the baseline static and dynamic power draw of silicon domains:

1. **CPU & Platform Subsystem**:
   - **Energy Performance Preference (EPP)**:
     - `/sys/devices/system/cpu/cpu*/power/energy_performance_preference`: Set to `power` or `balance_power` on battery.
   - **Turbo Boost Disabling**:
     - `/sys/devices/system/cpu/cpufreq/boost`: Setting to `0` cuts off high-voltage thermal spikes, reducing peak CPU power by 30% ~ 50% during bursty operations.
   - **RAPL Package Power Capping**:
     - `/sys/class/powercap/intel-rapl/intel-rapl:0/constraint_0_power_limit_uw`: Hard limits the SoC package power (e.g. 10W or 15W) at the hardware register level under critical battery conditions.
   - **ACPI Platform Profile**:
     - `/sys/firmware/acpi/platform_profile`: Sets system EC firmware to `low-power` or `quiet`.
2. **Graphics Subsystem (AMDGPU / Intel / NVIDIA)**:
   - **AMDGPU DPM States**:
     - `/sys/class/drm/card*/device/power_dpm_force_performance_level`: `low` or `auto`.
     - `/sys/class/drm/card*/device/power_dpm_state`: `battery`.
   - **Adaptive Backlight Modulation (ABM)**:
     - `/sys/class/drm/card*/device/amdgpu/abm_level`: Level 1~4 enables hardware pixel compensation, allowing the physical backlight to dim by 20%~35% without perceived contrast degradation.
3. **Display & Backlight**:
   - Backlight soft-capping via `/sys/class/backlight/*/brightness`.
   - Dynamic display refresh rate scaling (60Hz $\rightarrow$ 40Hz) via KMS/Wayland interface.
4. **Storage & Bus Interfaces**:
   - **PCIe ASPM**: `/sys/module/pcie_aspm/parameters/policy` $\rightarrow$ `powersave` (L1/L1.1/L1.2 sub-states).
   - **PCIe Runtime PM**: Set `power/control` to `auto` across all `/sys/bus/pci/devices/*`.
   - **SATA ALPM**: `/sys/class/scsi_host/host*/link_power_management_policy` $\rightarrow$ `med_power_with_dipm` or `min_power`.
5. **Peripherals & Codecs**:
   - **Audio Codec Power Save**: `/sys/module/snd_hda_intel/parameters/power_save = 1` (powers down audio DAC/ADC after 1s of silence, saving ~1W).
   - **WiFi 802.11 Power Save**: `iw dev <dev> set power_save on` (enables beacon listening sleep cycles).
   - **USB Autosuspend**: `/sys/bus/usb/devices/*/power/control` $\rightarrow$ `auto`.

---

### Dimension 4: Kernel Wakeup Storm & VFS Suppression

1. **NMI Watchdog Suppression**:
   - `/proc/sys/kernel/nmi_watchdog = 0`: Disables hardware performance counter overflow interrupts on every CPU core, preventing periodic artificial wakeups.
2. **VFS Dirty Page Writeback Extension**:
   - `/proc/sys/vm/dirty_writeback_centisecs`: Default is 500 (5s). On battery, extend to `6000` (60s).
   - `/proc/sys/vm/laptop_mode = 5`: Forces Linux to coalesce disk writes with mandatory reads, allowing NVMe SSDs and SATA drives to enter deep APST/ALPM sleep states for minutes at a time.
3. **Timer Migration Enablement**:
   - `/proc/sys/kernel/timer_migration = 1`: Migrates kernel timers to currently active CPU cores rather than waking up sleeping cores.

---

## 4. Synthesis: Roadmap for WattCurb Actuation Enhancements

Based on this comprehensive survey, WattCurb will expand its actuation capabilities across four progressive milestone phases:

| Phase | Target Capability | Actuation Knobs / Syscalls | Expected Power Impact |
| :--- | :--- | :--- | :--- |
| **Phase 1 (Current: M28)** | 3-Tier State Machine & Process Basics | `SCHED_IDLE`, `timerslack_ns`, `cgroup.freeze`, `memory.reclaim`, PCIe ASPM, EPP, Display Cap, Bidirectional Rollback. | **1.5W ~ 3.5W reduction** on active desktop |
| **Phase 2 (Hardware Scaling)** | Full Peripheral & Firmware Scaling | AMDGPU DPM/ABM, Audio codec power_save, WiFi 802.11 PS, ACPI `platform_profile`, CPU Boost disable. | **0.8W ~ 1.5W additional baseline drop** |
| **Phase 3 (VFS & Wakeup)** | Wakeup Storm & Timer Coalescing | `nmi_watchdog = 0`, `dirty_writeback = 60s`, `laptop_mode = 5`, Core Pinning (CCX confinement). | **+15% ~ 25% longer C-State residence** |
| **Phase 4 (Advanced cgroup)** | Micro-Quota & Utilization Clamping | `cpu.uclamp.max`, `cpu.max` quota, `memory.high`. | **Elimination of thermal runaway spikes** |

---

## 5. Architectural Integrity & Safety Invariants

1. **Strict Immunity Invariant (`CriticalImmune` & `DesktopCore`)**:
   - No actuation in any dimension may touch display compositors (`kwin_wayland`, `mutter`), audio servers (`pipewire`), or system bus (`dbus`, `systemd`).
2. **Zero-Residual Rollback Guarantee**:
   - All sysfs modifications, cgroup freezes, scheduler changes, and timer slack relaxations must be registered in the tracked ring buffer and restored upon AC plug-in or profile deactivation.
3. **Zero-Allocation Execution**:
   - All actuation routines must execute using direct POSIX syscalls (`open`, `write`, `sched_setscheduler`) with fixed stack buffers, strictly adhering to WattCurb's zero-heap design principle.
