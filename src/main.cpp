#include "core/daemon_runner.hpp"
#include "core/singleton_lock.hpp"
#include "hw/hardware_probe.hpp"
#include "proc/process_analyzer.hpp"
#include "policy/attribution_engine.hpp"
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
              << "WattCurb: Ultra-low-overhead Linux power profiler and analysis daemon\n\n"
              << "Modes:\n"
              << "  -d, --daemon           Run as a persistent low-overhead background daemon\n"
              << "  -s, --status           Query live status and report from running daemon\n"
              << "  -l, --live             Continuous live interactive monitoring mode (Ctrl+C to stop)\n"
              << "  (default)              Execute windowed sampling and print report\n\n"
              << "Options:\n"
              << "  -i, --interval <sec>   Sampling interval in seconds (default: 2.0s, daemon: 5.0s)\n"
              << "  -w, --duration <sec>   Total evaluation window duration in seconds (e.g. 10.0, 30.0)\n"
              << "  -c, --count <num>      Number of sampling intervals to aggregate (default: 1)\n"
              << "  -n, --top <count>      Number of top processes to display (default: 15)\n"
              << "  -j, --json             Output analysis in structured JSON format\n"
              << "  --dev-profile          Display fine-grained subsystem execution cost breakdown (REF-REQ-014)\n"
              << "  -h, --help             Display this help message and exit\n";
}

int query_daemon_status() {
    std::ifstream live_file("/tmp/wattcurb_live.json");
    if (!live_file.is_open()) {
        if (wattcurb::core::SingletonLock::is_daemon_running("wattcurb.lock")) {
            std::cout << "[*] Daemon is running. Waiting for initial sample report...\n";
            return 0;
        }
        std::cerr << "[!] No running WattCurb daemon found (or /tmp/wattcurb_live.json not yet generated).\n"
                  << "    Start daemon with: wattcurb --daemon\n";
        return 1;
    }

    std::string line;
    while (std::getline(live_file, line)) {
        std::cout << line << "\n";
    }
    return 0;
}

int main(int argc, char* argv[]) {
    double interval_sec = 2.0;
    double duration_sec = 0.0;
    size_t sample_count = 1;
    size_t top_n = 15;
    bool json_output = false;
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
        } else if (arg == "-l" || arg == "--live") {
            live_mode = true;
        } else if (arg == "--dev-profile") {
            dev_profile = true;
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

    if (daemon_mode) {
        double d_interval = (interval_sec == 2.0) ? 5.0 : interval_sec;
        wattcurb::core::DaemonRunner daemon(d_interval);
        return daemon.run();
    }

    if (duration_sec > 0.0) {
        sample_count = std::max<size_t>(1, static_cast<size_t>(std::round(duration_sec / interval_sec)));
    }

    wattcurb::hw::HardwareProbe hw_probe;
    wattcurb::proc::ProcessAnalyzer proc_analyzer;
    wattcurb::policy::AttributionEngine engine;

    auto sleep_ms = static_cast<int64_t>(interval_sec * 1000.0);

    // Continuous Live Interactive Monitoring Mode (REF-REQ-012 Sec 2.4)
    if (live_mode) {
        std::signal(SIGINT, handle_sigint);
        std::signal(SIGTERM, handle_sigint);

        std::cout << "\033[?25l"; // Hide cursor
        auto hw_prev = hw_probe.capture_sample();
        auto proc_prev = proc_analyzer.capture_active_processes();

        while (g_live_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
            if (!g_live_running) break;

            auto hw_cur = hw_probe.capture_sample();
            auto proc_cur = proc_analyzer.capture_active_processes(&proc_prev);
            auto report = engine.compute_attribution(hw_prev, hw_cur, proc_prev, proc_cur, top_n);

            if (json_output) {
                wattcurb::report::ReportGenerator::render_json(report, std::cout);
            } else {
                std::cout << "\033[H\033[2J"; // Clear screen & reset to top
                wattcurb::report::ReportGenerator::render_terminal(report, std::cout);
            }

            hw_prev = std::move(hw_cur);
            proc_prev = std::move(proc_cur);
        }

        std::cout << "\033[?25h\n"; // Restore cursor
        return 0;
    }

    // Windowed Multi-Sample or One-Shot Profiler (REF-REQ-012 Sec 2.1)
    std::vector<wattcurb::HardwareSample> hw_samples;
    std::vector<std::vector<wattcurb::ProcessSample>> proc_samples;
    hw_samples.reserve(sample_count + 1);
    proc_samples.reserve(sample_count + 1);

    // Initial Baseline Capture (T0)
    hw_samples.push_back(hw_probe.capture_sample());
    proc_samples.push_back(proc_analyzer.capture_active_processes());

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
        proc_samples.push_back(proc_analyzer.capture_active_processes(&proc_samples.back()));
    }

    if (!json_output && sample_count > 1) {
        std::cout << "\r\033[K" << std::flush; // Clear progress bar line
    }

    // Compute Windowed Attribution (Accumulating all intervals)
    auto report = engine.compute_windowed_attribution(hw_samples, proc_samples, top_n);

    // Render report
    if (json_output) {
        wattcurb::report::ReportGenerator::render_json(report, std::cout);
    } else {
        wattcurb::report::ReportGenerator::render_terminal(report, std::cout);
    }

    if (dev_profile) {
        wattcurb::core::ScopedProfilerRegistry::instance().print_summary(std::cout);
    }

    return 0;
}

