# REF-ARCH-078: Hardware Bus & Display Deep Power Minimization Architecture

- **Ref-ID**: `REF-ARCH-078`
- **Domain**: Silicon & Bus Power Optimization (Display ABM, PCIe ASPM, Realtek PHY, HDA Codec, Kernel VM)
- **Status**: Approved
- **Cross-References**: `REF-RES-034`, `REF-REQ-131`, `REF-TEST-085`

---

## 1. Architectural Overview & Component Topology

```
                  ┌────────────────────────────────────────────────────────┐
                  │                 PowerProfileController                 │
                  └───────────────────────────┬────────────────────────────┘
                                              │
                    ┌─────────────────────────┴─────────────────────────┐
                    ▼                                                   ▼
       [BATTERY DISCHARGE EVENT]                              [AC PLUG-IN EDGE / SHUTDOWN]
  ┌───────────────────────────────────┐                   ┌───────────────────────────────────┐
  │ MitigationEngine::apply_profile   │                   │ UnifiedRollbackCoordinator        │
  │ • Display ABM: Level 2 / 3        │                   │ • Display ABM: Restore Level 0    │
  │ • PCIe ASPM: powersave            │ ════════════════> │ • PCIe ASPM: Restore Baseline     │
  │ • HDA Power-Save: 1 sec           │     (Rollback)    │ • HDA Power-Save: Restore 10 sec  │
  │ • Realtek PHY Sleep: auto         │                   │ • Realtek PHY: Restore on/active  │
  │ • VM Stat Interval: 10 sec        │                   │ • VM Stat Interval: Restore 1 sec │
  └─────────────────┬─────────────────┘                   └───────────────────────────────────┘
                    │
                    ▼
  ┌───────────────────────────────────┐
  │      Sysfs Hardware Targets       │
  │ • /sys/class/drm/card*-eDP-*/     │
  │   amdgpu/panel_power_savings      │
  │ • /sys/module/pcie_aspm/policy    │
  │ • /sys/module/snd_hda_intel/      │
  │   parameters/power_save           │
  │ • /sys/bus/pci/devices/...        │
  │ • /proc/sys/vm/stat_interval      │
  └───────────────────────────────────┘
```

---

## 2. Baseline State Journaling (`HardwareBaselineSnapshot`)

The `HardwareBaselineSnapshot` structure in `MitigationEngine` is augmented with dedicated fields for the new optimization vectors:

```cpp
struct HardwareBaselineSnapshot {
    // Existing fields: cpu_governor, cpu_boost, aspm_policy, scaling_max_freq, etc.
    // ...
    // REF-REQ-131 New Deep Power Vectors:
    uint32_t display_abm_level{0};
    uint32_t hda_power_save_sec{10};
    uint32_t vm_stat_interval_sec{1};
    char edp_abm_path[128]{};
    bool has_edp_abm{false};
    bool ethernet_smart_power_active{false};
};
```

---

## 3. Actuation and Recovery Interface

```cpp
class MitigationEngine {
public:
    // Display ABM (Adaptive Backlight Modulation)
    static bool set_display_abm_level(uint32_t level) noexcept;

    // HDA Audio Codec Power-Saving Timeout
    static bool set_hda_power_save(uint32_t seconds) noexcept;

    // Kernel VM Stat Interval
    static bool set_vm_stat_interval(uint32_t seconds) noexcept;

    // Whitelisted Safe PCI Runtime PM & Realtek PHY Sleep
    static bool set_safe_pci_runtime_pm(bool powersave) noexcept;

    // Complete Clean Rollback
    static void rollback_deep_hardware_vectors() noexcept;
};
```

---

## 4. Safety & Invariant Guarantees

1. **Zero Blacklist Touch**: NVMe controllers, GPU 3D rendering engines, CPU host bridges, and active USB input ports are strictly excluded from runtime PM changes.
2. **Deterministic Audio Continuity**: If an active audio stream is detected (`REF-REQ-096`), HDA power save is maintained without audio underruns.
3. **No Network Dropout**: The Ethernet network interface is kept administratively `UP` so the kernel driver detects cable connection instantly.
