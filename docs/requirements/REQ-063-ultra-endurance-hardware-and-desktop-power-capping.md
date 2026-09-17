# REQ-063: Ultra-Endurance Deep Hardware & Plasma Desktop Power Capping

- **Ref-ID**: `REF-REQ-063`
- **Category**: Deep Power Capping & Desktop Environment Integration
- **Title**: Ultra-Endurance Profile Extensions: SMT Offlining, Display Brightness Capping, DRRS 48Hz, PCIe SuperSave, Bluetooth Radio Power-Down, and KWin/Baloo Suppression
- **Status**: Approved
- **Domain**: Kernel sysfs, SMT/HyperThreading, Display DRRS, Backlight PWM, rfkill, KDE Plasma 6 / KWin D-Bus, Baloo
- **Dependencies**: [`REF-REQ-044`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-044-absolute-zero-kill-and-non-halting-safety.md), [`REF-REQ-055`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-055-pre-transition-state-journaling-and-profile-enforcement.md), [`REF-REQ-058`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-058-ultra-endurance-frequency-capping-and-zero-io-logging-audit.md), [`REF-ARCH-031`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-031-dual-domain-state-journal-and-profile-actuation.md)

---

## 1. Problem Statement & Design Rationale

While the baseline Ultra-Endurance profile constrained CPU frequency to 1.4GHz and capped GPU clock to 40% (640MHz), modern laptop platforms exhibit significant additional static and active power dissipation across uncore, display, and desktop graphical subsystems:
1. **Unconstrained Simultaneous Multithreading (SMT)**: Running 16 logical threads on an 8-core CPU keeps rename buffers, reorder buffers, and scheduling queues active, dissipating ~0.5W–0.8W of leakage and causing unnecessary scheduler wakeups across sibling threads.
2. **Display Over-Illumination & 60Hz Panel Link Overhead**: Display backlight represents up to 40% of total system draw. Uncapped 65% brightness dissipates ~2.5W. Furthermore, driving the eDP link at 60.06Hz incurs 20% higher pixel clock power than the panel's native 48.04Hz mode.
3. **Idle Peripheral Leakage**: Bluetooth radios continually transmit beacons and listen for page scans (~0.3W–0.5W) even when no peripherals are connected. PCIe ASPM `powersave` lacks L1.1/L1.2 substate enforcement supported by `powersupersave`.
4. **Desktop GUI Compositing & Indexing Waste**: KWin blur shaders and desktop animations demand continuous 3D GPU clock bursts, while Baloo file indexing awakens NVMe storage and CPU cores periodically.

To maximize battery runtime under extreme constraint, the Ultra-Endurance profile must execute coordinated, deep hardware and desktop power suppression with **100% faithful restoration** upon profile de-escalation or AC reconnection.

---

## 2. Functional Requirements

### 2.1 Hardware Silicon & CPU Subsystem Suppression (`REF-REQ-063-F1`)
1. **SMT (Hyper-Threading) Dynamic Offlining**:
   - Under Ultra-Endurance, the daemon writes `off` to `/sys/devices/system/cpu/smt/control`, placing logical sibling threads offline.
   - Restricts active execution to pure physical cores, eliminating pipeline leakage and halving wakeup distribution overhead (~0.5W–0.8W saving).
   - Upon restoring Balanced or Performance profiles, `/sys/devices/system/cpu/smt/control` is restored to `on` (or its baseline state).
2. **PCIe ASPM SuperSave Enforcement**:
   - Under Ultra-Endurance, ASPM policy is escalated from `powersave` to `powersupersave` (`/sys/module/pcie_aspm/parameters/policy`).
   - Enables PCIe L1.1 and L1.2 deep low-power substates across all root complexes and NVMe endpoints.

### 2.2 Display Backlight & Dynamic Refresh Rate Switching (DRRS) (`REF-REQ-063-F2`)
1. **Display Backlight Capping & Dimming Floor**:
   - Backlight brightness is capped at **35%** of `max_brightness`. If the current brightness exceeds 35%, it is immediately reduced to the 35% cap.
   - Baseline brightness is recorded prior to clamping; upon mode exit or AC plug-in, the user's original brightness is faithfully restored.
2. **DRRS 48Hz Panel Frequency Scaling**:
   - The embedded DisplayPort (eDP) panel is transitioned to its low-power **48.04Hz** hardware mode via `kscreen-doctor output.1.mode.2`.
   - Restores native **60.06Hz** (`output.1.mode.1`) upon de-escalation.

### 2.3 Radio Frequency & Peripheral Power-Down (`REF-REQ-063-F3`)
1. **Bluetooth RF Hardware Block**:
   - Bluetooth RF kill switches (`/sys/class/rfkill/rfkill*/type == "bluetooth"`) are set to soft-blocked (`soft = 1`), powering down the RF amplifier.
   - Unblocked upon restoring standard profiles.

### 2.4 KDE Plasma Desktop & Compositor Suppression (`REF-REQ-063-F4`)
1. **KWin Blur Shader & Effects Unloading**:
   - Ingests user session Wayland connection (`XDG_RUNTIME_DIR=/run/user/1000`) and calls `qdbus6 org.kde.KWin /Effects unloadEffect blur` via asynchronous non-blocking dispatch.
   - Eliminates GPU 3D rendering spikes and freezes GPU core clock at idle floor.
2. **Baloo File Indexer Suspension**:
   - Invokes `balooctl6 suspend` to eliminate background VFS scans and disk wakeups.
   - Resumed (`balooctl6 resume`) upon profile restoration.

---

## 3. Verification & Oracle Gate Standards (`REF-TEST-028`)

1. **Deterministic Actuation Roundtrip (< 100ms)**:
   - Transition into UltraEndurance and full restoration back to Balanced must complete in under **100ms** total roundtrip latency (`REF-TEST-028`).
2. **Non-Blocking Daemon Guarantee**:
   - Desktop and session-level command dispatches must execute asynchronously in the background (`&`), maintaining zero blocking and zero jitter on the daemon's 3.0s monitoring cadence.
3. **100% Faithful Baseline Restoration Invariant**:
   - Every hardware attribute (SMT, Bluetooth rfkill, Backlight brightness, DRRS refresh rate, KWin blur, Baloo state) must achieve identical bit-for-bit parity with pre-transition baselines.
