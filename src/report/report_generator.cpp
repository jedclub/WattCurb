#include "report/report_generator.hpp"

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
constexpr const char* DIM = "\033[2m";

std::string format_bar(double percent, int width = 20) {
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
                       (r.hardware.cpu_package_watts + r.hardware.gpu_watts + r.hardware.display_watts + r.hardware.uncore_and_platform_watts);

    out << "\n" << BOLD << CYAN;
    out << "========================================================================================\n";
    out << "                 WattCurb: Hardware-to-Software Power Analysis Report                  \n";
    out << "========================================================================================\n";
    out << RESET;

    out << DIM << " Observation Window : " << RESET << BOLD << r.sample_duration.count() << " ms" << RESET;
    out << DIM << " | Monitored Processes : " << RESET << BOLD << r.total_monitored_processes << RESET;
    out << DIM << " | System Wakeups : " << RESET << BOLD << r.total_system_wakeups_per_sec << " /sec" << RESET;
    out << "\n";

    // 1. Hardware Power Breakdown Section
    out << "\n" << BOLD << "[1] Physical Hardware Power Breakdown" << RESET << "\n";
    out << "----------------------------------------------------------------------------------------\n";
    out << std::left << std::setw(24) << " Hardware Domain"
        << std::setw(12) << "Power (W)"
        << std::setw(10) << "Share (%)"
        << std::setw(24) << "Distribution"
        << "Source / Telemetry\n";
    out << "----------------------------------------------------------------------------------------\n";

    auto print_hw_row = [&](const std::string& name, double watts, const std::string& source) {
        double pct = (total_sys > 0.0) ? (watts / total_sys * 100.0) : 0.0;
        out << " " << std::left << std::setw(23) << name
            << std::right << std::fixed << std::setprecision(2) << std::setw(8) << watts << " W  "
            << std::setw(7) << std::setprecision(1) << pct << "%  "
            << std::left << std::setw(23) << format_bar(pct, 18)
            << DIM << source << RESET << "\n";
    };

    if (r.hardware.total_system_watts > 0.0) {
        std::string bat_source = r.hardware.is_battery_discharging ?
            (std::string(RED) + "Battery Discharging" + RESET) :
            (std::string(GREEN) + "AC Connected" + RESET);
        print_hw_row("Total System (DC Rail)", r.hardware.total_system_watts, bat_source);
    }

    std::string cpu_source = r.hardware.has_direct_rapl ? "Intel/AMD RAPL (energy_uj)" : "Attributed System Decomp.";
    print_hw_row("CPU Package", r.hardware.cpu_package_watts, cpu_source);
    print_hw_row("GPU (Graphics/VRAM)", r.hardware.gpu_watts, "AMDGPU/DRM hwmon (power1_input)");
    print_hw_row("Display / Backlight", r.hardware.display_watts, "sysfs backlight PWM model");
    print_hw_row("Uncore & Motherboard", r.hardware.uncore_and_platform_watts, "Chipset, DRAM & Platform Rail");

    // 2. Software (Process) Attribution Section
    out << "\n" << BOLD << "[2] Software-Level Power Attribution (Top Consumers)" << RESET << "\n";
    out << "----------------------------------------------------------------------------------------\n";
    out << std::left << std::setw(8) << " PID"
        << std::setw(18) << "Process Name"
        << std::right << std::setw(9) << "CPU (W)"
        << std::setw(9) << "GPU (W)"
        << std::setw(10) << "Wakeup/s"
        << std::setw(10) << "Tax (W)"
        << std::setw(10) << "Total(W)"
        << std::setw(8) << " WDI"
        << "  Classification\n";
    out << "----------------------------------------------------------------------------------------\n";

    for (const auto& p : r.top_processes) {
        std::string comm_trunc = p.comm.size() > 16 ? p.comm.substr(0, 15) + "…" : p.comm;

        std::string class_str;
        if (p.is_runaway_candidate) {
            class_str = std::string(BOLD) + RED + "[RUNAWAY DRAIN]" + RESET;
        } else if (p.total_attributed_watts > 0.5) {
            class_str = std::string(YELLOW) + "Active Workload" + RESET;
        } else {
            class_str = std::string(DIM) + "Normal / Low" + RESET;
        }

        out << " " << std::left << std::setw(7) << p.pid
            << std::setw(18) << comm_trunc
            << std::right << std::fixed << std::setprecision(2)
            << std::setw(8) << p.cpu_watts << " "
            << std::setw(8) << p.gpu_watts << " "
            << std::setw(9) << p.wakeups_per_sec << " "
            << std::setw(9) << p.wakeup_tax_watts << " "
            << BOLD << std::setw(9) << p.total_attributed_watts << RESET << " "
            << std::setw(7) << std::setprecision(1) << p.wdi_score << "  "
            << class_str << "\n";
    }

    out << "----------------------------------------------------------------------------------------\n";
    out << DIM << " * WDI: WattCurb Drain Index (Higher score indicates disproportionate battery impact)\n";
    out << " * Tax (W): Wakeup Tax (C-State Disruption penalty preventing CPU package deep sleep)\n" << RESET;

    // 3. Optimization Recommendations
    bool found_runaway = false;
    for (const auto& p : r.top_processes) {
        if (p.is_runaway_candidate) {
            if (!found_runaway) {
                out << "\n" << BOLD << YELLOW << "[!] WattCurb Automated Mitigation Suggestions:" << RESET << "\n";
                found_runaway = true;
            }
            out << " -> PID " << BOLD << p.pid << " (" << p.comm << ")" << RESET
                << " is consuming " << std::fixed << std::setprecision(2) << p.total_attributed_watts << " W"
                << " with " << p.wakeups_per_sec << " wakeups/sec."
                << " Recommended: [Stage 1: SCHED_IDLE] or [Stage 4: cgroup.freeze]\n";
        }
    }
    if (!found_runaway) {
        out << "\n" << GREEN << "[OK] System running optimally with no rogue background power abusers detected." << RESET << "\n";
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
    out << "    \"uncore_platform_watts\": " << r.hardware.uncore_and_platform_watts << ",\n";
    out << "    \"is_battery_discharging\": " << (r.hardware.is_battery_discharging ? "true" : "false") << ",\n";
    out << "    \"has_direct_rapl\": " << (r.hardware.has_direct_rapl ? "true" : "false") << "\n";
    out << "  },\n";
    out << "  \"top_processes\": [\n";

    for (size_t i = 0; i < r.top_processes.size(); ++i) {
        const auto& p = r.top_processes[i];
        out << "    {\n";
        out << "      \"pid\": " << p.pid << ",\n";
        out << "      \"comm\": \"" << p.comm << "\",\n";
        out << "      \"uid\": " << p.uid << ",\n";
        out << "      \"cpu_watts\": " << p.cpu_watts << ",\n";
        out << "      \"gpu_watts\": " << p.gpu_watts << ",\n";
        out << "      \"wakeup_tax_watts\": " << p.wakeup_tax_watts << ",\n";
        out << "      \"total_attributed_watts\": " << p.total_attributed_watts << ",\n";
        out << "      \"wdi_score\": " << p.wdi_score << ",\n";
        out << "      \"wakeups_per_sec\": " << p.wakeups_per_sec << ",\n";
        out << "      \"vram_kib\": " << p.vram_kib << ",\n";
        out << "      \"is_runaway\": " << (p.is_runaway_candidate ? "true" : "false") << "\n";
        out << "    }" << (i + 1 < r.top_processes.size() ? "," : "") << "\n";
    }

    out << "  ]\n";
    out << "}\n";
}

} // namespace wattcurb::report
