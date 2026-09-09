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

    // 1. Battery Total Power
    if (hw2.battery_power_uw.has_value()) {
        uint64_t p1 = hw1.battery_power_uw.value_or(*hw2.battery_power_uw);
        uint64_t p2 = *hw2.battery_power_uw;
        hw.total_system_watts = static_cast<double>(p1 + p2) / 2.0 / 1'000'000.0;
    } else {
        hw.total_system_watts = 0.0;
    }

    // 2. GPU Power
    if (hw2.gpu_power_uw.has_value()) {
        uint64_t g1 = hw1.gpu_power_uw.value_or(*hw2.gpu_power_uw);
        uint64_t g2 = *hw2.gpu_power_uw;
        hw.gpu_watts = static_cast<double>(g1 + g2) / 2.0 / 1'000'000.0;
    }

    // 3. Display / Backlight Power
    if (hw2.backlight_brightness.has_value() && hw2.backlight_max_brightness.has_value() &&
        *hw2.backlight_max_brightness > 0) {
        double ratio = static_cast<double>(*hw2.backlight_brightness) /
                       static_cast<double>(*hw2.backlight_max_brightness);
        hw.display_watts = 0.8 + (3.5 * std::pow(ratio, 1.2)); // Baseline 0.8W, up to 4.3W
    }

    // 4. CPU Package Power
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
            double estimated_cpu = hw.total_system_watts - hw.gpu_watts - hw.display_watts - 1.8;
            hw.cpu_package_watts = std::max(0.5, estimated_cpu);
        } else {
            hw.cpu_package_watts = 3.5; // AC / unmetered baseline default
        }
    }

    // 5. Uncore & Motherboard / Platform Loss
    if (hw.total_system_watts > 0.0) {
        double accounted = hw.cpu_package_watts + hw.gpu_watts + hw.display_watts;
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

        uint64_t gpu_ns1 = p1.drm_engine_gfx_ns + p1.drm_engine_compute_ns;
        uint64_t gpu_ns2 = p2.drm_engine_gfx_ns + p2.drm_engine_compute_ns;
        uint64_t d_gpu = (gpu_ns2 >= gpu_ns1) ? (gpu_ns2 - gpu_ns1) : 0;

        uint64_t wake1 = p1.voluntary_ctxt_switches + p1.nonvoluntary_ctxt_switches;
        uint64_t wake2 = p2.voluntary_ctxt_switches + p2.nonvoluntary_ctxt_switches;
        uint64_t d_wake = (wake2 >= wake1) ? (wake2 - wake1) : 0;

        uint64_t io1 = p1.read_bytes + p1.write_bytes;
        uint64_t io2 = p2.read_bytes + p2.write_bytes;
        uint64_t d_io = (io2 >= io1) ? (io2 - io1) : 0;

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
            .vram_kib = p2.drm_vram_kib
        });
    }

    report.total_system_wakeups_per_sec = static_cast<uint64_t>(static_cast<double>(total_system_wakeups) / delta_sec);

    // Attribution calculations
    double dyn_cpu_power = report.hardware.cpu_package_watts * 0.75;
    double static_cpu_power = report.hardware.cpu_package_watts * 0.25;
    double static_per_proc = deltas.empty() ? 0.0 : (static_cpu_power / static_cast<double>(deltas.size()));

    std::vector<ProcessAttributedPower> attributed;
    attributed.reserve(deltas.size());

    for (const auto& d : deltas) {
        ProcessAttributedPower pap;
        pap.pid = d.pid;
        pap.comm = d.comm;
        pap.uid = d.uid;
        pap.vram_kib = d.vram_kib;
        pap.wakeups_per_sec = static_cast<uint64_t>(static_cast<double>(d.delta_wakeups) / delta_sec);

        // 1. CPU Watts
        if (total_delta_cpu > 0 && d.delta_cpu_ticks > 0) {
            double share = static_cast<double>(d.delta_cpu_ticks) / static_cast<double>(total_delta_cpu);
            pap.cpu_watts = static_per_proc + (dyn_cpu_power * share);
        } else {
            pap.cpu_watts = static_per_proc;
        }

        // 2. GPU Watts
        if (total_delta_gpu_ns > 0 && d.delta_gpu_ns > 0) {
            double g_share = static_cast<double>(d.delta_gpu_ns) / static_cast<double>(total_delta_gpu_ns);
            pap.gpu_watts = report.hardware.gpu_watts * g_share;
        } else {
            pap.gpu_watts = 0.0;
        }

        // 3. I/O Watts (Estimated disk spin/flash active power)
        double io_mb_per_sec = (static_cast<double>(d.delta_io_bytes) / 1'048'576.0) / delta_sec;
        pap.io_watts = std::min(2.0, io_mb_per_sec * 0.02);

        // 4. Wakeup Tax (C-State Disruption Penalty)
        // High frequency context switches prevent CPU package from staying in C6/C8
        if (pap.wakeups_per_sec > 15) {
            pap.wakeup_tax_watts = std::min(1.5, static_cast<double>(pap.wakeups_per_sec) * 0.0012);
        }

        // Total
        pap.total_attributed_watts = pap.cpu_watts + pap.gpu_watts + pap.io_watts + pap.wakeup_tax_watts;

        // WattCurb Drain Index (WDI)
        pap.wdi_score = (pap.cpu_watts * 8.0) + (pap.gpu_watts * 12.0) +
                        (static_cast<double>(pap.wakeups_per_sec) * 0.05) + (pap.io_watts * 5.0);

        // Flag runaway candidates (e.g. background power hog or high wakeup spammer)
        if (pap.wdi_score > 6.0 || pap.wakeups_per_sec > 500) {
            pap.is_runaway_candidate = true;
        }

        attributed.push_back(pap);
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
