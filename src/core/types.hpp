#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace wattcurb {

// Implements REF-REQ-001, REF-REQ-010 & REF-ARCH-002
struct HardwareSample {
    std::chrono::steady_clock::time_point timestamp{};

    // 1. Power Supply & Battery Gas Gauge (REF-REQ-010 Sec 2.1)
    std::optional<uint64_t> battery_power_uw;
    std::optional<uint64_t> battery_voltage_uv;
    std::optional<int64_t> battery_current_ua;
    std::optional<uint64_t> battery_energy_now_uwh;
    std::optional<uint64_t> battery_energy_full_uwh;
    std::optional<uint64_t> battery_energy_full_design_uwh;
    std::optional<uint32_t> battery_cycle_count;
    std::optional<uint32_t> battery_capacity_percent;
    bool is_discharging{false};
    bool is_ac_online{false};

    // USB-C Power Delivery Input
    std::optional<uint64_t> usbc_pd_voltage_uv;
    std::optional<uint64_t> usbc_pd_current_ua;
    bool usbc_pd_online{false};

    // 2. CPU & Platform Subsystem (REF-REQ-010 Sec 2.2)
    std::optional<uint64_t> rapl_package_uj;
    std::optional<uint64_t> rapl_core_uj;
    std::optional<uint64_t> rapl_dram_uj;
    std::optional<int32_t> cpu_temp_mdeg; // k10temp / coretemp
    uint32_t cpu_freq_avg_khz{0};
    uint32_t cpu_freq_min_khz{0};
    uint32_t cpu_freq_max_khz{0};
    uint32_t cpu_cores_online{0};
    std::array<uint64_t, 4> cstate_time_us{}; // Aggregate POLL, C1, C2, C3
    std::array<char, 16> cpu_governor{};

    // 3. Graphics Processing Unit (AMDGPU / DRM) (REF-REQ-010 Sec 2.3)
    std::optional<uint64_t> gpu_power_uw; // Package Power Tracking (PPT)
    std::optional<uint32_t> gpu_busy_percent;
    std::optional<uint64_t> gpu_freq_hz;
    std::optional<int32_t> gpu_temp_mdeg;
    std::optional<uint32_t> gpu_vddgfx_mv;
    std::optional<uint32_t> gpu_vddsoc_mv;
    std::optional<uint64_t> gpu_vram_used_bytes;
    std::optional<uint64_t> gpu_vram_total_bytes;
    std::array<char, 32> gpu_pcie_link_speed{};
    std::optional<uint32_t> gpu_pcie_link_width;

    // 4. Storage Subsystem (NVMe SSD) (REF-REQ-010 Sec 2.4)
    bool nvme_active{false};
    std::optional<int32_t> nvme_temp_composite_mdeg;
    std::optional<int32_t> nvme_temp_sensor1_mdeg;
    uint64_t disk_read_sectors{0};
    uint64_t disk_write_sectors{0};
    uint64_t disk_io_ticks_ms{0};

    // 5. Thermal & Mechanical Chassis (ThinkPad EC) (REF-REQ-010 Sec 2.5)
    std::optional<uint32_t> fan_rpm;
    std::optional<uint32_t> fan_pwm;
    std::optional<int32_t> chassis_temp_mdeg;
    std::optional<uint32_t> kbdlight_level;
    std::optional<bool> bluetooth_enabled;

    // 6. Display Subsystem (REF-REQ-010 Sec 2.6)
    std::optional<uint32_t> backlight_brightness;
    std::optional<uint32_t> backlight_max_brightness;

    // 7. Bus & Wireless Peripherals (REF-REQ-010 Sec 2.7)
    bool wifi_active{false};
    std::optional<int32_t> wifi_temp_mdeg;
    std::array<char, 32> aspm_policy{};
};

// Implements REF-REQ-004 & REF-ARCH-002
struct ProcessSample {
    int32_t pid{0};
    int32_t ppid{0};
    std::string comm;
    uint32_t uid{0};
    uint64_t utime_ticks{0};
    uint64_t stime_ticks{0};
    uint64_t voluntary_ctxt_switches{0};
    uint64_t nonvoluntary_ctxt_switches{0};
    uint64_t read_bytes{0};
    uint64_t write_bytes{0};
    uint64_t drm_engine_gfx_ns{0};
    uint64_t drm_engine_compute_ns{0};
    uint64_t drm_vram_kib{0};
};

// Implements REF-REQ-001, REF-REQ-010 & REF-RES-002
struct HardwarePowerBreakdown {
    // Hardware Domain Power (Watts)
    double total_system_watts{0.0};
    double cpu_package_watts{0.0};
    double gpu_watts{0.0};
    double display_watts{0.0};
    double fan_estimated_watts{0.0};
    double storage_estimated_watts{0.0};
    double uncore_and_platform_watts{0.0};

    // Operational States
    bool is_battery_discharging{false};
    bool is_ac_online{false};
    bool has_direct_rapl{false};

    // Battery Health & Charging
    double battery_health_percent{0.0};
    double battery_remaining_hours{0.0};
    uint32_t battery_cycle_count{0};
    uint32_t battery_capacity_percent{0};
    double usbc_input_watts{0.0};
    bool usbc_online{false};

    // CPU & Platform Telemetry
    double cpu_temp_c{0.0};
    double cpu_freq_avg_mhz{0.0};
    double cpu_freq_min_mhz{0.0};
    double cpu_freq_max_mhz{0.0};
    std::string cpu_governor;
    double cstate_c0_active_percent{0.0};
    double cstate_c1_percent{0.0};
    double cstate_c2_percent{0.0};
    double cstate_c3_deep_percent{0.0};

    // GPU Telemetry
    uint32_t gpu_busy_percent{0};
    double gpu_freq_mhz{0.0};
    double gpu_temp_c{0.0};
    double gpu_vddgfx_v{0.0};
    double gpu_vddsoc_v{0.0};
    double gpu_vram_used_mb{0.0};
    double gpu_vram_total_mb{0.0};
    std::string gpu_pcie_link;

    // Storage Telemetry
    std::string nvme_status;
    double nvme_temp_c{0.0};
    double disk_read_mb_per_sec{0.0};
    double disk_write_mb_per_sec{0.0};

    // Chassis & Cooling
    uint32_t fan_rpm{0};
    uint32_t kbdlight_level{0};
    bool bluetooth_enabled{false};

    // Display & Wireless
    double display_brightness_percent{0.0};
    std::string wifi_status;
    double wifi_temp_c{0.0};
    std::string aspm_policy;
};

// Implements REF-REQ-004 & REF-RES-003
struct ProcessAttributedPower {
    int32_t pid{0};
    std::string comm;
    uint32_t uid{0};
    double cpu_watts{0.0};
    double gpu_watts{0.0};
    double io_watts{0.0};
    double wakeup_tax_watts{0.0};
    double total_attributed_watts{0.0};
    double wdi_score{0.0}; // WattCurb Drain Index
    uint64_t wakeups_per_sec{0};
    uint64_t vram_kib{0};
    bool is_runaway_candidate{false};
};

// Implements REF-REQ-005 & REF-ARCH-002
struct AnalysisReportData {
    std::chrono::milliseconds sample_duration{0};
    HardwarePowerBreakdown hardware;
    std::vector<ProcessAttributedPower> top_processes;
    size_t total_monitored_processes{0};
    uint64_t total_system_wakeups_per_sec{0};
};

} // namespace wattcurb
