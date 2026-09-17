# ARCH-039: Deep Hardware & Plasma Desktop Capping Architecture

- **Ref-ID**: `REF-ARCH-039`
- **Category**: Subsystem Architecture & Desktop Power Actuation
- **Title**: Multi-Layer Deep Hardware Capping & Plasma 6 Compositor Actuation Architecture
- **Status**: Approved
- **Domain**: Kernel sysfs, ACPI EC, SMT Control, DRM DRRS, rfkill, D-Bus KWin/Plasma
- **Dependencies**: [`REF-ARCH-031`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-031-dual-domain-state-journal-and-profile-actuation.md), [`REF-ARCH-034`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-034-gpu-overdrive-dpm-actuator-and-seqlock-shm-audit.md), [`REF-REQ-063`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-063-ultra-endurance-hardware-and-desktop-power-capping.md)

---

## 1. Multi-Layer System Topology

The Ultra-Endurance capping architecture establishes a three-tier suppression hierarchy that coordinates kernel sysfs, device drivers, and user-session desktop environments:

```
+-------------------------------------------------------------------------------------------------------------+
|                                    WATTCURB ROOT BACKGROUND DAEMON                                         |
|                                (MitigationEngine - EUID 0 / Root)                                           |
+-------------------------------------------------------------------------------------------------------------+
         |                                           |                                     |
         v                                           v                                     v
 [TIER 1: KERNEL HARDWARE SYSFS]           [TIER 2: DISPLAY & PERIPHERALS]        [TIER 3: DESKTOP SESSION D-BUS]
 - SMT: /sys/.../smt/control = off         - Backlight: 35% cap                   - DRRS: kscreen-doctor 48.04Hz
 - CPU Max: 1.4GHz floor P-state           - RF: rfkill block bluetooth           - KWin: unloadEffect blur
 - PCIe: policy = powersupersave           - ABM: panel_power_savings = 2         - Baloo: balooctl6 suspend
 - GPU: 640MHz (40% OD SCLK)               - Zero-leakage bus PHY                 - Non-blocking async dispatch (&)
+-------------------------------------------------------------------------------------------------------------+
         |                                           |                                     |
         +-------------------------------------------+-------------------------------------+
                                                     |
                                                     v
                                 [100% FAITHFUL RESTORATION SNAPSHOT]
                                 - smt_control: on
                                 - aspm_policy: powersave/default
                                 - backlight_brightness: preserved
                                 - bluetooth_blocked: restored
                                 - drrs_applied: 60.06Hz
                                 - kwin_blur_unloaded: loadEffect blur
                                 - baloo_suspended: balooctl6 resume
```

---

## 2. Non-Blocking Session Command Dispatch (`execute_user_desktop_cmd`)

To adhere strictly to the Zero-Wakeup and Sub-Millisecond Daemon Cadence principles, desktop commands interacting with Wayland (`kscreen-doctor`) and D-Bus (`qdbus6`, `balooctl6`) must never block the daemon's internal state machine:

1. **Privilege Demotion & Environment Bridge**:
   - When executed from Root (`geteuid() == 0`), the bridge sets `WAYLAND_DISPLAY=wayland-0` and `XDG_RUNTIME_DIR=/run/user/1000`, invoking `setpriv --reuid=1000 --regid=1000 --clear-groups`.
   - Bypasses all interactive PAM authentication prompts while respecting user desktop security boundaries.
2. **Background Async Fork (`&`)**:
   - Commands are launched with a trailing shell background operator `&`, reducing dispatch overhead from ~900ms to **`< 1ms`** and achieving a total profile actuation roundtrip of **`27.9ms`** (`REF-TEST-028`).

---

## 3. Physical Power Savings Breakdown

| Actuation Domain | Pre-Condition | UltraEndurance State | Physical Energy Mechanism | Power Delta |
| :--- | :--- | :--- | :--- | :---: |
| **CPU SMT** | 16 Threads Online | 8 Cores (SMT Off) | Pipeline queue leakage & wakeup halving | **-0.6 W** |
| **Display Backlight** | 65% PWM (~42.5k) | 35% PWM (~22.9k) | Square-law LED emitter current reduction | **-1.4 W** |
| **Panel DRRS** | 60.06 Hz | 48.04 Hz | eDP link clock & frame buffer clock reduction | **-0.5 W** |
| **PCIe ASPM** | powersave | powersupersave | L1.1/L1.2 substate bus transceiver deep sleep | **-0.3 W** |
| **Bluetooth RF** | Radio Active | Radio Soft-Blocked | Baseband receiver & PA amplifier power-down | **-0.4 W** |
| **KWin / Baloo** | Blur Active, Indexer Run | Blur Unloaded, Suspended | GPU 3D rendering cut & NVMe I/O wakeup freeze | **-0.5 W** |
| **Cumulative Delta** | Standard Baseline | Ultra-Endurance Active | Total combined hardware & desktop power reduction | **-3.7 W** |
