#include "hw/hardware_probe.hpp"
#include "proc/process_analyzer.hpp"
#include "policy/attribution_engine.hpp"
#include "report/report_generator.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <thread>

void print_help(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "WattCurb: Ultra-low-overhead Linux power profiler and analysis engine\n\n"
              << "Options:\n"
              << "  -i, --interval <sec>   Sampling interval in seconds (default: 2.0)\n"
              << "  -n, --top <count>      Number of top processes to display (default: 15)\n"
              << "  -j, --json             Output analysis in structured JSON format\n"
              << "  -h, --help             Display this help message and exit\n";
}

int main(int argc, char* argv[]) {
    double interval_sec = 2.0;
    size_t top_n = 15;
    bool json_output = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            return 0;
        } else if ((arg == "-i" || arg == "--interval") && i + 1 < argc) {
            interval_sec = std::max(0.5, std::strtod(argv[++i], nullptr));
        } else if ((arg == "-n" || arg == "--top") && i + 1 < argc) {
            top_n = static_cast<size_t>(std::max(1L, std::strtol(argv[++i], nullptr, 10)));
        } else if (arg == "-j" || arg == "--json") {
            json_output = true;
        }
    }

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

    // Snapshot 2 (T2)
    auto hw2 = hw_probe.capture_sample();
    auto proc2 = proc_analyzer.capture_active_processes();

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
