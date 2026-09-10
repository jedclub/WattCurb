#include "core/daemon_runner.hpp"
#include "core/singleton_lock.hpp"
#include "hw/hardware_probe.hpp"
#include "proc/process_analyzer.hpp"
#include "policy/attribution_engine.hpp"
#include "policy/mitigation_engine.hpp"
#include "report/report_generator.hpp"
#include "core/scoped_profiler.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <thread>
#include <vector>

namespace {
std::atomic<bool> g_live_running{true};
void handle_sigint(int) {
    g_live_running = false;
}
} // namespace

void print_help(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "WattCurb: Ultra-low-overhead Linux power profiler and adaptive mitigation daemon\n\n"
              << "Two-Part Telemetry & Executive Reporting (REF-REQ-019):\n"
              << "  -b, --briefing         Executive text briefing (human-readable summary & power tips)\n"
              << "  -j, --json             Machine-parsable JSON export of all structural telemetry fields\n"
              << "      --detail           Comprehensive engineering/developer terminal table dashboard\n\n"
              << "Daemon & Live Modes:\n"
              << "  -d, --daemon           Run persistent daemon (Default: 60s period with 5s observation)\n"
              << "  -s, --status           Query live report/status from running background daemon\n"
              << "  -l, --live             Continuous live interactive terminal dashboard (Ctrl+C to stop)\n\n"
              << "Tuning Options:\n"
              << "      --period <sec>     Daemon sleep period in seconds (default: 60.0s)\n"
              << "      --window <sec>     Daemon observation window in seconds (default: 5.0s)\n"
              << "  -i, --interval <sec>   Sampling interval in seconds (default: 2.0s)\n"
              << "  -w, --duration <sec>   Total window duration in seconds (e.g. 5.0, 10.0, 30.0)\n"
              << "  -n, --top <count>      Number of top processes to display (default: 15)\n"
              << "      --dev-profile      Display fine-grained subsystem execution cost breakdown\n"
              << "  -h, --help             Display this help message and exit\n";
}

int query_daemon_briefing() {
    std::ifstream file("/tmp/wattcurb_briefing.txt");
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            std::cout << line << "\n";
        }
        return 0;
    }

    if (wattcurb::core::SingletonLock::is_daemon_running("wattcurb.lock")) {
        std::cout << "[*] Daemon is running. Waiting for observation window to complete...\n";
        return 0;
    }

    return -1; // Daemon not running
}

int query_daemon_status() {
    std::ifstream live_file("/tmp/wattcurb_live.json");
    if (live_file.is_open()) {
        std::string line;
        while (std::getline(live_file, line)) {
            std::cout << line << "\n";
        }
        return 0;
    }

    if (wattcurb::core::SingletonLock::is_daemon_running("wattcurb.lock")) {
        std::cout << "[*] Daemon is running. Waiting for initial sample report...\n";
        return 0;
    }

    std::cerr << "[!] No running WattCurb daemon found (or /tmp/wattcurb_live.json not yet generated).\n"
              << "    Start daemon with: wattcurb --daemon\n";
    return 1;
}

int main(int argc, char* argv[]) {
    double interval_sec = 2.0;
    double duration_sec = 0.0;
    double period_sec = 60.0;
    double window_sec = 5.0;
    size_t sample_count = 1;
    size_t top_n = 15;
    bool json_output = false;
    bool briefing_mode = false;
    bool detail_mode = false;
    bool daemon_mode = false;
    bool status_query = false;
    bool live_mode = false;
    bool dev_profile = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            return 0;
        } else if (arg == "-d" || arg == "--daemon") {
            daemon_mode = true;
        } else if (arg == "-s" || arg == "--status") {
            status_query = true;
        } else if (arg == "-b" || arg == "--briefing") {
            briefing_mode = true;
        } else if (arg == "--detail") {
            detail_mode = true;
        } else if (arg == "-l" || arg == "--live") {
            live_mode = true;
        } else if (arg == "--dev-profile") {
            dev_profile = true;
        } else if (arg == "--period" && i + 1 < argc) {
            period_sec = std::max(5.0, std::strtod(argv[++i], nullptr));
        } else if (arg == "--window" && i + 1 < argc) {
            window_sec = std::max(1.0, std::strtod(argv[++i], nullptr));
        } else if ((arg == "-i" || arg == "--interval") && i + 1 < argc) {
            interval_sec = std::max(0.5, std::strtod(argv[++i], nullptr));
        } else if ((arg == "-w" || arg == "--duration") && i + 1 < argc) {
            duration_sec = std::max(0.5, std::strtod(argv[++i], nullptr));
        } else if ((arg == "-c" || arg == "--count") && i + 1 < argc) {
            sample_count = static_cast<size_t>(std::max(1L, std::strtol(argv[++i], nullptr, 10)));
        } else if ((arg == "-n" || arg == "--top") && i + 1 < argc) {
            top_n = static_cast<size_t>(std::max(1L, std::strtol(argv[++i], nullptr, 10)));
        } else if (arg == "-j" || arg == "--json") {
            json_output = true;
        }
    }

    if (status_query) {
        return query_daemon_status();
    }

    if (briefing_mode && !daemon_mode) {
        int ret = query_daemon_briefing();
        if (ret == 0) {
            return 0; // Printed cached briefing from running daemon
        }
        // If daemon is not running, proceed to run a 5s one-shot observation below
        if (duration_sec == 0.0) {
            duration_sec = 5.0;
        }
    }

    if (daemon_mode) {
        wattcurb::core::DaemonRunner daemon(period_sec, window_sec);
        return daemon.run();
    }

    if (duration_sec > 0.0) {
        sample_count = std::max<size_t>(1, static_cast<size_t>(std::round(duration_sec / interval_sec)));
    }

    wattcurb::hw::HardwareProbe hw_probe;
    wattcurb::proc::ProcessAnalyzer proc_analyzer;
    wattcurb::policy::AttributionEngine engine;
    wattcurb::policy::MitigationEngine mitigation_engine;

    auto sleep_ms = static_cast<int64_t>(interval_sec * 1000.0);

    // Continuous Live Interactive Monitoring Mode (REF-REQ-012 Sec 2.4)
    if (live_mode) {
        std::signal(SIGINT, handle_sigint);
        std::signal(SIGTERM, handle_sigint);

        std::cout << "\033[?25l"; // Hide cursor
        auto hw_prev = hw_probe.capture_sample();
        wattcurb::ProcessPool proc_pool;
        proc_analyzer.capture_snapshot(proc_pool.current());

        while (g_live_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
            if (!g_live_running) break;

            auto hw_cur = hw_probe.capture_sample();
            auto& prev_snapshot = proc_pool.current();
            auto& cur_snapshot = proc_pool.next();
            proc_analyzer.capture_snapshot(cur_snapshot, &prev_snapshot);
            auto report = engine.compute_attribution(hw_prev, hw_cur, prev_snapshot.span(), cur_snapshot.span(), top_n);
            mitigation_engine.evaluate_and_actuate(report, report.hardware.is_battery_discharging, static_cast<double>(report.hardware.battery_capacity_percent));

            if (json_output) {
                wattcurb::report::ReportGenerator::render_json(report, std::cout);
            } else if (briefing_mode) {
                std::cout << "\033[H\033[2J";
                wattcurb::report::ReportGenerator::render_executive_briefing(report, std::cout);
            } else {
                std::cout << "\033[H\033[2J";
                wattcurb::report::ReportGenerator::render_terminal(report, std::cout);
            }

            hw_prev = std::move(hw_cur);
            proc_pool.swap();
        }

        std::cout << "\033[?25h\n"; // Restore cursor
        return 0;
    }

    // Windowed Multi-Sample or One-Shot Profiler (REF-REQ-012 Sec 2.1)
    std::vector<wattcurb::HardwareSample> hw_samples;
    std::vector<wattcurb::ProcessSnapshot> proc_samples;
    hw_samples.reserve(sample_count + 1);
    proc_samples.reserve(sample_count + 1);

    // Initial Baseline Capture (T0)
    hw_samples.push_back(hw_probe.capture_sample());
    proc_samples.emplace_back();
    proc_analyzer.capture_snapshot(proc_samples.back());

    for (size_t step = 1; step <= sample_count; ++step) {
        if (!json_output && sample_count > 1) {
            double progress = static_cast<double>(step - 1) / static_cast<double>(sample_count);
            int bar_width = 20;
            int filled = static_cast<int>(progress * bar_width);
            std::string bar = "[";
            for (int b = 0; b < bar_width; ++b) {
                if (b < filled) bar += "=";
                else if (b == filled) bar += ">";
                else bar += " ";
            }
            bar += "]";
            std::cout << "\r\033[2m[*] WattCurb continuous window profiling: " << bar << " "
                      << std::fixed << std::setprecision(1) << (static_cast<double>(step - 1) * interval_sec) << "s / "
                      << (static_cast<double>(sample_count) * interval_sec) << "s (Interval " << step << "/" << sample_count << ")...\033[0m"
                      << std::flush;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));

        hw_samples.push_back(hw_probe.capture_sample());
        proc_samples.emplace_back();
        proc_analyzer.capture_snapshot(proc_samples.back(), &proc_samples[proc_samples.size() - 2]);
    }

    if (!json_output && sample_count > 1) {
        std::cout << "\r\033[K" << std::flush; // Clear progress bar line
    }

    // Compute Windowed Attribution (Accumulating all intervals)
    auto report = engine.compute_windowed_attribution(hw_samples, proc_samples, top_n);

    // Evaluate closed-loop mitigation status (REF-REQ-019)
    bool on_batt = report.hardware.is_battery_discharging;
    double b_pct = static_cast<double>(report.hardware.battery_capacity_percent);
    mitigation_engine.evaluate_and_actuate(report, on_batt, b_pct);

    // Render Two-Part Telemetry (REF-REQ-019)
    if (json_output) {
        // Part 2 (Machine): Structured JSON
        wattcurb::report::ReportGenerator::render_json(report, std::cout);
    } else if (detail_mode) {
        // Part 2 (Developer): Detailed Terminal Dashboard Table
        wattcurb::report::ReportGenerator::render_terminal(report, std::cout);
    } else {
        // Part 1 (Human): Executive Power & Battery Briefing (Default!)
        wattcurb::report::ReportGenerator::render_executive_briefing(report, std::cout);
    }

    if (dev_profile) {
        wattcurb::core::ScopedProfilerRegistry::instance().print_summary(std::cout);
    }

    return 0;
}
