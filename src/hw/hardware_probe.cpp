#include "hw/hardware_probe.hpp"
#include "core/cpu_features.hpp"
#include "core/scoped_profiler.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>
#include <fcntl.h>
#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace wattcurb::hw {

namespace {

int open_ro_cloexec(const std::filesystem::path& path) {
    if (path.empty()) return -1;
    return ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
}

void safe_close(int& fd) noexcept {
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}
} // anonymous namespace

// Fast string_view trim helper
static inline std::string_view trim_sv(std::string_view sv) noexcept {
    while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t' || sv.front() == '\r' || sv.front() == '\n')) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t' || sv.back() == '\r' || sv.back() == '\n')) {
        sv.remove_suffix(1);
    }
    return sv;
}

// Ultra-fast single-read uevent battery parser (REF-RES-007, REF-REQ-022, REF-ARCH-012)
void HardwareProbe::parse_battery_uevent_buf(std::string_view content, HardwareSample& sample) noexcept {
    WATTCURB_PROFILE_SCOPE("hw.battery.uevent_simd_parse");
    const char* cur = content.data();
    const char* end = cur + content.size();

    while (cur < end) {
        const char* next_nl = core::simd::find_char_fast(cur, end, '\n');
        std::string_view line(cur, static_cast<size_t>(next_nl - cur));
        cur = (next_nl < end) ? next_nl + 1 : end;

        constexpr std::string_view PREFIX = "POWER_SUPPLY_";
        if (line.size() <= PREFIX.size() || std::memcmp(line.data(), PREFIX.data(), PREFIX.size()) != 0) {
            continue;
        }

        std::string_view key_val = line.substr(PREFIX.size());
        switch (key_val[0]) {
        case 'S':
            if (key_val.rfind("STATUS=", 0) == 0) {
                sample.is_discharging = (trim_sv(key_val.substr(7)) == "Discharging");
            } else if (key_val.rfind("SERIAL_NUMBER=", 0) == 0) {
                sample.battery_serial_number = trim_sv(key_val.substr(14));
            }
            break;

        case 'V':
            if (key_val.rfind("VOLTAGE_NOW=", 0) == 0) {
                uint64_t v = 0;
                const char* p = key_val.data() + 12;
                if (std::from_chars(p, key_val.data() + key_val.size(), v).ec == std::errc()) {
                    sample.battery_voltage_uv = v;
                }
            } else if (key_val.rfind("VOLTAGE_MIN_DESIGN=", 0) == 0) {
                uint64_t v = 0;
                const char* p = key_val.data() + 19;
                if (std::from_chars(p, key_val.data() + key_val.size(), v).ec == std::errc()) {
                    sample.battery_voltage_min_design_uv = v;
                }
            }
            break;

        case 'C':
            if (key_val.rfind("CURRENT_NOW=", 0) == 0) {
                int64_t i = 0;
                const char* p = key_val.data() + 12;
                if (std::from_chars(p, key_val.data() + key_val.size(), i).ec == std::errc()) {
                    sample.battery_current_ua = i;
                }
            } else if (key_val.rfind("CAPACITY=", 0) == 0) {
                uint32_t cap = 0;
                const char* p = key_val.data() + 9;
                if (std::from_chars(p, key_val.data() + key_val.size(), cap).ec == std::errc()) {
                    sample.battery_capacity_percent = cap;
                }
            } else if (key_val.rfind("CAPACITY_LEVEL=", 0) == 0) {
                sample.battery_capacity_level = trim_sv(key_val.substr(15));
            } else if (key_val.rfind("CYCLE_COUNT=", 0) == 0) {
                uint32_t c = 0;
                const char* p = key_val.data() + 12;
                if (std::from_chars(p, key_val.data() + key_val.size(), c).ec == std::errc()) {
                    sample.battery_cycle_count = c;
                }
            }
            break;

        case 'P':
            if (key_val.rfind("POWER_NOW=", 0) == 0) {
                uint64_t p_val = 0;
                const char* p = key_val.data() + 10;
                if (std::from_chars(p, key_val.data() + key_val.size(), p_val).ec == std::errc()) {
                    sample.battery_power_uw = p_val;
                }
            }
            break;

        case 'E':
            if (key_val.rfind("ENERGY_NOW=", 0) == 0) {
                uint64_t e_val = 0;
                const char* p = key_val.data() + 11;
                if (std::from_chars(p, key_val.data() + key_val.size(), e_val).ec == std::errc()) {
                    sample.battery_energy_now_uwh = e_val;
                }
            } else if (key_val.rfind("ENERGY_FULL=", 0) == 0) {
                uint64_t e_val = 0;
                const char* p = key_val.data() + 12;
                if (std::from_chars(p, key_val.data() + key_val.size(), e_val).ec == std::errc()) {
                    sample.battery_energy_full_uwh = e_val;
                }
            } else if (key_val.rfind("ENERGY_FULL_DESIGN=", 0) == 0) {
                uint64_t e_val = 0;
                const char* p = key_val.data() + 19;
                if (std::from_chars(p, key_val.data() + key_val.size(), e_val).ec == std::errc()) {
                    sample.battery_energy_full_design_uwh = e_val;
                }
            }
            break;

        case 'M':
            if (key_val.rfind("MODEL_NAME=", 0) == 0) {
                sample.battery_model_name = trim_sv(key_val.substr(11));
            } else if (key_val.rfind("MANUFACTURER=", 0) == 0) {
                sample.battery_manufacturer = trim_sv(key_val.substr(13));
            }
            break;

        case 'T':
            if (key_val.rfind("TECHNOLOGY=", 0) == 0) {
                sample.battery_technology = trim_sv(key_val.substr(11));
            }
            break;

        default:
            break;
        }
    }

    if (!sample.battery_power_uw.has_value() && sample.battery_voltage_uv.has_value() && sample.battery_current_ua.has_value()) {
        WATTCURB_PROFILE_SCOPE("hw.battery.uevent_power_derive");
        int64_t abs_curr = *sample.battery_current_ua < 0 ? -*sample.battery_current_ua : *sample.battery_current_ua;
        sample.battery_power_uw = static_cast<uint64_t>((*sample.battery_voltage_uv * static_cast<uint64_t>(abs_curr)) / 1'000'000ULL);
    }
}

HardwareProbe::HardwareProbe(std::filesystem::path sysfs_root)
    : sysfs_root_(std::move(sysfs_root)) {
    refresh_device_paths();
}

HardwareProbe::~HardwareProbe() {
    close_fds();
}

HardwareProbe::HardwareProbe(HardwareProbe&& other) noexcept
    : sysfs_root_(std::move(other.sysfs_root_)),
      battery_path_(std::move(other.battery_path_)),
      battery_uevent_path_(std::move(other.battery_uevent_path_)),
      battery_threshold_start_path_(std::move(other.battery_threshold_start_path_)),
      battery_threshold_end_path_(std::move(other.battery_threshold_end_path_)),
      battery_behaviour_path_(std::move(other.battery_behaviour_path_)),
      ac_path_(std::move(other.ac_path_)),
      usbc_pd_path_(std::move(other.usbc_pd_path_)),
      usbc_type_path_(std::move(other.usbc_type_path_)),
      usbc_voltage_max_path_(std::move(other.usbc_voltage_max_path_)),
      usbc_current_max_path_(std::move(other.usbc_current_max_path_)),
      rapl_pkg_path_(std::move(other.rapl_pkg_path_)),
      rapl_core_path_(std::move(other.rapl_core_path_)),
      rapl_dram_path_(std::move(other.rapl_dram_path_)),
      cpu_temp_path_(std::move(other.cpu_temp_path_)),
      cpu_governor_path_(std::move(other.cpu_governor_path_)),
      gpu_power_path_(std::move(other.gpu_power_path_)),
      gpu_temp_path_(std::move(other.gpu_temp_path_)),
      gpu_freq_path_(std::move(other.gpu_freq_path_)),
      gpu_in0_path_(std::move(other.gpu_in0_path_)),
      gpu_in1_path_(std::move(other.gpu_in1_path_)),
      drm_device_path_(std::move(other.drm_device_path_)),
      nvme_status_path_(std::move(other.nvme_status_path_)),
      nvme_temp1_path_(std::move(other.nvme_temp1_path_)),
      nvme_temp2_path_(std::move(other.nvme_temp2_path_)),
      block_stat_path_(std::move(other.block_stat_path_)),
      fan_rpm_path_(std::move(other.fan_rpm_path_)),
      fan_pwm_path_(std::move(other.fan_pwm_path_)),
      chassis_temp_path_(std::move(other.chassis_temp_path_)),
      kbdlight_path_(std::move(other.kbdlight_path_)),
      bluetooth_path_(std::move(other.bluetooth_path_)),
      backlight_path_(std::move(other.backlight_path_)),
      wifi_status_path_(std::move(other.wifi_status_path_)),
      wifi_temp_path_(std::move(other.wifi_temp_path_)),
      aspm_policy_path_(std::move(other.aspm_policy_path_)),
      battery_uevent_fd_(std::exchange(other.battery_uevent_fd_, -1)),
      battery_power_fd_(std::exchange(other.battery_power_fd_, -1)),
      battery_status_fd_(std::exchange(other.battery_status_fd_, -1)),
      battery_voltage_fd_(std::exchange(other.battery_voltage_fd_, -1)),
      battery_current_fd_(std::exchange(other.battery_current_fd_, -1)),
      battery_energy_now_fd_(std::exchange(other.battery_energy_now_fd_, -1)),
      battery_energy_full_fd_(std::exchange(other.battery_energy_full_fd_, -1)),
      battery_energy_full_design_fd_(std::exchange(other.battery_energy_full_design_fd_, -1)),
      battery_cycle_fd_(std::exchange(other.battery_cycle_fd_, -1)),
      battery_capacity_fd_(std::exchange(other.battery_capacity_fd_, -1)),
      ac_online_fd_(std::exchange(other.ac_online_fd_, -1)),
      battery_threshold_start_fd_(std::exchange(other.battery_threshold_start_fd_, -1)),
      battery_threshold_end_fd_(std::exchange(other.battery_threshold_end_fd_, -1)),
      battery_behaviour_fd_(std::exchange(other.battery_behaviour_fd_, -1)),
      usbc_voltage_fd_(std::exchange(other.usbc_voltage_fd_, -1)),
      usbc_current_fd_(std::exchange(other.usbc_current_fd_, -1)),
      usbc_online_fd_(std::exchange(other.usbc_online_fd_, -1)),
      usbc_type_fd_(std::exchange(other.usbc_type_fd_, -1)),
      usbc_voltage_max_fd_(std::exchange(other.usbc_voltage_max_fd_, -1)),
      usbc_current_max_fd_(std::exchange(other.usbc_current_max_fd_, -1)),
      peripheral_probes_(std::move(other.peripheral_probes_)),
      peripheral_probe_count_(std::exchange(other.peripheral_probe_count_, 0)),
      rapl_pkg_fd_(std::exchange(other.rapl_pkg_fd_, -1)),
      rapl_core_fd_(std::exchange(other.rapl_core_fd_, -1)),
      rapl_dram_fd_(std::exchange(other.rapl_dram_fd_, -1)),
      cpu_temp_fd_(std::exchange(other.cpu_temp_fd_, -1)),
      cpu_governor_fd_(std::exchange(other.cpu_governor_fd_, -1)),
      cpu_freq_fds_(std::move(other.cpu_freq_fds_)),
      cpu_cstate_fds_(std::move(other.cpu_cstate_fds_)),
      gpu_power_fd_(std::exchange(other.gpu_power_fd_, -1)),
      gpu_temp_fd_(std::exchange(other.gpu_temp_fd_, -1)),
      gpu_freq_fd_(std::exchange(other.gpu_freq_fd_, -1)),
      gpu_in0_fd_(std::exchange(other.gpu_in0_fd_, -1)),
      gpu_in1_fd_(std::exchange(other.gpu_in1_fd_, -1)),
      gpu_busy_fd_(std::exchange(other.gpu_busy_fd_, -1)),
      gpu_vram_used_fd_(std::exchange(other.gpu_vram_used_fd_, -1)),
      gpu_vram_total_fd_(std::exchange(other.gpu_vram_total_fd_, -1)),
      gpu_link_speed_fd_(std::exchange(other.gpu_link_speed_fd_, -1)),
      gpu_link_width_fd_(std::exchange(other.gpu_link_width_fd_, -1)),
      nvme_status_fd_(std::exchange(other.nvme_status_fd_, -1)),
      nvme_temp1_fd_(std::exchange(other.nvme_temp1_fd_, -1)),
      nvme_temp2_fd_(std::exchange(other.nvme_temp2_fd_, -1)),
      block_stat_fd_(std::exchange(other.block_stat_fd_, -1)),
      fan_rpm_fd_(std::exchange(other.fan_rpm_fd_, -1)),
      fan_pwm_fd_(std::exchange(other.fan_pwm_fd_, -1)),
      chassis_temp_fd_(std::exchange(other.chassis_temp_fd_, -1)),
      kbdlight_fd_(std::exchange(other.kbdlight_fd_, -1)),
      bluetooth_fd_(std::exchange(other.bluetooth_fd_, -1)),
      backlight_cur_fd_(std::exchange(other.backlight_cur_fd_, -1)),
      backlight_max_fd_(std::exchange(other.backlight_max_fd_, -1)),
      wifi_status_fd_(std::exchange(other.wifi_status_fd_, -1)),
      wifi_temp_fd_(std::exchange(other.wifi_temp_fd_, -1)),
      aspm_policy_fd_(std::exchange(other.aspm_policy_fd_, -1)),
      pmu_instructions_fd_(std::exchange(other.pmu_instructions_fd_, -1)),
      pmu_cycles_fd_(std::exchange(other.pmu_cycles_fd_, -1)),
      pmu_llc_misses_fd_(std::exchange(other.pmu_llc_misses_fd_, -1)),
      pmu_branch_misses_fd_(std::exchange(other.pmu_branch_misses_fd_, -1)),
      pcie_gpu_config_fd_(std::exchange(other.pcie_gpu_config_fd_, -1)),
      pcie_nvme_config_fd_(std::exchange(other.pcie_nvme_config_fd_, -1)),
      cpu0_msr_fd_(std::exchange(other.cpu0_msr_fd_, -1)) {}

HardwareProbe& HardwareProbe::operator=(HardwareProbe&& other) noexcept {
    if (this != &other) {
        close_fds();
        sysfs_root_ = std::move(other.sysfs_root_);
        battery_path_ = std::move(other.battery_path_);
        battery_uevent_path_ = std::move(other.battery_uevent_path_);
        battery_threshold_start_path_ = std::move(other.battery_threshold_start_path_);
        battery_threshold_end_path_ = std::move(other.battery_threshold_end_path_);
        battery_behaviour_path_ = std::move(other.battery_behaviour_path_);
        ac_path_ = std::move(other.ac_path_);
        usbc_pd_path_ = std::move(other.usbc_pd_path_);
        usbc_type_path_ = std::move(other.usbc_type_path_);
        usbc_voltage_max_path_ = std::move(other.usbc_voltage_max_path_);
        usbc_current_max_path_ = std::move(other.usbc_current_max_path_);
        rapl_pkg_path_ = std::move(other.rapl_pkg_path_);
        rapl_core_path_ = std::move(other.rapl_core_path_);
        rapl_dram_path_ = std::move(other.rapl_dram_path_);
        cpu_temp_path_ = std::move(other.cpu_temp_path_);
        cpu_governor_path_ = std::move(other.cpu_governor_path_);
        gpu_power_path_ = std::move(other.gpu_power_path_);
        gpu_temp_path_ = std::move(other.gpu_temp_path_);
        gpu_freq_path_ = std::move(other.gpu_freq_path_);
        gpu_in0_path_ = std::move(other.gpu_in0_path_);
        gpu_in1_path_ = std::move(other.gpu_in1_path_);
        drm_device_path_ = std::move(other.drm_device_path_);
        nvme_status_path_ = std::move(other.nvme_status_path_);
        nvme_temp1_path_ = std::move(other.nvme_temp1_path_);
        nvme_temp2_path_ = std::move(other.nvme_temp2_path_);
        block_stat_path_ = std::move(other.block_stat_path_);
        fan_rpm_path_ = std::move(other.fan_rpm_path_);
        fan_pwm_path_ = std::move(other.fan_pwm_path_);
        chassis_temp_path_ = std::move(other.chassis_temp_path_);
        kbdlight_path_ = std::move(other.kbdlight_path_);
        bluetooth_path_ = std::move(other.bluetooth_path_);
        backlight_path_ = std::move(other.backlight_path_);
        wifi_status_path_ = std::move(other.wifi_status_path_);
        wifi_temp_path_ = std::move(other.wifi_temp_path_);
        aspm_policy_path_ = std::move(other.aspm_policy_path_);

        battery_uevent_fd_ = std::exchange(other.battery_uevent_fd_, -1);
        battery_power_fd_ = std::exchange(other.battery_power_fd_, -1);
        battery_status_fd_ = std::exchange(other.battery_status_fd_, -1);
        battery_voltage_fd_ = std::exchange(other.battery_voltage_fd_, -1);
        battery_current_fd_ = std::exchange(other.battery_current_fd_, -1);
        battery_energy_now_fd_ = std::exchange(other.battery_energy_now_fd_, -1);
        battery_energy_full_fd_ = std::exchange(other.battery_energy_full_fd_, -1);
        battery_energy_full_design_fd_ = std::exchange(other.battery_energy_full_design_fd_, -1);
        battery_cycle_fd_ = std::exchange(other.battery_cycle_fd_, -1);
        battery_capacity_fd_ = std::exchange(other.battery_capacity_fd_, -1);
        ac_online_fd_ = std::exchange(other.ac_online_fd_, -1);
        battery_threshold_start_fd_ = std::exchange(other.battery_threshold_start_fd_, -1);
        battery_threshold_end_fd_ = std::exchange(other.battery_threshold_end_fd_, -1);
        battery_behaviour_fd_ = std::exchange(other.battery_behaviour_fd_, -1);
        usbc_voltage_fd_ = std::exchange(other.usbc_voltage_fd_, -1);
        usbc_current_fd_ = std::exchange(other.usbc_current_fd_, -1);
        usbc_online_fd_ = std::exchange(other.usbc_online_fd_, -1);
        usbc_type_fd_ = std::exchange(other.usbc_type_fd_, -1);
        usbc_voltage_max_fd_ = std::exchange(other.usbc_voltage_max_fd_, -1);
        usbc_current_max_fd_ = std::exchange(other.usbc_current_max_fd_, -1);
        peripheral_probes_ = std::move(other.peripheral_probes_);
        peripheral_probe_count_ = std::exchange(other.peripheral_probe_count_, 0);
        rapl_pkg_fd_ = std::exchange(other.rapl_pkg_fd_, -1);
        rapl_core_fd_ = std::exchange(other.rapl_core_fd_, -1);
        rapl_dram_fd_ = std::exchange(other.rapl_dram_fd_, -1);
        cpu_temp_fd_ = std::exchange(other.cpu_temp_fd_, -1);
        cpu_governor_fd_ = std::exchange(other.cpu_governor_fd_, -1);
        cpu_freq_fds_ = std::move(other.cpu_freq_fds_);
        cpu_cstate_fds_ = std::move(other.cpu_cstate_fds_);
        gpu_power_fd_ = std::exchange(other.gpu_power_fd_, -1);
        gpu_temp_fd_ = std::exchange(other.gpu_temp_fd_, -1);
        gpu_freq_fd_ = std::exchange(other.gpu_freq_fd_, -1);
        gpu_in0_fd_ = std::exchange(other.gpu_in0_fd_, -1);
        gpu_in1_fd_ = std::exchange(other.gpu_in1_fd_, -1);
        gpu_busy_fd_ = std::exchange(other.gpu_busy_fd_, -1);
        gpu_vram_used_fd_ = std::exchange(other.gpu_vram_used_fd_, -1);
        gpu_vram_total_fd_ = std::exchange(other.gpu_vram_total_fd_, -1);
        gpu_link_speed_fd_ = std::exchange(other.gpu_link_speed_fd_, -1);
        gpu_link_width_fd_ = std::exchange(other.gpu_link_width_fd_, -1);
        nvme_status_fd_ = std::exchange(other.nvme_status_fd_, -1);
        nvme_temp1_fd_ = std::exchange(other.nvme_temp1_fd_, -1);
        nvme_temp2_fd_ = std::exchange(other.nvme_temp2_fd_, -1);
        block_stat_fd_ = std::exchange(other.block_stat_fd_, -1);
        fan_rpm_fd_ = std::exchange(other.fan_rpm_fd_, -1);
        fan_pwm_fd_ = std::exchange(other.fan_pwm_fd_, -1);
        chassis_temp_fd_ = std::exchange(other.chassis_temp_fd_, -1);
        kbdlight_fd_ = std::exchange(other.kbdlight_fd_, -1);
        bluetooth_fd_ = std::exchange(other.bluetooth_fd_, -1);
        backlight_cur_fd_ = std::exchange(other.backlight_cur_fd_, -1);
        backlight_max_fd_ = std::exchange(other.backlight_max_fd_, -1);
        wifi_status_fd_ = std::exchange(other.wifi_status_fd_, -1);
        wifi_temp_fd_ = std::exchange(other.wifi_temp_fd_, -1);
        aspm_policy_fd_ = std::exchange(other.aspm_policy_fd_, -1);
        pmu_instructions_fd_ = std::exchange(other.pmu_instructions_fd_, -1);
        pmu_cycles_fd_ = std::exchange(other.pmu_cycles_fd_, -1);
        pmu_llc_misses_fd_ = std::exchange(other.pmu_llc_misses_fd_, -1);
        pmu_branch_misses_fd_ = std::exchange(other.pmu_branch_misses_fd_, -1);
        pcie_gpu_config_fd_ = std::exchange(other.pcie_gpu_config_fd_, -1);
        pcie_nvme_config_fd_ = std::exchange(other.pcie_nvme_config_fd_, -1);
        cpu0_msr_fd_ = std::exchange(other.cpu0_msr_fd_, -1);
    }
    return *this;
}

void HardwareProbe::close_fds() noexcept {
    safe_close(battery_uevent_fd_);
    safe_close(battery_power_fd_);
    safe_close(battery_status_fd_);
    safe_close(battery_voltage_fd_);
    safe_close(battery_current_fd_);
    safe_close(battery_energy_now_fd_);
    safe_close(battery_energy_full_fd_);
    safe_close(battery_energy_full_design_fd_);
    safe_close(battery_cycle_fd_);
    safe_close(battery_capacity_fd_);
    safe_close(ac_online_fd_);
    safe_close(battery_threshold_start_fd_);
    safe_close(battery_threshold_end_fd_);
    safe_close(battery_behaviour_fd_);
    safe_close(usbc_voltage_fd_);
    safe_close(usbc_current_fd_);
    safe_close(usbc_online_fd_);
    safe_close(usbc_type_fd_);
    safe_close(usbc_voltage_max_fd_);
    safe_close(usbc_current_max_fd_);

    for (size_t i = 0; i < peripheral_probe_count_; ++i) {
        safe_close(peripheral_probes_[i].capacity_fd);
        safe_close(peripheral_probes_[i].status_fd);
    }
    peripheral_probe_count_ = 0;

    safe_close(rapl_pkg_fd_);
    safe_close(rapl_core_fd_);
    safe_close(rapl_dram_fd_);
    safe_close(cpu_temp_fd_);
    safe_close(cpu_governor_fd_);

    for (int& fd : cpu_freq_fds_) {
        safe_close(fd);
    }
    cpu_freq_fds_.clear();

    for (auto& states : cpu_cstate_fds_) {
        for (int& fd : states) {
            safe_close(fd);
        }
    }
    cpu_cstate_fds_.clear();

    safe_close(gpu_power_fd_);
    safe_close(gpu_temp_fd_);
    safe_close(gpu_freq_fd_);
    safe_close(gpu_in0_fd_);
    safe_close(gpu_in1_fd_);
    safe_close(gpu_busy_fd_);
    safe_close(gpu_vram_used_fd_);
    safe_close(gpu_vram_total_fd_);
    safe_close(gpu_link_speed_fd_);
    safe_close(gpu_link_width_fd_);

    safe_close(nvme_status_fd_);
    safe_close(nvme_temp1_fd_);
    safe_close(nvme_temp2_fd_);
    safe_close(block_stat_fd_);

    safe_close(fan_rpm_fd_);
    safe_close(fan_pwm_fd_);
    safe_close(chassis_temp_fd_);
    safe_close(kbdlight_fd_);
    safe_close(bluetooth_fd_);

    safe_close(backlight_cur_fd_);
    safe_close(backlight_max_fd_);

    safe_close(wifi_status_fd_);
    safe_close(wifi_temp_fd_);
    safe_close(aspm_policy_fd_);

    safe_close(pmu_instructions_fd_);
    safe_close(pmu_cycles_fd_);
    safe_close(pmu_llc_misses_fd_);
    safe_close(pmu_branch_misses_fd_);
    safe_close(pcie_gpu_config_fd_);
    safe_close(pcie_nvme_config_fd_);
    safe_close(cpu0_msr_fd_);
}

void HardwareProbe::open_persistent_fds() {
    close_fds();

    // 1. Battery & Power Rail
    if (!battery_uevent_path_.empty()) {
        battery_uevent_fd_ = open_ro_cloexec(battery_uevent_path_);
    }
    if (!battery_path_.empty()) {
        battery_power_fd_ = open_ro_cloexec(battery_path_ / "power_now");
        battery_status_fd_ = open_ro_cloexec(battery_path_ / "status");
        battery_voltage_fd_ = open_ro_cloexec(battery_path_ / "voltage_now");
        battery_current_fd_ = open_ro_cloexec(battery_path_ / "current_now");
        battery_energy_now_fd_ = open_ro_cloexec(battery_path_ / "energy_now");
        battery_energy_full_fd_ = open_ro_cloexec(battery_path_ / "energy_full");
        battery_energy_full_design_fd_ = open_ro_cloexec(battery_path_ / "energy_full_design");
        battery_cycle_fd_ = open_ro_cloexec(battery_path_ / "cycle_count");
        battery_capacity_fd_ = open_ro_cloexec(battery_path_ / "capacity");

        if (!battery_threshold_start_path_.empty()) battery_threshold_start_fd_ = open_ro_cloexec(battery_threshold_start_path_);
        if (!battery_threshold_end_path_.empty()) battery_threshold_end_fd_ = open_ro_cloexec(battery_threshold_end_path_);
        if (!battery_behaviour_path_.empty()) battery_behaviour_fd_ = open_ro_cloexec(battery_behaviour_path_);
    }
    if (!ac_path_.empty()) {
        ac_online_fd_ = open_ro_cloexec(ac_path_ / "online");
    }
    if (!usbc_pd_path_.empty()) {
        usbc_voltage_fd_ = open_ro_cloexec(usbc_pd_path_ / "voltage_now");
        usbc_current_fd_ = open_ro_cloexec(usbc_pd_path_ / "current_now");
        usbc_online_fd_ = open_ro_cloexec(usbc_pd_path_ / "online");
        if (!usbc_type_path_.empty()) usbc_type_fd_ = open_ro_cloexec(usbc_type_path_);
        if (!usbc_voltage_max_path_.empty()) usbc_voltage_max_fd_ = open_ro_cloexec(usbc_voltage_max_path_);
        if (!usbc_current_max_path_.empty()) usbc_current_max_fd_ = open_ro_cloexec(usbc_current_max_path_);
    }

    // 2. RAPL & CPU
    if (!rapl_pkg_path_.empty()) {
        rapl_pkg_fd_ = open_ro_cloexec(rapl_pkg_path_);
    }
    if (!rapl_core_path_.empty()) {
        rapl_core_fd_ = open_ro_cloexec(rapl_core_path_);
    }
    if (!rapl_dram_path_.empty()) {
        rapl_dram_fd_ = open_ro_cloexec(rapl_dram_path_);
    }
    if (!cpu_temp_path_.empty()) {
        cpu_temp_fd_ = open_ro_cloexec(cpu_temp_path_);
    }
    if (!cpu_governor_path_.empty()) {
        cpu_governor_fd_ = open_ro_cloexec(cpu_governor_path_);
    }

    // Discover and open all online CPU core freqs and C-states
    std::error_code ec;
    auto cpu_base = sysfs_root_ / "devices/system/cpu";
    if (std::filesystem::exists(cpu_base, ec)) {
        for (int i = 0; i < 256; ++i) {
            auto cpu_dir = cpu_base / ("cpu" + std::to_string(i));
            if (!std::filesystem::exists(cpu_dir, ec)) break;

            auto freq_file = cpu_dir / "cpufreq/scaling_cur_freq";
            int f_fd = open_ro_cloexec(freq_file);
            if (f_fd >= 0) {
                cpu_freq_fds_.push_back(f_fd);
            }

            std::array<int, 4> states{-1, -1, -1, -1};
            for (int s = 0; s < 4; ++s) {
                auto idle_file = cpu_dir / ("cpuidle/state" + std::to_string(s) + "/time");
                states[static_cast<size_t>(s)] = open_ro_cloexec(idle_file);
            }
            cpu_cstate_fds_.push_back(states);
        }
    }

    // 3. GPU
    if (!gpu_power_path_.empty()) gpu_power_fd_ = open_ro_cloexec(gpu_power_path_);
    if (!gpu_temp_path_.empty()) gpu_temp_fd_ = open_ro_cloexec(gpu_temp_path_);
    if (!gpu_freq_path_.empty()) gpu_freq_fd_ = open_ro_cloexec(gpu_freq_path_);
    if (!gpu_in0_path_.empty()) gpu_in0_fd_ = open_ro_cloexec(gpu_in0_path_);
    if (!gpu_in1_path_.empty()) gpu_in1_fd_ = open_ro_cloexec(gpu_in1_path_);
    if (!drm_device_path_.empty()) {
        gpu_busy_fd_ = open_ro_cloexec(drm_device_path_ / "gpu_busy_percent");
        gpu_vram_used_fd_ = open_ro_cloexec(drm_device_path_ / "mem_info_vram_used");
        gpu_vram_total_fd_ = open_ro_cloexec(drm_device_path_ / "mem_info_vram_total");
        gpu_link_speed_fd_ = open_ro_cloexec(drm_device_path_ / "current_link_speed");
        gpu_link_width_fd_ = open_ro_cloexec(drm_device_path_ / "current_link_width");
    }

    // 4. Storage
    if (!nvme_status_path_.empty()) nvme_status_fd_ = open_ro_cloexec(nvme_status_path_);
    if (!nvme_temp1_path_.empty()) nvme_temp1_fd_ = open_ro_cloexec(nvme_temp1_path_);
    if (!nvme_temp2_path_.empty()) nvme_temp2_fd_ = open_ro_cloexec(nvme_temp2_path_);
    if (!block_stat_path_.empty()) block_stat_fd_ = open_ro_cloexec(block_stat_path_);

    // 5. Chassis & Thermal
    if (!fan_rpm_path_.empty()) fan_rpm_fd_ = open_ro_cloexec(fan_rpm_path_);
    if (!fan_pwm_path_.empty()) fan_pwm_fd_ = open_ro_cloexec(fan_pwm_path_);
    if (!chassis_temp_path_.empty()) chassis_temp_fd_ = open_ro_cloexec(chassis_temp_path_);
    if (!kbdlight_path_.empty()) kbdlight_fd_ = open_ro_cloexec(kbdlight_path_);
    if (!bluetooth_path_.empty()) bluetooth_fd_ = open_ro_cloexec(bluetooth_path_);

    // 6. Display
    if (!backlight_path_.empty()) {
        auto cur_file = backlight_path_ / "actual_brightness";
        if (!std::filesystem::exists(cur_file, ec)) cur_file = backlight_path_ / "brightness";
        backlight_cur_fd_ = open_ro_cloexec(cur_file);
        backlight_max_fd_ = open_ro_cloexec(backlight_path_ / "max_brightness");
    }

    // 7. Wireless & ASPM
    if (!wifi_status_path_.empty()) wifi_status_fd_ = open_ro_cloexec(wifi_status_path_);
    if (!wifi_temp_path_.empty()) wifi_temp_fd_ = open_ro_cloexec(wifi_temp_path_);
    if (!aspm_policy_path_.empty()) aspm_policy_fd_ = open_ro_cloexec(aspm_policy_path_);

    // 8. Syscall-Level Direct Hardware Telemetry (REF-REQ-015)
    init_pmu_counters();
    init_pcie_binary_configs();
    init_msr_telemetry();
}

void HardwareProbe::init_pmu_counters() {
    struct perf_event_attr pe{};
    pe.size = sizeof(struct perf_event_attr);
    pe.disabled = 0;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;

    pe.type = PERF_TYPE_HARDWARE;
    pe.config = PERF_COUNT_HW_INSTRUCTIONS;
    pmu_instructions_fd_ = static_cast<int>(::syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0));

    pe.config = PERF_COUNT_HW_CPU_CYCLES;
    pmu_cycles_fd_ = static_cast<int>(::syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0));

    pe.config = PERF_COUNT_HW_CACHE_MISSES;
    pmu_llc_misses_fd_ = static_cast<int>(::syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0));

    pe.config = PERF_COUNT_HW_BRANCH_MISSES;
    pmu_branch_misses_fd_ = static_cast<int>(::syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0));
}

void HardwareProbe::init_pcie_binary_configs() {
    std::error_code ec;
    // Direct GPU PCIe Binary Config Space
    if (!drm_device_path_.empty()) {
        auto cfg = drm_device_path_ / "config";
        if (std::filesystem::exists(cfg, ec)) {
            pcie_gpu_config_fd_ = open_ro_cloexec(cfg);
        }
    }
    // Direct NVMe PCIe Binary Config Space
    if (!nvme_status_path_.empty()) {
        auto dev_dir = nvme_status_path_.parent_path().parent_path();
        auto cfg = dev_dir / "config";
        if (std::filesystem::exists(cfg, ec)) {
            pcie_nvme_config_fd_ = open_ro_cloexec(cfg);
        }
    }
}

void HardwareProbe::init_msr_telemetry() {
    cpu0_msr_fd_ = ::open("/dev/cpu/0/msr", O_RDONLY | O_CLOEXEC);
}

std::pair<uint8_t, uint8_t> HardwareProbe::decode_pcie_link_status(const uint8_t* config_data, size_t size) noexcept {
    if (!config_data || size < 64) return {0, 0};

    // Verify PCI Status register Bit 4 (Capabilities List bit)
    uint16_t status = 0;
    std::memcpy(&status, config_data + 0x06, sizeof(status));
    if (!(status & 0x0010)) return {0, 0};

    uint8_t cap_ptr = config_data[0x34];
    // Loop through capability chain (guarding against malformed loops with max 48 hops)
    for (int i = 0; i < 48 && cap_ptr >= 0x40 && (static_cast<size_t>(cap_ptr) + 0x14) <= size; ++i) {
        uint8_t cap_id = config_data[cap_ptr];
        if (cap_id == 0x10) { // PCI Express Capability
            uint16_t link_status = 0;
            std::memcpy(&link_status, config_data + cap_ptr + 0x12, sizeof(link_status));
            uint8_t speed_gen = static_cast<uint8_t>(link_status & 0x0F);
            uint8_t width_lanes = static_cast<uint8_t>((link_status >> 4) & 0x3F);
            return {speed_gen, width_lanes};
        }
        cap_ptr = config_data[cap_ptr + 1];
    }
    return {0, 0};
}

std::pair<uint8_t, uint8_t> HardwareProbe::read_pcie_binary_link_status(int config_fd) noexcept {
    if (config_fd < 0) return {0, 0};
    alignas(64) uint8_t buf[256]{};
    ssize_t n = ::pread(config_fd, buf, sizeof(buf), 0);
    if (n > 64) {
        return decode_pcie_link_status(buf, static_cast<size_t>(n));
    }
    return {0, 0};
}

void HardwareProbe::refresh_device_paths() {
    std::error_code ec;

    // 1. Power Supply Discovery (/sys/class/power_supply/*)
    auto power_supply_dir = sysfs_root_ / "class/power_supply";
    if (std::filesystem::exists(power_supply_dir, ec)) {
        for (const auto& entry : std::filesystem::directory_iterator(power_supply_dir, ec)) {
            const auto filename = entry.path().filename().string();
            if (filename.rfind("BAT", 0) == 0 && battery_path_.empty()) {
                battery_path_ = entry.path();
                battery_uevent_path_ = entry.path() / "uevent";

                auto c_start = entry.path() / "charge_control_start_threshold";
                if (!std::filesystem::exists(c_start, ec)) c_start = entry.path() / "charge_start_threshold";
                if (std::filesystem::exists(c_start, ec)) battery_threshold_start_path_ = c_start;

                auto c_end = entry.path() / "charge_control_end_threshold";
                if (!std::filesystem::exists(c_end, ec)) c_end = entry.path() / "charge_stop_threshold";
                if (std::filesystem::exists(c_end, ec)) battery_threshold_end_path_ = c_end;

                auto c_beh = entry.path() / "charge_behaviour";
                if (std::filesystem::exists(c_beh, ec)) battery_behaviour_path_ = c_beh;
            } else if (filename == "AC" && ac_path_.empty()) {
                ac_path_ = entry.path();
            } else if (filename.rfind("ucsi-source-psy-", 0) == 0) {
                // Pick online USB-C PD source if possible
                auto on_file = entry.path() / "online";
                int tmp_fd = open_ro_cloexec(on_file);
                if (tmp_fd >= 0) {
                    auto val = read_uint32_fd(tmp_fd);
                    ::close(tmp_fd);
                    if (val.value_or(0) == 1 || usbc_pd_path_.empty()) {
                        usbc_pd_path_ = entry.path();
                        auto ut_file = entry.path() / "usb_type";
                        if (std::filesystem::exists(ut_file, ec)) usbc_type_path_ = ut_file;
                        auto vmax_file = entry.path() / "voltage_max";
                        if (std::filesystem::exists(vmax_file, ec)) usbc_voltage_max_path_ = vmax_file;
                        auto imax_file = entry.path() / "current_max";
                        if (std::filesystem::exists(imax_file, ec)) usbc_current_max_path_ = imax_file;
                    }
                }
            } else if (filename != "AC" && peripheral_probe_count_ < 4) {
                auto cap_file = entry.path() / "capacity";
                if (std::filesystem::exists(cap_file, ec)) {
                    auto& probe = peripheral_probes_[peripheral_probe_count_++];
                    probe.name = filename;
                    probe.capacity_fd = open_ro_cloexec(cap_file);
                    auto stat_file = entry.path() / "status";
                    if (std::filesystem::exists(stat_file, ec)) {
                        probe.status_fd = open_ro_cloexec(stat_file);
                    }
                }
            }
        }
    }

    // 2. RAPL Discovery (/sys/class/powercap/intel-rapl/intel-rapl:0)
    auto rapl_dir = sysfs_root_ / "class/powercap/intel-rapl";
    if (std::filesystem::exists(rapl_dir, ec)) {
        auto pkg_file = rapl_dir / "intel-rapl:0/energy_uj";
        if (std::filesystem::exists(pkg_file, ec)) {
            rapl_pkg_path_ = pkg_file;
            auto core_file = rapl_dir / "intel-rapl:0/intel-rapl:0:0/energy_uj";
            if (std::filesystem::exists(core_file, ec)) {
                rapl_core_path_ = core_file;
            }
            auto dram_file = rapl_dir / "intel-rapl:0/intel-rapl:0:1/energy_uj";
            if (std::filesystem::exists(dram_file, ec)) {
                rapl_dram_path_ = dram_file;
            }
        }
    }

    // 3. Dynamic Hwmon Discovery (/sys/class/hwmon/hwmon*)
    auto hwmon_base = sysfs_root_ / "class/hwmon";
    if (std::filesystem::exists(hwmon_base, ec)) {
        for (const auto& entry : std::filesystem::directory_iterator(hwmon_base, ec)) {
            auto name_file = entry.path() / "name";
            int n_fd = open_ro_cloexec(name_file);
            if (n_fd < 0) continue;
            auto h_name = read_string_fd(n_fd);
            ::close(n_fd);

            if ((h_name == "k10temp" || h_name == "coretemp") && cpu_temp_path_.empty()) {
                cpu_temp_path_ = entry.path() / "temp1_input";
            } else if (h_name == "amdgpu" || h_name.find("nvidia") != std::string::npos || h_name.find("nouveau") != std::string::npos) {
                if (gpu_power_path_.empty()) {
                    auto p1 = entry.path() / "power1_input";
                    auto p_avg = entry.path() / "power1_average";
                    if (std::filesystem::exists(p1, ec)) gpu_power_path_ = p1;
                    else if (std::filesystem::exists(p_avg, ec)) gpu_power_path_ = p_avg;
                }
                if (gpu_temp_path_.empty()) gpu_temp_path_ = entry.path() / "temp1_input";
                if (gpu_freq_path_.empty()) gpu_freq_path_ = entry.path() / "freq1_input";
                if (gpu_in0_path_.empty()) gpu_in0_path_ = entry.path() / "in0_input";
                if (gpu_in1_path_.empty()) gpu_in1_path_ = entry.path() / "in1_input";
            } else if (h_name == "nvme" && nvme_temp1_path_.empty()) {
                nvme_temp1_path_ = entry.path() / "temp1_input";
                nvme_temp2_path_ = entry.path() / "temp2_input";
            } else if ((h_name == "thinkpad" || h_name.find("smm") != std::string::npos) && fan_rpm_path_.empty()) {
                fan_rpm_path_ = entry.path() / "fan1_input";
                fan_pwm_path_ = entry.path() / "pwm1";
                chassis_temp_path_ = entry.path() / "temp1_input";
            } else if ((h_name.find("wifi") != std::string::npos || h_name.find("iwl") != std::string::npos) && wifi_temp_path_.empty()) {
                wifi_temp_path_ = entry.path() / "temp1_input";
            }
        }
    }

    // 4. DRM Device Discovery (/sys/class/drm/card*)
    auto drm_dir = sysfs_root_ / "class/drm";
    if (std::filesystem::exists(drm_dir, ec)) {
        for (const auto& card_entry : std::filesystem::directory_iterator(drm_dir, ec)) {
            const auto card_name = card_entry.path().filename().string();
            if (card_name.rfind("card", 0) == 0 && card_name.find('-') == std::string::npos) {
                auto dev = card_entry.path() / "device";
                if (std::filesystem::exists(dev / "gpu_busy_percent", ec)) {
                    drm_device_path_ = dev;
                    break;
                }
            }
        }
    }

    // 5. Storage (NVMe power status and block stat)
    auto nvme_class = sysfs_root_ / "class/nvme";
    if (std::filesystem::exists(nvme_class, ec)) {
        for (const auto& nvme_entry : std::filesystem::directory_iterator(nvme_class, ec)) {
            auto r_stat = nvme_entry.path() / "device/power/runtime_status";
            if (std::filesystem::exists(r_stat, ec)) {
                nvme_status_path_ = r_stat;
                break;
            }
        }
    }
    // Block stat for NVMe SSD
    if (std::filesystem::exists("/sys/block/nvme0n1/stat", ec)) {
        block_stat_path_ = "/sys/block/nvme0n1/stat";
    } else if (std::filesystem::exists("/sys/block/sda/stat", ec)) {
        block_stat_path_ = "/sys/block/sda/stat";
    }

    // 6. Display Backlight Discovery (/sys/class/backlight/*)
    auto backlight_dir = sysfs_root_ / "class/backlight";
    if (std::filesystem::exists(backlight_dir, ec)) {
        for (const auto& bl_entry : std::filesystem::directory_iterator(backlight_dir, ec)) {
            if (std::filesystem::exists(bl_entry.path() / "brightness", ec) ||
                std::filesystem::exists(bl_entry.path() / "actual_brightness", ec)) {
                backlight_path_ = bl_entry.path();
                break;
            }
        }
    }

    // 7. CPU Governor & ACPI / Wireless
    cpu_governor_path_ = sysfs_root_ / "devices/system/cpu/cpu0/cpufreq/scaling_governor";

    if (std::filesystem::exists("/proc/acpi/ibm/kbdlight", ec)) {
        kbdlight_path_ = "/proc/acpi/ibm/kbdlight";
    }
    if (std::filesystem::exists("/proc/acpi/ibm/bluetooth", ec)) {
        bluetooth_path_ = "/proc/acpi/ibm/bluetooth";
    }
    if (std::filesystem::exists("/sys/class/net/wlan0/device/power/runtime_status", ec)) {
        wifi_status_path_ = "/sys/class/net/wlan0/device/power/runtime_status";
    }
    if (std::filesystem::exists("/sys/module/pcie_aspm/parameters/policy", ec)) {
        aspm_policy_path_ = "/sys/module/pcie_aspm/parameters/policy";
    }

    open_persistent_fds();
}

HardwareSample HardwareProbe::capture_sample() const {
    WATTCURB_PROFILE_SCOPE("hw.capture_all");
    ++sample_counter_;
    HardwareSample sample;
    sample.timestamp = std::chrono::steady_clock::now();

    // 1. Battery & Power Rail (REF-REQ-022, REF-ARCH-012, REF-REQ-023)
    {
        WATTCURB_PROFILE_SCOPE("hw.battery_rail");
        {
            WATTCURB_PROFILE_SCOPE("hw.battery.ac_check");
            if (ac_online_fd_ >= 0) {
                auto ac_val = read_uint32_fd(ac_online_fd_);
                sample.is_ac_online = (ac_val.value_or(0) == 1);
            }
        }

        // Fast Single-Read uevent Telemetry (REF-RES-007):
        // Read the entire battery state in a SINGLE 1KB pread() syscall!
        if (battery_uevent_fd_ >= 0) {
            bool skip_bat = false;
            if (cached_battery_static_initialized_) {
                if (sample.is_ac_online && !cached_is_discharging_) {
                    skip_bat = (sample_counter_ % 30 != 1);
                } else {
                    // Discharging / Battery mode: ACPI _BST transaction incurs I2C/SMBus bus wait (~3.4ms).
                    // Battery chemistry time-constant is tens of minutes; sample every 8 turns (16s).
                    skip_bat = (sample_counter_ % 8 != 1);
                }
            }

            if (!skip_bat) {
                alignas(64) char uevent_buf[1024];
                ssize_t n = 0;
                {
                    WATTCURB_PROFILE_SCOPE("hw.battery.uevent_io");
                    n = ::pread(battery_uevent_fd_, uevent_buf, sizeof(uevent_buf) - 1, 0);
                }
                if (n > 0) {
                    uevent_buf[n] = '\0';
                    parse_battery_uevent_buf(std::string_view(uevent_buf, static_cast<size_t>(n)), sample);
                    {
                        WATTCURB_PROFILE_SCOPE("hw.battery.cache_state_update");
                        cached_is_discharging_ = sample.is_discharging;
                        cached_bat_power_uw_ = sample.battery_power_uw;
                        cached_bat_current_ua_ = sample.battery_current_ua;
                        cached_bat_voltage_ = sample.battery_voltage_uv;
                        cached_bat_energy_now_ = sample.battery_energy_now_uwh;
                        cached_bat_capacity_percent_ = sample.battery_capacity_percent;
                        cached_energy_full_ = sample.battery_energy_full_uwh;
                        cached_energy_full_design_ = sample.battery_energy_full_design_uwh;
                        cached_cycle_count_ = sample.battery_cycle_count;

                        cached_voltage_min_design_ = sample.battery_voltage_min_design_uv;
                        cached_bat_capacity_level_ = sample.battery_capacity_level;
                        cached_bat_technology_ = sample.battery_technology;
                        cached_bat_model_name_ = sample.battery_model_name;
                        cached_bat_manufacturer_ = sample.battery_manufacturer;
                        cached_bat_serial_number_ = sample.battery_serial_number;
                        cached_battery_static_initialized_ = true;
                    }
                }
            } else {
                WATTCURB_PROFILE_SCOPE("hw.battery.cached_replay");
                sample.is_discharging = cached_is_discharging_;
                sample.battery_power_uw = cached_bat_power_uw_;
                sample.battery_current_ua = cached_bat_current_ua_;
                sample.battery_voltage_uv = cached_bat_voltage_;
                sample.battery_energy_now_uwh = cached_bat_energy_now_;
                sample.battery_capacity_percent = cached_bat_capacity_percent_;
                sample.battery_energy_full_uwh = cached_energy_full_;
                sample.battery_energy_full_design_uwh = cached_energy_full_design_;
                sample.battery_cycle_count = cached_cycle_count_;

                sample.battery_voltage_min_design_uv = cached_voltage_min_design_;
                sample.battery_capacity_level = cached_bat_capacity_level_;
                sample.battery_technology = cached_bat_technology_;
                sample.battery_model_name = cached_bat_model_name_;
                sample.battery_manufacturer = cached_bat_manufacturer_;
                sample.battery_serial_number = cached_bat_serial_number_;
            }
        } else {
            WATTCURB_PROFILE_SCOPE("hw.battery.fallback_sysfs");
            if (battery_status_fd_ >= 0) {
                std::array<char, 32> stat_buf{};
                if (read_string_buf(battery_status_fd_, stat_buf.data(), stat_buf.size())) {
                    sample.is_discharging = (std::strncmp(stat_buf.data(), "Discharging", 11) == 0);
                }
            }

            // Fast path for AC power (REF-RES-006):
            // When running on AC and battery is not discharging, battery rail draw is 0W.
            // Subsample slow ACPI gas-gauge nodes to once every 30 passes (~60 seconds).
            bool skip_detailed_bat = sample.is_ac_online && !sample.is_discharging && cached_battery_static_initialized_ && (sample_counter_ % 30 != 1);

            if (!skip_detailed_bat) {
                if (battery_power_fd_ >= 0) {
                    sample.battery_power_uw = read_uint64_fd(battery_power_fd_);
                } else if (battery_voltage_fd_ >= 0 && battery_current_fd_ >= 0) {
                    auto v = read_uint64_fd(battery_voltage_fd_);
                    auto i = read_int64_fd(battery_current_fd_);
                    if (v.has_value() && i.has_value()) {
                        sample.battery_voltage_uv = v;
                        sample.battery_current_ua = i;
                        int64_t abs_curr = *i < 0 ? -*i : *i;
                        sample.battery_power_uw = static_cast<uint64_t>((*v * static_cast<uint64_t>(abs_curr)) / 1'000'000ULL);
                    }
                }

                if (battery_voltage_fd_ >= 0 && !sample.battery_voltage_uv) {
                    sample.battery_voltage_uv = read_uint64_fd(battery_voltage_fd_);
                }
                if (battery_current_fd_ >= 0 && !sample.battery_current_ua) {
                    sample.battery_current_ua = read_int64_fd(battery_current_fd_);
                }
                if (battery_energy_now_fd_ >= 0) {
                    cached_bat_energy_now_ = read_uint64_fd(battery_energy_now_fd_);
                }
                if (battery_voltage_fd_ >= 0) {
                    cached_bat_voltage_ = sample.battery_voltage_uv;
                }
                sample.battery_energy_now_uwh = cached_bat_energy_now_;
                if (!cached_battery_static_initialized_ || sample_counter_ % 30 == 1) {
                    if (battery_energy_full_fd_ >= 0) cached_energy_full_ = read_uint64_fd(battery_energy_full_fd_);
                    if (battery_energy_full_design_fd_ >= 0) cached_energy_full_design_ = read_uint64_fd(battery_energy_full_design_fd_);
                    if (battery_cycle_fd_ >= 0) cached_cycle_count_ = read_uint32_fd(battery_cycle_fd_);
                    cached_battery_static_initialized_ = true;
                }
                if (battery_capacity_fd_ >= 0) sample.battery_capacity_percent = read_uint32_fd(battery_capacity_fd_);
            } else {
                sample.battery_energy_now_uwh = cached_bat_energy_now_;
                sample.battery_voltage_uv = cached_bat_voltage_;
            }
            sample.battery_energy_full_uwh = cached_energy_full_;
            sample.battery_energy_full_design_uwh = cached_energy_full_design_;
            sample.battery_cycle_count = cached_cycle_count_;
        }

        // ThinkPad Charge Thresholds & Behavior (REF-REQ-022, REF-REQ-023, REF-REQ-024)
        // Subsample slow ACPI EC transactions: threshold registers rarely change.
        // Eliminate periodic EC SMBus wakeups completely: only sample on initial pass or AC plug/unplug transition.
        {
            WATTCURB_PROFILE_SCOPE("hw.battery.thresholds");
            bool ac_transition = (sample.is_ac_online != cached_ac_online_);
            cached_ac_online_ = sample.is_ac_online;
            bool poll_thresholds = !cached_battery_static_initialized_ || ac_transition;
            if (poll_thresholds) {
                WATTCURB_PROFILE_SCOPE("hw.battery.threshold_io");
                if (battery_threshold_start_fd_ >= 0) {
                    sample.battery_charge_start_threshold = read_uint32_fd(battery_threshold_start_fd_);
                    cached_bat_charge_start_threshold_ = sample.battery_charge_start_threshold;
                } else {
                    sample.battery_charge_start_threshold = cached_bat_charge_start_threshold_;
                }
                if (battery_threshold_end_fd_ >= 0) {
                    sample.battery_charge_end_threshold = read_uint32_fd(battery_threshold_end_fd_);
                    cached_bat_charge_end_threshold_ = sample.battery_charge_end_threshold;
                } else {
                    sample.battery_charge_end_threshold = cached_bat_charge_end_threshold_;
                }
                if (battery_behaviour_fd_ >= 0) {
                    std::array<char, 32> b_buf{};
                    if (read_string_buf(battery_behaviour_fd_, b_buf.data(), b_buf.size())) {
                        sample.battery_charge_behaviour = trim_sv(b_buf.data());
                        cached_bat_charge_behaviour_ = sample.battery_charge_behaviour;
                    }
                } else {
                    sample.battery_charge_behaviour = cached_bat_charge_behaviour_;
                }
            } else {
                sample.battery_charge_start_threshold = cached_bat_charge_start_threshold_;
                sample.battery_charge_end_threshold = cached_bat_charge_end_threshold_;
                sample.battery_charge_behaviour = cached_bat_charge_behaviour_;
            }
        }

        // USB-C Power Delivery & Source Profile
        {
            WATTCURB_PROFILE_SCOPE("hw.battery.usbc_pd");
            if (usbc_online_fd_ >= 0) {
                WATTCURB_PROFILE_SCOPE("hw.battery.usbc_io");
                auto u_on = read_uint32_fd(usbc_online_fd_);
                sample.usbc_pd_online = (u_on.value_or(0) == 1);
                if (usbc_voltage_fd_ >= 0) sample.usbc_pd_voltage_uv = read_uint64_fd(usbc_voltage_fd_);
                if (usbc_current_fd_ >= 0) sample.usbc_pd_current_ua = read_uint64_fd(usbc_current_fd_);
                if (usbc_voltage_max_fd_ >= 0) sample.usbc_pd_voltage_max_uv = read_uint64_fd(usbc_voltage_max_fd_);
                if (usbc_current_max_fd_ >= 0) sample.usbc_pd_current_max_ua = read_uint64_fd(usbc_current_max_fd_);
                if (usbc_type_fd_ >= 0) {
                    std::array<char, 32> t_buf{};
                    if (read_string_buf(usbc_type_fd_, t_buf.data(), t_buf.size())) {
                        sample.usbc_pd_type = trim_sv(t_buf.data());
                        cached_usbc_pd_type_ = sample.usbc_pd_type;
                    }
                } else {
                    sample.usbc_pd_type = cached_usbc_pd_type_;
                }
            }
        }

        // Connected Peripheral Batteries (Bluetooth/HID/Stylus)
        {
            WATTCURB_PROFILE_SCOPE("hw.battery.peripherals");
            for (size_t i = 0; i < peripheral_probe_count_; ++i) {
                WATTCURB_PROFILE_SCOPE("hw.battery.peripheral_scan");
                const auto& p = peripheral_probes_[i];
                if (p.capacity_fd >= 0) {
                    auto cap = read_uint32_fd(p.capacity_fd);
                    if (cap.has_value()) {
                        HardwareSample::PeripheralBattery pb;
                        pb.name = p.name;
                        pb.capacity_percent = *cap;
                        if (p.status_fd >= 0) {
                            std::array<char, 16> s_buf{};
                            if (read_string_buf(p.status_fd, s_buf.data(), s_buf.size())) {
                                pb.is_charging = (std::strncmp(s_buf.data(), "Charging", 8) == 0);
                            }
                        }
                        sample.peripheral_batteries.push_back(pb);
                    }
                }
            }
        }
    }

    // 2. RAPL & CPU Telemetry
    {
        WATTCURB_PROFILE_SCOPE("hw.cpu_metrics");
        {
            WATTCURB_PROFILE_SCOPE("hw.cpu_rapl_temp");
            if (rapl_pkg_fd_ >= 0) sample.rapl_package_uj = read_uint64_fd(rapl_pkg_fd_);
            if (rapl_core_fd_ >= 0) sample.rapl_core_uj = read_uint64_fd(rapl_core_fd_);
            if (rapl_dram_fd_ >= 0) sample.rapl_dram_uj = read_uint64_fd(rapl_dram_fd_);
            if (cpu_temp_fd_ >= 0) sample.cpu_temp_mdeg = read_int32_fd(cpu_temp_fd_);

            if (cpu_governor_fd_ >= 0) {
                read_string_buf(cpu_governor_fd_, sample.cpu_governor.data(), sample.cpu_governor.size());
            }
        }

        // CPU Frequencies
        if (!cpu_freq_fds_.empty()) {
            WATTCURB_PROFILE_SCOPE("hw.cpu_freqs");
            uint64_t sum_khz = 0;
            uint32_t min_khz = UINT32_MAX;
            uint32_t max_khz = 0;
            uint32_t count = 0;

            for (int fd : cpu_freq_fds_) {
                auto khz = read_uint32_fd(fd);
                if (khz.has_value() && *khz > 0) {
                    sum_khz += *khz;
                    min_khz = std::min(min_khz, *khz);
                    max_khz = std::max(max_khz, *khz);
                    ++count;
                }
            }
            if (count > 0) {
                sample.cpu_cores_online = count;
                sample.cpu_freq_min_khz = min_khz;
                sample.cpu_freq_max_khz = max_khz;
                sample.cpu_freq_avg_khz = static_cast<uint32_t>(sum_khz / count);
            }
        }

        // CPU C-States
        {
            WATTCURB_PROFILE_SCOPE("hw.cpu_cstates");
            for (const auto& states : cpu_cstate_fds_) {
                for (size_t s = 0; s < 4; ++s) {
                    if (states[s] >= 0) {
                        auto us = read_uint64_fd(states[s]);
                        if (us.has_value()) {
                            sample.cstate_time_us[s] += *us;
                        }
                    }
                }
            }
        }
    }

    // 3. GPU Telemetry
    {
        WATTCURB_PROFILE_SCOPE("hw.gpu_metrics");
        {
            WATTCURB_PROFILE_SCOPE("hw.gpu_power_core");
            if (gpu_power_fd_ >= 0) sample.gpu_power_uw = read_uint64_fd(gpu_power_fd_);
            if (gpu_temp_fd_ >= 0) sample.gpu_temp_mdeg = read_int32_fd(gpu_temp_fd_);
            if (gpu_freq_fd_ >= 0) sample.gpu_freq_hz = read_uint64_fd(gpu_freq_fd_);
            if (gpu_in0_fd_ >= 0) sample.gpu_vddgfx_mv = read_uint32_fd(gpu_in0_fd_);
            if (gpu_in1_fd_ >= 0) sample.gpu_vddsoc_mv = read_uint32_fd(gpu_in1_fd_);
            if (gpu_busy_fd_ >= 0) sample.gpu_busy_percent = read_uint32_fd(gpu_busy_fd_);
        }
        {
            WATTCURB_PROFILE_SCOPE("hw.gpu_vram_pcie");
            if (gpu_vram_used_fd_ >= 0) sample.gpu_vram_used_bytes = read_uint64_fd(gpu_vram_used_fd_);
            if (gpu_vram_total_fd_ >= 0) sample.gpu_vram_total_bytes = read_uint64_fd(gpu_vram_total_fd_);
            if (gpu_link_speed_fd_ >= 0) {
                read_string_buf(gpu_link_speed_fd_, sample.gpu_pcie_link_speed.data(), sample.gpu_pcie_link_speed.size());
            }
            if (gpu_link_width_fd_ >= 0) sample.gpu_pcie_link_width = read_uint32_fd(gpu_link_width_fd_);
        }
    }

    // 4. Storage Telemetry
    {
        WATTCURB_PROFILE_SCOPE("hw.storage_metrics");
        {
            WATTCURB_PROFILE_SCOPE("hw.storage_nvme");
            if (nvme_status_fd_ >= 0) {
                std::array<char, 32> stat_buf{};
                if (read_string_buf(nvme_status_fd_, stat_buf.data(), stat_buf.size())) {
                    sample.nvme_active = (std::strncmp(stat_buf.data(), "active", 6) == 0);
                }
            }

            // Sub-sample NVMe SMART temperatures to prevent PCIe link wakeups and D0 latency
            // Strictly query only when NVMe is active and in steady state (pass 10, 40, 70...)
            if (sample.nvme_active && (sample_counter_ % 30 == 10)) {
                if (nvme_temp1_fd_ >= 0) cached_nvme_temp1_ = read_int32_fd(nvme_temp1_fd_);
                if (nvme_temp2_fd_ >= 0) cached_nvme_temp2_ = read_int32_fd(nvme_temp2_fd_);
            }
            sample.nvme_temp_composite_mdeg = cached_nvme_temp1_;
            sample.nvme_temp_sensor1_mdeg = cached_nvme_temp2_;
        }

        if (block_stat_fd_ >= 0) {
            WATTCURB_PROFILE_SCOPE("hw.storage_block");
            std::array<char, 256> buf{};
            ssize_t n = ::pread(block_stat_fd_, buf.data(), buf.size() - 1, 0);
            if (n > 0) {
                buf[static_cast<size_t>(n)] = '\0';
                const char* p = buf.data();
                const char* end = buf.data() + n;

                // Parse tokens 0 to 9
                uint64_t val = 0;
                for (int token_idx = 0; token_idx <= 9 && p < end; ++token_idx) {
                    while (p < end && (*p == ' ' || *p == '\t')) ++p;
                    const char* tok_start = p;
                    while (p < end && *p != ' ' && *p != '\t' && *p != '\n') ++p;
                    if (std::from_chars(tok_start, p, val).ec == std::errc()) {
                        if (token_idx == 2) sample.disk_read_sectors = val;
                        else if (token_idx == 6) sample.disk_write_sectors = val;
                        else if (token_idx == 9) sample.disk_io_ticks_ms = val;
                    }
                }
            }
        }
    }

    // 5. Chassis & Mechanical Thermal
    {
        WATTCURB_PROFILE_SCOPE("hw.fan_chassis");
        // Sub-sample slow ACPI EC fan queries (7~15ms EC bus stall)
        if (!cached_fan_rpm_.has_value() || (sample_counter_ % 2 == 1)) {
            if (fan_rpm_fd_ >= 0) cached_fan_rpm_ = read_uint32_fd(fan_rpm_fd_);
            if (fan_pwm_fd_ >= 0) cached_fan_pwm_ = read_uint32_fd(fan_pwm_fd_);
            if (chassis_temp_fd_ >= 0) cached_chassis_temp_ = read_int32_fd(chassis_temp_fd_);
        }
        sample.fan_rpm = cached_fan_rpm_;
        sample.fan_pwm = cached_fan_pwm_;
        sample.chassis_temp_mdeg = cached_chassis_temp_;

        // Sub-sample slow ACPI EC kbdlight (16ms per read) and bluetooth status
        if (kbdlight_fd_ >= 0) {
            if (sample_counter_ % 30 == 15) {
                std::array<char, 64> kbd_buf{};
                if (read_string_buf(kbdlight_fd_, kbd_buf.data(), kbd_buf.size())) {
                    const char* s_pos = std::strstr(kbd_buf.data(), "status:\t");
                    if (s_pos) {
                        uint32_t lvl = 0;
                        s_pos += 8;
                        if (std::from_chars(s_pos, kbd_buf.data() + kbd_buf.size(), lvl).ec == std::errc()) {
                            cached_kbdlight_level_ = lvl;
                        }
                    }
                }
            }
            sample.kbdlight_level = cached_kbdlight_level_;
        }

        if (bluetooth_fd_ >= 0) {
            if (sample_counter_ % 30 == 1) {
                std::array<char, 64> bt_buf{};
                if (read_string_buf(bluetooth_fd_, bt_buf.data(), bt_buf.size())) {
                    cached_bluetooth_enabled_ = (std::strstr(bt_buf.data(), "enabled") != nullptr);
                }
            }
            sample.bluetooth_enabled = cached_bluetooth_enabled_;
        }
    }

    // 6. Display & Wireless
    {
        WATTCURB_PROFILE_SCOPE("hw.display_wireless");
        {
            WATTCURB_PROFILE_SCOPE("hw.display_backlight");
            if (backlight_cur_fd_ >= 0) sample.backlight_brightness = read_uint32_fd(backlight_cur_fd_);
            if (backlight_max_fd_ >= 0) sample.backlight_max_brightness = read_uint32_fd(backlight_max_fd_);
        }

        {
            WATTCURB_PROFILE_SCOPE("hw.wireless_wifi");
            if (wifi_status_fd_ >= 0) {
                std::array<char, 32> w_buf{};
                if (read_string_buf(wifi_status_fd_, w_buf.data(), w_buf.size())) {
                    sample.wifi_active = (std::strncmp(w_buf.data(), "active", 6) == 0);
                }
            }
            if (sample_counter_ % 5 == 1 || !cached_wifi_temp_.has_value()) {
                if (wifi_temp_fd_ >= 0) cached_wifi_temp_ = read_int32_fd(wifi_temp_fd_);
                if (aspm_policy_fd_ >= 0 && !cached_aspm_policy_initialized_) {
                    read_string_buf(aspm_policy_fd_, cached_aspm_policy_.data(), cached_aspm_policy_.size());
                    cached_aspm_policy_initialized_ = true;
                }
            }
            sample.wifi_temp_mdeg = cached_wifi_temp_;
            sample.aspm_policy = cached_aspm_policy_;
        }
    }

    // 8. Syscall-Level Direct Hardware Telemetry (REF-REQ-015)
    {
        WATTCURB_PROFILE_SCOPE("hw.syscall_telemetry");

        // PMU Hardware Counters via perf_event_open (Direct single-read syscalls)
        if (pmu_instructions_fd_ >= 0) {
            uint64_t inst = 0;
            if (::read(pmu_instructions_fd_, &inst, sizeof(inst)) == sizeof(inst)) {
                sample.pmu_instructions = inst;
            }
        }
        if (pmu_cycles_fd_ >= 0) {
            uint64_t cyc = 0;
            if (::read(pmu_cycles_fd_, &cyc, sizeof(cyc)) == sizeof(cyc)) {
                sample.pmu_cycles = cyc;
            }
        }
        if (sample.pmu_cycles > 0 && sample.pmu_instructions > 0) {
            sample.pmu_ipc = static_cast<double>(sample.pmu_instructions) / static_cast<double>(sample.pmu_cycles);
        }
        if (pmu_llc_misses_fd_ >= 0) {
            uint64_t llc = 0;
            if (::read(pmu_llc_misses_fd_, &llc, sizeof(llc)) == sizeof(llc)) {
                sample.pmu_llc_misses = llc;
            }
        }
        if (pmu_branch_misses_fd_ >= 0) {
            uint64_t bm = 0;
            if (::read(pmu_branch_misses_fd_, &bm, sizeof(bm)) == sizeof(bm)) {
                sample.pmu_branch_misses = bm;
            }
        }

        // Standalone Sample PMU Energy Proxy (REF-REQ-024)
        if (sample.pmu_instructions > 0) {
            double epi = (static_cast<double>(sample.pmu_instructions) * sample.pmu_ipc) +
                         (200.0 * static_cast<double>(sample.pmu_llc_misses)) +
                         (30.0 * static_cast<double>(sample.pmu_branch_misses));
            sample.pmu_energy_proxy_index = epi;
            double waste = (200.0 * static_cast<double>(sample.pmu_llc_misses)) +
                           (30.0 * static_cast<double>(sample.pmu_branch_misses));
            sample.pmu_energy_waste_ratio = epi > 0.0 ? std::clamp((waste / epi) * 100.0, 0.0, 100.0) : 0.0;
        }

        // Direct PCIe Binary Config Space Decoding
        if (pcie_gpu_config_fd_ >= 0) {
            auto [speed, width] = read_pcie_binary_link_status(pcie_gpu_config_fd_);
            if (speed > 0) {
                sample.pcie_link_speed_gen = speed;
                sample.pcie_link_width_lanes = width;
            }
        }
        // Graceful fallback for non-root 64-byte config limit
        if (sample.pcie_link_speed_gen == 0 && gpu_link_speed_fd_ >= 0) {
            std::array<char, 32> spd_buf{};
            if (read_string_buf(gpu_link_speed_fd_, spd_buf.data(), spd_buf.size())) {
                if (std::strstr(spd_buf.data(), "16.0")) sample.pcie_link_speed_gen = 4;
                else if (std::strstr(spd_buf.data(), "8.0")) sample.pcie_link_speed_gen = 3;
                else if (std::strstr(spd_buf.data(), "5.0")) sample.pcie_link_speed_gen = 2;
                else if (std::strstr(spd_buf.data(), "2.5")) sample.pcie_link_speed_gen = 1;
                else if (std::strstr(spd_buf.data(), "32.0")) sample.pcie_link_speed_gen = 5;
            }
        }
        if (sample.pcie_link_width_lanes == 0 && gpu_link_width_fd_ >= 0) {
            auto w = read_uint32_fd(gpu_link_width_fd_);
            if (w) sample.pcie_link_width_lanes = static_cast<uint8_t>(*w);
        }

        // Direct AMD Zen Silicon MSR Core VID Telemetry
        if (cpu0_msr_fd_ >= 0) {
            uint64_t msr_val = 0;
            if (::pread(cpu0_msr_fd_, &msr_val, sizeof(msr_val), 0xC0010064) == sizeof(msr_val)) {
                uint32_t vid = static_cast<uint32_t>((msr_val >> 14) & 0xFF);
                if (vid > 0 && vid <= 0xFF) {
                    int32_t mv = 1550 - static_cast<int32_t>(vid * 625 / 100);
                    if (mv > 200 && mv < 2000) {
                        sample.cpu_core_vid_mv = static_cast<uint32_t>(mv);
                    }
                }
            }
        }
        if (!sample.cpu_core_vid_mv.has_value() && sample.gpu_vddgfx_mv.has_value()) {
            sample.cpu_core_vid_mv = sample.gpu_vddgfx_mv;
        }
    }

    return sample;
}

std::optional<uint64_t> HardwareProbe::read_uint64_fd(int fd) {
    if (fd < 0) return std::nullopt;

    std::array<char, 64> buffer{};
    ssize_t bytes_read = ::pread(fd, buffer.data(), buffer.size() - 1, 0);
    if (bytes_read <= 0) return std::nullopt;
    buffer[static_cast<size_t>(bytes_read)] = '\0';

    uint64_t value = 0;
    const char* start = buffer.data();
    while (*start == ' ' || *start == '\t' || *start == '\n') ++start;
    const char* end = buffer.data() + bytes_read;

    auto [ptr, ec] = std::from_chars(start, end, value);
    if (ec == std::errc()) {
        return value;
    }
    return std::nullopt;
}

std::optional<int64_t> HardwareProbe::read_int64_fd(int fd) {
    if (fd < 0) return std::nullopt;

    std::array<char, 64> buffer{};
    ssize_t bytes_read = ::pread(fd, buffer.data(), buffer.size() - 1, 0);
    if (bytes_read <= 0) return std::nullopt;
    buffer[static_cast<size_t>(bytes_read)] = '\0';

    int64_t value = 0;
    const char* start = buffer.data();
    while (*start == ' ' || *start == '\t' || *start == '\n') ++start;
    const char* end = buffer.data() + bytes_read;

    auto [ptr, ec] = std::from_chars(start, end, value);
    if (ec == std::errc()) {
        return value;
    }
    return std::nullopt;
}

std::optional<uint32_t> HardwareProbe::read_uint32_fd(int fd) {
    if (fd < 0) return std::nullopt;

    std::array<char, 32> buffer{};
    ssize_t bytes_read = ::pread(fd, buffer.data(), buffer.size() - 1, 0);
    if (bytes_read <= 0) return std::nullopt;
    buffer[static_cast<size_t>(bytes_read)] = '\0';

    uint32_t value = 0;
    const char* start = buffer.data();
    while (*start == ' ' || *start == '\t' || *start == '\n') ++start;
    const char* end = buffer.data() + bytes_read;

    auto [ptr, ec] = std::from_chars(start, end, value);
    if (ec == std::errc()) {
        return value;
    }
    return std::nullopt;
}

std::optional<int32_t> HardwareProbe::read_int32_fd(int fd) {
    if (fd < 0) return std::nullopt;

    std::array<char, 32> buffer{};
    ssize_t bytes_read = ::pread(fd, buffer.data(), buffer.size() - 1, 0);
    if (bytes_read <= 0) return std::nullopt;
    buffer[static_cast<size_t>(bytes_read)] = '\0';

    int32_t value = 0;
    const char* start = buffer.data();
    while (*start == ' ' || *start == '\t' || *start == '\n') ++start;
    const char* end = buffer.data() + bytes_read;

    auto [ptr, ec] = std::from_chars(start, end, value);
    if (ec == std::errc()) {
        return value;
    }
    return std::nullopt;
}

bool HardwareProbe::read_string_buf(int fd, char* buf, size_t max_len) {
    if (fd < 0 || buf == nullptr || max_len == 0) return false;

    ssize_t bytes_read = ::pread(fd, buf, max_len - 1, 0);
    if (bytes_read <= 0) {
        buf[0] = '\0';
        return false;
    }

    while (bytes_read > 0 && (buf[bytes_read - 1] == '\n' ||
                              buf[bytes_read - 1] == '\r' ||
                              buf[bytes_read - 1] == ' ')) {
        --bytes_read;
    }
    buf[static_cast<size_t>(bytes_read)] = '\0';
    return true;
}

std::string HardwareProbe::read_string_fd(int fd) {
    if (fd < 0) return {};

    std::array<char, 64> buffer{};
    if (!read_string_buf(fd, buffer.data(), buffer.size())) {
        return {};
    }
    return std::string(buffer.data());
}

} // namespace wattcurb::hw
