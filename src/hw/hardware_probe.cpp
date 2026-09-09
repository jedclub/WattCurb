#include "hw/hardware_probe.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>
#include <fcntl.h>
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

} // namespace

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
      ac_path_(std::move(other.ac_path_)),
      usbc_pd_path_(std::move(other.usbc_pd_path_)),
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
      usbc_voltage_fd_(std::exchange(other.usbc_voltage_fd_, -1)),
      usbc_current_fd_(std::exchange(other.usbc_current_fd_, -1)),
      usbc_online_fd_(std::exchange(other.usbc_online_fd_, -1)),
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
      aspm_policy_fd_(std::exchange(other.aspm_policy_fd_, -1)) {}

HardwareProbe& HardwareProbe::operator=(HardwareProbe&& other) noexcept {
    if (this != &other) {
        close_fds();
        sysfs_root_ = std::move(other.sysfs_root_);
        battery_path_ = std::move(other.battery_path_);
        ac_path_ = std::move(other.ac_path_);
        usbc_pd_path_ = std::move(other.usbc_pd_path_);
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
        usbc_voltage_fd_ = std::exchange(other.usbc_voltage_fd_, -1);
        usbc_current_fd_ = std::exchange(other.usbc_current_fd_, -1);
        usbc_online_fd_ = std::exchange(other.usbc_online_fd_, -1);
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
    }
    return *this;
}

void HardwareProbe::close_fds() noexcept {
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
    safe_close(usbc_voltage_fd_);
    safe_close(usbc_current_fd_);
    safe_close(usbc_online_fd_);
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
}

void HardwareProbe::open_persistent_fds() {
    close_fds();

    // 1. Battery & Power Rail
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
    }
    if (!ac_path_.empty()) {
        ac_online_fd_ = open_ro_cloexec(ac_path_ / "online");
    }
    if (!usbc_pd_path_.empty()) {
        usbc_voltage_fd_ = open_ro_cloexec(usbc_pd_path_ / "voltage_now");
        usbc_current_fd_ = open_ro_cloexec(usbc_pd_path_ / "current_now");
        usbc_online_fd_ = open_ro_cloexec(usbc_pd_path_ / "online");
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
    HardwareSample sample;
    sample.timestamp = std::chrono::steady_clock::now();

    // 1. Battery & Power Rail
    if (battery_status_fd_ >= 0) {
        std::array<char, 32> stat_buf{};
        if (read_string_buf(battery_status_fd_, stat_buf.data(), stat_buf.size())) {
            sample.is_discharging = (std::strncmp(stat_buf.data(), "Discharging", 11) == 0);
        }
    }

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
    if (battery_energy_now_fd_ >= 0) sample.battery_energy_now_uwh = read_uint64_fd(battery_energy_now_fd_);
    if (battery_energy_full_fd_ >= 0) sample.battery_energy_full_uwh = read_uint64_fd(battery_energy_full_fd_);
    if (battery_energy_full_design_fd_ >= 0) sample.battery_energy_full_design_uwh = read_uint64_fd(battery_energy_full_design_fd_);
    if (battery_cycle_fd_ >= 0) sample.battery_cycle_count = read_uint32_fd(battery_cycle_fd_);
    if (battery_capacity_fd_ >= 0) sample.battery_capacity_percent = read_uint32_fd(battery_capacity_fd_);

    if (ac_online_fd_ >= 0) {
        auto ac_val = read_uint32_fd(ac_online_fd_);
        sample.is_ac_online = (ac_val.value_or(0) == 1);
    }

    if (usbc_online_fd_ >= 0) {
        auto u_on = read_uint32_fd(usbc_online_fd_);
        sample.usbc_pd_online = (u_on.value_or(0) == 1);
        if (usbc_voltage_fd_ >= 0) sample.usbc_pd_voltage_uv = read_uint64_fd(usbc_voltage_fd_);
        if (usbc_current_fd_ >= 0) sample.usbc_pd_current_ua = read_uint64_fd(usbc_current_fd_);
    }

    // 2. RAPL & CPU Telemetry
    if (rapl_pkg_fd_ >= 0) sample.rapl_package_uj = read_uint64_fd(rapl_pkg_fd_);
    if (rapl_core_fd_ >= 0) sample.rapl_core_uj = read_uint64_fd(rapl_core_fd_);
    if (rapl_dram_fd_ >= 0) sample.rapl_dram_uj = read_uint64_fd(rapl_dram_fd_);
    if (cpu_temp_fd_ >= 0) sample.cpu_temp_mdeg = read_int32_fd(cpu_temp_fd_);

    if (cpu_governor_fd_ >= 0) {
        read_string_buf(cpu_governor_fd_, sample.cpu_governor.data(), sample.cpu_governor.size());
    }

    // CPU Frequencies
    if (!cpu_freq_fds_.empty()) {
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

    // 3. GPU Telemetry
    if (gpu_power_fd_ >= 0) sample.gpu_power_uw = read_uint64_fd(gpu_power_fd_);
    if (gpu_temp_fd_ >= 0) sample.gpu_temp_mdeg = read_int32_fd(gpu_temp_fd_);
    if (gpu_freq_fd_ >= 0) sample.gpu_freq_hz = read_uint64_fd(gpu_freq_fd_);
    if (gpu_in0_fd_ >= 0) sample.gpu_vddgfx_mv = read_uint32_fd(gpu_in0_fd_);
    if (gpu_in1_fd_ >= 0) sample.gpu_vddsoc_mv = read_uint32_fd(gpu_in1_fd_);
    if (gpu_busy_fd_ >= 0) sample.gpu_busy_percent = read_uint32_fd(gpu_busy_fd_);
    if (gpu_vram_used_fd_ >= 0) sample.gpu_vram_used_bytes = read_uint64_fd(gpu_vram_used_fd_);
    if (gpu_vram_total_fd_ >= 0) sample.gpu_vram_total_bytes = read_uint64_fd(gpu_vram_total_fd_);
    if (gpu_link_speed_fd_ >= 0) {
        read_string_buf(gpu_link_speed_fd_, sample.gpu_pcie_link_speed.data(), sample.gpu_pcie_link_speed.size());
    }
    if (gpu_link_width_fd_ >= 0) sample.gpu_pcie_link_width = read_uint32_fd(gpu_link_width_fd_);

    // 4. Storage Telemetry
    if (nvme_status_fd_ >= 0) {
        std::array<char, 32> stat_buf{};
        if (read_string_buf(nvme_status_fd_, stat_buf.data(), stat_buf.size())) {
            sample.nvme_active = (std::strncmp(stat_buf.data(), "active", 6) == 0);
        }
    }
    if (nvme_temp1_fd_ >= 0) sample.nvme_temp_composite_mdeg = read_int32_fd(nvme_temp1_fd_);
    if (nvme_temp2_fd_ >= 0) sample.nvme_temp_sensor1_mdeg = read_int32_fd(nvme_temp2_fd_);

    if (block_stat_fd_ >= 0) {
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
                if (p >= end) break;
                auto [next_p, ec] = std::from_chars(p, end, val);
                if (ec == std::errc()) {
                    if (token_idx == 2) sample.disk_read_sectors = val;
                    else if (token_idx == 6) sample.disk_write_sectors = val;
                    else if (token_idx == 9) sample.disk_io_ticks_ms = val;
                    p = next_p;
                } else {
                    break;
                }
            }
        }
    }

    // 5. Chassis & Mechanical Thermal
    if (fan_rpm_fd_ >= 0) sample.fan_rpm = read_uint32_fd(fan_rpm_fd_);
    if (fan_pwm_fd_ >= 0) sample.fan_pwm = read_uint32_fd(fan_pwm_fd_);
    if (chassis_temp_fd_ >= 0) sample.chassis_temp_mdeg = read_int32_fd(chassis_temp_fd_);

    if (kbdlight_fd_ >= 0) {
        std::array<char, 64> kbd_buf{};
        if (read_string_buf(kbdlight_fd_, kbd_buf.data(), kbd_buf.size())) {
            const char* s_pos = std::strstr(kbd_buf.data(), "status:\t");
            if (s_pos) {
                uint32_t lvl = 0;
                s_pos += 8;
                if (std::from_chars(s_pos, kbd_buf.data() + kbd_buf.size(), lvl).ec == std::errc()) {
                    sample.kbdlight_level = lvl;
                }
            }
        }
    }

    if (bluetooth_fd_ >= 0) {
        std::array<char, 64> bt_buf{};
        if (read_string_buf(bluetooth_fd_, bt_buf.data(), bt_buf.size())) {
            sample.bluetooth_enabled = (std::strstr(bt_buf.data(), "enabled") != nullptr);
        }
    }

    // 6. Display Subsystem
    if (backlight_cur_fd_ >= 0) sample.backlight_brightness = read_uint32_fd(backlight_cur_fd_);
    if (backlight_max_fd_ >= 0) sample.backlight_max_brightness = read_uint32_fd(backlight_max_fd_);

    // 7. Wireless & ASPM
    if (wifi_status_fd_ >= 0) {
        std::array<char, 32> w_buf{};
        if (read_string_buf(wifi_status_fd_, w_buf.data(), w_buf.size())) {
            sample.wifi_active = (std::strncmp(w_buf.data(), "active", 6) == 0);
        }
    }
    if (wifi_temp_fd_ >= 0) sample.wifi_temp_mdeg = read_int32_fd(wifi_temp_fd_);
    if (aspm_policy_fd_ >= 0) {
        read_string_buf(aspm_policy_fd_, sample.aspm_policy.data(), sample.aspm_policy.size());
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
