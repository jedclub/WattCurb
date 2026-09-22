#pragma once

#include "core/types.hpp"
#include "core/environment_profile.hpp"
#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace wattcurb::hw {

// Implements REF-REQ-001, REF-REQ-007, REF-REQ-010, REF-ARCH-004 & REF-REQ-026
class HardwareProbe {
public:
    explicit HardwareProbe(std::filesystem::path sysfs_root = "/sys");
    ~HardwareProbe();

    HardwareProbe(const HardwareProbe&) = delete;
    HardwareProbe& operator=(const HardwareProbe&) = delete;
    HardwareProbe(HardwareProbe&& other) noexcept;
    HardwareProbe& operator=(HardwareProbe&& other) noexcept;

    [[gnu::noinline, gnu::cold]] void refresh_device_paths();
    [[nodiscard]] HardwareSample capture_sample() const;
    [[nodiscard]] HardwareSample capture_sample_desktop() const;

    // C++23 Zero-Cost Environment-Specialized Sample Capture (REF-REQ-026, REF-ARCH-016)
    template <core::PlatformPolicyConcept PlatPolicy>
    [[nodiscard]] HardwareSample capture_sample_policy() const {
        if constexpr (!PlatPolicy::has_battery()) {
            return capture_sample_desktop();
        } else {
            return capture_sample();
        }
    }

    [[nodiscard]] bool has_battery() const noexcept { return battery_power_fd_ >= 0 || battery_voltage_fd_ >= 0; }
    [[nodiscard]] bool has_rapl() const noexcept { return rapl_pkg_fd_ >= 0; }
    [[nodiscard]] bool has_gpu() const noexcept { return gpu_power_fd_ >= 0 || gpu_busy_fd_ >= 0; }
    [[nodiscard]] bool has_backlight() const noexcept { return backlight_cur_fd_ >= 0; }
    [[nodiscard]] bool has_fan() const noexcept { return fan_rpm_fd_ >= 0; }

    // Direct pread helper exposed for testing (REF-TEST-004)
    static std::optional<uint64_t> read_uint64_fd(int fd);
    static std::optional<int64_t> read_int64_fd(int fd);
    static std::optional<uint32_t> read_uint32_fd(int fd);
    static std::optional<int32_t> read_int32_fd(int fd);
    static std::string read_string_fd(int fd);
    static bool read_string_buf(int fd, char* buf, size_t max_len);

    // Fast single-read uevent battery parser exposed for testing (REF-RES-007, REF-REQ-022, REF-TEST-008)
    static void parse_battery_uevent_buf(std::string_view content, HardwareSample& sample) noexcept;

    // Direct PCIe Capability 0x10 binary decoder (REF-REQ-015, REF-TEST-006)
    static std::pair<uint8_t, uint8_t> decode_pcie_link_status(const uint8_t* config_data, size_t size) noexcept;
    static std::pair<uint8_t, uint8_t> read_pcie_binary_link_status(int config_fd) noexcept;

    // Direct PMU counter query (REF-REQ-015, REF-TEST-005)
    [[nodiscard]] bool has_pmu_counters() const noexcept { return pmu_instructions_fd_ >= 0; }

private:
    std::filesystem::path sysfs_root_;

    // 1. Power Supply & Battery paths (REF-REQ-022)
    std::filesystem::path battery_path_;
    std::filesystem::path battery_uevent_path_;
    std::filesystem::path battery_threshold_start_path_;
    std::filesystem::path battery_threshold_end_path_;
    std::filesystem::path battery_behaviour_path_;
    std::filesystem::path ac_path_;
    std::filesystem::path usbc_pd_path_;
    std::filesystem::path usbc_type_path_;
    std::filesystem::path usbc_voltage_max_path_;
    std::filesystem::path usbc_current_max_path_;

    // 2. RAPL & CPU paths
    std::filesystem::path rapl_pkg_path_;
    std::filesystem::path rapl_core_path_;
    std::filesystem::path rapl_dram_path_;
    uint64_t rapl_pkg_max_range_uj_{0}; // from max_energy_range_uj (0 = unknown)
    std::filesystem::path cpu_temp_path_;
    std::filesystem::path cpu_governor_path_;

    // 3. GPU paths
    std::filesystem::path gpu_power_path_;
    std::filesystem::path gpu_temp_path_;
    std::filesystem::path gpu_freq_path_;
    std::filesystem::path gpu_in0_path_;
    std::filesystem::path gpu_in1_path_;
    std::filesystem::path gpu_busy_path_;
    std::filesystem::path gpu_vram_used_path_;
    std::filesystem::path gpu_vram_total_path_;
    std::filesystem::path gpu_link_speed_path_;
    std::filesystem::path gpu_link_width_path_;
    std::filesystem::path drm_device_path_;
    bool gpu_is_apu_ppt_{false};
    core::FixedString<16> gpu_power_label_{};

    // 4. Storage / NVMe paths
    std::filesystem::path nvme_status_path_;
    std::filesystem::path nvme_temp1_path_;
    std::filesystem::path nvme_temp2_path_;
    std::filesystem::path block_stat_path_;

    // 5. Chassis / Fan paths
    std::filesystem::path fan_rpm_path_;
    std::filesystem::path fan_pwm_path_;
    std::filesystem::path chassis_temp_path_;
    std::filesystem::path kbdlight_path_;
    std::filesystem::path bluetooth_path_;

    // 6. Display path
    std::filesystem::path backlight_path_;

    // 7. Wireless & ASPM paths
    std::filesystem::path wifi_status_path_;
    std::filesystem::path wifi_temp_path_;
    std::filesystem::path aspm_policy_path_;

    // Persistent file descriptors for direct kernel access without pathname traversal
    // 1. Battery & Power Rail
    int battery_uevent_fd_{-1}; // REF-RES-007: Single-read battery telemetry
    int battery_power_fd_{-1};
    int battery_status_fd_{-1};
    int battery_voltage_fd_{-1};
    int battery_current_fd_{-1};
    int battery_energy_now_fd_{-1};
    int battery_energy_full_fd_{-1};
    int battery_energy_full_design_fd_{-1};
    int battery_cycle_fd_{-1};
    int battery_capacity_fd_{-1};
    int ac_online_fd_{-1};

    // ThinkPad Battery Charge Thresholds & Behaviour (REF-REQ-022)
    int battery_threshold_start_fd_{-1};
    int battery_threshold_end_fd_{-1};
    int battery_behaviour_fd_{-1};

    // USB-PD Input & Negotiation
    int usbc_voltage_fd_{-1};
    int usbc_current_fd_{-1};
    int usbc_online_fd_{-1};
    int usbc_type_fd_{-1};
    int usbc_voltage_max_fd_{-1};
    int usbc_current_max_fd_{-1};

    // Peripheral Batteries (Bluetooth/HID/Stylus)
    struct PeripheralProbe {
        core::FixedString<32> name{};
        std::filesystem::path capacity_path{};
        std::filesystem::path status_path{};
        int capacity_fd{-1};
        int status_fd{-1};
    };
    std::array<PeripheralProbe, 4> peripheral_probes_{};
    size_t peripheral_probe_count_{0};

    // 2. RAPL & CPU
    int rapl_pkg_fd_{-1};
    int rapl_core_fd_{-1};
    int rapl_dram_fd_{-1};
    int cpu_temp_fd_{-1};
    int cpu_governor_fd_{-1};
    std::vector<int> cpu_freq_fds_;
    std::vector<std::array<int, 4>> cpu_cstate_fds_;

    // 3. GPU
    int gpu_power_fd_{-1};
    int gpu_temp_fd_{-1};
    int gpu_freq_fd_{-1};
    int gpu_in0_fd_{-1};
    int gpu_in1_fd_{-1};
    int gpu_busy_fd_{-1};
    int gpu_vram_used_fd_{-1};
    int gpu_vram_total_fd_{-1};
    int gpu_link_speed_fd_{-1};
    int gpu_link_width_fd_{-1};

    // 4. Storage
    int nvme_status_fd_{-1};
    int nvme_temp1_fd_{-1};
    int nvme_temp2_fd_{-1};
    int block_stat_fd_{-1};

    // 5. Chassis & Thermal
    int fan_rpm_fd_{-1};
    int fan_pwm_fd_{-1};
    int chassis_temp_fd_{-1};
    int kbdlight_fd_{-1};
    int bluetooth_fd_{-1};

    // 6. Display
    int backlight_cur_fd_{-1};
    int backlight_max_fd_{-1};

    // 7. Wireless
    int wifi_status_fd_{-1};
    int wifi_temp_fd_{-1};
    int aspm_policy_fd_{-1};

    // 8. Syscall-Level Direct Hardware Telemetry (REF-REQ-015, REF-REQ-024)
    int pmu_instructions_fd_{-1};
    int pmu_cycles_fd_{-1};
    int pmu_llc_misses_fd_{-1};
    int pmu_branch_misses_fd_{-1};
    int pcie_gpu_config_fd_{-1};
    int pcie_nvme_config_fd_{-1};
    int cpu0_msr_fd_{-1};

    // Sub-sampling caches to eliminate ACPI EC & NVMe wake latency
    mutable uint64_t sample_counter_{0};
    mutable bool cached_ac_online_{false};
    // Set when a fresh AC read differs from the previous value. The threshold
    // block below needs the transition, but sample.is_ac_online has by then been
    // overwritten with the cached value, so the swap must be captured here.
    mutable bool ac_transition_{false};
    mutable bool cached_kbdlight_initialized_{false};
    mutable uint32_t cached_kbdlight_level_{0};
    mutable bool cached_bluetooth_enabled_{false};
    mutable std::optional<uint32_t> cached_fan_rpm_{std::nullopt};
    mutable std::optional<uint32_t> cached_fan_pwm_{std::nullopt};
    mutable std::optional<int32_t> cached_chassis_temp_{std::nullopt};
    mutable std::optional<int32_t> cached_wifi_temp_{std::nullopt};
    mutable std::array<char, 32> cached_aspm_policy_{};
    mutable bool cached_aspm_policy_initialized_{false};
    mutable std::optional<uint64_t> cached_bat_power_uw_{std::nullopt};
    mutable std::optional<int64_t> cached_bat_current_ua_{std::nullopt};
    mutable std::optional<uint32_t> cached_bat_capacity_percent_{std::nullopt};
    mutable bool cached_is_discharging_{false};
    mutable std::optional<uint64_t> cached_bat_voltage_{std::nullopt};
    mutable std::optional<uint64_t> cached_bat_energy_now_{std::nullopt};
    mutable std::optional<int32_t> cached_nvme_temp1_{std::nullopt};
    mutable std::optional<int32_t> cached_nvme_temp2_{std::nullopt};
    mutable bool cached_battery_static_initialized_{false};
    mutable std::optional<uint64_t> cached_energy_full_{std::nullopt};
    mutable std::optional<uint64_t> cached_energy_full_design_{std::nullopt};
    mutable std::optional<uint32_t> cached_cycle_count_{std::nullopt};
    mutable std::optional<uint64_t> cached_voltage_min_design_{std::nullopt};
    mutable core::FixedString<16> cached_bat_capacity_level_{};
    mutable core::FixedString<16> cached_bat_technology_{};
    mutable core::FixedString<32> cached_bat_model_name_{};
    mutable core::FixedString<24> cached_bat_manufacturer_{};
    mutable core::FixedString<24> cached_bat_serial_number_{};
    mutable std::optional<uint32_t> cached_bat_charge_start_threshold_{std::nullopt};
    mutable std::optional<uint32_t> cached_bat_charge_end_threshold_{std::nullopt};
    mutable core::FixedString<32> cached_bat_charge_behaviour_{};
    mutable core::FixedString<32> cached_usbc_pd_type_{};
    mutable std::optional<uint64_t> cached_usbc_pd_voltage_max_{std::nullopt};
    mutable std::optional<uint64_t> cached_usbc_pd_current_max_{std::nullopt};
    mutable core::FixedVector<HardwareSample::PeripheralBattery, 4> cached_peripheral_batteries_{};

    void capture_subsystems(HardwareSample& sample) const;
    [[gnu::noinline, gnu::cold]] void init_pmu_counters();
    [[gnu::noinline, gnu::cold]] void init_pcie_binary_configs();
    [[gnu::noinline, gnu::cold]] void init_msr_telemetry();
    [[gnu::noinline, gnu::cold]] void open_persistent_fds();
    void close_fds() noexcept;
};

} // namespace wattcurb::hw
