#include "policy/attribution_engine.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace wattcurb::policy {

HardwarePowerBreakdown AttributionEngine::compute_hardware_power(
    const HardwareSample& hw1,
    const HardwareSample& hw2,
    double delta_sec
) const {
    HardwarePowerBreakdown hw;
    hw.is_battery_discharging = hw2.is_discharging;
    hw.is_ac_online = hw2.is_ac_online;

    // 1. Battery Gas Gauge & Health (REF-REQ-010 Sec 2.1)
    if (hw2.battery_power_uw.has_value()) {
        uint64_t p1 = hw1.battery_power_uw.value_or(*hw2.battery_power_uw);
        uint64_t p2 = *hw2.battery_power_uw;
        hw.total_system_watts = static_cast<double>(p1 + p2) / 2.0 / 1'000'000.0;
    } else {
        hw.total_system_watts = 0.0;
    }

    if (hw2.battery_energy_full_uwh.has_value() && hw2.battery_energy_full_design_uwh.has_value() &&
        *hw2.battery_energy_full_design_uwh > 0) {
        hw.battery_health_percent = (static_cast<double>(*hw2.battery_energy_full_uwh) * 100.0) /
                                    static_cast<double>(*hw2.battery_energy_full_design_uwh);
    } else {
        hw.battery_health_percent = 100.0;
    }
    hw.battery_cycle_count = hw2.battery_cycle_count.value_or(0);
    hw.battery_capacity_percent = hw2.battery_capacity_percent.value_or(0);

    if (hw2.is_discharging && hw.total_system_watts > 0.1 && hw2.battery_energy_now_uwh.has_value()) {
        hw.battery_remaining_hours = (static_cast<double>(*hw2.battery_energy_now_uwh) / 1'000'000.0) /
                                     hw.total_system_watts;
    }

    if (hw2.usbc_pd_voltage_uv.has_value() && hw2.usbc_pd_current_ua.has_value()) {
        hw.usbc_input_watts = (static_cast<double>(*hw2.usbc_pd_voltage_uv) *
                               static_cast<double>(*hw2.usbc_pd_current_ua)) / 1e12;
        hw.usbc_online = hw2.usbc_pd_online;
    }

    // 2. GPU Subsystem Telemetry & PPT (REF-REQ-010 Sec 2.3)
    if (hw2.gpu_power_uw.has_value()) {
        uint64_t g1 = hw1.gpu_power_uw.value_or(*hw2.gpu_power_uw);
        uint64_t g2 = *hw2.gpu_power_uw;
        hw.gpu_watts = static_cast<double>(g1 + g2) / 2.0 / 1'000'000.0;
    }
    hw.gpu_busy_percent = hw2.gpu_busy_percent.value_or(0);
    hw.gpu_freq_mhz = hw2.gpu_freq_hz.has_value() ? (static_cast<double>(*hw2.gpu_freq_hz) / 1'000'000.0) : 0.0;
    hw.gpu_temp_c = hw2.gpu_temp_mdeg.has_value() ? (static_cast<double>(*hw2.gpu_temp_mdeg) / 1000.0) : 0.0;
    hw.gpu_vddgfx_v = hw2.gpu_vddgfx_mv.has_value() ? (static_cast<double>(*hw2.gpu_vddgfx_mv) / 1000.0) : 0.0;
    hw.gpu_vddsoc_v = hw2.gpu_vddsoc_mv.has_value() ? (static_cast<double>(*hw2.gpu_vddsoc_mv) / 1000.0) : 0.0;
    hw.gpu_vram_used_mb = hw2.gpu_vram_used_bytes.has_value() ?
        (static_cast<double>(*hw2.gpu_vram_used_bytes) / (1024.0 * 1024.0)) : 0.0;
    hw.gpu_vram_total_mb = hw2.gpu_vram_total_bytes.has_value() ?
        (static_cast<double>(*hw2.gpu_vram_total_bytes) / (1024.0 * 1024.0)) : 0.0;
    if (hw2.gpu_pcie_link_speed[0] != '\0') {
        hw.gpu_pcie_link = hw2.gpu_pcie_link_speed.data();
        if (hw2.gpu_pcie_link_width.has_value()) {
            hw.gpu_pcie_link += " x" + std::to_string(*hw2.gpu_pcie_link_width);
        }
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
        hw.fan_estimated_watts = 0.05 + 1.7 * std::pow(static_cast<double>(hw.fan_rpm) / 4200.0, 3.0);
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
        uint64_t delta_uj = (e2 >= e1) ? (e2 - e1) : (e2 + (0xFFFFFFFFULL - e1));
        hw.cpu_package_watts = (static_cast<double>(delta_uj) / 1'000'000.0) / delta_sec;
    } else {
        // Fallback unprivileged decomposition (REF-RES-002)
        hw.has_direct_rapl = false;
        if (hw.total_system_watts > 0.0) {
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

    return hw;
}

AnalysisReportData AttributionEngine::compute_attribution(
    const HardwareSample& hw1,
    const HardwareSample& hw2,
    const std::vector<ProcessSample>& proc1,
    const std::vector<ProcessSample>& proc2,
    size_t top_n
) const {
    AnalysisReportData report;

    auto dur_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(hw2.timestamp - hw1.timestamp).count();
    double delta_sec = static_cast<double>(dur_ns) / 1'000'000'000.0;
    if (delta_sec <= 0.001) delta_sec = 0.001; // Avoid division by zero
    report.sample_duration = std::chrono::duration_cast<std::chrono::milliseconds>(hw2.timestamp - hw1.timestamp);

    report.hardware = compute_hardware_power(hw1, hw2, delta_sec);
    report.total_monitored_processes = proc2.size();

    // Map proc1 by PID
    std::unordered_map<int32_t, const ProcessSample*> p1_map;
    p1_map.reserve(proc1.size());
    for (const auto& p : proc1) {
        p1_map[p.pid] = &p;
    }

    struct IntermediateProc {
        int32_t pid;
        std::string comm;
        uint32_t uid;
        uint64_t delta_cpu_ticks;
        uint64_t delta_gpu_ns;
        uint64_t delta_wakeups;
        uint64_t delta_io_bytes;
        uint64_t delta_io_syscalls;
        uint64_t vram_kib;
    };

    std::vector<IntermediateProc> deltas;
    deltas.reserve(proc2.size());

    uint64_t total_delta_cpu = 0;
    uint64_t total_delta_gpu_ns = 0;
    uint64_t total_system_wakeups = 0;

    for (const auto& p2 : proc2) {
        auto it = p1_map.find(p2.pid);
        if (it == p1_map.end()) continue; // Process just started during window
        const auto& p1 = *it->second;

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

        total_delta_cpu += d_cpu;
        total_delta_gpu_ns += d_gpu;
        total_system_wakeups += d_wake;

        deltas.push_back(IntermediateProc{
            .pid = p2.pid,
            .comm = p2.comm,
            .uid = p2.uid,
            .delta_cpu_ticks = d_cpu,
            .delta_gpu_ns = d_gpu,
            .delta_wakeups = d_wake,
            .delta_io_bytes = d_io,
            .delta_io_syscalls = d_syscalls,
            .vram_kib = p2.drm_vram_kib
        });
    }

    report.total_system_wakeups_per_sec = static_cast<uint64_t>(static_cast<double>(total_system_wakeups) / delta_sec);

    // 1. Initial pass: CPU, GPU, Storage, and Wakeup power
    double dyn_cpu_power = report.hardware.cpu_package_watts * 0.75;
    double static_cpu_power = report.hardware.cpu_package_watts * 0.25;
    double static_per_proc = deltas.empty() ? 0.0 : (static_cpu_power / static_cast<double>(deltas.size()));

    std::vector<ProcessAttributedPower> attributed;
    attributed.reserve(deltas.size());

    double total_thermal_watts = 0.0;

    for (const auto& d : deltas) {
        ProcessAttributedPower pap;
        pap.pid = d.pid;
        pap.comm = d.comm;
        pap.uid = d.uid;
        pap.vram_kib = d.vram_kib;
        pap.wakeups_per_sec = static_cast<uint64_t>(static_cast<double>(d.delta_wakeups) / delta_sec);

        // CPU Watts
        if (total_delta_cpu > 0 && d.delta_cpu_ticks > 0) {
            double share = static_cast<double>(d.delta_cpu_ticks) / static_cast<double>(total_delta_cpu);
            pap.cpu_watts = static_per_proc + (dyn_cpu_power * share);
        } else {
            pap.cpu_watts = static_per_proc;
        }

        // GPU Watts
        if (total_delta_gpu_ns > 0 && d.delta_gpu_ns > 0) {
            double g_share = static_cast<double>(d.delta_gpu_ns) / static_cast<double>(total_delta_gpu_ns);
            pap.gpu_watts = report.hardware.gpu_watts * g_share;
        } else {
            pap.gpu_watts = 0.0;
        }

        // Storage / NVMe Watts & throughput
        pap.disk_io_mb_per_sec = (static_cast<double>(d.delta_io_bytes) / 1'048'576.0) / delta_sec;
        double iops = static_cast<double>(d.delta_io_syscalls) / delta_sec;
        pap.io_watts = (pap.disk_io_mb_per_sec * 0.015) + (iops > 50.0 ? 0.05 : 0.0);

        // Wakeup Tax (C-State Disruption Penalty)
        if (pap.wakeups_per_sec > 15) {
            pap.wakeup_tax_watts = std::min(1.5, static_cast<double>(pap.wakeups_per_sec) * 0.0012);
        }

        total_thermal_watts += (pap.cpu_watts + pap.gpu_watts);
        attributed.push_back(pap);
    }

    // 2. Second pass: Thermal Fan Attribution & Hardware Mechanism Labeling
    for (size_t idx = 0; idx < attributed.size(); ++idx) {
        auto& pap = attributed[idx];
        const auto& d = deltas[idx];

        // Mechanical Fan Power Attribution (REF-REQ-011 Sec 2.3)
        if (report.hardware.fan_estimated_watts > 0.0 && total_thermal_watts > 0.0) {
            double thermal_share = (pap.cpu_watts + pap.gpu_watts) / total_thermal_watts;
            pap.fan_attributed_watts = report.hardware.fan_estimated_watts * thermal_share;
        }

        pap.total_attributed_watts = pap.cpu_watts + pap.gpu_watts + pap.io_watts +
                                     pap.wakeup_tax_watts + pap.fan_attributed_watts;

        // WattCurb Drain Index (WDI)
        pap.wdi_score = (pap.cpu_watts * 8.0) + (pap.gpu_watts * 12.0) +
                        (static_cast<double>(pap.wakeups_per_sec) * 0.05) +
                        (pap.io_watts * 8.0) + (pap.fan_attributed_watts * 10.0);

        if (pap.wdi_score > 6.0 || pap.wakeups_per_sec > 500) {
            pap.is_runaway_candidate = true;
        }

        // Identify Primary Physical Hardware Domain & Mechanism (REF-REQ-011)
        if (pap.gpu_watts >= 0.5 && pap.gpu_watts >= pap.cpu_watts && pap.gpu_watts >= pap.wakeup_tax_watts) {
            pap.primary_hw_domain = "GPU Silicon";
            int pct = (total_delta_gpu_ns > 0) ? static_cast<int>((static_cast<double>(d.delta_gpu_ns) * 100.0) / static_cast<double>(total_delta_gpu_ns)) : 100;
            pap.hardware_mechanism = "AMDGPU GFX Engine (" + std::to_string(pap.vram_kib / 1024) + "MB VRAM, " +
                                     std::to_string(pct) + "% GPU)";
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
    }

    // 3. Build Domain Culprits Registry (REF-REQ-011 Sec 3.2)
    // Domain A: GPU Silicon
    if (report.hardware.gpu_watts > 0.05) {
        DomainCulprit gpu_culprit;
        gpu_culprit.domain_name = "GPU Silicon (AMDGPU / DRM)";
        gpu_culprit.domain_total_watts = report.hardware.gpu_watts;

        std::vector<ProcessAttributedPower> gpu_procs;
        for (const auto& p : attributed) {
            if (p.gpu_watts > 0.01) gpu_procs.push_back(p);
        }
        std::sort(gpu_procs.begin(), gpu_procs.end(), [](const auto& a, const auto& b) {
            return a.gpu_watts > b.gpu_watts;
        });
        for (size_t i = 0; i < std::min<size_t>(5, gpu_procs.size()); ++i) {
            double sh = (gpu_procs[i].gpu_watts / report.hardware.gpu_watts) * 100.0;
            gpu_culprit.top_culprits.push_back(ProcessDomainShare{
                .pid = gpu_procs[i].pid,
                .comm = gpu_procs[i].comm,
                .watts = gpu_procs[i].gpu_watts,
                .share_percent = sh,
                .detail = gpu_procs[i].hardware_mechanism
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

        std::vector<ProcessAttributedPower> wake_procs;
        for (const auto& p : attributed) {
            if (p.wakeups_per_sec > 25) wake_procs.push_back(p);
        }
        std::sort(wake_procs.begin(), wake_procs.end(), [](const auto& a, const auto& b) {
            return a.wakeups_per_sec > b.wakeups_per_sec;
        });
        for (size_t i = 0; i < std::min<size_t>(5, wake_procs.size()); ++i) {
            double sh = (report.total_system_wakeups_per_sec > 0) ?
                (static_cast<double>(wake_procs[i].wakeups_per_sec) * 100.0 / static_cast<double>(report.total_system_wakeups_per_sec)) : 0.0;
            wake_culprit.top_culprits.push_back(ProcessDomainShare{
                .pid = wake_procs[i].pid,
                .comm = wake_procs[i].comm,
                .watts = wake_procs[i].wakeup_tax_watts,
                .share_percent = sh,
                .detail = std::to_string(wake_procs[i].wakeups_per_sec) + " wakeups/s (" +
                          std::to_string(wake_procs[i].wakeup_tax_watts).substr(0, 4) + "W Tax)"
            });
        }
        if (!wake_culprit.top_culprits.empty()) {
            report.domain_culprits.push_back(std::move(wake_culprit));
        }
    }

    // Domain C: Cooling Fan Mechanical Power (Thermal Drivers)
    if (report.hardware.fan_estimated_watts > 0.1) {
        DomainCulprit fan_culprit;
        fan_culprit.domain_name = "Cooling Fan Mechanical Drain (" + std::to_string(report.hardware.fan_rpm) + " RPM ThinkPad EC)";
        fan_culprit.domain_total_watts = report.hardware.fan_estimated_watts;

        std::vector<ProcessAttributedPower> fan_procs;
        for (const auto& p : attributed) {
            if (p.fan_attributed_watts > 0.01) fan_procs.push_back(p);
        }
        std::sort(fan_procs.begin(), fan_procs.end(), [](const auto& a, const auto& b) {
            return a.fan_attributed_watts > b.fan_attributed_watts;
        });
        for (size_t i = 0; i < std::min<size_t>(5, fan_procs.size()); ++i) {
            double sh = (fan_procs[i].fan_attributed_watts / report.hardware.fan_estimated_watts) * 100.0;
            fan_culprit.top_culprits.push_back(ProcessDomainShare{
                .pid = fan_procs[i].pid,
                .comm = fan_procs[i].comm,
                .watts = fan_procs[i].fan_attributed_watts,
                .share_percent = sh,
                .detail = "Thermally induced by " + std::to_string(fan_procs[i].cpu_watts + fan_procs[i].gpu_watts).substr(0, 4) + "W silicon heat"
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

        std::vector<ProcessAttributedPower> io_procs;
        for (const auto& p : attributed) {
            if (p.io_watts > 0.005 || p.disk_io_mb_per_sec > 0.01) io_procs.push_back(p);
        }
        std::sort(io_procs.begin(), io_procs.end(), [](const auto& a, const auto& b) {
            return a.io_watts > b.io_watts;
        });
        for (size_t i = 0; i < std::min<size_t>(5, io_procs.size()); ++i) {
            io_culprit.top_culprits.push_back(ProcessDomainShare{
                .pid = io_procs[i].pid,
                .comm = io_procs[i].comm,
                .watts = io_procs[i].io_watts,
                .share_percent = 0.0,
                .detail = "I/O: " + std::to_string(io_procs[i].disk_io_mb_per_sec).substr(0, 4) + " MB/s"
            });
        }
        if (!io_culprit.top_culprits.empty()) {
            report.domain_culprits.push_back(std::move(io_culprit));
        }
    }

    // Sort descending by WDI score
    std::sort(attributed.begin(), attributed.end(), [](const auto& a, const auto& b) {
        return a.wdi_score > b.wdi_score;
    });

    if (attributed.size() > top_n) {
        attributed.resize(top_n);
    }

    report.top_processes = std::move(attributed);
    return report;
}

} // namespace wattcurb::policy
