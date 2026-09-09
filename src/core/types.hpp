#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace wattcurb {

// Implements REF-REQ-001 & REF-ARCH-002
struct HardwareSample {
    std::chrono::steady_clock::time_point timestamp{};
    std::optional<uint64_t> rapl_package_uj;
    std::optional<uint64_t> rapl_core_uj;
    std::optional<uint64_t> rapl_dram_uj;
    std::optional<uint64_t> battery_power_uw;
    std::optional<uint64_t> battery_voltage_uv;
    std::optional<int64_t> battery_current_ua;
    bool is_discharging{false};
    std::optional<uint64_t> gpu_power_uw;
    std::optional<uint32_t> backlight_brightness;
    std::optional<uint32_t> backlight_max_brightness;
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

// Implements REF-REQ-001 & REF-RES-002
struct HardwarePowerBreakdown {
    double total_system_watts{0.0};
    double cpu_package_watts{0.0};
    double gpu_watts{0.0};
    double display_watts{0.0};
    double uncore_and_platform_watts{0.0};
    bool is_battery_discharging{false};
    bool has_direct_rapl{false};
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
