# REF-REQ-131: Hardware Bus & Display Deep Power Minimization Specification

- **Ref-ID**: `REF-REQ-131`
- **Domain**: Silicon & Bus Power Optimization (Display ABM, PCIe ASPM, Realtek PHY, HDA Codec, Kernel VM)
- **Status**: Approved
- **Cross-References**: `REF-RES-034`, `REF-ARCH-078`, `REF-TEST-085`

---

## 1. Functional Requirements

### 1.1 AMD Display ABM (Adaptive Backlight Modulation)
1. **Dynamic eDP Detection**: At bootstrap, inspect `/sys/class/drm/card*-eDP-*/amdgpu/panel_power_savings`. If present, record the pre-existing hardware baseline value (typically `0`).
2. **Profile-Specific Level Assignment**:
   - `Performance`: Level `0` (ABM disabled, maximum color fidelity).
   - `Balanced` (on AC): Level `0` (Disabled).
   - `Balanced` (on Battery): Level `2` (Moderate contrast-compensated backlight reduction, ~20% power savings).
   - `PowerSaver` / `UltraEndurance`: Level `3` (High energy conservation, ~30% backlight power savings).
3. **External Monitor Immunity**: External HDMI/DP display outputs must never be subjected to ABM or contrast modification.

### 1.2 PCIe ASPM & Whitelisted Safe PCI Runtime PM
1. **Global ASPM Modulation**:
   - On battery power (`PowerSaver` / `UltraEndurance` / `Balanced`), set `/sys/module/pcie_aspm/parameters/policy` to `powersave`.
   - On AC power or `Performance`, restore the captured baseline ASPM policy (`default` or `performance`).
2. **Strict Blacklist Isolation**:
   - The following PCI devices are **STRICTLY IMMUNE** from any runtime PM alteration:
     - Primary NVMe SSD controller (`01:00.0` or class `0108`).
     - Primary GPU display controller (`06:00.0` or class `0300`).
     - CPU Host bridges (`00:18.*`, `00:00.0` or class `0600`).
     - USB host controllers hosting active keyboard/mouse input devices.
3. **Safe Whitelist Activation**:
   - Idle auxiliary Realtek controllers (`02:00.1`~`02:00.4` UART/IPMI) and unattached HDMI audio (`06:00.1`) shall be safely set to `power/control = auto`.

### 1.3 Unplugged Ethernet Smart Power Save
1. **Carrier Status Polling**: Inspect `/sys/class/net/<iface>/carrier` (e.g. `enp2s0f0`).
2. **Non-Destructive Invariant**: The network interface must remain administratively `UP` to preserve kernel carrier detection interrupts.
3. **PHY Low-Power Transition**: If `carrier == 0` (`NO-CARRIER`), the parent PCI device (`0000:02:00.0`) is set to `power/control = auto`, enabling hardware sleep. Upon physical cable connection, the hardware triggers a link-change interrupt and resumes full 1Gbps throughput within 1ms.

### 1.4 HDA Audio Codec 1-Second Power Save
1. **Timeout Reduction**: On battery, `/sys/module/snd_hda_intel/parameters/power_save` is reduced from default `10` to `1` second.
2. **Audio Stream Protection**: As mandated by `REF-REQ-096`, any active audio playback stream (PipeWire/PulseAudio) holds PM QoS, ensuring DAC power is maintained during active listening without popping or audio underruns.

### 1.5 Kernel VM Stat Interval
1. **Wakeup Suppression**: On battery, `/proc/sys/vm/stat_interval` is relaxed from `1` to `10` seconds, eliminating 54 wakeups/minute per core.
2. **Clean Rollback**: On AC connection, restored immediately to `1`.

---

## 2. Safety & Rollback Guarantees

1. **Dual-Domain Baseline Journaling**: Before modifying any sysfs or procfs node, WattCurb records the exact pre-existing string/integer in `HardwareBaselineSnapshot`.
2. **Instant AC Plug-In Rollback**: The `UnifiedRollbackCoordinator` (`REF-ARCH-024`) must restore 100% of these parameters to baseline upon receiving an AC online edge interrupt.
3. **Idempotence**: Re-evaluating profiles or repeated battery ticks must not corrupt the initial captured baseline.
