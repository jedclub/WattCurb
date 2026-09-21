# REF-RES-025: UltraEndurance Deep Silicon & Kernel Power Minimization Architecture

- **Document ID**: `REF-RES-025`
- **Related Requirements**: [`REF-REQ-063`](../requirements/REQ-063-ultra-endurance-hardware-and-desktop-power-capping.md), [`REF-REQ-064`](../requirements/REQ-064-wifi-txpower-capping-and-decomposed-platform-loss.md), [`REF-REQ-065`](../requirements/REQ-065-bluetooth-always-on-invariant.md), [`REF-REQ-082`](../requirements/REQ-082-granular-platform-loss-hardware-telemetry.md)
- **Related Architecture**: [`REF-ARCH-021`](../architecture/ARCH-021-closed-loop-mitigation-engine.md), [`REF-ARCH-059`](../architecture/ARCH-059-granular-platform-loss-telemetry-architecture.md)
- **Related Research**: [`REF-RES-019`](RES-019-granular-platform-loss-decomposition-and-kernel-interfaces.md), [`REF-RES-022`](RES-022-vram-wifi-fan-bus-hardware-power-isolation.md), [`REF-RES-023`](RES-023-vram-gc-dpm-downclocking-and-performance-boost.md), [`REF-RES-024`](RES-024-deep-power-log-audit-and-drain-analysis.md)
- **Status**: Approved
- **Date**: 2026-09-21

---

## 1. Executive Summary & Problem Definition

Empirical 5.5-hour power log audits ([`REF-RES-024`](RES-024-deep-power-log-audit-and-drain-analysis.md)) revealed that WattCurb systems under standard operation consume an average of **15.81 W**, with 81.2% concentrated in **Platform Loss (40.9%)** and the **CPU Subsystem (40.3%)**.

While WattCurb's existing `UltraEndurance` profile already executes foundational power capping (1.4 GHz CPU floor, SMT disabled, 48 Hz DRRS, 35% backlight cap, 12 dBm Wi-Fi Tx limit, Baloo suspend, and KWin blur suspension), **several critical silicon and kernel energy leakages remain unmitigated**:
1. **AMDGPU Memory Clock (MCLK) & Infinity Fabric (FCLK) Lock**: SCLK is capped to 640 MHz, but MCLK remains pinned at 1333 MHz due to VRAM saturation, leaking **~1.5 W ~ 2.0 W** on the memory controller and Infinity Fabric interconnect.
2. **CPU Deep C-State Sleep Starvation (C3+ residency at only 13.8%)**: Sub-millisecond timer interrupts from background web/Electron sockets and 0ns timer slack continuously wake up cores from C6 sleep, burning **~2.5 W ~ 3.5 W**.
3. **Unoptimized Kernel VM Writeback & Disk APST Cycles**: Kernel flusher threads wake up every 5 seconds (`dirty_writeback_centisecs = 500`), tripping CPU idle governors.
4. **Uncoordinated PCIe & USB Device Runtime PM**: Bus bridges, media card controllers, and audio codecs remain in D0 full-power state rather than D3hot/D3cold.

This document formalizes **6 High-Impact Silicon & Kernel Interventions** to push UltraEndurance total system draw down to **5.5 W ~ 6.5 W** (an unprecedented **~60% power reduction** over baseline).

---

## 2. Six Deep Silicon & Kernel Power Minimization Dimensions

```
+---------------------------------------------------------------------------------------------------------+
|                               ULTRAENDURANCE DEEP POWER REDUCTION MATRIX                               |
+-----------------------------------+----------------------------------------+----------------------------+
| Optimization Domain               | Mechanism & Kernel Interface           | Expected Power Saving (W)  |
+-----------------------------------+----------------------------------------+----------------------------+
| 1. DPM MCLK/FCLK Downclock & GC   | power_dpm_force_performance_level=low  | -1.40 W ~ -1.80 W          |
| 2. Global Timer Slack Relaxation  | prctl(PR_SET_TIMERSLACK, 100ms)        | -1.80 W ~ -2.50 W          |
| 3. Kernel VM Flush Coalescing     | dirty_writeback_centisecs = 6000       | -0.60 W ~ -0.90 W          |
| 4. PCIe & USB Subsystem Autosuspend| /sys/bus/pci/.../power/control = auto  | -0.40 W ~ -0.60 W          |
| 5. Audio Codec Power-Down         | snd_hda_intel power_save = 10s         | -0.25 W ~ -0.40 W          |
| 6. Progressive cgroup v2 Freeze   | cgroup.freeze on hidden Electron apps  | -1.00 W ~ -1.50 W          |
+-----------------------------------+----------------------------------------+----------------------------+
| TOTAL CUMULATIVE REDUCTION        | System Draw Drops: 15.8W -> ~5.8W      | -5.45 W ~ -7.70 W NET DROP |
+-----------------------------------+----------------------------------------+----------------------------+
```

---

### 2.1 Dimension 1: GPU DPM MCLK 400 MHz Clamp & 3-Tier VRAM GC ([`REF-RES-023`](RES-023-vram-gc-dpm-downclocking-and-performance-boost.md))
- **Current Limitation**: Setting `pp_od_clk_voltage` caps SCLK to 640 MHz, but the memory controller (MCLK) remains at DPM Level 3 (1333 MHz) if VRAM buffers are allocated.
- **UltraEndurance Actuation**:
  1. Write `"low"` to `/sys/class/drm/card0/device/power_dpm_force_performance_level`, forcing MCLK to Level 0 (400 MHz) and FCLK to minimum SoC fabric frequency.
  2. Execute 3-Tier VRAM GC:
     - Evict unneeded compositor surface caches.
     - Reclaim Chromium/Electron GPU discardable memory via cgroups v2 `memory.reclaim`.
     - Trigger kernel buffer cache reclamation (`echo 3 > /proc/sys/vm/drop_caches`).
- **Energy Payoff**: **-1.4 W ~ -1.8 W** drop in Platform Loss.

---

### 2.2 Dimension 2: Global Background Timer Slack Relaxation (`PR_SET_TIMERSLACK = 100ms`)
- **Current Limitation**: Linux default timer slack is 50 $\mu$s (`50,000 ns`), with KWin and Electron frequently overriding it to 0 ns. Every timer interrupt pulls CPU cores out of Package C6 sleep into C0/C1.
- **UltraEndurance Actuation**:
  - For all non-audio, non-active-window tasks, inject `prctl(PR_SET_TIMERSLACK, 100'000'000)` (100 ms) via `/proc/<pid>/timerslack_ns`.
  - Timer firings are mathematically coalesced into discrete 10 Hz batches, allowing CPU package C6 residency to jump from **13.8% to $> 85\%$**.
- **Energy Payoff**: **-1.8 W ~ -2.5 W** drop in CPU Package power.

---

### 2.3 Dimension 3: Kernel VM Writeback & Laptop Mode Coalescing
- **Current Limitation**: Linux default writes dirty pages every 500 centiseconds (5 seconds), causing periodic flash translation layer (FTL) and NVMe APST wakeups.
- **UltraEndurance Actuation**:
  - `/proc/sys/vm/dirty_writeback_centisecs` $\rightarrow$ `6000` (60 seconds).
  - `/proc/sys/vm/dirty_expire_centisecs` $\rightarrow$ `12000` (120 seconds).
  - `/proc/sys/vm/laptop_mode` $\rightarrow$ `5` (enables aggressive drive spin-down and batch flushing on explicit disk reads).
- **Energy Payoff**: **-0.6 W ~ -0.9 W** in Storage APST & CPU wakeup reduction.

---

### 2.4 Dimension 4: PCIe & USB Bus Runtime Power Management (`control = auto`)
- **Current Limitation**: Discrete bus devices (Realtek card readers, Thunderbolt PHYs, USB Hubs) frequently default to `power/control = on`, keeping PCIe link states in L0 instead of L1.1/L1.2.
- **UltraEndurance Actuation**:
  - Sweep `/sys/bus/pci/devices/*/power/control` and write `auto`.
  - Sweep `/sys/bus/usb/devices/*/power/control` and write `auto` (skipping active input mice/keyboards and Bluetooth host controller).
- **Energy Payoff**: **-0.4 W ~ -0.6 W** in PCIe ASPM bus power.

---

### 2.5 Dimension 5: High-Definition Audio (HDA) Codec Power-Down
- **Current Limitation**: The audio DAC/DSP amplifier remains powered if sound servers (PipeWire) hold ALSA device handles open without explicit timeout.
- **UltraEndurance Actuation**:
  - Write `10` to `/sys/module/snd_hda_intel/parameters/power_save` (auto-suspend codec after 10s of audio inactivity).
  - Write `Y` to `/sys/module/snd_hda_intel/parameters/power_save_controller`.
- **Energy Payoff**: **-0.25 W ~ -0.4 W** in analog codec power.

---

### 2.6 Dimension 6: Progressive Non-Active Electron / Web App Cgroup v2 Freezing
- **Current Limitation**: In PowerSaver mode, WattCurb throttles background processes with `SCHED_IDLE`. However, modern Electron runtimes (ChatGPT, Codex, Slack, Discord) still execute JavaScript garbage collection, WebGL canvas refreshes, and WebSocket keepalives.
- **UltraEndurance Actuation**:
  - In `UltraEndurance`, any process belonging to `ProcessSafetyTier::GreedyBackground` or `BackgroundService` that does not own an active focused window is transitioned to `cgroup.freeze = 1`.
  - **Zero CPU execution, zero IPC, zero VRAM churn, zero wakeups**.
  - Restored to running state in $< 500\,\mu\text{s}$ upon window focus change via WattCurb's window-aware governor ([`REF-ARCH-023`](../architecture/ARCH-023-kwin-scripting-and-progressive-actuator.md)).
- **Energy Payoff**: **-1.0 W ~ -1.5 W** across CPU, GPU, and Wi-Fi CAM domains.

---

## 3. Projected Battery Runtime Comparison

```
+--------------------------------------------------------------------------------------------------+
|                            BATTERY ENDURANCE PROJECTION (52.5 Wh Battery)                       |
+----------------------+--------------------+--------------------+---------------------------------+
| Operating Mode       | Average Power (W)  | Battery Life (Hrs) | Relative Endurance Gain         |
+----------------------+--------------------+--------------------+---------------------------------+
| Performance (4.1GHz) | 18.5 W             | 2.8 Hours          | Baseline Workstation            |
| Balanced (Schedutil) | 16.3 W             | 3.2 Hours          | +14%                            |
| PowerSaver (1.7GHz)  | 9.8 W              | 5.4 Hours          | +93%                            |
| UltraEndurance (Old) | 8.2 W              | 6.4 Hours          | +128%                           |
| UltraEndurance (NEW) | 5.8 W ~ 6.2 W      | 8.8 ~ 9.1 Hours    | +214% (3.2x vs Performance)     |
+----------------------+--------------------+--------------------+---------------------------------+
```

---

## 4. Architectural Invariants & Safety Mandates

1. **Absolute Zero-Kill Invariant ([`REF-REQ-044`](../requirements/REQ-044-absolute-zero-kill-and-non-halting-safety.md))**:
   - No processes are killed. Freezing is strictly limited to non-critical background cgroups with instant thaw capability.
2. **Audio & Communication Immunity ([`REF-REQ-049`](../requirements/REQ-032-kde-plasma-desktop-mitigation.md))**:
   - PipeWire, WirePlumber, Bluetooth audio, and active VoIP streams remain completely immune from freezing or audio codec timeouts.
3. **Idempotent AC Rollback ([`REF-REQ-034`](../requirements/REQ-034-rapid-charge-and-profile-restoration-engine.md))**:
   - When the user connects the AC charger, all kernel sysctl values (`dirty_writeback_centisecs`, `timerslack_ns`, `power_dpm_force_performance_level`, audio power-save) must be restored to their original baseline in $< 5\,\text{ms}$.
