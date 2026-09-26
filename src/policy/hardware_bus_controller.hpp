#pragma once

#include "core/types.hpp"
#include <cstdint>
#include <cstddef>

namespace wattcurb::policy {

// Implements REF-REQ-131, REF-ARCH-078:
// HardwareBusController: Deep Hardware Bus, Display ABM, and Peripheral Power Minimization
//
// Governs platform hardware power sinks that survive CPU/Memory idle states:
//   1. AMD eDP Display ABM (Adaptive Backlight Modulation): Level 2/3 on battery, 0 on AC.
//   2. PCIe Global ASPM: `powersave` on battery, baseline on AC.
//   3. HDA Audio Codec Power-Save: 1s on battery, baseline (10s) on AC.
//   4. Unplugged Realtek Ethernet PHY: `power/control = auto` when NO-CARRIER on battery.
//   5. Kernel VM Stat Interval: 10s on battery, 1s on AC (Zero-Wakeup tick suppression).
//
// Invariants & Safety Guarantees:
//   - Zero Blacklist Touch: NVMe SSD, primary GPU display, and CPU host bridges are STRICTLY IMMUNE.
//   - Instant AC Rollback: 100% restored to captured hardware baseline on AC plug-in or shutdown.
//   - Network Carrier Liveness: Interface remains administratively UP for immediate cable plug-in wake.
struct alignas(64) HardwareBusBaseline {
    bool captured{false};
    uint32_t display_abm_level{0};
    char aspm_policy[32]{"default"};
    uint32_t hda_power_save_sec{10};
    uint32_t vm_stat_interval_sec{1};
    char edp_abm_path[128]{};
    bool has_edp_abm{false};
    char eth_pci_path[128]{};
    bool has_eth_pci{false};
    char eth_iface_carrier_path[128]{};
};

class HardwareBusController {
public:
    HardwareBusController() noexcept = default;
    ~HardwareBusController() noexcept { rollback_all(); }

    HardwareBusController(const HardwareBusController&) = delete;
    HardwareBusController& operator=(const HardwareBusController&) = delete;

    // Discovers hardware paths and captures initial pre-existing baseline state
    bool initialize() noexcept;

    // Evaluates power profile and AC/battery state, actuating hardware knobs idempotently
    void evaluate_and_actuate(bool on_battery, PowerProfileMode mode) noexcept;

    // Pure rollback restoring every managed knob to its pre-WattCurb baseline
    void rollback_all() noexcept;

    // Individual Actuation Primitives
    static bool set_display_abm(const char* path, uint32_t level) noexcept;
    static bool set_pcie_aspm(const char* policy) noexcept;
    static bool set_hda_power_save(uint32_t seconds) noexcept;
    static bool set_vm_stat_interval(uint32_t seconds) noexcept;
    static bool set_pci_power_control(const char* pci_device_path, bool auto_suspend) noexcept;

    // Carrier query for unplugged Ethernet
    [[nodiscard]] static bool is_ethernet_carrier_connected(const char* carrier_path) noexcept;

    [[nodiscard]] const HardwareBusBaseline& baseline() const noexcept { return m_baseline; }
    [[nodiscard]] bool is_applied() const noexcept { return m_applied; }

private:
    HardwareBusBaseline m_baseline{};
    bool m_applied{false};
    bool m_last_on_battery{false};
    PowerProfileMode m_last_mode{PowerProfileMode::Balanced};
};

} // namespace wattcurb::policy
