#include "report/report_generator.hpp"

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

} // namespace

void ReportGenerator::render_terminal(const AnalysisReportData& r, std::ostream& out) {
    double total_sys = r.hardware.total_system_watts > 0.0 ? r.hardware.total_system_watts :
                       (r.hardware.cpu_package_watts + r.hardware.gpu_watts + r.hardware.display_watts +
                        r.hardware.fan_estimated_watts + r.hardware.storage_estimated_watts + r.hardware.uncore_and_platform_watts);

    out << "\n" << BOLD << CYAN;
    out << "========================================================================================================================\n";
    out << "                       WattCurb: Full-Domain Hardware Power & Process Causation Dashboard                               \n";
    out << "========================================================================================================================\n";
    out << RESET;

    out << DIM << " Observation Window : " << RESET << BOLD << r.sample_duration.count() << " ms" << RESET;
    out << DIM << " | Monitored Processes : " << RESET << BOLD << r.total_monitored_processes << RESET;
    out << DIM << " | System Wakeups : " << RESET << BOLD << r.total_system_wakeups_per_sec << " /sec" << RESET;
    out << "\n";

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

    // 2. Software (Process) Attribution Section
    out << "\n" << BOLD << "[2] Software-to-Hardware Power Attribution (Per-Process Hardware Usage)" << RESET << "\n";
    out << "------------------------------------------------------------------------------------------------------------------------\n";
    out << std::left << std::setw(7) << " PID"
        << std::setw(17) << "Process Name"
        << std::right << std::setw(8) << "CPU(W)"
        << std::setw(8) << "GPU(W)"
        << std::setw(8) << "NVMe(W)"
        << std::setw(10) << "WakeTax(W)"
        << std::setw(8) << "Fan(W)"
        << std::setw(10) << "Total(W)"
        << std::setw(7) << "WDI"
        << "  Primary Hardware Mechanism\n";
    out << "------------------------------------------------------------------------------------------------------------------------\n";

    for (const auto& p : r.top_processes) {
        std::string comm_trunc = p.comm.size() > 15 ? p.comm.substr(0, 14) + "…" : p.comm;

        out << " " << std::left << std::setw(6) << p.pid
            << std::setw(17) << comm_trunc
            << std::right << std::fixed << std::setprecision(2)
            << std::setw(7) << p.cpu_watts << " "
            << std::setw(7) << p.gpu_watts << " "
            << std::setw(7) << p.io_watts << " "
            << std::setw(9) << p.wakeup_tax_watts << " "
            << std::setw(7) << p.fan_attributed_watts << " "
            << BOLD << std::setw(9) << p.total_attributed_watts << RESET << " "
            << std::setw(6) << std::setprecision(1) << p.wdi_score << "  "
            << "[" << BOLD << p.primary_hw_domain << RESET << "] " << DIM << p.hardware_mechanism << RESET << "\n";
    }

    out << "------------------------------------------------------------------------------------------------------------------------\n";
    out << DIM << " * WakeTax (W): Penalty for context switches forcing CPU out of C3 deep sleep into C0 active state.\n";
    out << " * Fan (W): Proportional share of mechanical cooling fan power thermally induced by CPU/GPU silicon heat load.\n" << RESET;

    // 3. Hardware Domain Direct Attribution (Culprits) Section (REF-REQ-011)
    if (!r.domain_culprits.empty()) {
        out << "\n" << BOLD << "[3] Hardware Domain Culprits (Direct Cause & Effect Attribution)" << RESET << "\n";
        out << "------------------------------------------------------------------------------------------------------------------------\n";

        for (const auto& d : r.domain_culprits) {
            out << " " << BOLD << YELLOW << "▶ " << d.domain_name << RESET
                << " — Total: " << BOLD << std::fixed << std::setprecision(2) << d.domain_total_watts << " W" << RESET << "\n";

            for (const auto& c : d.top_culprits) {
                out << "   -> PID " << std::left << std::setw(7) << c.pid
                    << std::setw(18) << (c.comm.size() > 16 ? c.comm.substr(0, 15) + "…" : c.comm)
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
    out << "  \"total_monitored_processes\": " << r.total_monitored_processes << ",\n";
    out << "  \"total_system_wakeups_per_sec\": " << r.total_system_wakeups_per_sec << ",\n";
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
    out << "    \"wifi_temp_c\": " << r.hardware.wifi_temp_c << "\n";
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
        out << "      \"primary_hw_domain\": \"" << p.primary_hw_domain << "\",\n";
        out << "      \"hardware_mechanism\": \"" << p.hardware_mechanism << "\",\n";
        out << "      \"is_runaway\": " << (p.is_runaway_candidate ? "true" : "false") << "\n";
        out << "    }" << (i + 1 < r.top_processes.size() ? "," : "") << "\n";
    }

    out << "  ]\n";
    out << "}\n";
}

} // namespace wattcurb::report
