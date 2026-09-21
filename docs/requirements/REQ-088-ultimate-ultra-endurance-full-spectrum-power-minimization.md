# REF-REQ-088: Ultimate UltraEndurance Full-Spectrum Silicon & Desktop Power Minimization

- **Document ID**: `REF-REQ-088`
- **Related Requirements**: [`REF-REQ-031`](REQ-028-adaptive-power-state-machine-and-actuation.md), [`REF-REQ-034`](REQ-034-rapid-charge-and-profile-restoration-engine.md), [`REF-REQ-044`](REQ-044-absolute-zero-kill-and-non-halting-safety.md), [`REF-REQ-049`](REQ-032-kde-plasma-desktop-mitigation.md), [`REF-REQ-063`](REQ-063-ultra-endurance-hardware-and-desktop-power-capping.md), [`REF-REQ-087`](REQ-087-kernel-vm-writeback-and-laptop-mode-coalescing.md)
- **Related Architecture**: [`REF-ARCH-065`](../architecture/ARCH-065-ultimate-ultra-endurance-actuation-architecture.md)
- **Related Research**: [`REF-RES-023`](../research/RES-023-vram-gc-dpm-downclocking-and-performance-boost.md), [`REF-RES-024`](../research/RES-024-deep-power-log-audit-and-drain-analysis.md), [`REF-RES-025`](../research/RES-025-ultra-endurance-deep-silicon-and-kernel-power-minimization.md)
- **Status**: Approved
- **Date**: 2026-09-21

---

## 1. Objective & Scope

Under `UltraEndurance` mode, WattCurb must exhaustively mobilize all available Linux kernel, APU silicon, bus runtime, and desktop power management primitives to reduce overall system power consumption to the theoretical minimum operating boundary (**5.5 W ~ 6.5 W**), while strictly adhering to the **Zero-Kill Safety Invariant** ([`REF-REQ-044`](REQ-044-absolute-zero-kill-and-non-halting-safety.md)) and preserving immediate user interactivity upon window focus.

`REF-REQ-088` defines the full-spectrum deployment of the 6 dimensions formulated in [`REF-RES-025`](../research/RES-025-ultra-endurance-deep-silicon-and-kernel-power-minimization.md):
1. **Dimension 1**: AMDGPU DPM `low` forcing (MCLK 400 MHz / FCLK clamp) & 3-Tier VRAM GC.
2. **Dimension 2**: Global background timer slack relaxation to 100 ms (`prctl PR_SET_TIMERSLACK = 100,000,000`).
3. **Dimension 3**: Kernel VM writeback & laptop mode coalescing (already verified via [`REF-REQ-087`](REQ-087-kernel-vm-writeback-and-laptop-mode-coalescing.md)).
4. **Dimension 4**: PCIe & USB bus subsystem runtime power management (`power/control = auto`).
5. **Dimension 5**: High-Definition Audio (HDA) codec power-down (`power_save = 10`, `power_save_controller = Y`).
6. **Dimension 6**: Non-active background Electron / heavy GUI app cgroups v2 auto-freeze with sub-millisecond instant thaw.

---

## 2. Detailed Technical Requirements

### 2.1 Dimension 1: DPM MCLK 400 MHz Clamp & 3-Tier VRAM GC (`REF-REQ-088-F01`)
- When entering `UltraEndurance`:
  - Set `/sys/class/drm/card*/device/power_dpm_force_performance_level` to `"low"`.
  - Trigger 3-Tier VRAM reclamation:
    1. Unload heavy KWin blur shader effects via DBus.
    2. Evict Chromium/Electron GPU discardable memory caches via cgroups v2 `memory.reclaim`.
    3. Trigger kernel page cache reclamation (`echo 3 > /proc/sys/vm/drop_caches`).
- When exiting `UltraEndurance` to `Balanced` or `Performance`:
  - Restore GPU DPM to original baseline (`"auto"` or baseline string).

### 2.2 Dimension 2: Global Background Timer Slack Relaxation (`REF-REQ-088-F02`)
- For all non-audio, non-terminal background candidate processes:
  - Inject `100'000'000` (100 ms) into `/proc/<pid>/timerslack_ns`.
  - Record original timer slack in `TrackedMitigation` and restore upon profile change.

### 2.3 Dimension 4: PCIe & USB Runtime PM Autosuspend (`REF-REQ-088-F03`)
- Traverse `/sys/bus/pci/devices/*/power/control` and write `"auto\n"`.
- Traverse `/sys/bus/usb/devices/*/power/control` and write `"auto\n"`, bypassing active input devices (keyboards/mice) and Bluetooth controller to maintain [`REF-REQ-065`](REQ-065-bluetooth-always-on-invariant.md).

### 2.4 Dimension 5: Audio Codec Autosuspend (`REF-REQ-088-F04`)
- Snapshot baseline `/sys/module/snd_hda_intel/parameters/power_save`.
- In `UltraEndurance`, write `"10\n"` (10-second power-down timeout) and `"Y\n"` to `power_save_controller`.
- Restore baseline value upon rollback.

### 2.5 Dimension 6: Non-Active Electron / Heavy GUI Cgroup v2 Freeze (`REF-REQ-088-F05`)
- In `UltraEndurance`, any process categorized as `ProcessSafetyTier::GreedyBackground` or `BackgroundService` that does not own an active focused window (e.g. background ChatGPT, Electron, Chrome helper) must be transitioned to `cgroup.freeze = 1`.
- Audio-critical (`AudioCritical`), system core (`SystemCritical`), and focused active windows remain strictly immune.
- When the user focuses the window or profile transitions away from `UltraEndurance`, the daemon must thaw the frozen cgroup in $< 500\,\mu\text{s}$.

---

## 3. Non-Functional & Oracle Gate Invariants (`REF-TEST-052`)

1. **Sub-5ms Rapid Rollback**: Exiting `UltraEndurance` must restore all 6 dimensions back to baseline within 5.0 ms.
2. **Zero-Allocation Execution**: Inner actuation loops must not perform dynamic heap allocations.
3. **Graceful Fallback**: Missing sysfs files or non-root permissions must log a debug warning and continue execution without exceptions or termination.
