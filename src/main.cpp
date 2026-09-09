#include "core/daemon_runner.hpp"
#include "core/singleton_lock.hpp"
#include "hw/hardware_probe.hpp"
#include "proc/process_analyzer.hpp"
#include "policy/attribution_engine.hpp"
#include "report/report_generator.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string_view>
#include <thread>

void print_help(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "WattCurb: Ultra-low-overhead Linux power profiler and analysis daemon\n\n"
              << "Modes:\n"
              << "  -d, --daemon           Run as a persistent low-overhead background daemon\n"
              << "  -s, --status           Query live status and report from running daemon\n"
              << "  (default)              Execute one-shot sampling and print report\n\n"
              << "Options:\n"
              << "  -i, --interval <sec>   Sampling interval in seconds (default: 2.0s, daemon: 5.0s)\n"
              << "  -n, --top <count>      Number of top processes to display (default: 15)\n"
              << "  -j, --json             Output analysis in structured JSON format\n"
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
    size_t top_n = 15;
    bool json_output = false;
    bool daemon_mode = false;
    bool status_query = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            return 0;
        } else if (arg == "-d" || arg == "--daemon") {
            daemon_mode = true;
        } else if (arg == "-s" || arg == "--status") {
            status_query = true;
        } else if ((arg == "-i" || arg == "--interval") && i + 1 < argc) {
            interval_sec = std::max(0.5, std::strtod(argv[++i], nullptr));
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

    // Default: One-shot CLI profiler
    if (!json_output) {
        std::cout << "\033[2m[*] WattCurb sampling physical hardware and active processes ("
                  << interval_sec << "s window)...\033[0m\n" << std::flush;
    }

    wattcurb::hw::HardwareProbe hw_probe;
    wattcurb::proc::ProcessAnalyzer proc_analyzer;
    wattcurb::policy::AttributionEngine engine;

    // Snapshot 1 (T1)
    auto hw1 = hw_probe.capture_sample();
    auto proc1 = proc_analyzer.capture_active_processes();

    // Wait sampling delta
    auto sleep_ms = static_cast<int64_t>(interval_sec * 1000.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));

    // Snapshot 2 (T2) - Passes &proc1 for Lazy Deep Inspection (REF-REQ-007)
    auto hw2 = hw_probe.capture_sample();
    auto proc2 = proc_analyzer.capture_active_processes(&proc1);

    // Compute attribution
    auto report = engine.compute_attribution(hw1, hw2, proc1, proc2, top_n);

    // Render report
    if (json_output) {
        wattcurb::report::ReportGenerator::render_json(report, std::cout);
    } else {
        wattcurb::report::ReportGenerator::render_terminal(report, std::cout);
    }

    return 0;
}
