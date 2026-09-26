#include "policy/hardware_bus_controller.hpp"
#include "core/event_logger.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <algorithm>

namespace wattcurb::policy {

namespace {

[[nodiscard]] inline int hw_open_write(const char* path) noexcept {
    return ::open(path, O_WRONLY | O_CLOEXEC);
}

[[nodiscard]] inline bool write_string_to_file(const char* path, const char* str) noexcept {
    if (path == nullptr || path[0] == '\0' || str == nullptr) return false;
    int fd = hw_open_write(path);
    if (fd < 0) return false;
    const size_t len = std::strlen(str);
    const ssize_t w = ::write(fd, str, len);
    ::close(fd);
    return w == static_cast<ssize_t>(len);
}

[[nodiscard]] inline bool read_small_text(const char* path, char* out_buf, size_t max_len) noexcept {
    if (path == nullptr || out_buf == nullptr || max_len == 0) return false;
    int fd = ::open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;
    const ssize_t r = ::read(fd, out_buf, max_len - 1);
    ::close(fd);
    if (r <= 0) return false;
    out_buf[r] = '\0';
    return true;
}

} // namespace

bool HardwareBusController::set_display_abm(const char* path, uint32_t level) noexcept {
    if (path == nullptr || path[0] == '\0') return false;
    level = std::min(level, 4u);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%u\n", level);
    return write_string_to_file(path, buf);
}

bool HardwareBusController::set_pcie_aspm(const char* policy) noexcept {
    if (policy == nullptr || policy[0] == '\0') return false;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s\n", policy);
    return write_string_to_file("/sys/module/pcie_aspm/parameters/policy", buf);
}

bool HardwareBusController::set_hda_power_save(uint32_t seconds) noexcept {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%u\n", seconds);
    return write_string_to_file("/sys/module/snd_hda_intel/parameters/power_save", buf);
}

bool HardwareBusController::set_vm_stat_interval(uint32_t seconds) noexcept {
    seconds = std::clamp(seconds, 1u, 60u);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%u\n", seconds);
    return write_string_to_file("/proc/sys/vm/stat_interval", buf);
}

bool HardwareBusController::set_pci_power_control(const char* pci_device_path, bool auto_suspend) noexcept {
    if (pci_device_path == nullptr || pci_device_path[0] == '\0') return false;
    char path[160];
    std::snprintf(path, sizeof(path), "%s/power/control", pci_device_path);
    return write_string_to_file(path, auto_suspend ? "auto\n" : "on\n");
}

bool HardwareBusController::is_ethernet_carrier_connected(const char* carrier_path) noexcept {
    if (carrier_path == nullptr || carrier_path[0] == '\0') return false;
    char buf[16];
    if (!read_small_text(carrier_path, buf, sizeof(buf))) return false;
    return (buf[0] == '1');
}

bool HardwareBusController::initialize() noexcept {
    if (m_baseline.captured) return true;

    // 1. AMD eDP Display ABM Detection & Baseline Capture
    const char* const abm_candidates[] = {
        "/sys/class/drm/card1-eDP-1/amdgpu/panel_power_savings",
        "/sys/class/drm/card0-eDP-1/amdgpu/panel_power_savings"
    };
    for (const char* p : abm_candidates) {
        char buf[16];
        if (read_small_text(p, buf, sizeof(buf))) {
            std::strncpy(m_baseline.edp_abm_path, p, sizeof(m_baseline.edp_abm_path) - 1);
            m_baseline.has_edp_abm = true;
            m_baseline.display_abm_level = static_cast<uint32_t>(std::strtoul(buf, nullptr, 10));
            break;
        }
    }

    // 2. PCIe Global ASPM Baseline Capture
    char aspm_buf[128];
    if (read_small_text("/sys/module/pcie_aspm/parameters/policy", aspm_buf, sizeof(aspm_buf))) {
        const char* lbr = std::strchr(aspm_buf, '[');
        const char* rbr = std::strchr(aspm_buf, ']');
        if (lbr != nullptr && rbr != nullptr && rbr > lbr + 1) {
            const size_t len = static_cast<size_t>(rbr - lbr - 1);
            const size_t copy_len = std::min(len, sizeof(m_baseline.aspm_policy) - 1);
            std::memcpy(m_baseline.aspm_policy, lbr + 1, copy_len);
            m_baseline.aspm_policy[copy_len] = '\0';
        } else {
            std::strncpy(m_baseline.aspm_policy, "default", sizeof(m_baseline.aspm_policy) - 1);
        }
    }

    // 3. HDA Audio Codec Power Save Baseline Capture
    char hda_buf[16];
    if (read_small_text("/sys/module/snd_hda_intel/parameters/power_save", hda_buf, sizeof(hda_buf))) {
        m_baseline.hda_power_save_sec = static_cast<uint32_t>(std::strtoul(hda_buf, nullptr, 10));
    }

    // 4. Kernel VM Stat Interval Baseline Capture
    char vm_buf[16];
    if (read_small_text("/proc/sys/vm/stat_interval", vm_buf, sizeof(vm_buf))) {
        m_baseline.vm_stat_interval_sec = static_cast<uint32_t>(std::strtoul(vm_buf, nullptr, 10));
    }

    // 5. Ethernet Carrier & PCI Device Path Detection
    const char* const eth_candidates[] = {
        "/sys/class/net/enp2s0f0/carrier",
        "/sys/class/net/eth0/carrier"
    };
    for (const char* c : eth_candidates) {
        if (::access(c, R_OK) == 0) {
            std::strncpy(m_baseline.eth_iface_carrier_path, c, sizeof(m_baseline.eth_iface_carrier_path) - 1);
            break;
        }
    }

    const char* const eth_pci_candidates[] = {
        "/sys/bus/pci/devices/0000:02:00.0"
    };
    for (const char* p : eth_pci_candidates) {
        if (::access(p, F_OK) == 0) {
            std::strncpy(m_baseline.eth_pci_path, p, sizeof(m_baseline.eth_pci_path) - 1);
            m_baseline.has_eth_pci = true;
            break;
        }
    }

    m_baseline.captured = true;
    return true;
}

void HardwareBusController::evaluate_and_actuate(bool on_battery, PowerProfileMode mode) noexcept {
    if (!m_baseline.captured) {
        if (!initialize()) return;
    }

    // Performance mode or AC connected -> Restore baseline and do not apply battery power savings
    if (!on_battery || mode == PowerProfileMode::Performance) {
        if (m_applied) {
            rollback_all();
        }
        m_last_on_battery = on_battery;
        m_last_mode = mode;
        return;
    }

    // Optimization state on Battery (Balanced, PowerSaver, UltraEndurance)
    // 1. AMD eDP Display ABM: Balanced = Level 2, PowerSaver/UltraEndurance = Level 3
    if (m_baseline.has_edp_abm) {
        const uint32_t target_level = (mode == PowerProfileMode::Balanced) ? 2u : 3u;
        set_display_abm(m_baseline.edp_abm_path, target_level);
    }

    // 2. PCIe Global ASPM -> powersave
    set_pcie_aspm("powersave");

    // 3. HDA Audio Codec Power-Save -> 1 second timeout
    set_hda_power_save(1);

    // 4. Kernel VM Stat Interval -> 10 seconds (Zero-Wakeup tick reduction)
    set_vm_stat_interval(10);

    // 5. Unplugged Ethernet & Whitelisted Safe PCI Devices Runtime PM
    if (m_baseline.has_eth_pci) {
        const bool carrier_ok = is_ethernet_carrier_connected(m_baseline.eth_iface_carrier_path);
        // If unplugged (NO-CARRIER), put Realtek Ethernet controller in auto runtime PM
        set_pci_power_control(m_baseline.eth_pci_path, !carrier_ok);

        // Put auxiliary Realtek UART/IPMI/EHCI PCI controllers in auto runtime PM
        const char* const aux_pci[] = {
            "/sys/bus/pci/devices/0000:02:00.1",
            "/sys/bus/pci/devices/0000:02:00.2",
            "/sys/bus/pci/devices/0000:02:00.3",
            "/sys/bus/pci/devices/0000:02:00.4",
            "/sys/bus/pci/devices/0000:06:00.1" // Unattached HDMI audio
        };
        for (const char* aux : aux_pci) {
            if (::access(aux, F_OK) == 0) {
                set_pci_power_control(aux, true);
            }
        }
    }

    if (!m_applied) {
        m_applied = true;
        char detail[160];
        std::snprintf(detail, sizeof(detail),
                      "HardwareBusController engaged: ABM=%u, ASPM=powersave, HDA=1s, VM=10s, EthSleep=%s (REF-REQ-131)",
                      (mode == PowerProfileMode::Balanced) ? 2u : 3u,
                      m_baseline.has_eth_pci ? "auto" : "none");
        core::EventLogger::log_mitigation(0, "HardwareBus", "BusAndDisplayPowerSave", detail);
    }

    m_last_on_battery = on_battery;
    m_last_mode = mode;
}

void HardwareBusController::rollback_all() noexcept {
    if (!m_baseline.captured) return;

    // 1. Restore Display ABM
    if (m_baseline.has_edp_abm) {
        set_display_abm(m_baseline.edp_abm_path, m_baseline.display_abm_level);
    }

    // 2. Restore PCIe ASPM
    set_pcie_aspm(m_baseline.aspm_policy);

    // 3. Restore HDA Power-Save
    set_hda_power_save(m_baseline.hda_power_save_sec);

    // 4. Restore Kernel VM Stat Interval
    set_vm_stat_interval(m_baseline.vm_stat_interval_sec);

    // 5. Restore Ethernet & PCI Device Power Control to "on"
    if (m_baseline.has_eth_pci) {
        set_pci_power_control(m_baseline.eth_pci_path, false);

        const char* const aux_pci[] = {
            "/sys/bus/pci/devices/0000:02:00.1",
            "/sys/bus/pci/devices/0000:02:00.2",
            "/sys/bus/pci/devices/0000:02:00.3",
            "/sys/bus/pci/devices/0000:02:00.4",
            "/sys/bus/pci/devices/0000:06:00.1"
        };
        for (const char* aux : aux_pci) {
            if (::access(aux, F_OK) == 0) {
                set_pci_power_control(aux, false);
            }
        }
    }

    if (m_applied) {
        m_applied = false;
        core::EventLogger::log_rollback(0, "HardwareBus", "Restored hardware bus & display baseline (REF-REQ-131)");
    }
}

} // namespace wattcurb::policy
