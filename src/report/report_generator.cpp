#include "report/report_generator.hpp"
#include "policy/process_classifier.hpp"
#include "policy/battery_feature.hpp"

#include <algorithm>
#include <charconv>
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

// Zero-allocation bar writer: writes directly to stream via stack buffer (REF-REQ-007, REF-ARCH-005)
void write_bar(std::ostream& out, double percent, int width = 16) {
    if (percent < 0.0) percent = 0.0;
    if (percent > 100.0) percent = 100.0;
    int filled = static_cast<int>((percent / 100.0) * static_cast<double>(width));
    char buf[32];
    int idx = 0;
    buf[idx++] = '[';
    for (int i = 0; i < width; ++i) {
        if (i < filled) buf[idx++] = '=';
        else if (i == filled) buf[idx++] = '>';
        else buf[idx++] = ' ';
    }
    buf[idx++] = ']';
    out.write(buf, idx);
}

double get_effective_total_watts(const AnalysisReportData& r) {
    double hw_sum = r.hardware.cpu_package_watts + r.hardware.gpu_watts + r.hardware.display_watts +
                    r.hardware.fan_estimated_watts + r.hardware.storage_estimated_watts + r.hardware.uncore_and_platform_watts;
    if (r.hardware.is_battery_discharging && r.hardware.total_system_watts > 0.0) {
        return std::max(r.hardware.total_system_watts, hw_sum);
    }
    return hw_sum > 0.0 ? hw_sum : r.hardware.total_system_watts;
}

// Common hardware row printer: eliminates intermediate std::string allocations and duplicate code
template <typename TelemWriter>
void print_hw_row(std::ostream& out, double total_sys, std::string_view name, double watts, TelemWriter&& write_telem) {
    double pct = (total_sys > 0.0) ? (watts / total_sys * 100.0) : 0.0;
    out << " " << std::left << std::setw(25) << name
        << std::right << std::fixed << std::setprecision(2) << std::setw(8) << watts << " W  "
        << std::setw(7) << std::setprecision(1) << pct << "%  "
        << std::left << std::setw(19);
    write_bar(out, pct, 14);
    out << DIM << " ";
    write_telem(out);
    out << RESET << "\n";
}

// Deduplicated physical hardware domain rendering engine (REF-ARCH-005, REF-REQ-010)
void render_hw_domains_common(const AnalysisReportData& r, double total_sys, std::ostream& out, bool is_extreme) {
    if (!is_extreme && r.hardware.total_system_watts > 0.0) {
        print_hw_row(out, total_sys, "Total System (DC Rail)", r.hardware.total_system_watts, [&](std::ostream& o) {
            if (r.hardware.is_battery_discharging) {
                o << RED << "Discharging (" << r.hardware.battery_capacity_percent << "%), Rem: ";
                if (r.hardware.battery_remaining_hours > 0.0) {
                    o << std::fixed << std::setprecision(1) << r.hardware.battery_remaining_hours << "h";
                } else {
                    o << "N/A";
                }
                o << RESET;
            } else {
                o << GREEN << "AC Online";
                if (r.hardware.usbc_online) {
                    o << " [USB-PD: " << std::fixed << std::setprecision(1) << r.hardware.usbc_input_watts << "W]";
                }
                o << RESET;
            }
        });
    }

    // CPU Package
    print_hw_row(out, total_sys, is_extreme ? "CPU Package (RAPL)" : "CPU Package", r.hardware.cpu_package_watts, [&](std::ostream& o) {
        o << (r.hardware.has_direct_rapl ? "RAPL | " : "Model | ");
        if (r.hardware.cpu_temp_c > 0.0) {
            o << std::fixed << std::setprecision(1) << r.hardware.cpu_temp_c << "°C | ";
        }
        o << "Avg " << static_cast<int>(r.hardware.cpu_freq_avg_mhz) << "MHz ("
          << static_cast<int>(r.hardware.cpu_freq_min_mhz) << "-"
          << static_cast<int>(r.hardware.cpu_freq_max_mhz) << "MHz)";
        if (is_extreme && !r.hardware.cpu_governor.empty()) {
            o << " Gov: " << r.hardware.cpu_governor;
        }
    });

    // GPU Subsystem
    print_hw_row(out, total_sys, is_extreme ? "GPU (Graphics & VRAM)" : "GPU (Graphics/VRAM)", r.hardware.gpu_watts, [&](std::ostream& o) {
        o << "Load: " << r.hardware.gpu_busy_percent << "% | ";
        if (r.hardware.gpu_freq_mhz > 0.0) {
            o << static_cast<int>(r.hardware.gpu_freq_mhz) << "MHz | ";
        }
        if (r.hardware.gpu_temp_c > 0.0) {
            o << std::fixed << std::setprecision(1) << r.hardware.gpu_temp_c << "°C | ";
        }
        o << "VRAM: " << static_cast<int>(r.hardware.gpu_vram_used_mb) << "/"
          << static_cast<int>(r.hardware.gpu_vram_total_mb) << "MB";
        if (!r.hardware.gpu_pcie_link.empty()) {
            o << " [" << r.hardware.gpu_pcie_link << "]";
        }
    });

    // Display
    print_hw_row(out, total_sys, is_extreme ? "Display Panel / Backlight" : "Display / Backlight", r.hardware.display_watts, [&](std::ostream& o) {
        o << "Brightness: " << static_cast<int>(r.hardware.display_brightness_percent) << "%";
    });

    // Cooling Fan
    print_hw_row(out, total_sys, is_extreme ? "Cooling Fan (Thermal)" : "Cooling Fan (Mechanical)", r.hardware.fan_estimated_watts, [&](std::ostream& o) {
        if (r.hardware.fan_rpm > 0) {
            o << r.hardware.fan_rpm << " RPM (ThinkPad EC)";
        } else {
            o << "Fan Idle / 0 RPM";
        }
    });

    // Storage NVMe
    print_hw_row(out, total_sys, is_extreme ? "NVMe Storage (APST)" : "Storage (NVMe SSD)", r.hardware.storage_estimated_watts, [&](std::ostream& o) {
        o << "APST: " << r.hardware.nvme_status << " | ";
        if (r.hardware.nvme_temp_c > 0.0) {
            o << std::fixed << std::setprecision(1) << r.hardware.nvme_temp_c << "°C | ";
        }
        o << "R: " << std::fixed << std::setprecision(1) << r.hardware.disk_read_mb_per_sec << " MB/s, W: "
          << std::fixed << std::setprecision(1) << r.hardware.disk_write_mb_per_sec << " MB/s";
    });

    // Platform SoC & DRAM
    print_hw_row(out, total_sys, is_extreme ? "Platform SoC & DRAM" : "Uncore & Platform Rail", r.hardware.uncore_and_platform_watts, [&](std::ostream& o) {
        o << (is_extreme ? "Infinity Fabric, DRAM refresh & VRM" : "SoC, DRAM, Chipset & VRM loss");
    });
}

// Deduplicated C-State, Device, & Direct PMU telemetry strip (REF-ARCH-005, REF-REQ-015)
void render_telemetry_summary_strip(const AnalysisReportData& r, std::ostream& out, bool bullet_style) {
    const char* pfx = bullet_style ? "  • " : " ";
    out << "------------------------------------------------------------------------------------------------------------------------\n";
    out << pfx << DIM << "[Sleep C-States] " << RESET
        << "C0 Active: " << BOLD << (r.hardware.cstate_c0_active_percent > 20.0 ? RED : GREEN)
        << std::fixed << std::setprecision(1) << r.hardware.cstate_c0_active_percent << "%" << RESET << " | "
        << "C1: " << r.hardware.cstate_c1_percent << "% | "
        << "C2: " << r.hardware.cstate_c2_percent << "% | "
        << "C3 Deep: " << BOLD << GREEN << r.hardware.cstate_c3_deep_percent << "%" << RESET << "\n";

    if (!bullet_style) {
        out << pfx << DIM << "[Device States]  " << RESET
            << "WiFi: " << r.hardware.wifi_status << " (" << r.hardware.wifi_temp_c << "°C) | "
            << "BT: " << (r.hardware.bluetooth_enabled ? "On" : "Off") << " | "
            << "KbdLight: Lvl " << r.hardware.kbdlight_level << " | "
            << "Bat Health: " << BOLD << r.hardware.battery_health_percent << "%" << RESET << " (Cycles: " << r.hardware.battery_cycle_count << ")\n";
    }

    out << pfx << DIM << (bullet_style ? "[Direct PMU]    : " : "[Direct Syscall] ") << RESET
        << (bullet_style ? "Instructions: " : "PMU Instr: ") << BOLD << r.hardware.pmu_instructions << RESET
        << " | Cycles: " << r.hardware.pmu_cycles
        << " | IPC: " << BOLD << (r.hardware.pmu_ipc > 1.0 ? GREEN : YELLOW) << std::fixed << std::setprecision(2) << r.hardware.pmu_ipc << RESET
        << " | LLC Miss: " << (r.hardware.pmu_llc_misses > 500'000 ? RED : GREEN) << r.hardware.pmu_llc_misses << RESET
        << " | Branch Miss: " << r.hardware.pmu_branch_misses << "\n";

    out << pfx << DIM << "[PMU Power Proxy]" << (bullet_style ? ": " : " ") << RESET
        << "EPI: " << BOLD << CYAN << std::fixed << std::setprecision(1) << (r.hardware.pmu_energy_proxy_index / 1'000'000.0) << "M" << RESET
        << " | EWR: " << BOLD << (r.hardware.pmu_energy_waste_ratio > 30.0 ? RED : GREEN) << std::fixed << std::setprecision(1) << r.hardware.pmu_energy_waste_ratio << "%" << RESET
        << " | Est. Power: " << BOLD << std::fixed << std::setprecision(1) << r.hardware.pmu_estimated_power_mw << " mW" << RESET;
    if (r.hardware.cpu_core_vid_mv.has_value()) {
        out << " | " << (bullet_style ? "Core VID: " : "VID: ") << BOLD << *r.hardware.cpu_core_vid_mv << "mV" << RESET;
    }
    if (r.hardware.pcie_link_speed_gen > 0) {
        out << " | PCIe: " << BOLD << "Gen" << static_cast<int>(r.hardware.pcie_link_speed_gen)
            << " x" << static_cast<int>(r.hardware.pcie_link_width_lanes) << RESET;
    }
    out << "\n";
}

struct ProcRowBuffers {
    char core[16]{'-', '\0'};
    char pss[16]{'-', '\0'};
    char skt[16]{'-', '\0'};
    char th[16]{'\0'};
    char flt[16]{'-', '\0'};
};

inline void format_proc_buffers(const ProcessAttributedPower& p, ProcRowBuffers& b) {
    if (p.cpu_core >= 0) {
        b.core[0] = 'C';
        auto [ptr, ec] = std::to_chars(b.core + 1, b.core + 14, p.cpu_core);
        if (p.cross_ccx_migration) { *ptr++ = '!'; }
        *ptr = '\0';
    }
    if (p.pss_kib > 0) {
        auto [ptr, ec] = std::to_chars(b.pss, b.pss + 14, p.pss_kib / 1024);
        *ptr++ = 'M';
        *ptr = '\0';
    }
    if (p.open_sockets > 0) {
        auto [ptr, ec] = std::to_chars(b.skt, b.skt + 14, p.open_sockets);
        *ptr = '\0';
    }
    {
        auto [ptr, ec] = std::to_chars(b.th, b.th + 14, p.num_threads);
        *ptr = '\0';
    }
    if (p.majflt_per_sec > 0) {
        auto [ptr, ec] = std::to_chars(b.flt, b.flt + 14, p.majflt_per_sec);
        *ptr++ = 'M';
        *ptr = '\0';
    } else if (p.minflt_per_sec > 0) {
        auto [ptr, ec] = std::to_chars(b.flt, b.flt + 14, p.minflt_per_sec);
        *ptr++ = 'm';
        *ptr = '\0';
    }
}

inline const char* get_safety_tier_short_name(policy::ProcessSafetyTier tier) noexcept {
    switch (tier) {
        case policy::ProcessSafetyTier::CriticalImmune: return "T0:Critical";
        case policy::ProcessSafetyTier::DesktopCore:    return "T1:DesktopCore";
        case policy::ProcessSafetyTier::DesktopShell:   return "T2:Shell";
        case policy::ProcessSafetyTier::UserInteractive: return "T3:UserApp";
        case policy::ProcessSafetyTier::BackgroundWorker: return "T4:BgWorker";
        case policy::ProcessSafetyTier::RunawayCandidate: return "T5:Runaway";
    }
    return "Unknown";
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

    // 1. System Battery & Power Overview (REF-REQ-022)
    out << BOLD << " [1] System Battery & Power Supply Deep Telemetry" << RESET << "\n";
    out << "  - Total System Drain      : " << BOLD << (total_sys > 25.0 ? RED : (total_sys > 12.0 ? YELLOW : GREEN))
        << std::fixed << std::setprecision(2) << total_sys << " Watts" << RESET;
    if (r.hardware.is_ac_passthrough) {
        out << BOLD << GREEN << " [AC Hardware Pass-Through Active: Zero Battery Wear]" << RESET;
    }
    out << "\n";

    out << "  - Power Supply State      : "
        << (r.hardware.is_battery_discharging ? RED : GREEN)
        << (r.hardware.is_battery_discharging ? "Discharging (On Battery)" : "AC Connected (Line Power / Charging)")
        << RESET;
    if (r.hardware.usbc_online) {
        out << DIM << " [USB-PD Input: " << std::fixed << std::setprecision(1) << r.hardware.usbc_input_watts << "W";
        if (!r.hardware.usbc_pd_type.empty()) {
            out << " (" << r.hardware.usbc_pd_type << ")";
        }
        out << "]" << RESET;
    }
    out << "\n";

    if (r.hardware.is_battery_discharging || r.hardware.battery_capacity_percent > 0) {
        out << "  - Battery Capacity        : " << BOLD << r.hardware.battery_capacity_percent << "%" << RESET;
        if (!r.hardware.battery_capacity_level.empty()) {
            out << " [" << r.hardware.battery_capacity_level << "]";
        }
        if (r.hardware.is_battery_discharging) {
            if (r.hardware.battery_remaining_hours_to_empty > 0.0) {
                out << " (" << BOLD << std::fixed << std::setprecision(1) << r.hardware.battery_remaining_hours_to_empty << "h to empty" << RESET << ")";
            }
        } else {
            if (r.hardware.battery_remaining_hours_to_threshold > 0.0) {
                out << " (" << BOLD << std::fixed << std::setprecision(1) << r.hardware.battery_remaining_hours_to_threshold << "h to limit" << RESET << ")";
            } else if (r.hardware.battery_remaining_hours_to_full > 0.0) {
                out << " (" << BOLD << std::fixed << std::setprecision(1) << r.hardware.battery_remaining_hours_to_full << "h to full" << RESET << ")";
            }
        }
        out << " | Health: " << std::fixed << std::setprecision(1) << r.hardware.battery_health_percent << "%"
            << " (" << r.hardware.battery_cycle_count << " cycles)\n";

        // Battery ID, Vendor, Model, Chemistry & Serial
        if (!r.hardware.battery_model_name.empty() || !r.hardware.battery_manufacturer.empty()) {
            out << "  - Battery Hardware ID     : " << BOLD << r.hardware.battery_manufacturer << " " << r.hardware.battery_model_name << RESET;
            if (!r.hardware.battery_serial_number.empty()) {
                out << " (S/N: " << r.hardware.battery_serial_number << ")";
            }
            if (!r.hardware.battery_technology.empty()) {
                out << " [" << r.hardware.battery_technology << "]";
            }
            out << "\n";
        }

        // Voltage, Current Flow & Energy Breakdown
        if (r.hardware.battery_voltage_now_v > 0.0) {
            out << "  - Voltage & Current Flow  : " << std::fixed << std::setprecision(3) << r.hardware.battery_voltage_now_v << " V";
            if (r.hardware.battery_voltage_min_design_v > 0.0) {
                out << " (Design Nominal: " << std::fixed << std::setprecision(2) << r.hardware.battery_voltage_min_design_v << " V)";
            }
            out << " | Flow: " << (r.hardware.battery_current_now_a < 0 ? RED : (r.hardware.battery_current_now_a > 0.05 ? GREEN : RESET))
                << std::fixed << std::setprecision(3) << r.hardware.battery_current_now_a << " A" << RESET << "\n";
        }

        // Energy Degradation & Wear Telemetry
        if (r.hardware.battery_energy_full_wh > 0.0) {
            out << "  - Energy & Degradation    : " << std::fixed << std::setprecision(2) << r.hardware.battery_energy_now_wh << " Wh now / "
                << r.hardware.battery_energy_full_wh << " Wh full (Design: " << r.hardware.battery_energy_design_wh << " Wh) | "
                << BOLD << (r.hardware.battery_degradation_percent > 20.0 ? RED : (r.hardware.battery_degradation_percent > 10.0 ? YELLOW : GREEN))
                << std::fixed << std::setprecision(1) << r.hardware.battery_degradation_percent << "% wear ("
                << std::fixed << std::setprecision(2) << r.hardware.battery_lost_capacity_wh << " Wh lost)" << RESET << "\n";
        }

        // ThinkPad Charge Thresholds & Conservation Mode
        if (r.hardware.battery_charge_end_threshold.has_value() || r.hardware.battery_charge_start_threshold.has_value()) {
            out << "  - ThinkPad Charge Guard   : ";
            if (r.hardware.is_conservation_mode_active) {
                out << BOLD << GREEN << "CONSERVATION ACTIVE" << RESET;
            } else {
                out << "STANDARD";
            }
            out << " (Stop: " << r.hardware.battery_charge_end_threshold.value_or(100) << "%, Start: "
                << r.hardware.battery_charge_start_threshold.value_or(0) << "%)";
            if (!r.hardware.battery_charge_behaviour.empty()) {
                out << " [" << r.hardware.battery_charge_behaviour << "]";
            }
            out << "\n";
        }

        // Connected Wireless / External Peripheral Batteries
        if (!r.hardware.peripheral_batteries.empty()) {
            out << "  - Connected Peripherals   : ";
            for (size_t k = 0; k < r.hardware.peripheral_batteries.size(); ++k) {
                const auto& pb = r.hardware.peripheral_batteries[k];
                out << pb.name << ": " << BOLD << pb.capacity_percent << "%" << RESET;
                if (pb.is_charging) out << " [Charging]";
                if (k + 1 < r.hardware.peripheral_batteries.size()) out << ", ";
            }
            out << "\n";
        }
    }

    // 2. Hardware Subsystem & Domain Power Breakdown
    out << "\n" << BOLD << " [2] Physical Hardware Domain Power & State Breakdown" << RESET << "\n";
    auto print_hw = [&](const char* domain, double watts, auto&& write_details) {
        double pct = total_sys > 0.0 ? (watts / total_sys) * 100.0 : 0.0;
        out << "  * " << std::left << std::setw(22) << domain << ": "
            << std::right << std::setw(6) << std::fixed << std::setprecision(2) << watts << " W "
            << "(" << std::setw(5) << std::fixed << std::setprecision(1) << pct << "%) "
            << DIM;
        write_details(out);
        out << RESET << "\n";
    };

    // CPU Telemetry Line
    print_hw("CPU Package (RAPL)", r.hardware.cpu_package_watts, [&](std::ostream& o) {
        o << "Temp: " << static_cast<int>(r.hardware.cpu_temp_c) << "°C, "
          << static_cast<int>(r.hardware.cpu_freq_avg_mhz) << " MHz avg ("
          << r.hardware.cpu_governor << ")";
    });

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
            << " | LLC Miss: " << r.hardware.pmu_llc_misses
            << " | Branch Miss: " << r.hardware.pmu_branch_misses << "\n"
            << "       " << DIM << "PMU Power Proxy (REF-REQ-024): " << RESET
            << "EPI: " << BOLD << CYAN << std::fixed << std::setprecision(1) << (r.hardware.pmu_energy_proxy_index / 1'000'000.0) << "M" << RESET
            << " | EWR: " << BOLD << (r.hardware.pmu_energy_waste_ratio > 30.0 ? RED : GREEN) << std::fixed << std::setprecision(1) << r.hardware.pmu_energy_waste_ratio << "%" << RESET
            << " | Est. Power: " << BOLD << std::fixed << std::setprecision(1) << r.hardware.pmu_estimated_power_mw << " mW" << RESET << "\n";
    }

    // GPU Telemetry Line
    print_hw("GPU Silicon (DRM)", r.hardware.gpu_watts, [&](std::ostream& o) {
        o << "Busy: " << r.hardware.gpu_busy_percent << "%, "
          << static_cast<int>(r.hardware.gpu_vram_used_mb) << " MB VRAM, "
          << r.hardware.gpu_pcie_link;
    });

    // Display
    print_hw("Display Backlight", r.hardware.display_watts, [&](std::ostream& o) {
        o << "Brightness: " << static_cast<int>(r.hardware.display_brightness_percent) << "%";
    });

    // Storage
    print_hw("Storage / NVMe APST", r.hardware.storage_estimated_watts, [&](std::ostream& o) {
        o << "NVMe: " << r.hardware.nvme_status << " (Read "
          << std::fixed << std::setprecision(1) << r.hardware.disk_read_mb_per_sec << " MB/s, Write "
          << std::fixed << std::setprecision(1) << r.hardware.disk_write_mb_per_sec << " MB/s)";
    });

    // Cooling & Platform
    print_hw("Mechanical Fan", r.hardware.fan_estimated_watts, [&](std::ostream& o) {
        o << r.hardware.fan_rpm << " RPM";
    });
    print_hw("Uncore & Platform Loss", r.hardware.uncore_and_platform_watts, [&](std::ostream& o) {
        o << "ASPM: " << r.hardware.aspm_policy;
    });

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

    render_hw_domains_common(r, total_sys, out, /*is_extreme=*/false);
    render_telemetry_summary_strip(r, out, /*bullet_style=*/false);

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
        ProcRowBuffers buf;
        format_proc_buffers(p, buf);

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
            << std::setw(5) << buf.core << " "
            << std::setw(3) << buf.th << " "
            << std::setw(6) << buf.flt << " "
            << std::setw(6) << buf.pss << " "
            << std::setw(3) << buf.skt << "  "
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
    out << "    \"battery_degradation_percent\": " << r.hardware.battery_degradation_percent << ",\n";
    out << "    \"battery_lost_capacity_wh\": " << r.hardware.battery_lost_capacity_wh << ",\n";
    out << "    \"battery_energy_now_wh\": " << r.hardware.battery_energy_now_wh << ",\n";
    out << "    \"battery_energy_full_wh\": " << r.hardware.battery_energy_full_wh << ",\n";
    out << "    \"battery_energy_design_wh\": " << r.hardware.battery_energy_design_wh << ",\n";
    out << "    \"battery_voltage_now_v\": " << r.hardware.battery_voltage_now_v << ",\n";
    out << "    \"battery_voltage_min_design_v\": " << r.hardware.battery_voltage_min_design_v << ",\n";
    out << "    \"battery_current_now_a\": " << r.hardware.battery_current_now_a << ",\n";
    out << "    \"battery_remaining_hours\": " << r.hardware.battery_remaining_hours << ",\n";
    out << "    \"battery_remaining_hours_to_empty\": " << r.hardware.battery_remaining_hours_to_empty << ",\n";
    out << "    \"battery_remaining_hours_to_threshold\": " << r.hardware.battery_remaining_hours_to_threshold << ",\n";
    out << "    \"battery_remaining_hours_to_full\": " << r.hardware.battery_remaining_hours_to_full << ",\n";
    out << "    \"battery_cycle_count\": " << r.hardware.battery_cycle_count << ",\n";
    out << "    \"battery_capacity_percent\": " << r.hardware.battery_capacity_percent << ",\n";
    out << "    \"is_conservation_mode_active\": " << (r.hardware.is_conservation_mode_active ? "true" : "false") << ",\n";
    out << "    \"is_ac_passthrough\": " << (r.hardware.is_ac_passthrough ? "true" : "false") << ",\n";
    out << "    \"battery_technology\": \"" << r.hardware.battery_technology << "\",\n";
    out << "    \"battery_capacity_level\": \"" << r.hardware.battery_capacity_level << "\",\n";
    out << "    \"battery_model_name\": \"" << r.hardware.battery_model_name << "\",\n";
    out << "    \"battery_manufacturer\": \"" << r.hardware.battery_manufacturer << "\",\n";
    out << "    \"battery_serial_number\": \"" << r.hardware.battery_serial_number << "\",\n";
    out << "    \"battery_charge_start_threshold\": " << (r.hardware.battery_charge_start_threshold ? std::to_string(*r.hardware.battery_charge_start_threshold) : "null") << ",\n";
    out << "    \"battery_charge_end_threshold\": " << (r.hardware.battery_charge_end_threshold ? std::to_string(*r.hardware.battery_charge_end_threshold) : "null") << ",\n";
    out << "    \"battery_charge_behaviour\": \"" << r.hardware.battery_charge_behaviour << "\",\n";
    out << "    \"usbc_input_watts\": " << r.hardware.usbc_input_watts << ",\n";
    out << "    \"usbc_pd_type\": \"" << r.hardware.usbc_pd_type << "\",\n";
    out << "    \"peripheral_batteries\": [\n";
    for (size_t k = 0; k < r.hardware.peripheral_batteries.size(); ++k) {
        const auto& pb = r.hardware.peripheral_batteries[k];
        out << "      {\"name\": \"" << pb.name << "\", \"capacity_percent\": " << pb.capacity_percent
            << ", \"is_charging\": " << (pb.is_charging ? "true" : "false") << "}"
            << (k + 1 < r.hardware.peripheral_batteries.size() ? "," : "") << "\n";
    }
    out << "    ],\n";
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
    out << "    \"pmu_branch_misses\": " << r.hardware.pmu_branch_misses << ",\n";
    out << "    \"pmu_energy_proxy_index\": " << r.hardware.pmu_energy_proxy_index << ",\n";
    out << "    \"pmu_energy_waste_ratio\": " << r.hardware.pmu_energy_waste_ratio << ",\n";
    out << "    \"pmu_estimated_power_mw\": " << r.hardware.pmu_estimated_power_mw << ",\n";
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

    // Battery / Power Rail (REF-REQ-022)
    out << DIM << " Power Delivery   : " << RESET;
    if (r.hardware.is_battery_discharging) {
        out << RED << BOLD << "Battery Discharging" << RESET << " (" << r.hardware.battery_capacity_percent << "%) "
            << "| Rate: " << BOLD << total_sys << "W" << RESET;
        if (r.hardware.battery_remaining_hours_to_empty > 0.0) {
            out << " | Remaining: " << BOLD << std::fixed << std::setprecision(1) << r.hardware.battery_remaining_hours_to_empty << "h" << RESET;
        }
        out << " | Health: " << std::fixed << std::setprecision(1) << r.hardware.battery_health_percent << "% (Wear: "
            << std::fixed << std::setprecision(1) << r.hardware.battery_degradation_percent << "%, Cycles: "
            << r.hardware.battery_cycle_count << ")";
    } else {
        out << GREEN << BOLD << "AC / External Power Online" << RESET;
        if (r.hardware.is_ac_passthrough) {
            out << BOLD << GREEN << " [Hardware Direct Pass-Through]" << RESET;
        }
        if (r.hardware.usbc_online) {
            out << " (USB-PD Input: " << r.hardware.usbc_input_watts << "W";
            if (!r.hardware.usbc_pd_type.empty()) out << ", " << r.hardware.usbc_pd_type;
            out << ")";
        }
    }
    if (!r.hardware.battery_model_name.empty()) {
        out << DIM << " [" << r.hardware.battery_manufacturer << " " << r.hardware.battery_model_name << "]" << RESET;
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

    render_hw_domains_common(r, total_sys, out, /*is_extreme=*/true);

    // [2] CPU Core Residency & Microarchitecture Bottlenecks
    out << "\n" << BOLD << "[2] CPU Core Residency & Microarchitecture PMU Bottlenecks" << RESET << "\n";
    render_telemetry_summary_strip(r, out, /*bullet_style=*/true);

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
        ProcRowBuffers buf;
        format_proc_buffers(p, buf);
        const char* tier_str = get_safety_tier_short_name(static_cast<policy::ProcessSafetyTier>(p.safety_tier));

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
            << std::setw(6) << buf.pss << "  "
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
