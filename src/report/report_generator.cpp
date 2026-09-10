#include "report/report_generator.hpp"
#include "policy/process_classifier.hpp"
#include "policy/battery_feature.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>

namespace wattcurb::report {

namespace {

// ANSI Colors
constexpr const char* RESET = "\033[0m";
constexpr const char* BOLD = "\033[1m";
constexpr const char* CYAN = "\033[36m";
constexpr const char* GREEN = "\033[32m";
constexpr const char* YELLOW = "\033[33m";
constexpr const char* RED = "\033[31m";
constexpr const char* MAGENTA = "\033[35m";
constexpr const char* BLUE = "\033[34m";
constexpr const char* DIM = "\033[2m";

std::string format_bar(double percent, int width = 16) {
    if (percent < 0.0) percent = 0.0;
    if (percent > 100.0) percent = 100.0;
    int filled = static_cast<int>((percent / 100.0) * static_cast<double>(width));
    std::string bar = "[";
    for (int i = 0; i < width; ++i) {
        if (i < filled) bar += "=";
        else if (i == filled) bar += ">";
        else bar += " ";
    }
    bar += "]";
    return bar;
}

double get_effective_total_watts(const AnalysisReportData& r) {
    double hw_sum = r.hardware.cpu_package_watts + r.hardware.gpu_watts + r.hardware.display_watts +
                    r.hardware.fan_estimated_watts + r.hardware.storage_estimated_watts + r.hardware.uncore_and_platform_watts;
    if (r.hardware.is_battery_discharging && r.hardware.total_system_watts > 0.0) {
        return std::max(r.hardware.total_system_watts, hw_sum);
    }
    return hw_sum > 0.0 ? hw_sum : r.hardware.total_system_watts;
}

} // namespace

void ReportGenerator::render_executive_briefing(const AnalysisReportData& r, std::ostream& out) {
    double total_sys = get_effective_total_watts(r);

    out << "\n" << BOLD << CYAN;
    out << "================================================================================================================\n";
    out << "                             WattCurb High-Fidelity Executive & Physical Power Briefing                         \n";
    out << "================================================================================================================\n";
    out << RESET;

    // Header Timing & Scope
    out << DIM << " Observation Scope    : " << RESET << BOLD << r.sample_duration.count() << " ms" << RESET;
    if (r.sample_count > 1) {
        out << DIM << " (" << r.sample_count << " continuous intervals)" << RESET;
    }
    if (r.total_energy_joules > 0.0) {
        out << DIM << " | Energy Consumption: " << RESET << BOLD << std::fixed << std::setprecision(2) << r.total_energy_joules << " Joules" << RESET;
    }
    out << DIM << " | Monitored Processes: " << RESET << BOLD << r.total_monitored_processes << RESET;
    out << DIM << " | Wakeups: " << RESET << BOLD << r.total_system_wakeups_per_sec << " /sec" << RESET << "\n\n";

    // 1. System Battery & Power Overview
    out << BOLD << " [1] System Battery & Power Supply Deep Telemetry" << RESET << "\n";
    out << "  - Total System Drain      : " << BOLD << (total_sys > 25.0 ? RED : (total_sys > 12.0 ? YELLOW : GREEN))
        << std::fixed << std::setprecision(2) << total_sys << " Watts" << RESET << "\n";
    out << "  - Power Supply State      : "
        << (r.hardware.is_battery_discharging ? (RED + std::string("Discharging (On Battery)")) : (GREEN + std::string("AC Connected (Line Power / Charging)")))
        << RESET;
    if (r.hardware.usbc_online) {
        out << DIM << " [USB-PD: " << std::fixed << std::setprecision(1) << r.hardware.usbc_input_watts << "W]" << RESET;
    }
    out << "\n";

    if (r.hardware.is_battery_discharging || r.hardware.battery_capacity_percent > 0) {
        out << "  - Battery Capacity        : " << BOLD << r.hardware.battery_capacity_percent << "%" << RESET;
        if (r.hardware.battery_remaining_hours > 0.0) {
            out << " (" << BOLD << std::fixed << std::setprecision(1) << r.hardware.battery_remaining_hours << " hours remaining" << RESET << ")";
        }
        out << " | Health: " << std::fixed << std::setprecision(1) << r.hardware.battery_health_percent << "%"
            << " (" << r.hardware.battery_cycle_count << " cycles)\n";
    }

    // 2. Hardware Subsystem & Domain Power Breakdown
    out << "\n" << BOLD << " [2] Physical Hardware Domain Power & State Breakdown" << RESET << "\n";
    auto print_hw = [&](const char* domain, double watts, const std::string& details) {
        double pct = total_sys > 0.0 ? (watts / total_sys) * 100.0 : 0.0;
        out << "  * " << std::left << std::setw(22) << domain << ": "
            << std::right << std::setw(6) << std::fixed << std::setprecision(2) << watts << " W "
            << "(" << std::setw(5) << std::fixed << std::setprecision(1) << pct << "%) "
            << DIM << details << RESET << "\n";
    };

    // CPU Telemetry Line
    std::string cpu_detail = "Temp: " + std::to_string(static_cast<int>(r.hardware.cpu_temp_c)) + "°C, " +
                            std::to_string(static_cast<int>(r.hardware.cpu_freq_avg_mhz)) + " MHz avg (" +
                            r.hardware.cpu_governor + ")";
    print_hw("CPU Package (RAPL)", r.hardware.cpu_package_watts, cpu_detail);

    // C-State Sleep Residencies
    out << "    └─ " << DIM << "C-State Sleep Residency : " << RESET
        << "C0 (Active): " << BOLD << std::fixed << std::setprecision(1) << r.hardware.cstate_c0_active_percent << "%" << RESET << ", "
        << "C1: " << std::fixed << std::setprecision(1) << r.hardware.cstate_c1_percent << "%, "
        << "C2: " << std::fixed << std::setprecision(1) << r.hardware.cstate_c2_percent << "%, "
        << "C3 (Deep Sleep): " << BOLD << (r.hardware.cstate_c3_deep_percent > 70.0 ? GREEN : YELLOW)
        << std::fixed << std::setprecision(1) << r.hardware.cstate_c3_deep_percent << "%" << RESET << "\n";

    if (r.hardware.pmu_instructions > 0) {
        out << "    └─ " << DIM << "Direct PMU Telemetry    : " << RESET
            << "IPC: " << BOLD << std::fixed << std::setprecision(2) << r.hardware.pmu_ipc << RESET
            << " | Cycles: " << r.hardware.pmu_cycles
            << " | LLC Misses: " << r.hardware.pmu_llc_misses << "\n";
    }

    // GPU Telemetry Line
    std::string gpu_detail = "Busy: " + std::to_string(r.hardware.gpu_busy_percent) + "%, " +
                             std::to_string(static_cast<int>(r.hardware.gpu_vram_used_mb)) + " MB VRAM, " +
                             r.hardware.gpu_pcie_link;
    print_hw("GPU Silicon (DRM)", r.hardware.gpu_watts, gpu_detail);

    // Display
    std::string disp_detail = "Brightness: " + std::to_string(static_cast<int>(r.hardware.display_brightness_percent)) + "%";
    print_hw("Display Backlight", r.hardware.display_watts, disp_detail);

    // Storage
    std::string nvme_detail = "NVMe: " + r.hardware.nvme_status + " (Read " +
                              std::to_string(r.hardware.disk_read_mb_per_sec).substr(0, 4) + " MB/s, Write " +
                              std::to_string(r.hardware.disk_write_mb_per_sec).substr(0, 4) + " MB/s)";
    print_hw("Storage / NVMe APST", r.hardware.storage_estimated_watts, nvme_detail);

    // Cooling & Platform
    std::string fan_detail = std::to_string(r.hardware.fan_rpm) + " RPM";
    print_hw("Mechanical Fan", r.hardware.fan_estimated_watts, fan_detail);
    print_hw("Uncore & Platform Loss", r.hardware.uncore_and_platform_watts, "ASPM: " + r.hardware.aspm_policy);

    // 3. Top Culprit Processes & Root Causes
    out << "\n" << BOLD << " [3] Top Battery Drain Culprits & Physical Causation Breakdown" << RESET << "\n";
    size_t count = std::min(size_t{8}, r.top_processes.size());
    if (count == 0) {
        out << "  (No active power draining processes detected in observation window)\n";
    } else {
        for (size_t i = 0; i < count; ++i) {
            const auto& p = r.top_processes[i];
            auto tier = static_cast<policy::ProcessSafetyTier>(p.safety_tier);
            const char* tier_label = policy::ProcessClassifierDB::tier_name(tier);
            double pct_of_sys = total_sys > 0.0 ? (p.total_attributed_watts / total_sys) * 100.0 : 0.0;

            out << "  " << (i + 1) << ". " << BOLD << p.comm.c_str() << RESET
                << " (PID: " << p.pid << ", " << CYAN << tier_label << RESET
                << ", " << p.num_threads << " thr, Nice: " << p.nice << ")\n";
            out << "     Drain: " << BOLD << (p.total_attributed_watts > 1.5 ? RED : (p.total_attributed_watts > 0.5 ? YELLOW : GREEN))
                << std::fixed << std::setprecision(2) << p.total_attributed_watts << " W" << RESET
                << " (" << std::fixed << std::setprecision(1) << pct_of_sys << "% of system, WDI: "
                << std::fixed << std::setprecision(1) << p.wdi_score << ")"
                << " | Primary Domain: " << MAGENTA << p.primary_hw_domain.c_str() << RESET << "\n";
            out << "     Causation Mechanism: " << BOLD << p.hardware_mechanism.c_str() << RESET << "\n";
            out << "     Physical Metrics   : " << DIM
                << "Wakeups: " << p.wakeups_per_sec << "/s, "
                << "RAM PSS: " << (p.pss_kib / 1024) << "MB, "
                << "Faults: " << p.minflt_per_sec << " min/s " << p.majflt_per_sec << " maj/s, "
                << "Sockets: " << p.open_sockets
                << (p.cross_ccx_migration ? " [Cross-CCX Migration Active]" : "")
                << RESET << "\n";
        }
    }

    // 4. Modular Battery Optimization Features Status
    out << "\n" << BOLD << " [4] Modular Battery Optimization Features Execution Status" << RESET << "\n";
    const auto& ms = r.mitigation_status;
    out << "  - Overall Status      : " << BOLD << (ms.throttled_count > 0 || ms.frozen_count > 0 ? GREEN : DIM)
        << ms.active_summary.c_str() << RESET << "\n";

    if (ms.feature_summary_count > 0) {
        out << "  - Active Feature Details:\n";
        for (size_t k = 0; k < ms.feature_summary_count; ++k) {
            out << "    * " << GREEN << ms.feature_summaries[k].c_str() << RESET << "\n";
        }
    } else {
        out << "    * All optimization features running in passive baseline observation mode.\n";
    }

    // 5. Actionable Power Saving Recommendations
    out << "\n" << BOLD << " [5] Actionable Engineering Recommendations" << RESET << "\n";
    bool has_tip = false;
    if (r.hardware.display_brightness_percent > 50.0) {
        out << "  * [Display] Brightness is currently at " << static_cast<int>(r.hardware.display_brightness_percent)
            << "%. Lowering to 40% will save ~"
            << std::fixed << std::setprecision(2) << (r.hardware.display_watts * 0.35) << " Watts on this panel.\n";
        has_tip = true;
    }
    if (r.hardware.is_battery_discharging && r.hardware.gpu_watts > 3.0) {
        out << "  * [GPU Silicon] High GPU render engine activity detected (" << std::fixed << std::setprecision(1) << r.hardware.gpu_watts
            << " W). Closing hardware-accelerated background tabs/canvas will conserve battery.\n";
        has_tip = true;
    }
    if (r.total_system_wakeups_per_sec > 400) {
        out << "  * [C-State Breaker] Wakeup frequency (" << r.total_system_wakeups_per_sec
            << " wakeups/sec) is impeding CPU C3 deep sleep. Enabling TimerSlackCoalescing will bundle timers.\n";
        has_tip = true;
    }
    if (r.hardware.aspm_policy != "powersave" && r.hardware.is_battery_discharging) {
        out << "  * [PCIe Bus] PCIe ASPM policy is '" << r.hardware.aspm_policy
            << "'. Enforcing 'powersave' policy will allow PCIe link substates L1.1/L1.2.\n";
        has_tip = true;
    }
    if (!has_tip) {
        out << "  * System power distribution is well-balanced within optimal hardware efficiency boundaries.\n";
    }
    out << "================================================================================================================\n\n";
}

void ReportGenerator::render_terminal(const AnalysisReportData& r, std::ostream& out) {
    double total_sys = get_effective_total_watts(r);

    out << "\n" << BOLD << CYAN;
    out << "========================================================================================================================\n";
    out << "                       WattCurb: Full-Domain Hardware Power & Process Causation Dashboard                               \n";
    out << "========================================================================================================================\n";
    out << RESET;

    out << DIM << " Observation Window : " << RESET << BOLD << r.sample_duration.count() << " ms" << RESET;
    if (r.sample_count > 1) {
        out << DIM << " (" << r.sample_count << " continuous samples)" << RESET;
    }
    if (r.total_energy_joules > 0.0) {
        out << DIM << " | Total Energy : " << RESET << BOLD << std::fixed << std::setprecision(1) << r.total_energy_joules << " J" << RESET;
    }
    out << DIM << " | Monitored Processes : " << RESET << BOLD << r.total_monitored_processes << RESET;
    out << DIM << " | System Wakeups : " << RESET << BOLD << r.total_system_wakeups_per_sec << " /sec" << RESET;
    out << "\n";

    if (r.is_short_window) {
        out << YELLOW << " [!] Notice: Short evaluation window (" << std::fixed << std::setprecision(1)
            << (static_cast<double>(r.sample_duration.count()) / 1000.0)
            << "s). Physical hardware telemetry (Battery Fuel Gauge, Fan Thermal Lag, NVMe APST) may exhibit transient variance.\n"
            << "            Use '--duration 10' or '-c 5' for sustained attribution without transient noise." << RESET << "\n";
    }


    // 1. Hardware Power Breakdown Section
    out << "\n" << BOLD << "[1] Physical Hardware Power Breakdown" << RESET << "\n";
    out << "------------------------------------------------------------------------------------------------------------------------\n";
    out << std::left << std::setw(26) << " Hardware Domain"
        << std::setw(12) << "Power (W)"
        << std::setw(10) << "Share (%)"
        << std::setw(20) << "Distribution"
        << "Live Hardware Telemetry\n";
    out << "------------------------------------------------------------------------------------------------------------------------\n";

    auto print_hw_row = [&](const std::string& name, double watts, const std::string& telem) {
        double pct = (total_sys > 0.0) ? (watts / total_sys * 100.0) : 0.0;
        out << " " << std::left << std::setw(25) << name
            << std::right << std::fixed << std::setprecision(2) << std::setw(8) << watts << " W  "
            << std::setw(7) << std::setprecision(1) << pct << "%  "
            << std::left << std::setw(19) << format_bar(pct, 14)
            << DIM << telem << RESET << "\n";
    };

    if (r.hardware.total_system_watts > 0.0) {
        std::string bat_source = r.hardware.is_battery_discharging ?
            (std::string(RED) + "Discharging (" + std::to_string(r.hardware.battery_capacity_percent) + "%), Rem: " +
             (r.hardware.battery_remaining_hours > 0.0 ? (std::to_string(r.hardware.battery_remaining_hours).substr(0, 4) + "h") : "N/A") + RESET) :
            (std::string(GREEN) + "AC Online" + (r.hardware.usbc_online ? " [USB-PD: " + std::to_string(r.hardware.usbc_input_watts).substr(0, 4) + "W]" : "") + RESET);
        print_hw_row("Total System (DC Rail)", r.hardware.total_system_watts, bat_source);
    }

    // CPU Package
    std::string cpu_telem = (r.hardware.has_direct_rapl ? "RAPL | " : "Model | ") +
        (r.hardware.cpu_temp_c > 0.0 ? (std::to_string(r.hardware.cpu_temp_c).substr(0, 4) + "°C | ") : "") +
        "Avg " + std::to_string(static_cast<int>(r.hardware.cpu_freq_avg_mhz)) + "MHz (" +
        std::to_string(static_cast<int>(r.hardware.cpu_freq_min_mhz)) + "-" +
        std::to_string(static_cast<int>(r.hardware.cpu_freq_max_mhz)) + "MHz)";
    print_hw_row("CPU Package", r.hardware.cpu_package_watts, cpu_telem);

    // GPU Subsystem
    std::string gpu_telem = "Load: " + std::to_string(r.hardware.gpu_busy_percent) + "% | " +
        (r.hardware.gpu_freq_mhz > 0.0 ? (std::to_string(static_cast<int>(r.hardware.gpu_freq_mhz)) + "MHz | ") : "") +
        (r.hardware.gpu_temp_c > 0.0 ? (std::to_string(r.hardware.gpu_temp_c).substr(0, 4) + "°C | ") : "") +
        "VRAM: " + std::to_string(static_cast<int>(r.hardware.gpu_vram_used_mb)) + "/" +
        std::to_string(static_cast<int>(r.hardware.gpu_vram_total_mb)) + "MB" +
        (!r.hardware.gpu_pcie_link.empty() ? (" [" + r.hardware.gpu_pcie_link + "]") : "");
    print_hw_row("GPU (Graphics/VRAM)", r.hardware.gpu_watts, gpu_telem);

    // Display
    std::string disp_telem = "Brightness: " + std::to_string(static_cast<int>(r.hardware.display_brightness_percent)) + "%";
    print_hw_row("Display / Backlight", r.hardware.display_watts, disp_telem);

    // Cooling Fan
    std::string fan_telem = r.hardware.fan_rpm > 0 ?
        (std::to_string(r.hardware.fan_rpm) + " RPM (ThinkPad EC)") : "Fan Idle / 0 RPM";
    print_hw_row("Cooling Fan (Mechanical)", r.hardware.fan_estimated_watts, fan_telem);

    // Storage NVMe
    std::string nvme_telem = "APST: " + r.hardware.nvme_status + " | " +
        (r.hardware.nvme_temp_c > 0.0 ? (std::to_string(r.hardware.nvme_temp_c).substr(0, 4) + "°C | ") : "") +
        "R: " + std::to_string(r.hardware.disk_read_mb_per_sec).substr(0, 4) + " MB/s, W: " +
        std::to_string(r.hardware.disk_write_mb_per_sec).substr(0, 4) + " MB/s";
    print_hw_row("Storage (NVMe SSD)", r.hardware.storage_estimated_watts, nvme_telem);

    // Uncore & Motherboard
    print_hw_row("Uncore & Platform Rail", r.hardware.uncore_and_platform_watts, "SoC, DRAM, Chipset & VRM loss");

    // Telemetry Summary Strip
    out << "------------------------------------------------------------------------------------------------------------------------\n";
    out << DIM << " [Sleep C-States] " << RESET
        << "C0 Active: " << BOLD << std::fixed << std::setprecision(1) << r.hardware.cstate_c0_active_percent << "%" << RESET << " | "
        << "C1: " << r.hardware.cstate_c1_percent << "% | "
        << "C2: " << r.hardware.cstate_c2_percent << "% | "
        << "C3 Deep: " << BOLD << GREEN << r.hardware.cstate_c3_deep_percent << "%" << RESET << "\n";
    out << DIM << " [Device States]  " << RESET
        << "WiFi: " << r.hardware.wifi_status << " (" << r.hardware.wifi_temp_c << "°C) | "
        << "BT: " << (r.hardware.bluetooth_enabled ? "On" : "Off") << " | "
        << "KbdLight: Lvl " << r.hardware.kbdlight_level << " | "
        << "Bat Health: " << BOLD << r.hardware.battery_health_percent << "%" << RESET << " (Cycles: " << r.hardware.battery_cycle_count << ")\n";
    out << DIM << " [Direct Syscall] " << RESET
        << "PMU Instr: " << BOLD << r.hardware.pmu_instructions << RESET
        << " | Cycles: " << r.hardware.pmu_cycles
        << " | IPC: " << BOLD << GREEN << std::fixed << std::setprecision(2) << r.hardware.pmu_ipc << RESET
        << " | LLC Miss: " << r.hardware.pmu_llc_misses;
    if (r.hardware.cpu_core_vid_mv.has_value()) {
        out << " | VID: " << BOLD << *r.hardware.cpu_core_vid_mv << "mV" << RESET;
    }
    if (r.hardware.pcie_link_speed_gen > 0) {
        out << " | PCIe: " << BOLD << "Gen" << static_cast<int>(r.hardware.pcie_link_speed_gen)
            << " x" << static_cast<int>(r.hardware.pcie_link_width_lanes) << RESET;
    }
    out << "\n";

    // 2. Software (Process) Attribution Section
    out << "\n" << BOLD << "[2] Software-to-Hardware Power Attribution (Per-Process Hardware Usage)" << RESET << "\n";
    out << "------------------------------------------------------------------------------------------------------------------------------------\n";
    out << std::left << std::setw(7) << " PID"
        << std::setw(15) << "Process Name"
        << std::right << std::setw(6) << "CPU(W)"
        << std::setw(6) << "GPU(W)"
        << std::setw(6) << "DRAM(W)"
        << std::setw(6) << "NVMe(W)"
        << std::setw(8) << "WakeTax"
        << std::setw(6) << "Fan(W)"
        << std::setw(8) << "Total(W)"
        << std::setw(5) << "WDI"
        << std::setw(6) << "Core"
        << std::setw(4) << "Th"
        << std::setw(7) << "Fault/s"
        << std::setw(7) << "PSS"
        << std::setw(4) << "Skt"
        << "  Primary Hardware Mechanism\n";
    out << "------------------------------------------------------------------------------------------------------------------------------------\n";

    for (const auto& p : r.top_processes) {
        std::string comm_trunc = p.comm.size() > 13 ? p.comm.substr(0, 12) + "…" : std::string(p.comm.view());

        char core_buf[16] = "-";
        if (p.cpu_core >= 0) {
            core_buf[0] = 'C';
            auto [ptr, ec] = std::to_chars(core_buf + 1, core_buf + 14, p.cpu_core);
            if (p.cross_ccx_migration) { *ptr++ = '!'; }
            *ptr = '\0';
        }

        char pss_buf[16] = "-";
        if (p.pss_kib > 0) {
            auto [ptr, ec] = std::to_chars(pss_buf, pss_buf + 14, p.pss_kib / 1024);
            *ptr++ = 'M';
            *ptr = '\0';
        }

        char skt_buf[16] = "-";
        if (p.open_sockets > 0) {
            auto [ptr, ec] = std::to_chars(skt_buf, skt_buf + 14, p.open_sockets);
            *ptr = '\0';
        }

        char th_buf[16];
        {
            auto [ptr, ec] = std::to_chars(th_buf, th_buf + 14, p.num_threads);
            *ptr = '\0';
        }

        char flt_buf[16] = "-";
        if (p.majflt_per_sec > 0) {
            auto [ptr, ec] = std::to_chars(flt_buf, flt_buf + 14, p.majflt_per_sec);
            *ptr++ = 'M';
            *ptr = '\0';
        } else if (p.minflt_per_sec > 0) {
            auto [ptr, ec] = std::to_chars(flt_buf, flt_buf + 14, p.minflt_per_sec);
            *ptr++ = 'm';
            *ptr = '\0';
        }

        out << " " << std::left << std::setw(6) << p.pid
            << std::setw(15) << comm_trunc
            << std::right << std::fixed << std::setprecision(2)
            << std::setw(5) << p.cpu_watts << " "
            << std::setw(5) << p.gpu_watts << " "
            << std::setw(5) << p.dram_attributed_watts << " "
            << std::setw(5) << p.io_watts << " "
            << std::setw(7) << p.wakeup_tax_watts << " "
            << std::setw(5) << p.fan_attributed_watts << " "
            << BOLD << std::setw(7) << p.total_attributed_watts << RESET << " "
            << std::setw(4) << std::setprecision(1) << p.wdi_score << " "
            << std::setw(5) << core_buf << " "
            << std::setw(3) << th_buf << " "
            << std::setw(6) << flt_buf << " "
            << std::setw(6) << pss_buf << " "
            << std::setw(3) << skt_buf << "  "
            << "[" << BOLD << p.primary_hw_domain << RESET << "] " << DIM << p.hardware_mechanism << RESET << "\n";
    }

    out << "------------------------------------------------------------------------------------------------------------------------------------\n";
    out << DIM << " * Core: CPU Core (! indicates CCX thrashing). Th: Active worker threads. Fault/s: Page faults (M: Major disk-backed, m: Minor).\n";
    out << " * DRAM(W): PSS memory retention & Infinity Fabric bus churn. WakeTax: Context switch penalty. Fan: Induced thermal cooling.\n" << RESET;


    // 3. Hardware Domain Direct Attribution (Culprits) Section (REF-REQ-011)
    if (!r.domain_culprits.empty()) {
        out << "\n" << BOLD << "[3] Hardware Domain Culprits (Direct Cause & Effect Attribution)" << RESET << "\n";
        out << "------------------------------------------------------------------------------------------------------------------------\n";

        for (const auto& d : r.domain_culprits) {
            out << " " << BOLD << YELLOW << "▶ " << d.domain_name << RESET
                << " — Total: " << BOLD << std::fixed << std::setprecision(2) << d.domain_total_watts << " W" << RESET << "\n";

            for (const auto& c : d.top_culprits) {
                out << "   -> PID " << std::left << std::setw(7) << c.pid
                    << std::setw(18) << (c.comm.size() > 16 ? c.comm.substr(0, 15) + "…" : std::string(c.comm.view()))
                    << std::right << std::fixed << std::setprecision(2)
                    << std::setw(7) << c.watts << " W "
                    << "(" << std::setw(5) << std::setprecision(1) << c.share_percent << "%) "
                    << DIM << "— " << c.detail << RESET << "\n";
            }
            out << "\n";
        }
    }

    // 4. Optimization Recommendations
    bool found_runaway = false;
    for (const auto& p : r.top_processes) {
        if (p.is_runaway_candidate) {
            if (!found_runaway) {
                out << BOLD << YELLOW << "[!] WattCurb Automated Mitigation Suggestions:" << RESET << "\n";
                found_runaway = true;
            }
            out << " -> PID " << BOLD << p.pid << " (" << p.comm << ")" << RESET
                << " is consuming " << std::fixed << std::setprecision(2) << p.total_attributed_watts << " W"
                << " (" << p.primary_hw_domain << ": " << p.hardware_mechanism << ")."
                << " Recommended: [Stage 1: SCHED_IDLE] or [Stage 4: cgroup.freeze]\n";
        }
    }
    if (!found_runaway) {
        out << GREEN << "[OK] System running optimally with no rogue background power abusers detected." << RESET << "\n";
    }
    out << "\n";
}

void ReportGenerator::render_json(const AnalysisReportData& r, std::ostream& out) {
    out << "{\n";
    out << "  \"sample_duration_ms\": " << r.sample_duration.count() << ",\n";
    out << "  \"sample_count\": " << r.sample_count << ",\n";
    out << "  \"total_energy_joules\": " << r.total_energy_joules << ",\n";
    out << "  \"is_short_window\": " << (r.is_short_window ? "true" : "false") << ",\n";
    out << "  \"total_monitored_processes\": " << r.total_monitored_processes << ",\n";
    out << "  \"total_system_wakeups_per_sec\": " << r.total_system_wakeups_per_sec << ",\n";
    out << "  \"mitigation_status\": {\n";
    out << "    \"throttled_count\": " << r.mitigation_status.throttled_count << ",\n";
    out << "    \"frozen_count\": " << r.mitigation_status.frozen_count << ",\n";
    out << "    \"reclaimed_bytes\": " << r.mitigation_status.reclaimed_bytes << ",\n";
    out << "    \"estimated_savings_watts\": " << r.mitigation_status.estimated_savings_watts << ",\n";
    out << "    \"active_summary\": \"" << r.mitigation_status.active_summary << "\",\n";
    out << "    \"feature_summaries\": [\n";
    for (size_t k = 0; k < r.mitigation_status.feature_summary_count; ++k) {
        out << "      \"" << r.mitigation_status.feature_summaries[k] << "\""
            << (k + 1 < r.mitigation_status.feature_summary_count ? "," : "") << "\n";
    }
    out << "    ]\n";
    out << "  },\n";
    out << "  \"hardware_breakdown\": {\n";
    out << "    \"total_system_watts\": " << r.hardware.total_system_watts << ",\n";
    out << "    \"cpu_package_watts\": " << r.hardware.cpu_package_watts << ",\n";
    out << "    \"gpu_watts\": " << r.hardware.gpu_watts << ",\n";
    out << "    \"display_watts\": " << r.hardware.display_watts << ",\n";
    out << "    \"fan_estimated_watts\": " << r.hardware.fan_estimated_watts << ",\n";
    out << "    \"storage_estimated_watts\": " << r.hardware.storage_estimated_watts << ",\n";
    out << "    \"uncore_platform_watts\": " << r.hardware.uncore_and_platform_watts << ",\n";
    out << "    \"is_battery_discharging\": " << (r.hardware.is_battery_discharging ? "true" : "false") << ",\n";
    out << "    \"is_ac_online\": " << (r.hardware.is_ac_online ? "true" : "false") << ",\n";
    out << "    \"has_direct_rapl\": " << (r.hardware.has_direct_rapl ? "true" : "false") << ",\n";
    out << "    \"battery_health_percent\": " << r.hardware.battery_health_percent << ",\n";
    out << "    \"battery_remaining_hours\": " << r.hardware.battery_remaining_hours << ",\n";
    out << "    \"battery_cycle_count\": " << r.hardware.battery_cycle_count << ",\n";
    out << "    \"battery_capacity_percent\": " << r.hardware.battery_capacity_percent << ",\n";
    out << "    \"usbc_input_watts\": " << r.hardware.usbc_input_watts << ",\n";
    out << "    \"cpu_temp_c\": " << r.hardware.cpu_temp_c << ",\n";
    out << "    \"cpu_freq_avg_mhz\": " << r.hardware.cpu_freq_avg_mhz << ",\n";
    out << "    \"cpu_freq_min_mhz\": " << r.hardware.cpu_freq_min_mhz << ",\n";
    out << "    \"cpu_freq_max_mhz\": " << r.hardware.cpu_freq_max_mhz << ",\n";
    out << "    \"cpu_governor\": \"" << r.hardware.cpu_governor << "\",\n";
    out << "    \"cstate_c0_active_percent\": " << r.hardware.cstate_c0_active_percent << ",\n";
    out << "    \"cstate_c1_percent\": " << r.hardware.cstate_c1_percent << ",\n";
    out << "    \"cstate_c2_percent\": " << r.hardware.cstate_c2_percent << ",\n";
    out << "    \"cstate_c3_deep_percent\": " << r.hardware.cstate_c3_deep_percent << ",\n";
    out << "    \"gpu_busy_percent\": " << r.hardware.gpu_busy_percent << ",\n";
    out << "    \"gpu_freq_mhz\": " << r.hardware.gpu_freq_mhz << ",\n";
    out << "    \"gpu_temp_c\": " << r.hardware.gpu_temp_c << ",\n";
    out << "    \"gpu_vram_used_mb\": " << r.hardware.gpu_vram_used_mb << ",\n";
    out << "    \"gpu_vram_total_mb\": " << r.hardware.gpu_vram_total_mb << ",\n";
    out << "    \"gpu_pcie_link\": \"" << r.hardware.gpu_pcie_link << "\",\n";
    out << "    \"nvme_status\": \"" << r.hardware.nvme_status << "\",\n";
    out << "    \"nvme_temp_c\": " << r.hardware.nvme_temp_c << ",\n";
    out << "    \"disk_read_mb_per_sec\": " << r.hardware.disk_read_mb_per_sec << ",\n";
    out << "    \"disk_write_mb_per_sec\": " << r.hardware.disk_write_mb_per_sec << ",\n";
    out << "    \"fan_rpm\": " << r.hardware.fan_rpm << ",\n";
    out << "    \"kbdlight_level\": " << r.hardware.kbdlight_level << ",\n";
    out << "    \"bluetooth_enabled\": " << (r.hardware.bluetooth_enabled ? "true" : "false") << ",\n";
    out << "    \"wifi_status\": \"" << r.hardware.wifi_status << "\",\n";
    out << "    \"wifi_temp_c\": " << r.hardware.wifi_temp_c << ",\n";
    out << "    \"pmu_instructions\": " << r.hardware.pmu_instructions << ",\n";
    out << "    \"pmu_cycles\": " << r.hardware.pmu_cycles << ",\n";
    out << "    \"pmu_ipc\": " << r.hardware.pmu_ipc << ",\n";
    out << "    \"pmu_llc_misses\": " << r.hardware.pmu_llc_misses << ",\n";
    out << "    \"cpu_core_vid_mv\": " << (r.hardware.cpu_core_vid_mv ? std::to_string(*r.hardware.cpu_core_vid_mv) : "null") << ",\n";
    out << "    \"pcie_link_speed_gen\": " << static_cast<int>(r.hardware.pcie_link_speed_gen) << ",\n";
    out << "    \"pcie_link_width_lanes\": " << static_cast<int>(r.hardware.pcie_link_width_lanes) << "\n";
    out << "  },\n";
    out << "  \"domain_culprits\": [\n";

    for (size_t i = 0; i < r.domain_culprits.size(); ++i) {
        const auto& d = r.domain_culprits[i];
        out << "    {\n";
        out << "      \"domain\": \"" << d.domain_name << "\",\n";
        out << "      \"total_watts\": " << d.domain_total_watts << ",\n";
        out << "      \"top_culprits\": [\n";
        for (size_t j = 0; j < d.top_culprits.size(); ++j) {
            const auto& c = d.top_culprits[j];
            out << "        {\"pid\": " << c.pid << ", \"comm\": \"" << c.comm << "\", \"watts\": "
                << c.watts << ", \"share_percent\": " << c.share_percent << ", \"detail\": \"" << c.detail << "\"}"
                << (j + 1 < d.top_culprits.size() ? "," : "") << "\n";
        }
        out << "      ]\n";
        out << "    }" << (i + 1 < r.domain_culprits.size() ? "," : "") << "\n";
    }

    out << "  ],\n";
    out << "  \"top_processes\": [\n";

    for (size_t i = 0; i < r.top_processes.size(); ++i) {
        const auto& p = r.top_processes[i];
        out << "    {\n";
        out << "      \"pid\": " << p.pid << ",\n";
        out << "      \"comm\": \"" << p.comm << "\",\n";
        out << "      \"uid\": " << p.uid << ",\n";
        out << "      \"cpu_watts\": " << p.cpu_watts << ",\n";
        out << "      \"gpu_watts\": " << p.gpu_watts << ",\n";
        out << "      \"io_watts\": " << p.io_watts << ",\n";
        out << "      \"wakeup_tax_watts\": " << p.wakeup_tax_watts << ",\n";
        out << "      \"fan_attributed_watts\": " << p.fan_attributed_watts << ",\n";
        out << "      \"total_attributed_watts\": " << p.total_attributed_watts << ",\n";
        out << "      \"wdi_score\": " << p.wdi_score << ",\n";
        out << "      \"wakeups_per_sec\": " << p.wakeups_per_sec << ",\n";
        out << "      \"vram_kib\": " << p.vram_kib << ",\n";
        out << "      \"disk_io_mb_per_sec\": " << p.disk_io_mb_per_sec << ",\n";
        out << "      \"cpu_core\": " << p.cpu_core << ",\n";
        out << "      \"num_threads\": " << p.num_threads << ",\n";
        out << "      \"nice\": " << p.nice << ",\n";
        out << "      \"priority\": " << p.priority << ",\n";
        out << "      \"cross_ccx_migration\": " << (p.cross_ccx_migration ? "true" : "false") << ",\n";
        out << "      \"timerslack_ns\": " << p.timerslack_ns << ",\n";
        out << "      \"pss_kib\": " << p.pss_kib << ",\n";
        out << "      \"minflt_per_sec\": " << p.minflt_per_sec << ",\n";
        out << "      \"majflt_per_sec\": " << p.majflt_per_sec << ",\n";
        out << "      \"open_sockets\": " << p.open_sockets << ",\n";
        out << "      \"wifi_watts\": " << p.wifi_attributed_watts << ",\n";
        out << "      \"dram_watts\": " << p.dram_attributed_watts << ",\n";
        out << "      \"primary_hw_domain\": \"" << p.primary_hw_domain << "\",\n";
        out << "      \"hardware_mechanism\": \"" << p.hardware_mechanism << "\",\n";
        out << "      \"safety_tier\": " << static_cast<int>(p.safety_tier) << ",\n";
        out << "      \"recommended_action\": " << static_cast<int>(p.recommended_action) << ",\n";
        out << "      \"is_runaway\": " << (p.is_runaway_candidate ? "true" : "false") << "\n";
        out << "    }" << (i + 1 < r.top_processes.size() ? "," : "") << "\n";
    }

    out << "  ],\n";
    out << "  \"feature_catalog\": [\n";
    for (size_t i = 0; i < static_cast<size_t>(policy::FeatureId::Count); ++i) {
        auto desc = policy::FeatureManager::descriptor(static_cast<policy::FeatureId>(i));
        out << "    {\n";
        out << "      \"code\": \"" << desc.feature_code << "\",\n";
        out << "      \"name\": \"" << desc.name << "\",\n";
        out << "      \"target_domain\": \"" << desc.target_domain << "\",\n";
        out << "      \"kernel_mechanism\": \"" << desc.kernel_mechanism << "\",\n";
        out << "      \"power_saving_rationale\": \"" << desc.power_saving_rationale << "\",\n";
        out << "      \"safety_constraints\": \"" << desc.safety_constraints << "\",\n";
        out << "      \"description\": \"" << desc.description << "\",\n";
        out << "      \"default_enabled\": " << (desc.default_enabled ? "true" : "false") << "\n";
        out << "    }" << (i + 1 < static_cast<size_t>(policy::FeatureId::Count) ? "," : "") << "\n";
    }
    out << "  ]\n";
    out << "}\n";
}

void ReportGenerator::render_feature_catalog(std::ostream& out) {
    out << "\n" << BOLD << CYAN;
    out << "========================================================================================================================\n";
    out << "                 WattCurb Modular Battery Optimization Features Catalog (REF-REQ-020, REF-ARCH-010)                     \n";
    out << "========================================================================================================================\n";
    out << RESET << "\n";

    for (size_t i = 0; i < static_cast<size_t>(policy::FeatureId::Count); ++i) {
        auto desc = policy::FeatureManager::descriptor(static_cast<policy::FeatureId>(i));
        out << BOLD << "[" << desc.feature_code << "] " << GREEN << desc.name << RESET
            << " (Domain: " << BOLD << desc.target_domain << RESET
            << " | Default: " << (desc.default_enabled ? "ENABLED" : "DISABLED") << ")\n";
        out << "  " << BOLD << "• Summary       : " << RESET << desc.description << "\n";
        out << "  " << BOLD << "• Kernel Mech   : " << RESET << CYAN << desc.kernel_mechanism << RESET << "\n";
        out << "  " << BOLD << "• Power Rationale: " << RESET << desc.power_saving_rationale << "\n";
        out << "  " << BOLD << "• Safety Guard  : " << RESET << YELLOW << desc.safety_constraints << RESET << "\n\n";
    }
    out << "------------------------------------------------------------------------------------------------------------------------\n";
    out << DIM << " * Total Catalogued Optimization Features: " << static_cast<size_t>(policy::FeatureId::Count) << " units\n" << RESET;
}

void ReportGenerator::render_extreme_profile(const AnalysisReportData& r, std::ostream& out) {
    double total_sys = get_effective_total_watts(r);

    out << "\n" << BOLD << MAGENTA;
    out << "========================================================================================================================\n";
    out << "           WattCurb: Extreme 30-Second Physical Hardware Power & Process Causation Profile (REF-REQ-021)                 \n";
    out << "========================================================================================================================\n";
    out << RESET;

    out << DIM << " Sampling Window  : " << RESET << BOLD << r.sample_duration.count() << " ms" << RESET
        << DIM << " (" << r.sample_count << " high-frequency samples)" << RESET;
    if (r.total_energy_joules > 0.0) {
        out << DIM << " | Energy Expended : " << RESET << BOLD << std::fixed << std::setprecision(1)
            << r.total_energy_joules << " Joules" << RESET;
    }
    out << DIM << " | Total System Load : " << RESET << BOLD << std::fixed << std::setprecision(2)
        << total_sys << " W" << RESET << "\n";

    // Battery / Power Rail
    out << DIM << " Power Delivery   : " << RESET;
    if (r.hardware.is_battery_discharging) {
        out << RED << BOLD << "Battery Discharging" << RESET << " (" << r.hardware.battery_capacity_percent << "%) "
            << "| Rate: " << BOLD << total_sys << "W" << RESET;
        if (r.hardware.battery_remaining_hours > 0.0) {
            out << " | Remaining: " << BOLD << std::fixed << std::setprecision(1) << r.hardware.battery_remaining_hours << "h" << RESET;
        }
        out << " | Health: " << r.hardware.battery_health_percent << "% (Cycles: " << r.hardware.battery_cycle_count << ")";
    } else {
        out << GREEN << BOLD << "AC / External Power Online" << RESET;
        if (r.hardware.usbc_online) {
            out << " (USB-PD Input: " << r.hardware.usbc_input_watts << "W)";
        }
    }
    out << "\n";

    // [1] Physical Hardware Power Attribution Matrix
    out << "\n" << BOLD << "[1] Physical Hardware Domain Attribution Matrix (30-Second Sustained Mean)" << RESET << "\n";
    out << "------------------------------------------------------------------------------------------------------------------------\n";
    out << std::left << std::setw(26) << " Physical Domain"
        << std::setw(12) << "Power (W)"
        << std::setw(10) << "Share (%)"
        << std::setw(20) << "Distribution"
        << "Domain Telemetry & States\n";
    out << "------------------------------------------------------------------------------------------------------------------------\n";

    auto print_row = [&](const std::string& name, double watts, const std::string& telem) {
        double pct = (total_sys > 0.0) ? (watts / total_sys * 100.0) : 0.0;
        out << " " << std::left << std::setw(25) << name
            << std::right << std::fixed << std::setprecision(2) << std::setw(8) << watts << " W  "
            << std::setw(7) << std::setprecision(1) << pct << "%  "
            << std::left << std::setw(19) << format_bar(pct, 14)
            << DIM << telem << RESET << "\n";
    };

    // CPU Package
    std::string cpu_telem = (r.hardware.has_direct_rapl ? "RAPL | " : "Model | ") +
        (r.hardware.cpu_temp_c > 0.0 ? (std::to_string(r.hardware.cpu_temp_c).substr(0, 4) + "°C | ") : "") +
        "Avg " + std::to_string(static_cast<int>(r.hardware.cpu_freq_avg_mhz)) + "MHz (" +
        std::to_string(static_cast<int>(r.hardware.cpu_freq_min_mhz)) + "-" +
        std::to_string(static_cast<int>(r.hardware.cpu_freq_max_mhz)) + "MHz) Gov: " +
        r.hardware.cpu_governor;
    print_row("CPU Package (RAPL)", r.hardware.cpu_package_watts, cpu_telem);

    // GPU
    std::string gpu_telem = "Load: " + std::to_string(r.hardware.gpu_busy_percent) + "% | " +
        (r.hardware.gpu_freq_mhz > 0.0 ? (std::to_string(static_cast<int>(r.hardware.gpu_freq_mhz)) + "MHz | ") : "") +
        (r.hardware.gpu_temp_c > 0.0 ? (std::to_string(r.hardware.gpu_temp_c).substr(0, 4) + "°C | ") : "") +
        "VRAM: " + std::to_string(static_cast<int>(r.hardware.gpu_vram_used_mb)) + "/" +
        std::to_string(static_cast<int>(r.hardware.gpu_vram_total_mb)) + "MB" +
        (!r.hardware.gpu_pcie_link.empty() ? (" [" + r.hardware.gpu_pcie_link + "]") : "");
    print_row("GPU (Graphics & VRAM)", r.hardware.gpu_watts, gpu_telem);

    // Display
    std::string disp_telem = "Brightness: " + std::to_string(static_cast<int>(r.hardware.display_brightness_percent)) + "%";
    print_row("Display Panel / Backlight", r.hardware.display_watts, disp_telem);

    // Fan
    std::string fan_telem = r.hardware.fan_rpm > 0 ?
        (std::to_string(r.hardware.fan_rpm) + " RPM (ThinkPad EC)") : "Fan Idle / 0 RPM";
    print_row("Cooling Fan (Thermal)", r.hardware.fan_estimated_watts, fan_telem);

    // Storage NVMe
    std::string nvme_telem = "APST: " + r.hardware.nvme_status + " | " +
        (r.hardware.nvme_temp_c > 0.0 ? (std::to_string(r.hardware.nvme_temp_c).substr(0, 4) + "°C | ") : "") +
        "R: " + std::to_string(r.hardware.disk_read_mb_per_sec).substr(0, 4) + " MB/s, W: " +
        std::to_string(r.hardware.disk_write_mb_per_sec).substr(0, 4) + " MB/s";
    print_row("NVMe Storage (APST)", r.hardware.storage_estimated_watts, nvme_telem);

    // Platform SoC & Memory
    print_row("Platform SoC & DRAM", r.hardware.uncore_and_platform_watts, "Infinity Fabric, DRAM refresh & VRM");

    // [2] CPU Core Residency & Microarchitecture Bottlenecks
    out << "\n" << BOLD << "[2] CPU Core Residency & Microarchitecture PMU Bottlenecks" << RESET << "\n";
    out << "------------------------------------------------------------------------------------------------------------------------\n";
    out << "  • Sleep C-States : C0 Active: " << BOLD << (r.hardware.cstate_c0_active_percent > 20.0 ? RED : GREEN)
        << std::fixed << std::setprecision(1) << r.hardware.cstate_c0_active_percent << "%" << RESET
        << " | C1: " << r.hardware.cstate_c1_percent << "%"
        << " | C2: " << r.hardware.cstate_c2_percent << "%"
        << " | C3 Deep: " << BOLD << GREEN << r.hardware.cstate_c3_deep_percent << "%" << RESET << "\n";
    out << "  • Direct PMU    : Instructions: " << BOLD << r.hardware.pmu_instructions << RESET
        << " | Cycles: " << r.hardware.pmu_cycles
        << " | IPC: " << BOLD << (r.hardware.pmu_ipc > 1.0 ? GREEN : YELLOW) << std::fixed << std::setprecision(2) << r.hardware.pmu_ipc << RESET
        << " | LLC Misses: " << (r.hardware.pmu_llc_misses > 500'000 ? RED : GREEN) << r.hardware.pmu_llc_misses << RESET;
    if (r.hardware.cpu_core_vid_mv.has_value()) {
        out << " | Core VID: " << *r.hardware.cpu_core_vid_mv << "mV";
    }
    if (r.hardware.pcie_link_speed_gen > 0) {
        out << " | PCIe: Gen" << static_cast<int>(r.hardware.pcie_link_speed_gen) << " x" << static_cast<int>(r.hardware.pcie_link_width_lanes);
    }
    out << "\n";

    // [3] Deep Process Attribution & Wakeup Tax Table
    out << "\n" << BOLD << "[3] Sustained Process Attribution & Causation Table (Top 12 Consumers)" << RESET << "\n";
    out << "------------------------------------------------------------------------------------------------------------------------------------\n";
    out << std::left << std::setw(7) << " PID"
        << std::setw(15) << "Process"
        << std::setw(15) << "Safety Tier"
        << std::right << std::setw(7) << "CPU(W)"
        << std::setw(7) << "WakeTax"
        << std::setw(7) << "DRAM(W)"
        << std::setw(7) << "GPU(W)"
        << std::setw(8) << "Total(W)"
        << std::setw(5) << "WDI"
        << std::setw(8) << "Wake/s"
        << std::setw(7) << "PSS"
        << "  Hardware Mechanism\n";
    out << "------------------------------------------------------------------------------------------------------------------------------------\n";

    size_t count = 0;
    for (const auto& p : r.top_processes) {
        if (count++ >= 12) break;
        std::string comm_trunc = p.comm.size() > 14 ? p.comm.substr(0, 13) + "…" : std::string(p.comm.view());

        char pss_buf[16] = "-";
        if (p.pss_kib > 0) {
            auto [ptr, ec] = std::to_chars(pss_buf, pss_buf + 14, p.pss_kib / 1024);
            *ptr++ = 'M';
            *ptr = '\0';
        }

        const char* tier_str = "Unknown";
        auto tier = static_cast<policy::ProcessSafetyTier>(p.safety_tier);
        switch (tier) {
            case policy::ProcessSafetyTier::CriticalImmune: tier_str = "T0:Critical"; break;
            case policy::ProcessSafetyTier::DesktopCore:    tier_str = "T1:DesktopCore"; break;
            case policy::ProcessSafetyTier::DesktopShell:   tier_str = "T2:Shell"; break;
            case policy::ProcessSafetyTier::UserInteractive: tier_str = "T3:UserApp"; break;
            case policy::ProcessSafetyTier::BackgroundWorker: tier_str = "T4:BgWorker"; break;
            case policy::ProcessSafetyTier::RunawayCandidate: tier_str = "T5:Runaway"; break;
        }

        out << " " << std::left << std::setw(6) << p.pid
            << std::setw(15) << comm_trunc
            << std::setw(15) << tier_str
            << std::right << std::fixed << std::setprecision(2)
            << std::setw(6) << p.cpu_watts << " "
            << std::setw(6) << p.wakeup_tax_watts << " "
            << std::setw(6) << p.dram_attributed_watts << " "
            << std::setw(6) << p.gpu_watts << " "
            << BOLD << std::setw(7) << p.total_attributed_watts << RESET << " "
            << std::setw(4) << std::setprecision(1) << p.wdi_score << " "
            << std::setw(7) << p.wakeups_per_sec << " "
            << std::setw(6) << pss_buf << "  "
            << "[" << BOLD << p.primary_hw_domain << RESET << "] " << DIM << p.hardware_mechanism << RESET << "\n";
    }
    out << "------------------------------------------------------------------------------------------------------------------------------------\n";

    // [4] Active Feature Mitigations & Verified Energy Savings
    out << "\n" << BOLD << "[4] Active Feature Mitigations & Verified Energy Savings" << RESET << "\n";
    out << "------------------------------------------------------------------------------------------------------------------------\n";
    out << "  • Overall Status: " << BOLD << GREEN << r.mitigation_status.active_summary.c_str() << RESET << "\n";
    if (r.mitigation_status.feature_summary_count > 0) {
        for (size_t k = 0; k < r.mitigation_status.feature_summary_count; ++k) {
            out << "    " << GREEN << "✔" << RESET << " " << r.mitigation_status.feature_summaries[k].c_str() << "\n";
        }
    } else {
        out << "    " << DIM << "No intrusive actuations required during this window (Optimal Baseline)." << RESET << "\n";
    }

    // [5] Unmitigated Power Drain Opportunities for LLM Feature Synthesis (REF-REQ-021, REF-ARCH-010)
    out << "\n" << BOLD << YELLOW;
    out << "========================================================================================================================\n";
    out << "      [5] Unmitigated Power Drain Opportunities for LLM Feature Synthesis (REF-REQ-021, REF-ARCH-010)                   \n";
    out << "========================================================================================================================\n";
    out << RESET;
    out << "The following high-entropy power drain candidates were observed during the 30-second sustained window\n"
        << "and represent prime opportunities for the AI Agent (LLM) to synthesize new modular BatteryOptimizationFeatures:\n\n";

    size_t opp_count = 0;
    for (const auto& p : r.top_processes) {
        if (p.pid <= 1) continue;
        auto p_tier = static_cast<policy::ProcessSafetyTier>(p.safety_tier);
        // Check for unmitigated non-immune processes consuming notable energy or wakeups
        if (p_tier != policy::ProcessSafetyTier::CriticalImmune &&
            p_tier != policy::ProcessSafetyTier::DesktopCore &&
            (p.total_attributed_watts >= 0.08 || p.wdi_score >= 1.5 || p.wakeups_per_sec >= 40)) {
            
            ++opp_count;
            out << BOLD << "▶ Opportunity #" << opp_count << ": PID " << p.pid << " (" << p.comm << ")" << RESET << "\n";
            out << "  - Domain & Impact   : " << p.primary_hw_domain << " | Attributed: " << BOLD << std::fixed << std::setprecision(2)
                << p.total_attributed_watts << "W" << RESET << " (CPU: " << p.cpu_watts << "W, WakeTax: "
                << p.wakeup_tax_watts << "W, DRAM: " << p.dram_attributed_watts << "W)\n";
            out << "  - Causation Vector  : " << p.hardware_mechanism << " (WDI Score: " << std::setprecision(1) << p.wdi_score
                << ", Wakeups: " << p.wakeups_per_sec << "/sec, TimerSlack: " << (p.timerslack_ns / 1000) << "us, PSS: "
                << (p.pss_kib / 1024) << "MB)\n";
            out << "  - Synthesis Prompt  : LLM should evaluate if a targeted feature (e.g. adaptive frame limiter, polling coalescer,\n"
                << "                        or memory compaction) can safely reclaim power without violating "
                << (p_tier == policy::ProcessSafetyTier::DesktopShell ? "Shell" : "UserApp") << " interactivity.\n\n";
        }
        if (opp_count >= 5) break;
    }

    if (opp_count == 0) {
        out << GREEN << "  [✓] Zero unmitigated anomalies detected. All physical hardware power domains operating at theoretical peak efficiency.\n" << RESET;
    }

    out << "------------------------------------------------------------------------------------------------------------------------\n";
    out << BOLD << "LLM Feature Synthesis JSON Directive:" << RESET << "\n";
    out << "```json\n"
        << "{\n"
        << "  \"synthesis_goal\": \"Generate next C++23 BatteryOptimizationFeature (FEAT-xxx)\",\n"
        << "  \"sample_duration_sec\": 30.0,\n"
        << "  \"unmitigated_opportunities_detected\": " << opp_count << ",\n"
        << "  \"target_domains\": [\"CPU/Scheduler\", \"Timers\", \"ZRAM/Memory\", \"Display\", \"PCIe/NVMe\"]\n"
        << "}\n"
        << "```\n";
    out << "========================================================================================================================\n\n";
}

} // namespace wattcurb::report
