#include "policy/attribution_engine.hpp"
#include "policy/process_classifier.hpp"
#include "core/scoped_profiler.hpp"

#include <algorithm>
#include <cmath>

namespace wattcurb::policy {

HardwarePowerBreakdown AttributionEngine::compute_hardware_power(
    const HardwareSample& hw1,
    const HardwareSample& hw2,
    double delta_sec
) const {
    WATTCURB_PROFILE_SCOPE("policy.hw_power_calc");
    HardwarePowerBreakdown hw;
    hw.is_battery_discharging = hw2.is_discharging;
    hw.is_ac_online = hw2.is_ac_online;

    // 1. Battery Gas Gauge, Chemistry, Health & Flow (REF-REQ-010 Sec 2.1, REF-REQ-022, REF-ARCH-012, REF-REQ-023)
    {
        WATTCURB_PROFILE_SCOPE("attr.battery_physics");
        {
            WATTCURB_PROFILE_SCOPE("attr.battery.system_watts");
            if (hw2.battery_power_uw.has_value()) {
                uint64_t p1 = hw1.battery_power_uw.value_or(*hw2.battery_power_uw);
                uint64_t p2 = *hw2.battery_power_uw;
                hw.total_system_watts = static_cast<double>(p1 + p2) / 2.0 / 1'000'000.0;
            } else {
                hw.total_system_watts = 0.0;
            }
        }

        {
            WATTCURB_PROFILE_SCOPE("attr.battery.wear_and_health");
            if (hw2.battery_energy_full_uwh.has_value() && hw2.battery_energy_full_design_uwh.has_value() &&
                *hw2.battery_energy_full_design_uwh > 0) {
                hw.battery_health_percent = (static_cast<double>(*hw2.battery_energy_full_uwh) * 100.0) /
                                            static_cast<double>(*hw2.battery_energy_full_design_uwh);
                hw.battery_degradation_percent = std::max(0.0, 100.0 - hw.battery_health_percent);
                hw.battery_lost_capacity_wh = (*hw2.battery_energy_full_design_uwh >= *hw2.battery_energy_full_uwh) ?
                    (static_cast<double>(*hw2.battery_energy_full_design_uwh - *hw2.battery_energy_full_uwh) / 1'000'000.0) : 0.0;
                hw.battery_energy_design_wh = static_cast<double>(*hw2.battery_energy_full_design_uwh) / 1'000'000.0;
                hw.battery_energy_full_wh = static_cast<double>(*hw2.battery_energy_full_uwh) / 1'000'000.0;
            } else {
                hw.battery_health_percent = 100.0;
                hw.battery_degradation_percent = 0.0;
                hw.battery_lost_capacity_wh = 0.0;
            }

            if (hw2.battery_energy_now_uwh.has_value()) {
                hw.battery_energy_now_wh = static_cast<double>(*hw2.battery_energy_now_uwh) / 1'000'000.0;
            }
            if (hw2.battery_voltage_uv.has_value()) {
                hw.battery_voltage_now_v = static_cast<double>(*hw2.battery_voltage_uv) / 1'000'000.0;
            }
            if (hw2.battery_voltage_min_design_uv.has_value()) {
                hw.battery_voltage_min_design_v = static_cast<double>(*hw2.battery_voltage_min_design_uv) / 1'000'000.0;
            }
            if (hw2.battery_current_ua.has_value()) {
                hw.battery_current_now_a = static_cast<double>(*hw2.battery_current_ua) / 1'000'000.0;
            }

            hw.battery_cycle_count = hw2.battery_cycle_count.value_or(0);
            hw.battery_capacity_percent = hw2.battery_capacity_percent.value_or(0);
            hw.battery_technology = hw2.battery_technology;
            hw.battery_capacity_level = hw2.battery_capacity_level;
            hw.battery_model_name = hw2.battery_model_name;
            hw.battery_manufacturer = hw2.battery_manufacturer;
            hw.battery_serial_number = hw2.battery_serial_number;
            hw.battery_charge_start_threshold = hw2.battery_charge_start_threshold;
            hw.battery_charge_end_threshold = hw2.battery_charge_end_threshold;
            hw.battery_charge_behaviour = hw2.battery_charge_behaviour;

            if (hw.battery_charge_end_threshold.has_value() && *hw.battery_charge_end_threshold <= 85) {
                hw.is_conservation_mode_active = true;
            }
        }

        // Time-to-Empty (Discharge) & Time-to-Full / Time-to-Threshold (Charge)
        {
            WATTCURB_PROFILE_SCOPE("attr.battery.runtime_projection");
            if (hw2.is_discharging) {
                if (hw.total_system_watts > 0.1 && hw.battery_energy_now_wh > 0.0) {
                    hw.battery_remaining_hours_to_empty = hw.battery_energy_now_wh / hw.total_system_watts;
                    hw.battery_remaining_hours = hw.battery_remaining_hours_to_empty;
                }
            } else {
                // Charging / AC Connected
                double charge_inflow_w = 0.0;
                if (hw2.battery_power_uw.has_value() && *hw2.battery_power_uw > 100'000) {
                    charge_inflow_w = static_cast<double>(*hw2.battery_power_uw) / 1'000'000.0;
                } else if (hw2.battery_current_ua.has_value() && *hw2.battery_current_ua != 0 && hw2.battery_voltage_uv.has_value()) {
                    int64_t cur = *hw2.battery_current_ua < 0 ? -*hw2.battery_current_ua : *hw2.battery_current_ua;
                    charge_inflow_w = (static_cast<double>(cur) * static_cast<double>(*hw2.battery_voltage_uv)) / 1e12;
                }

                if (charge_inflow_w > 0.5 && hw.battery_energy_full_wh > 0.0) {
                    double target_thresh_pct = hw.battery_charge_end_threshold.has_value() ?
                        static_cast<double>(*hw.battery_charge_end_threshold) : 100.0;
                    double target_thresh_wh = hw.battery_energy_full_wh * (target_thresh_pct / 100.0);
                    if (target_thresh_wh > hw.battery_energy_now_wh) {
                        hw.battery_remaining_hours_to_threshold = (target_thresh_wh - hw.battery_energy_now_wh) / charge_inflow_w;
                    }
                    if (hw.battery_energy_full_wh > hw.battery_energy_now_wh) {
                        hw.battery_remaining_hours_to_full = (hw.battery_energy_full_wh - hw.battery_energy_now_wh) / charge_inflow_w;
                    }
                }
            }
        }

        // AC Hardware Direct Pass-Through Detection
        {
            WATTCURB_PROFILE_SCOPE("attr.battery.passthrough_detect");
            if (hw.is_ac_online && !hw.is_battery_discharging) {
                uint32_t thresh = hw.battery_charge_end_threshold.value_or(100);
                if (hw.battery_capacity_percent >= thresh || hw.total_system_watts < 0.2) {
                    hw.is_ac_passthrough = true;
                }
            }
        }

        {
            WATTCURB_PROFILE_SCOPE("attr.battery.usbc_flow");
            if (hw2.usbc_pd_voltage_uv.has_value() && hw2.usbc_pd_current_ua.has_value()) {
                hw.usbc_input_watts = (static_cast<double>(*hw2.usbc_pd_voltage_uv) *
                                       static_cast<double>(*hw2.usbc_pd_current_ua)) / 1e12;
            }
            hw.usbc_online = hw2.usbc_pd_online;
            hw.usbc_pd_type = hw2.usbc_pd_type;
            hw.peripheral_batteries = hw2.peripheral_batteries;
        }
    }

    // 2. GPU Subsystem Telemetry & PPT (REF-REQ-010 Sec 2.3, REF-REQ-051, REF-ARCH-022)
    double raw_gpu_w = 0.0;
    if (hw2.gpu_power_uw.has_value()) {
        uint64_t g1 = hw1.gpu_power_uw.value_or(*hw2.gpu_power_uw);
        uint64_t g2 = *hw2.gpu_power_uw;
        raw_gpu_w = static_cast<double>(g1 + g2) / 2.0 / 1'000'000.0;
    }
    hw.gpu_busy_percent = hw2.gpu_busy_percent.value_or(0);

    if (hw2.gpu_is_apu_ppt) {
        // AMD APU Package Power Tracking (PPT) Disambiguation:
        // On AMD APUs, power1_input is the total socket power (CPU + iGPU + SoC).
        // Dynamic iGPU power scales with GPU engine activity; idle baseline leakage is ~0.05W.
        double busy_ratio = std::clamp(static_cast<double>(hw.gpu_busy_percent) / 100.0, 0.0, 1.0);
        double igpu_active_w = raw_gpu_w * busy_ratio;
        hw.gpu_watts = (hw.gpu_busy_percent > 0) ? std::min(raw_gpu_w * 0.70, igpu_active_w + 0.15) : 0.05;
    } else {
        // Standalone dGPU (Discrete Board Power)
        hw.gpu_watts = raw_gpu_w;
    }
    hw.gpu_freq_mhz = hw2.gpu_freq_hz.has_value() ? (static_cast<double>(*hw2.gpu_freq_hz) / 1'000'000.0) : 0.0;
    hw.gpu_temp_c = hw2.gpu_temp_mdeg.has_value() ? (static_cast<double>(*hw2.gpu_temp_mdeg) / 1000.0) : 0.0;
    hw.gpu_vddgfx_v = hw2.gpu_vddgfx_mv.has_value() ? (static_cast<double>(*hw2.gpu_vddgfx_mv) / 1000.0) : 0.0;
    hw.gpu_vddsoc_v = hw2.gpu_vddsoc_mv.has_value() ? (static_cast<double>(*hw2.gpu_vddsoc_mv) / 1000.0) : 0.0;
    hw.gpu_vram_used_mb = hw2.gpu_vram_used_bytes.has_value() ?
        (static_cast<double>(*hw2.gpu_vram_used_bytes) / (1024.0 * 1024.0)) : 0.0;
    hw.gpu_vram_total_mb = hw2.gpu_vram_total_bytes.has_value() ?
        (static_cast<double>(*hw2.gpu_vram_total_bytes) / (1024.0 * 1024.0)) : 0.0;
    if (hw2.gpu_pcie_link_speed[0] != '\0') {
        char link_buf[32];
        if (hw2.gpu_pcie_link_width.has_value()) {
            std::snprintf(link_buf, sizeof(link_buf), "%s x%u", hw2.gpu_pcie_link_speed.data(), *hw2.gpu_pcie_link_width);
        } else {
            std::snprintf(link_buf, sizeof(link_buf), "%s", hw2.gpu_pcie_link_speed.data());
        }
        hw.gpu_pcie_link = link_buf;
    }

    // 3. Display / Backlight Subsystem (REF-REQ-010 Sec 2.6)
    if (hw2.backlight_brightness.has_value() && hw2.backlight_max_brightness.has_value() &&
        *hw2.backlight_max_brightness > 0) {
        double ratio = static_cast<double>(*hw2.backlight_brightness) /
                       static_cast<double>(*hw2.backlight_max_brightness);
        hw.display_brightness_percent = ratio * 100.0;
        hw.display_watts = 0.8 + (3.5 * std::pow(ratio, 1.2)); // Baseline 0.8W, up to 4.3W
    }

    // 4. Mechanical Chassis & Cooling Fan (REF-REQ-010 Sec 2.5)
    hw.fan_rpm = hw2.fan_rpm.value_or(0);
    if (hw.fan_rpm > 500) {
        // Fan power scales with cube of RPM: P_fan ~= base + k * (RPM/4200)^3
        double rpm_ratio = static_cast<double>(hw.fan_rpm) / 4200.0;
        hw.fan_estimated_watts = 0.05 + 1.7 * (rpm_ratio * rpm_ratio * rpm_ratio);
    }
    hw.kbdlight_level = hw2.kbdlight_level.value_or(0);
    hw.bluetooth_enabled = hw2.bluetooth_enabled.value_or(false);

    // 5. Storage Subsystem (NVMe SSD) (REF-REQ-010 Sec 2.4)
    hw.nvme_status = hw2.nvme_active ? "active" : "suspended";
    hw.nvme_temp_c = hw2.nvme_temp_composite_mdeg.has_value() ?
        (static_cast<double>(*hw2.nvme_temp_composite_mdeg) / 1000.0) : 0.0;
    if (delta_sec > 0.0) {
        uint64_t d_read = (hw2.disk_read_sectors >= hw1.disk_read_sectors) ?
            (hw2.disk_read_sectors - hw1.disk_read_sectors) : 0;
        uint64_t d_write = (hw2.disk_write_sectors >= hw1.disk_write_sectors) ?
            (hw2.disk_write_sectors - hw1.disk_write_sectors) : 0;
        hw.disk_read_mb_per_sec = (static_cast<double>(d_read) * 512.0 / 1'048'576.0) / delta_sec;
        hw.disk_write_mb_per_sec = (static_cast<double>(d_write) * 512.0 / 1'048'576.0) / delta_sec;
        hw.storage_estimated_watts = (hw2.nvme_active ? 0.35 : 0.05) +
            ((hw.disk_read_mb_per_sec + hw.disk_write_mb_per_sec) * 0.012);
    }

    // 6. Wireless & Peripherals (REF-REQ-010 Sec 2.7)
    hw.wifi_status = hw2.wifi_active ? "active" : "suspended";
    hw.wifi_temp_c = hw2.wifi_temp_mdeg.has_value() ?
        (static_cast<double>(*hw2.wifi_temp_mdeg) / 1000.0) : 0.0;
    if (hw2.aspm_policy[0] != '\0') {
        hw.aspm_policy = hw2.aspm_policy.data();
    }

    // 7. CPU & Platform Subsystem (REF-REQ-010 Sec 2.2)
    hw.cpu_temp_c = hw2.cpu_temp_mdeg.has_value() ? (static_cast<double>(*hw2.cpu_temp_mdeg) / 1000.0) : 0.0;
    hw.cpu_freq_avg_mhz = static_cast<double>(hw2.cpu_freq_avg_khz) / 1000.0;
    hw.cpu_freq_min_mhz = static_cast<double>(hw2.cpu_freq_min_khz) / 1000.0;
    hw.cpu_freq_max_mhz = static_cast<double>(hw2.cpu_freq_max_khz) / 1000.0;
    hw.cpu_governor = hw2.cpu_governor[0] != '\0' ? hw2.cpu_governor.data() : "schedutil";

    // C-State Sleep Residencies
    uint32_t cores = hw2.cpu_cores_online > 0 ? hw2.cpu_cores_online : 1;
    double total_wall_us = delta_sec * 1'000'000.0 * static_cast<double>(cores);
    uint64_t d_poll = (hw2.cstate_time_us[0] >= hw1.cstate_time_us[0]) ? (hw2.cstate_time_us[0] - hw1.cstate_time_us[0]) : 0;
    uint64_t d_c1 = (hw2.cstate_time_us[1] >= hw1.cstate_time_us[1]) ? (hw2.cstate_time_us[1] - hw1.cstate_time_us[1]) : 0;
    uint64_t d_c2 = (hw2.cstate_time_us[2] >= hw1.cstate_time_us[2]) ? (hw2.cstate_time_us[2] - hw1.cstate_time_us[2]) : 0;
    uint64_t d_c3 = (hw2.cstate_time_us[3] >= hw1.cstate_time_us[3]) ? (hw2.cstate_time_us[3] - hw1.cstate_time_us[3]) : 0;
    double idle_sum_us = static_cast<double>(d_poll + d_c1 + d_c2 + d_c3);
    double c0_active_us = std::max(0.0, total_wall_us - idle_sum_us);

    if (total_wall_us > 0.0) {
        hw.cstate_c0_active_percent = std::clamp((c0_active_us / total_wall_us) * 100.0, 0.0, 100.0);
        hw.cstate_c1_percent = std::clamp((static_cast<double>(d_c1) / total_wall_us) * 100.0, 0.0, 100.0);
        hw.cstate_c2_percent = std::clamp((static_cast<double>(d_c2) / total_wall_us) * 100.0, 0.0, 100.0);
        hw.cstate_c3_deep_percent = std::clamp((static_cast<double>(d_c3) / total_wall_us) * 100.0, 0.0, 100.0);
    }

    // CPU Package Power
    if (hw1.rapl_package_uj.has_value() && hw2.rapl_package_uj.has_value() && delta_sec > 0.0) {
        hw.has_direct_rapl = true;
        uint64_t e1 = *hw1.rapl_package_uj;
        uint64_t e2 = *hw2.rapl_package_uj;
        uint64_t delta_uj;
        if (e2 >= e1) {
            delta_uj = e2 - e1;
        } else {
            // The counter went backwards: a wrap or a reset. Wrap with the real
            // max_energy_range_uj when the probe knows it; otherwise treat it as
            // a reset (delta 0) rather than inventing a ~2^32 uJ spike from a
            // hard-coded modulus that need not match this platform.
            const uint64_t range = hw2.rapl_package_max_range_uj;
            delta_uj = (range > e1) ? (e2 + (range - e1)) : 0;
        }
        hw.cpu_package_watts = (static_cast<double>(delta_uj) / 1'000'000.0) / delta_sec;
    } else {
        // Fallback unprivileged decomposition (REF-RES-002, REF-REQ-051)
        hw.has_direct_rapl = false;
        if (hw2.gpu_is_apu_ppt && raw_gpu_w > 0.5) {
            // APU Socket Decomposition: PPT Socket Power minus iGPU power leaves CPU Package & SoC!
            hw.cpu_package_watts = std::max(0.5, raw_gpu_w - hw.gpu_watts);
        } else if (hw.total_system_watts > 0.0) {
            double accounted_other = hw.gpu_watts + hw.display_watts + hw.fan_estimated_watts + hw.storage_estimated_watts + 1.2;
            hw.cpu_package_watts = std::max(0.5, hw.total_system_watts - accounted_other);
        } else {
            hw.cpu_package_watts = 3.5; // AC / unmetered baseline default
        }
    }

    // 8. Uncore & Motherboard / Platform Loss
    if (hw.total_system_watts > 0.0) {
        double accounted = hw.cpu_package_watts + hw.gpu_watts + hw.display_watts +
                           hw.fan_estimated_watts + hw.storage_estimated_watts;
        hw.uncore_and_platform_watts = std::max(0.0, hw.total_system_watts - accounted);
    } else {
        hw.uncore_and_platform_watts = 1.2;
    }

    // 9. Direct Syscall Hardware Telemetry (REF-REQ-015, REF-REQ-024)
    hw.pmu_instructions = hw2.pmu_instructions;
    hw.pmu_cycles = hw2.pmu_cycles;
    hw.pmu_ipc = hw2.pmu_ipc;
    hw.pmu_llc_misses = hw2.pmu_llc_misses;
    hw.pmu_branch_misses = hw2.pmu_branch_misses;
    hw.cpu_core_vid_mv = hw2.cpu_core_vid_mv;
    hw.pcie_link_speed_gen = hw2.pcie_link_speed_gen;
    hw.pcie_link_width_lanes = hw2.pcie_link_width_lanes;

    // PMU Micro-Energy & Power Proxy Models (REF-REQ-024)
    uint64_t d_inst = (hw2.pmu_instructions >= hw1.pmu_instructions && hw1.pmu_instructions > 0)
        ? (hw2.pmu_instructions - hw1.pmu_instructions) : hw2.pmu_instructions;
    uint64_t d_cyc = (hw2.pmu_cycles >= hw1.pmu_cycles && hw1.pmu_cycles > 0)
        ? (hw2.pmu_cycles - hw1.pmu_cycles) : hw2.pmu_cycles;
    uint64_t d_llc = (hw2.pmu_llc_misses >= hw1.pmu_llc_misses && hw1.pmu_llc_misses > 0)
        ? (hw2.pmu_llc_misses - hw1.pmu_llc_misses) : hw2.pmu_llc_misses;
    uint64_t d_bm = (hw2.pmu_branch_misses >= hw1.pmu_branch_misses && hw1.pmu_branch_misses > 0)
        ? (hw2.pmu_branch_misses - hw1.pmu_branch_misses) : hw2.pmu_branch_misses;

    double interval_ipc = (d_cyc > 0 && d_inst > 0) ? (static_cast<double>(d_inst) / static_cast<double>(d_cyc)) : hw2.pmu_ipc;

    double epi = (static_cast<double>(d_inst) * interval_ipc) +
                 (200.0 * static_cast<double>(d_llc)) +
                 (30.0 * static_cast<double>(d_bm));
    hw.pmu_energy_proxy_index = epi;

    double waste = (200.0 * static_cast<double>(d_llc)) + (30.0 * static_cast<double>(d_bm));
    hw.pmu_energy_waste_ratio = epi > 0.0 ? std::clamp((waste / epi) * 100.0, 0.0, 100.0) : 0.0;

    if (delta_sec > 0.0 && d_inst > 0) {
        // Physical silicon power estimation (REF-REQ-024):
        // Base idle ~500mW + dynamic core + DRAM bus + branch recovery tax
        double p_dyn_mw = (static_cast<double>(d_inst) * interval_ipc * 0.015) / (delta_sec * 1'000'000.0);
        double p_dram_mw = (static_cast<double>(d_llc) * 3.0) / (delta_sec * 1'000'000.0);
        double p_branch_mw = (static_cast<double>(d_bm) * 0.20) / (delta_sec * 1'000'000.0);
        hw.pmu_estimated_power_mw = 500.0 + p_dyn_mw + p_dram_mw + p_branch_mw;
    } else {
        hw.pmu_estimated_power_mw = 0.0;
    }

    return hw;
}

AnalysisReportData AttributionEngine::compute_attribution(
    const HardwareSample& hw1,
    const HardwareSample& hw2,
    std::span<const ProcessSample> proc1,
    std::span<const ProcessSample> proc2,
    size_t top_n
) const {
    WATTCURB_PROFILE_SCOPE("policy.attribution_all");
    AnalysisReportData report;

    auto dur_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(hw2.timestamp - hw1.timestamp).count();
    double delta_sec = static_cast<double>(dur_ns) / 1'000'000'000.0;
    if (delta_sec <= 0.001) delta_sec = 0.001; // Avoid division by zero
    report.sample_duration = std::chrono::duration_cast<std::chrono::milliseconds>(hw2.timestamp - hw1.timestamp);
    report.sample_count = 1;
    report.is_short_window = (delta_sec < 3.0);

    report.hardware = compute_hardware_power(hw1, hw2, delta_sec);
    double total_sys_power = report.hardware.total_system_watts > 0.0 ? report.hardware.total_system_watts :
        (report.hardware.cpu_package_watts + report.hardware.gpu_watts + report.hardware.display_watts +
         report.hardware.fan_estimated_watts + report.hardware.storage_estimated_watts + report.hardware.uncore_and_platform_watts);
    report.total_energy_joules = total_sys_power * delta_sec;
    report.total_monitored_processes = proc2.size();

    // REF-ARCH-005: Zero-allocation Two-Pointer matching on sorted PID streams
    // Eliminates std::unordered_map completely (0 heap allocations, 100% L1D sequential access)
    // REF-ARCH-005, REF-ARCH-007 & REF-RES-011: 64-byte Hot-Aligned Intermediate Process Structure
    // Keeps all fields accessed during WDI scoring within a single L1D cache line.
    struct alignas(64) IntermediateProc {
        // --- Line 0 (Hot Cacheline: Bytes 0..63) ---
        int32_t pid{0};
        uint32_t delta_cpu_ticks{0};
        uint64_t delta_wakeups{0};
        uint32_t rss_kib{0};
        uint32_t pss_kib{0};
        uint32_t delta_minflt{0};
        uint32_t delta_majflt{0};

        // Bit-packed metadata word (8 bytes = 64 bits)
        int64_t cpu_core : 10 {-1};
        int64_t prev_core : 10 {-1};
        uint64_t num_threads : 16 {1};
        int64_t nice : 6 {0};
        // priority is 0..139 (see ProcessSample); keep it unsigned.
        uint64_t priority : 8 {0};
        uint64_t open_sockets : 12 {0};
        uint64_t cross_ccx_migration : 1 {0};
        uint64_t reserved : 1 {0};

        uint64_t timerslack_ns{50000};
        uint64_t vram_kib{0};

        // --- Line 1 (Cold Cacheline: Bytes 64..127) ---
        ProcessComm comm{};
        uint32_t uid{0};
        uint32_t pad0{0};
        uint64_t delta_gpu_ns{0};
        uint64_t delta_io_bytes{0};
        uint64_t delta_io_syscalls{0};
    };
    static_assert(sizeof(IntermediateProc) == 128, "IntermediateProc must span exactly 2 cache lines (1 Hot + 1 Cold)");

    core::FixedVector<IntermediateProc, 2048> deltas;

    uint64_t total_delta_cpu = 0;
    uint64_t total_delta_gpu_ns = 0;
    uint64_t total_system_wakeups = 0;

    {
        WATTCURB_PROFILE_SCOPE("policy.two_pointer_delta");
        size_t idx1 = 0;
        size_t idx2 = 0;
        const size_t sz1 = proc1.size();
        const size_t sz2 = proc2.size();

        while (idx1 < sz1 && idx2 < sz2) {
            const auto& p1 = proc1[idx1];
            const auto& p2 = proc2[idx2];

            if (p1.pid < p2.pid) {
                ++idx1;
            } else if (p1.pid > p2.pid) {
                ++idx2;
            } else {
                // p1.pid == p2.pid (Match found in O(1) sequential L1D stream)
                uint64_t ticks1 = p1.utime_ticks + p1.stime_ticks;
                uint64_t ticks2 = p2.utime_ticks + p2.stime_ticks;
                uint64_t d_cpu = (ticks2 >= ticks1) ? (ticks2 - ticks1) : 0;

                uint64_t gpu_ns1 = p1.drm_engine_gfx_ns + p1.drm_engine_compute_ns + p1.drm_engine_dec_ns + p1.drm_engine_enc_ns;
                uint64_t gpu_ns2 = p2.drm_engine_gfx_ns + p2.drm_engine_compute_ns + p2.drm_engine_dec_ns + p2.drm_engine_enc_ns;
                uint64_t d_gpu = (gpu_ns2 >= gpu_ns1) ? (gpu_ns2 - gpu_ns1) : 0;

                uint64_t wake1 = p1.voluntary_ctxt_switches + p1.nonvoluntary_ctxt_switches;
                uint64_t wake2 = p2.voluntary_ctxt_switches + p2.nonvoluntary_ctxt_switches;
                uint64_t d_wake = (wake2 >= wake1) ? (wake2 - wake1) : 0;

                uint64_t io1 = p1.read_bytes + p1.write_bytes;
                uint64_t io2 = p2.read_bytes + p2.write_bytes;
                uint64_t d_io = (io2 >= io1) ? (io2 - io1) : 0;

                uint64_t d_syscalls = (p2.io_syscalls >= p1.io_syscalls) ? (p2.io_syscalls - p1.io_syscalls) : 0;
                uint64_t d_minflt = (p2.minflt >= p1.minflt) ? (p2.minflt - p1.minflt) : 0;
                uint64_t d_majflt = (p2.majflt >= p1.majflt) ? (p2.majflt - p1.majflt) : 0;

                bool cross_ccx = (p1.cpu_core >= 0 && p2.cpu_core >= 0 &&
                                  (p1.cpu_core / 8) != (p2.cpu_core / 8) && d_cpu > 0);

                total_delta_cpu += d_cpu;
                total_delta_gpu_ns += d_gpu;
                total_system_wakeups += d_wake;

                IntermediateProc ip;
                ip.pid = p2.pid;
                ip.delta_cpu_ticks = static_cast<uint32_t>(d_cpu);
                ip.delta_wakeups = d_wake;
                ip.rss_kib = p2.rss_kib > 0 ? p2.rss_kib : p1.rss_kib;
                ip.pss_kib = p2.pss_kib > 0 ? p2.pss_kib : p1.pss_kib;
                ip.delta_minflt = static_cast<uint32_t>(d_minflt);
                ip.delta_majflt = static_cast<uint32_t>(d_majflt);
                ip.cpu_core = p2.cpu_core;
                ip.prev_core = p1.cpu_core;
                ip.num_threads = p2.num_threads;
                ip.nice = p2.nice;
                ip.priority = p2.priority;
                ip.open_sockets = std::max(static_cast<uint32_t>(p1.open_sockets), static_cast<uint32_t>(p2.open_sockets));
                ip.cross_ccx_migration = cross_ccx ? 1 : 0;
                ip.timerslack_ns = p2.timerslack_ns;
                ip.vram_kib = p2.drm_vram_kib;
                ip.comm = p2.comm;
                ip.uid = p2.uid;
                ip.delta_gpu_ns = d_gpu;
                ip.delta_io_bytes = d_io;
                ip.delta_io_syscalls = d_syscalls;
                deltas.push_back(ip);

                ++idx1;
                ++idx2;
            }
        }
    }

    report.total_system_wakeups_per_sec = static_cast<uint64_t>(static_cast<double>(total_system_wakeups) / delta_sec);

    // 1. Initial pass: CPU, GPU, Storage, and Wakeup power
    double dyn_cpu_power = report.hardware.cpu_package_watts * 0.75;
    double static_cpu_power = report.hardware.cpu_package_watts * 0.25;
    double static_per_proc = deltas.empty() ? 0.0 : (static_cpu_power / static_cast<double>(deltas.size()));

    core::FixedVector<ProcessAttributedPower, 2048> attributed;

    double total_thermal_watts = 0.0;

    {
        WATTCURB_PROFILE_SCOPE("policy.attr_pass1");
        for (const auto& d : deltas) {
            ProcessAttributedPower pap;
            pap.pid = d.pid;
            pap.comm = d.comm;
            pap.uid = d.uid;
            pap.vram_kib = d.vram_kib;
            pap.wakeups_per_sec = static_cast<uint64_t>(static_cast<double>(d.delta_wakeups) / delta_sec);

            // Physical Telemetry fields (REF-REQ-013, REF-REQ-016)
            pap.cpu_core = d.cpu_core;
            pap.num_threads = d.num_threads;
            pap.nice = d.nice;
            pap.priority = d.priority;
            pap.cross_ccx_migration = d.cross_ccx_migration;
            pap.timerslack_ns = d.timerslack_ns;
            pap.pss_kib = d.pss_kib;
            pap.open_sockets = d.open_sockets;
            pap.minflt_per_sec = static_cast<uint64_t>(static_cast<double>(d.delta_minflt) / delta_sec);
            pap.majflt_per_sec = static_cast<uint64_t>(static_cast<double>(d.delta_majflt) / delta_sec);

            // CPU Watts
            if (total_delta_cpu > 0 && d.delta_cpu_ticks > 0) {
                double share = static_cast<double>(d.delta_cpu_ticks) / static_cast<double>(total_delta_cpu);
                pap.cpu_watts = static_per_proc + (dyn_cpu_power * share);
            } else {
                pap.cpu_watts = static_per_proc;
            }

            // GPU Watts (REF-REQ-010, REF-REQ-051, REF-ARCH-022)
            // Physical Duty-Cycle & Dynamic Workload Proportional Attribution:
            // Prevents Heisenbug where a process rendering 1 frame (2ms) is charged 100% of GPU power!
            if (total_delta_gpu_ns > 0 && d.delta_gpu_ns > 0) {
                uint64_t interval_ns = static_cast<uint64_t>(delta_sec * 1'000'000'000.0);
                if (interval_ns == 0) interval_ns = 1'000'000'000ULL;

                double g_share = static_cast<double>(d.delta_gpu_ns) / static_cast<double>(total_delta_gpu_ns);
                double duty_cycle = std::min(1.0, static_cast<double>(d.delta_gpu_ns) / static_cast<double>(interval_ns));

                // Dynamic GPU power across the interval
                double dyn_gpu_power = 0.0;
                if (report.hardware.gpu_busy_percent > 0) {
                    double busy_ratio = std::clamp(static_cast<double>(report.hardware.gpu_busy_percent) / 100.0, 0.0, 1.0);
                    dyn_gpu_power = report.hardware.gpu_watts * busy_ratio;
                } else {
                    // If hardware reports 0% busy or unmetered, dynamic power cannot exceed active time fraction
                    double active_ratio = std::min(1.0, static_cast<double>(total_delta_gpu_ns) / static_cast<double>(interval_ns));
                    dyn_gpu_power = report.hardware.gpu_watts * active_ratio;
                }

                // Process cannot be charged more than the GPU's active wattage scaled by its actual duty cycle!
                double max_duty_watts = report.hardware.gpu_watts * duty_cycle;
                double proportional_dyn_watts = dyn_gpu_power * g_share;

                pap.gpu_watts = std::min(proportional_dyn_watts, max_duty_watts);
            } else {
                pap.gpu_watts = 0.0;
            }

            // Storage / NVMe Watts & Major Fault penalty (REF-REQ-013)
            pap.disk_io_mb_per_sec = (static_cast<double>(d.delta_io_bytes) / 1'048'576.0) / delta_sec;
            double iops = static_cast<double>(d.delta_io_syscalls) / delta_sec;
            pap.io_watts = (pap.disk_io_mb_per_sec * 0.015) + (iops > 50.0 ? 0.05 : 0.0);
            if (pap.majflt_per_sec > 0) {
                pap.io_watts += std::min(0.35, static_cast<double>(pap.majflt_per_sec) * 0.03);
            }

            // Wakeup Tax (C-State Disruption Penalty & Timer Slack penalty)
            if (pap.wakeups_per_sec > 15) {
                pap.wakeup_tax_watts = std::min(1.5, static_cast<double>(pap.wakeups_per_sec) * 0.0012);
                if (pap.timerslack_ns < 50000) {
                    double penalty = 1.0 + static_cast<double>(50000 - pap.timerslack_ns) / 50000.0;
                    pap.wakeup_tax_watts = std::min(2.5, pap.wakeup_tax_watts * penalty);
                }
            }

            // WiFi Radio CAM Mode Attribution (REF-REQ-013)
            if (pap.open_sockets > 0 && pap.wakeups_per_sec > 10) {
                pap.wifi_attributed_watts = std::min(0.65, 0.08 + static_cast<double>(pap.open_sockets) * 0.02 +
                                                               static_cast<double>(pap.wakeups_per_sec) * 0.0002);
            }

            // Cross-CCX Migration & DRAM Memory Attribution (REF-REQ-013)
            if (pap.cross_ccx_migration) {
                pap.dram_attributed_watts += 0.15; // Infinity Fabric cache coherency transfer cost
            }
            if (pap.pss_kib > 100 * 1024) {
                pap.dram_attributed_watts += (static_cast<double>(pap.pss_kib) / (1024.0 * 1024.0)) * 0.05;
            }

            total_thermal_watts += (pap.cpu_watts + pap.gpu_watts);
            attributed.push_back(pap);
        }
    }

    // 2. Second pass: Thermal Fan Attribution & Hardware Mechanism Labeling
    {
        WATTCURB_PROFILE_SCOPE("policy.attr_fan_wdi_pass");
        for (size_t idx = 0; idx < attributed.size(); ++idx) {
        auto& pap = attributed[idx];
        const auto& d = deltas[idx];

        // Mechanical Fan Power Attribution (REF-REQ-011 Sec 2.3)
        if (report.hardware.fan_estimated_watts > 0.0 && total_thermal_watts > 0.0) {
            double thermal_share = (pap.cpu_watts + pap.gpu_watts) / total_thermal_watts;
            pap.fan_attributed_watts = report.hardware.fan_estimated_watts * thermal_share;
        }

        pap.total_attributed_watts = pap.cpu_watts + pap.gpu_watts + pap.io_watts +
                                     pap.wakeup_tax_watts + pap.fan_attributed_watts +
                                     pap.wifi_attributed_watts + pap.dram_attributed_watts;

        // WattCurb Drain Index (WDI) - Enhanced with Deep Metrics (REF-REQ-013)
        pap.wdi_score = (pap.cpu_watts * 8.0) + (pap.gpu_watts * 12.0) +
                        (static_cast<double>(pap.wakeups_per_sec) * 0.05) +
                        (pap.io_watts * 8.0) + (pap.fan_attributed_watts * 10.0) +
                        (pap.wifi_attributed_watts * 10.0) + (pap.dram_attributed_watts * 8.0) +
                        (pap.cross_ccx_migration ? 6.0 : 0.0) +
                        (pap.timerslack_ns < 50000 ? 5.0 : 0.0) +
                        (pap.majflt_per_sec > 0 ? 4.0 : 0.0);

        if (pap.wdi_score > 6.0 || pap.wakeups_per_sec > 500) {
            pap.is_runaway_candidate = true;
        }

        // Identify Primary Physical Hardware Domain & Mechanism (REF-REQ-011 & REF-REQ-013)
        if (pap.cross_ccx_migration && pap.dram_attributed_watts >= 0.15) {
            pap.primary_hw_domain = "AMD Zen CCX Migration";
            pap.hardware_mechanism = "Cross-CCX L3 Thrash (Core " + std::to_string(d.prev_core) +
                                     "->" + std::to_string(pap.cpu_core) + ", IF Power)";
        } else if (pap.timerslack_ns < 50000 && pap.wakeup_tax_watts >= 0.3) {
            pap.primary_hw_domain = "Kernel Timer Slack";
            pap.hardware_mechanism = "Aggressive Timer Slack (" + std::to_string(pap.timerslack_ns) + "ns, Breaks NO_HZ)";
        } else if (pap.wifi_attributed_watts >= 0.2) {
            pap.primary_hw_domain = "WiFi Radio CAM";
            pap.hardware_mechanism = "Active Network Sockets (" + std::to_string(pap.open_sockets) + " skt, CAM Mode)";
        } else if (pap.majflt_per_sec > 0 && pap.io_watts >= 0.05) {
            pap.primary_hw_domain = "NVMe Storage / MajFlt";
            pap.hardware_mechanism = "Major Page Faults (" + std::to_string(pap.majflt_per_sec) + " flt/s, NVMe Active)";
        } else if (pap.gpu_watts >= 0.5 && pap.gpu_watts >= pap.cpu_watts && pap.gpu_watts >= pap.wakeup_tax_watts) {
            pap.primary_hw_domain = "GPU Silicon";
            uint64_t interval_ns = static_cast<uint64_t>(delta_sec * 1'000'000'000.0);
            if (interval_ns == 0) interval_ns = 1'000'000'000ULL;
            int duty_pct = std::min(100, static_cast<int>((static_cast<double>(d.delta_gpu_ns) * 100.0) / static_cast<double>(interval_ns)));
            pap.hardware_mechanism = "AMDGPU GFX Engine (" + std::to_string(pap.vram_kib / 1024) + "MB VRAM, " +
                                     std::to_string(duty_pct) + "% Duty)";
        } else if (pap.wakeup_tax_watts >= 0.3 && pap.wakeup_tax_watts >= pap.cpu_watts) {
            pap.primary_hw_domain = "CPU C-State Wakeup";
            pap.hardware_mechanism = "C3 Sleep Breaker (" + std::to_string(pap.wakeups_per_sec) + " wakeups/s)";
        } else if (pap.cpu_watts >= 0.4) {
            pap.primary_hw_domain = "CPU Compute";
            int pct = (total_delta_cpu > 0) ? static_cast<int>((static_cast<double>(d.delta_cpu_ticks) * 100.0) / static_cast<double>(total_delta_cpu)) : 100;
            pap.hardware_mechanism = "Core Execution (" + std::to_string(d.delta_cpu_ticks) + " ticks, " +
                                     std::to_string(pct) + "% CPU)";
        } else if (pap.io_watts >= 0.05 || pap.disk_io_mb_per_sec >= 0.5) {
            pap.primary_hw_domain = "NVMe Storage";
            pap.hardware_mechanism = "NVMe Active (" + std::to_string(pap.disk_io_mb_per_sec).substr(0, 4) + " MB/s)";
        } else {
            pap.primary_hw_domain = "Platform/Idle";
            pap.hardware_mechanism = "Background Poll (" + std::to_string(pap.wakeups_per_sec) + " w/s)";
        }

        // Determine Process-Level CPU C-State Affinity & Residency (REF-REQ-090, REF-ARCH-067)
        if (pap.cpu_watts >= 0.25 || d.delta_cpu_ticks > 15) {
            pap.cstate_affinity = "C0"; // Active Core Execution
        } else if (pap.wakeups_per_sec >= 30 || pap.wakeup_tax_watts >= 0.15 || pap.timerslack_ns < 50000) {
            pap.cstate_affinity = "C1"; // Light Idle / Wakeup Storm
        } else if (pap.wakeups_per_sec >= 5 || pap.io_watts >= 0.05) {
            pap.cstate_affinity = "C2"; // Moderate Idle / I/O Wait
        } else {
            pap.cstate_affinity = "C3"; // Deep Sleep Retention
        }

        // Process Safety Classification & Recommended Action (REF-REQ-019, REF-RES-008, REF-REQ-049 & REF-REQ-051)
        auto classification = ProcessClassifierDB::classify(pap.comm.c_str());
        pap.safety_tier = static_cast<uint8_t>(classification.tier);
        pap.recommended_action = static_cast<uint8_t>(classification.default_action);

        // Immune processes (Tier 0 & Tier 1) can NEVER be flagged as runaway candidates
        if (classification.tier == ProcessSafetyTier::CriticalImmune ||
            classification.tier == ProcessSafetyTier::DesktopCore) {
            pap.is_runaway_candidate = false;
        }
    }
}

    // 3. Build Domain Culprits Registry (REF-REQ-011 Sec 3.2) - Zero-Allocation TopKHeap (REF-ARCH-006)
    // Uses stack-resident TopKHeap<const ProcessAttributedPower*, 5> to find top 5 in 1 pass O(N log K)
    // Completely eliminates dynamic vector allocations and sort overheads!
    auto select_top_culprits = [](const auto& procs, auto filter_pred, auto sort_pred) {
        core::TopKHeap<const ProcessAttributedPower*, 5, decltype(sort_pred)> heap;
        for (const auto& p : procs) {
            if (filter_pred(p)) {
                heap.push(&p);
            }
        }
        return heap.extract_sorted();
    };

    // Domain A: GPU Silicon
    if (report.hardware.gpu_watts > 0.05) {
        DomainCulprit gpu_culprit;
        gpu_culprit.domain_name = "GPU Silicon (AMDGPU / DRM)";
        gpu_culprit.domain_total_watts = report.hardware.gpu_watts;

        auto top_gpu = select_top_culprits(attributed,
            [](const auto& p) { return p.gpu_watts > 0.01; },
            [](const auto* a, const auto* b) { return a->gpu_watts > b->gpu_watts; });

        for (const auto* p : top_gpu) {
            double sh = (p->gpu_watts / report.hardware.gpu_watts) * 100.0;
            gpu_culprit.top_culprits.push_back(ProcessDomainShare{
                .pid = p->pid,
                .comm = p->comm,
                .watts = p->gpu_watts,
                .share_percent = sh,
                .detail = p->hardware_mechanism.view()
            });
        }
        if (!gpu_culprit.top_culprits.empty()) {
            report.domain_culprits.push_back(std::move(gpu_culprit));
        }
    }

    // Domain B: CPU C-State Sleep Breakers (Wakeup Tax)
    {
        DomainCulprit wake_culprit;
        wake_culprit.domain_name = "CPU C-State Sleep Breakers (Preventing C3 Deep Sleep)";
        double total_wake_tax = 0.0;
        for (const auto& p : attributed) total_wake_tax += p.wakeup_tax_watts;
        wake_culprit.domain_total_watts = total_wake_tax;

        auto top_wake = select_top_culprits(attributed,
            [](const auto& p) { return p.wakeups_per_sec > 25; },
            [](const auto* a, const auto* b) { return a->wakeups_per_sec > b->wakeups_per_sec; });

        for (const auto* p : top_wake) {
            double sh = (report.total_system_wakeups_per_sec > 0) ?
                (static_cast<double>(p->wakeups_per_sec) * 100.0 / static_cast<double>(report.total_system_wakeups_per_sec)) : 0.0;
            char det_buf[80];
            std::snprintf(det_buf, sizeof(det_buf), "%lu wakeups/s (%.2fW Tax)",
                          static_cast<unsigned long>(p->wakeups_per_sec), p->wakeup_tax_watts);
            wake_culprit.top_culprits.push_back(ProcessDomainShare{
                .pid = p->pid,
                .comm = p->comm,
                .watts = p->wakeup_tax_watts,
                .share_percent = sh,
                .detail = det_buf
            });
        }
        if (!wake_culprit.top_culprits.empty()) {
            report.domain_culprits.push_back(std::move(wake_culprit));
        }
    }

    // Domain C: Cooling Fan Mechanical Power (Thermal Drivers)
    if (report.hardware.fan_estimated_watts > 0.1) {
        DomainCulprit fan_culprit;
        char fan_name[64];
        std::snprintf(fan_name, sizeof(fan_name), "Cooling Fan Mechanical Drain (%u RPM ThinkPad EC)", report.hardware.fan_rpm);
        fan_culprit.domain_name = fan_name;
        fan_culprit.domain_total_watts = report.hardware.fan_estimated_watts;

        auto top_fan = select_top_culprits(attributed,
            [](const auto& p) { return p.fan_attributed_watts > 0.01; },
            [](const auto* a, const auto* b) { return a->fan_attributed_watts > b->fan_attributed_watts; });

        for (const auto* p : top_fan) {
            double sh = (p->fan_attributed_watts / report.hardware.fan_estimated_watts) * 100.0;
            char det_buf[80];
            std::snprintf(det_buf, sizeof(det_buf), "Thermally induced by %.2fW silicon heat", p->cpu_watts + p->gpu_watts);
            fan_culprit.top_culprits.push_back(ProcessDomainShare{
                .pid = p->pid,
                .comm = p->comm,
                .watts = p->fan_attributed_watts,
                .share_percent = sh,
                .detail = det_buf
            });
        }
        if (!fan_culprit.top_culprits.empty()) {
            report.domain_culprits.push_back(std::move(fan_culprit));
        }
    }

    // Domain D: Storage / NVMe APST Disrupters
    if (report.hardware.storage_estimated_watts > 0.05) {
        DomainCulprit io_culprit;
        io_culprit.domain_name = "Storage / NVMe Subsystem (APST Disrupters)";
        io_culprit.domain_total_watts = report.hardware.storage_estimated_watts;

        auto top_io = select_top_culprits(attributed,
            [](const auto& p) { return p.io_watts > 0.005 || p.disk_io_mb_per_sec > 0.01; },
            [](const auto* a, const auto* b) { return a->io_watts > b->io_watts; });

        for (const auto* p : top_io) {
            char det_buf[80];
            std::snprintf(det_buf, sizeof(det_buf), "I/O: %.2f MB/s", p->disk_io_mb_per_sec);
            io_culprit.top_culprits.push_back(ProcessDomainShare{
                .pid = p->pid,
                .comm = p->comm,
                .watts = p->io_watts,
                .share_percent = 0.0,
                .detail = det_buf
            });
        }
        if (!io_culprit.top_culprits.empty()) {
            report.domain_culprits.push_back(std::move(io_culprit));
        }
    }

    // Domain E: Wireless Transceiver (WiFi 802.11 CAM Breakers - REF-REQ-013)
    {
        DomainCulprit wifi_culprit;
        wifi_culprit.domain_name = "WiFi Wireless Transceiver (Active Sockets in CAM Mode)";
        double total_wifi_watts = 0.0;
        for (const auto& p : attributed) {
            if (p.wifi_attributed_watts > 0.01 || p.open_sockets > 0) total_wifi_watts += p.wifi_attributed_watts;
        }
        wifi_culprit.domain_total_watts = total_wifi_watts;

        auto top_wifi = select_top_culprits(attributed,
            [](const auto& p) { return p.wifi_attributed_watts > 0.01 || p.open_sockets > 0; },
            [](const auto* a, const auto* b) { return a->wifi_attributed_watts > b->wifi_attributed_watts; });

        for (const auto* p : top_wifi) {
            double sh = (total_wifi_watts > 0.0) ? (p->wifi_attributed_watts / total_wifi_watts) * 100.0 : 0.0;
            char det_buf[80];
            std::snprintf(det_buf, sizeof(det_buf), "%u active sockets (%lu w/s)",
                          p->open_sockets, static_cast<unsigned long>(p->wakeups_per_sec));
            wifi_culprit.top_culprits.push_back(ProcessDomainShare{
                .pid = p->pid,
                .comm = p->comm,
                .watts = p->wifi_attributed_watts,
                .share_percent = sh,
                .detail = det_buf
            });
        }
        if (!wifi_culprit.top_culprits.empty()) {
            report.domain_culprits.push_back(std::move(wifi_culprit));
        }
    }

    // Domain F: AMD Zen CCX Migration (Cross-CCX Cache & Interconnect Thrashing - REF-REQ-013)
    {
        DomainCulprit ccx_culprit;
        ccx_culprit.domain_name = "AMD Zen CCX Core Migrations (Infinity Fabric & L3 Thrashing)";
        double total_ccx_watts = 0.0;
        for (const auto& p : attributed) {
            if (p.cross_ccx_migration) total_ccx_watts += 0.18;
        }
        ccx_culprit.domain_total_watts = total_ccx_watts;

        auto top_ccx = select_top_culprits(attributed,
            [](const auto& p) { return p.cross_ccx_migration; },
            [](const auto* a, const auto* b) { return a->total_attributed_watts > b->total_attributed_watts; });

        for (const auto* p : top_ccx) {
            char det_buf[80];
            std::snprintf(det_buf, sizeof(det_buf), "Cross-CCX migration to Core %d (%u threads)",
                          p->cpu_core, p->num_threads);
            ccx_culprit.top_culprits.push_back(ProcessDomainShare{
                .pid = p->pid,
                .comm = p->comm,
                .watts = 0.18,
                .share_percent = 0.0,
                .detail = det_buf
            });
        }
        if (!ccx_culprit.top_culprits.empty()) {
            report.domain_culprits.push_back(std::move(ccx_culprit));
        }
    }

    // Domain G: Memory & DRAM Subsystem (PSS Retention & Memory Bus Contention - REF-REQ-016)
    {
        DomainCulprit dram_culprit;
        dram_culprit.domain_name = "Memory & DRAM Subsystem (PSS Retention & Page Faults)";
        double total_dram_watts = 0.0;
        for (const auto& p : attributed) {
            if (p.dram_attributed_watts > 0.01 || p.pss_kib > 50 * 1024) total_dram_watts += p.dram_attributed_watts;
        }
        dram_culprit.domain_total_watts = total_dram_watts;

        auto top_dram = select_top_culprits(attributed,
            [](const auto& p) { return p.dram_attributed_watts > 0.01 || p.pss_kib > 50 * 1024; },
            [](const auto* a, const auto* b) { return a->dram_attributed_watts > b->dram_attributed_watts; });

        for (const auto* p : top_dram) {
            double sh = (total_dram_watts > 0.0) ? (p->dram_attributed_watts / total_dram_watts) * 100.0 : 0.0;
            char det_buf[80];
            if (p->majflt_per_sec > 0) {
                std::snprintf(det_buf, sizeof(det_buf), "%luMB PSS DRAM, %lu majflt/s",
                              static_cast<unsigned long>(p->pss_kib / 1024), static_cast<unsigned long>(p->majflt_per_sec));
            } else {
                std::snprintf(det_buf, sizeof(det_buf), "%luMB PSS DRAM",
                              static_cast<unsigned long>(p->pss_kib / 1024));
            }
            dram_culprit.top_culprits.push_back(ProcessDomainShare{
                .pid = p->pid,
                .comm = p->comm,
                .watts = p->dram_attributed_watts,
                .share_percent = sh,
                .detail = det_buf
            });
        }
        if (!dram_culprit.top_culprits.empty()) {
            report.domain_culprits.push_back(std::move(dram_culprit));
        }
    }

    // Sort descending by WDI score and populate FixedVector top_processes
    {
        WATTCURB_PROFILE_SCOPE("policy.wdi_ranking");
        std::sort(attributed.begin(), attributed.end(), [](const auto& a, const auto& b) {
            return a.wdi_score > b.wdi_score;
        });

        size_t limit = std::min({top_n, attributed.size(), report.top_processes.capacity()});
        report.top_processes.clear();
        for (size_t i = 0; i < limit; ++i) {
            report.top_processes.push_back(attributed[i]);
        }
    }

    return report;
}

template <typename HwContainer, typename ProcContainer>
static AnalysisReportData do_compute_windowed_attribution(
    const AttributionEngine& engine,
    const HwContainer& hw_samples,
    const ProcContainer& proc_samples,
    size_t top_n
) {
    WATTCURB_PROFILE_SCOPE("policy.windowed_accum");
    if (hw_samples.empty() || proc_samples.empty()) {
        return AnalysisReportData{};
    }
    if (hw_samples.size() == 1 || proc_samples.size() == 1) {
        std::span<const ProcessSample> p_span(proc_samples.front().data(), proc_samples.front().size());
        return engine.compute_attribution(hw_samples.front(), hw_samples.front(), p_span, p_span, top_n);
    }
    if (hw_samples.size() == 2 && proc_samples.size() == 2) {
        std::span<const ProcessSample> p_front(proc_samples.front().data(), proc_samples.front().size());
        std::span<const ProcessSample> p_back(proc_samples.back().data(), proc_samples.back().size());
        return engine.compute_attribution(hw_samples.front(), hw_samples.back(), p_front, p_back, top_n);
    }

    size_t num_intervals = std::min(hw_samples.size(), proc_samples.size()) - 1;

    // 1. Process intermediate delta accumulation using sorted Two-Pointer stream merge (REF-ARCH-005)
    // Completely eliminates std::unordered_map (0 heap node allocations, 100% L1D sequential access)
    core::FixedVector<ProcessSample, 2048> accumulated_procs;

    for (size_t step = 1; step <= num_intervals; ++step) {
        const auto& prev_procs = proc_samples[step - 1];
        const auto& cur_procs = proc_samples[step];

        core::FixedVector<ProcessSample, 2048> next_accum;

        size_t idx_prev = 0;
        size_t idx_cur = 0;
        size_t idx_acc = 0;
        const size_t sz_prev = prev_procs.size();
        const size_t sz_cur = cur_procs.size();
        const size_t sz_acc = accumulated_procs.size();

        while (idx_cur < sz_cur) {
            const auto& cur = cur_procs[idx_cur];

            // Match prev_procs (sorted by PID)
            while (idx_prev < sz_prev && prev_procs[idx_prev].pid < cur.pid) {
                ++idx_prev;
            }
            const ProcessSample* prev = (idx_prev < sz_prev && prev_procs[idx_prev].pid == cur.pid) ? &prev_procs[idx_prev] : nullptr;

            // Advance accumulated_procs to match cur.pid
            while (idx_acc < sz_acc && accumulated_procs[idx_acc].pid < cur.pid) {
                next_accum.push_back(accumulated_procs[idx_acc++]);
            }

            ProcessSample acc{};
            if (idx_acc < sz_acc && accumulated_procs[idx_acc].pid == cur.pid) {
                acc = accumulated_procs[idx_acc++];
            } else {
                acc.pid = cur.pid;
                acc.ppid = cur.ppid;
                acc.comm = cur.comm;
                acc.uid = cur.uid;
            }

            acc.cpu_core = cur.cpu_core;
            acc.num_threads = cur.num_threads;
            acc.nice = cur.nice;
            acc.priority = cur.priority;
            acc.timerslack_ns = std::min(acc.timerslack_ns, cur.timerslack_ns);
            acc.pss_kib = std::max(acc.pss_kib, cur.pss_kib);
            acc.rss_kib = std::max(acc.rss_kib, cur.rss_kib);
            acc.open_sockets = std::max(acc.open_sockets, cur.open_sockets);
            acc.drm_vram_kib = std::max(acc.drm_vram_kib, cur.drm_vram_kib);

            if (prev != nullptr) {
                if (cur.utime_ticks >= prev->utime_ticks) acc.utime_ticks += (cur.utime_ticks - prev->utime_ticks);
                if (cur.stime_ticks >= prev->stime_ticks) acc.stime_ticks += (cur.stime_ticks - prev->stime_ticks);
                if (cur.voluntary_ctxt_switches >= prev->voluntary_ctxt_switches)
                    acc.voluntary_ctxt_switches += (cur.voluntary_ctxt_switches - prev->voluntary_ctxt_switches);
                if (cur.nonvoluntary_ctxt_switches >= prev->nonvoluntary_ctxt_switches)
                    acc.nonvoluntary_ctxt_switches += (cur.nonvoluntary_ctxt_switches - prev->nonvoluntary_ctxt_switches);
                if (cur.read_bytes >= prev->read_bytes) acc.read_bytes += (cur.read_bytes - prev->read_bytes);
                if (cur.write_bytes >= prev->write_bytes) acc.write_bytes += (cur.write_bytes - prev->write_bytes);
                if (cur.io_syscalls >= prev->io_syscalls) acc.io_syscalls += (cur.io_syscalls - prev->io_syscalls);
                if (cur.minflt >= prev->minflt) acc.minflt += (cur.minflt - prev->minflt);
                if (cur.majflt >= prev->majflt) acc.majflt += (cur.majflt - prev->majflt);
                if (cur.drm_engine_gfx_ns >= prev->drm_engine_gfx_ns)
                    acc.drm_engine_gfx_ns += (cur.drm_engine_gfx_ns - prev->drm_engine_gfx_ns);
                if (cur.drm_engine_compute_ns >= prev->drm_engine_compute_ns)
                    acc.drm_engine_compute_ns += (cur.drm_engine_compute_ns - prev->drm_engine_compute_ns);
                if (cur.drm_engine_dec_ns >= prev->drm_engine_dec_ns)
                    acc.drm_engine_dec_ns += (cur.drm_engine_dec_ns - prev->drm_engine_dec_ns);
                if (cur.drm_engine_enc_ns >= prev->drm_engine_enc_ns)
                    acc.drm_engine_enc_ns += (cur.drm_engine_enc_ns - prev->drm_engine_enc_ns);
            } else {
                acc.utime_ticks += cur.utime_ticks;
                acc.stime_ticks += cur.stime_ticks;
                acc.voluntary_ctxt_switches += cur.voluntary_ctxt_switches;
                acc.nonvoluntary_ctxt_switches += cur.nonvoluntary_ctxt_switches;
                acc.read_bytes += cur.read_bytes;
                acc.write_bytes += cur.write_bytes;
                acc.io_syscalls += cur.io_syscalls;
                acc.minflt += cur.minflt;
                acc.majflt += cur.majflt;
                acc.drm_engine_gfx_ns += cur.drm_engine_gfx_ns;
                acc.drm_engine_compute_ns += cur.drm_engine_compute_ns;
                acc.drm_engine_dec_ns += cur.drm_engine_dec_ns;
                acc.drm_engine_enc_ns += cur.drm_engine_enc_ns;
            }

            next_accum.push_back(acc);
            ++idx_cur;
        }

        while (idx_acc < sz_acc) {
            next_accum.push_back(accumulated_procs[idx_acc++]);
        }

        accumulated_procs = std::move(next_accum);
    }

    // 2. Hardware telemetry averaging
    HardwareSample hw_start = hw_samples.front();
    HardwareSample hw_end = hw_samples.back();

    double sum_battery_power = 0.0; size_t count_bat = 0;
    double sum_gpu_power = 0.0; size_t count_gpu = 0;
    double sum_fan_rpm = 0.0; size_t count_fan = 0;
    double sum_cpu_temp = 0.0; size_t count_cpu_temp = 0;
    double sum_cpu_freq = 0.0; size_t count_cpu_freq = 0;
    double sum_gpu_busy = 0.0; size_t count_gpu_busy = 0;
    double sum_gpu_freq = 0.0; size_t count_gpu_freq = 0;
    double sum_gpu_temp = 0.0; size_t count_gpu_temp = 0;
    double sum_gpu_vram = 0.0; size_t count_gpu_vram = 0;
    double sum_nvme_temp = 0.0; size_t count_nvme_temp = 0;

    for (const auto& h : hw_samples) {
        if (h.battery_power_uw.has_value()) { sum_battery_power += static_cast<double>(*h.battery_power_uw); count_bat++; }
        if (h.gpu_power_uw.has_value()) { sum_gpu_power += static_cast<double>(*h.gpu_power_uw); count_gpu++; }
        if (h.fan_rpm.has_value()) { sum_fan_rpm += static_cast<double>(*h.fan_rpm); count_fan++; }
        if (h.cpu_temp_mdeg.has_value()) { sum_cpu_temp += static_cast<double>(*h.cpu_temp_mdeg); count_cpu_temp++; }
        if (h.cpu_freq_avg_khz > 0) { sum_cpu_freq += static_cast<double>(h.cpu_freq_avg_khz); count_cpu_freq++; }
        if (h.gpu_busy_percent.has_value()) { sum_gpu_busy += static_cast<double>(*h.gpu_busy_percent); count_gpu_busy++; }
        if (h.gpu_freq_hz.has_value()) { sum_gpu_freq += static_cast<double>(*h.gpu_freq_hz); count_gpu_freq++; }
        if (h.gpu_temp_mdeg.has_value()) { sum_gpu_temp += static_cast<double>(*h.gpu_temp_mdeg); count_gpu_temp++; }
        if (h.gpu_vram_used_bytes.has_value()) { sum_gpu_vram += static_cast<double>(*h.gpu_vram_used_bytes); count_gpu_vram++; }
        if (h.nvme_temp_composite_mdeg.has_value()) { sum_nvme_temp += static_cast<double>(*h.nvme_temp_composite_mdeg); count_nvme_temp++; }
    }

    if (count_bat > 0) hw_end.battery_power_uw = static_cast<uint64_t>(sum_battery_power / static_cast<double>(count_bat));
    if (count_gpu > 0) hw_end.gpu_power_uw = static_cast<uint64_t>(sum_gpu_power / static_cast<double>(count_gpu));
    if (count_fan > 0) hw_end.fan_rpm = static_cast<uint32_t>(sum_fan_rpm / static_cast<double>(count_fan));
    if (count_cpu_temp > 0) hw_end.cpu_temp_mdeg = static_cast<int32_t>(sum_cpu_temp / static_cast<double>(count_cpu_temp));
    if (count_cpu_freq > 0) hw_end.cpu_freq_avg_khz = static_cast<uint32_t>(sum_cpu_freq / static_cast<double>(count_cpu_freq));
    if (count_gpu_busy > 0) hw_end.gpu_busy_percent = static_cast<uint32_t>(sum_gpu_busy / static_cast<double>(count_gpu_busy));
    if (count_gpu_freq > 0) hw_end.gpu_freq_hz = static_cast<uint64_t>(sum_gpu_freq / static_cast<double>(count_gpu_freq));
    if (count_gpu_temp > 0) hw_end.gpu_temp_mdeg = static_cast<int32_t>(sum_gpu_temp / static_cast<double>(count_gpu_temp));
    if (count_gpu_vram > 0) hw_end.gpu_vram_used_bytes = static_cast<uint64_t>(sum_gpu_vram / static_cast<double>(count_gpu_vram));
    if (count_nvme_temp > 0) hw_end.nvme_temp_composite_mdeg = static_cast<int32_t>(sum_nvme_temp / static_cast<double>(count_nvme_temp));

    // 3. Construct synthetic zero-base proc1 and accumulated delta proc2
    core::FixedVector<ProcessSample, 2048> proc_zero;
    core::FixedVector<ProcessSample, 2048> proc_delta;

    for (const auto& acc : accumulated_procs) {
        ProcessSample z{};
        z.pid = acc.pid;
        z.ppid = acc.ppid;
        z.comm = acc.comm;
        z.uid = acc.uid;
        z.cpu_core = acc.cpu_core;
        z.num_threads = acc.num_threads;
        z.nice = acc.nice;
        z.priority = acc.priority;
        z.timerslack_ns = acc.timerslack_ns;
        z.pss_kib = acc.pss_kib;
        z.rss_kib = acc.rss_kib;
        z.open_sockets = acc.open_sockets;
        proc_zero.push_back(z);

        proc_delta.push_back(acc);
    }

    auto report = engine.compute_attribution(hw_start, hw_end, proc_zero.span(), proc_delta.span(), top_n);
    report.sample_count = num_intervals;
    auto dur_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(hw_end.timestamp - hw_start.timestamp).count();
    double total_sec = static_cast<double>(dur_ns) / 1'000'000'000.0;
    report.is_short_window = (total_sec < 3.0);
    double total_sys_power = report.hardware.total_system_watts > 0.0 ? report.hardware.total_system_watts :
        (report.hardware.cpu_package_watts + report.hardware.gpu_watts + report.hardware.display_watts +
         report.hardware.fan_estimated_watts + report.hardware.storage_estimated_watts + report.hardware.uncore_and_platform_watts);
    report.total_energy_joules = total_sys_power * total_sec;

    return report;
}

AnalysisReportData AttributionEngine::compute_windowed_attribution(
    const std::vector<HardwareSample>& hw_samples,
    const std::vector<std::vector<ProcessSample>>& proc_samples,
    size_t top_n
) const {
    return do_compute_windowed_attribution(*this, hw_samples, proc_samples, top_n);
}

AnalysisReportData AttributionEngine::compute_windowed_attribution(
    std::span<const HardwareSample> hw_samples,
    std::span<const ProcessSnapshot> proc_samples,
    size_t top_n
) const {
    return do_compute_windowed_attribution(*this, hw_samples, proc_samples, top_n);
}

} // namespace wattcurb::policy

