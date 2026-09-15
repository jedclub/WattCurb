#include "core/daemon_runner.hpp"
#include "core/singleton_lock.hpp"
#include "hw/hardware_probe.hpp"
#include "proc/process_analyzer.hpp"
#include "policy/attribution_engine.hpp"
#include "policy/battery_feature.hpp"
#include "report/report_generator.hpp"
#include "core/scoped_profiler.hpp"
#include "ipc/tray_shared_state.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {
std::atomic<bool> g_live_running{true};
void handle_sigint(int) {
    g_live_running = false;
}
} // namespace

void print_help(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "WattCurb: Ultra-low-overhead Linux power profiler and modular battery mitigation daemon\n\n"
              << "Developer & Debugging Reporting (REF-REQ-020):\n"
              << "  -b, --briefing         High-fidelity detailed executive briefing (10s observation by default)\n"
              << "      --detail           Comprehensive engineering/developer terminal table dashboard\n"
              << "  -F, --features         Print catalog of all modular optimization features with rationale\n"
              << "  -X, --extreme-profile  Execute 30s extreme battery profile for LLM feature synthesis\n\n"
              << "Daemon & Live Modes:\n"
              << "  -d, --daemon           Run persistent daemon (128-byte binary Seqlock POD state in /dev/shm)\n"
              << "  -s, --status           Query live binary state from running daemon via 128-byte Seqlock POD\n"
              << "  -l, --live             Continuous live interactive terminal dashboard (Ctrl+C to stop)\n\n"
              << "Observation & Feature Tuning Options:\n"
              << "      --period <sec>     Daemon sleep period in seconds (default: 60.0s)\n"
              << "      --window <sec>     Daemon observation window in seconds (default: 5.0s)\n"
              << "  -i, --interval <sec>   Sampling interval in seconds (default: 2.0s)\n"
              << "  -w, --duration <sec>   Total window duration in seconds (default for briefing: 10.0s)\n"
              << "  -n, --top <count>      Number of top processes to display (default: 15)\n"
              << "      --dev-profile      Display fine-grained subsystem execution cost breakdown\n"
              << "  -h, --help             Display this help message and exit\n";
}

int query_daemon_briefing() {
    // 1. First attempt direct on-demand IPC query to running daemon (Zero disk I/O)
    std::string resp;
    if (wattcurb::core::SingletonLock::query_daemon("BRIEFING", resp)) {
        std::cout << resp << std::flush;
        return 0;
    }

    // 2. Fallback to cached disk file if available (Zero-Allocation POSIX read)
    int file_fd = ::open("/tmp/wattcurb_briefing.txt", O_RDONLY | O_CLOEXEC);
    if (file_fd >= 0) {
        char buf[4096];
        ssize_t n;
        while ((n = ::read(file_fd, buf, sizeof(buf))) > 0) {
            std::cout.write(buf, n);
        }
        ::close(file_fd);
        return 0;
    }

    if (wattcurb::core::SingletonLock::is_daemon_running("wattcurb.lock")) {
        std::cout << "[*] Daemon is running. Waiting for initial observation window to complete...\n";
        return 0;
    }

    return -1; // Daemon not running
}

int query_daemon_status() {
    // 1. Ultra-fast direct read from 128-byte Seqlock POD Shared Memory (REF-REQ-028, REF-ARCH-018)
    int fd = ::open(wattcurb::ipc::SHARED_STATE_SHM_PATH, O_RDONLY | O_CLOEXEC);
    if (fd >= 0) {
        void* ptr = ::mmap(nullptr, sizeof(wattcurb::ipc::WattCurbSharedState), PROT_READ, MAP_SHARED, fd, 0);
        if (ptr != MAP_FAILED) {
            auto* shm = static_cast<const wattcurb::ipc::WattCurbSharedState*>(ptr);
            wattcurb::ipc::WattCurbSharedState state{};
            if (shm->read_atomic(state)) {
                std::cout << "\033[1m[WattCurb Resident Daemon Binary Status (REF-ARCH-018)]\033[0m\n"
                          << "  - Total System Drain : " << std::fixed << std::setprecision(2) << (state.system_drain_mw / 1000.0) << " W\n"
                          << "  - CPU Package Drain  : " << (state.cpu_drain_mw / 1000.0) << " W (" << state.cpu_temp_c << "°C)\n"
                          << "  - GPU Silicon Drain  : " << (state.gpu_drain_mw / 1000.0) << " W\n"
                          << "  - Battery Level      : " << static_cast<int>(state.battery_percent) << "% ("
                          << (state.battery_state == 1 ? "Discharging" : (state.battery_state == 2 ? "AC Pass-through" : "AC Connected")) << ")\n"
                          << "  - System Wakeups     : " << state.wakeups_per_sec << " wakeups/sec\n"
                          << "  - Cooling Fan        : " << state.fan_rpm << " RPM\n"
                          << "  - Active Mitigations : " << state.active_mitigations << " features active\n";
                if (state.culprits[0].pid > 0) {
                    std::cout << "  - Top Drain Culprit  : PID " << state.culprits[0].pid << " (" << state.culprits[0].comm
                              << ") -> " << (state.culprits[0].drain_mw / 1000.0) << " W\n";
                }
                ::munmap(ptr, sizeof(wattcurb::ipc::WattCurbSharedState));
                ::close(fd);
                return 0;
            }
            ::munmap(ptr, sizeof(wattcurb::ipc::WattCurbSharedState));
        }
        ::close(fd);
    }

    if (wattcurb::core::SingletonLock::is_daemon_running("wattcurb.lock")) {
        std::cout << "[*] Daemon is running. Initializing binary shared state...\n";
        return 0;
    }

    std::cerr << "[!] No running WattCurb daemon found.\n"
              << "    Start daemon with: wattcurb --daemon\n";
    return 1;
}

int main(int argc, char* argv[]) {
    double interval_sec = 2.0;
    double duration_sec = 0.0;
    double period_sec = 3.0;
    double window_sec = 1.0;
    size_t sample_count = 1;
    size_t top_n = 15;
    bool briefing_mode = false;
    bool detail_mode = false;
    bool daemon_mode = false;
    bool status_query = false;
    bool live_mode = false;
    bool dev_profile = false;
    bool feature_catalog_mode = false;
    bool extreme_profile_mode = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            return 0;
        } else if (arg == "-F" || arg == "--features") {
            feature_catalog_mode = true;
        } else if (arg == "-X" || arg == "--extreme-profile") {
            extreme_profile_mode = true;
            if (duration_sec == 0.0) duration_sec = 30.0;
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
            period_sec = std::max(1.0, std::strtod(argv[++i], nullptr));
        } else if (arg == "--window" && i + 1 < argc) {
            window_sec = std::max(0.5, std::strtod(argv[++i], nullptr));
        } else if ((arg == "-i" || arg == "--interval") && i + 1 < argc) {
            interval_sec = std::max(0.5, std::strtod(argv[++i], nullptr));
        } else if ((arg == "-w" || arg == "--duration") && i + 1 < argc) {
            duration_sec = std::max(0.5, std::strtod(argv[++i], nullptr));
        } else if ((arg == "-c" || arg == "--count") && i + 1 < argc) {
            sample_count = static_cast<size_t>(std::max(1L, std::strtol(argv[++i], nullptr, 10)));
        } else if ((arg == "-n" || arg == "--top") && i + 1 < argc) {
            top_n = static_cast<size_t>(std::max(1L, std::strtol(argv[++i], nullptr, 10)));
        }
    }

    if (feature_catalog_mode) {
        wattcurb::report::ReportGenerator::render_feature_catalog(std::cout);
        return 0;
    }

    if (status_query) {
        return query_daemon_status();
    }

    // Extended High-Fidelity Executive Briefing query/execution (REF-REQ-020)
    if (!extreme_profile_mode && (briefing_mode || (!daemon_mode && !detail_mode && !live_mode)) && duration_sec == 0.0) {
        // First try to fetch on-demand live briefing from active daemon
        int ret = query_daemon_briefing();
        if (ret == 0) {
            return 0;
        }
        // If daemon is not running, configure sufficiently long default sampling (10.0 seconds)
        // to isolate steady-state power and filter out transient noise spikes
        duration_sec = 10.0;
        briefing_mode = true;
    }

    if (daemon_mode) {
        wattcurb::core::DaemonRunner daemon(period_sec, window_sec);
        return daemon.run();
    }

    if (duration_sec > 0.0) {
        sample_count = static_cast<size_t>(std::max<size_t>(1, static_cast<size_t>((duration_sec / interval_sec) + 0.5)));
    }

    wattcurb::hw::HardwareProbe hw_probe;
    wattcurb::proc::ProcessAnalyzer proc_analyzer;
    wattcurb::policy::AttributionEngine engine;
    wattcurb::policy::FeatureManager feature_manager;

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
            feature_manager.evaluate_and_actuate(report, report.hardware.is_battery_discharging, static_cast<double>(report.hardware.battery_capacity_percent));

            if (briefing_mode) {
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

    // Extended Windowed Multi-Sample Profiler (REF-REQ-012, REF-REQ-020)
    std::vector<wattcurb::HardwareSample> hw_samples;
    std::vector<wattcurb::ProcessSnapshot> proc_samples;
    hw_samples.reserve(sample_count + 1);
    proc_samples.reserve(sample_count + 1);

    // Initial Baseline Capture (T0)
    hw_samples.push_back(hw_probe.capture_sample());
    proc_samples.emplace_back();
    proc_analyzer.capture_snapshot(proc_samples.back());

    for (size_t step = 1; step <= sample_count; ++step) {
        if (sample_count > 1) {
            double progress = static_cast<double>(step - 1) / static_cast<double>(sample_count);
            int bar_width = 24;
            int filled = static_cast<int>(progress * bar_width);
            std::string bar = "[";
            for (int b = 0; b < bar_width; ++b) {
                if (b < filled) bar += "=";
                else if (b == filled) bar += ">";
                else bar += " ";
            }
            bar += "]";
            std::cout << "\r\033[2m[*] WattCurb deep observation window: " << bar << " "
                      << std::fixed << std::setprecision(1) << (static_cast<double>(step - 1) * interval_sec) << "s / "
                      << (static_cast<double>(sample_count) * interval_sec) << "s (Interval " << step << "/" << sample_count << ")...\033[0m"
                      << std::flush;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));

        hw_samples.push_back(hw_probe.capture_sample());
        proc_samples.emplace_back();
        proc_analyzer.capture_snapshot(proc_samples.back(), &proc_samples[proc_samples.size() - 2]);
    }

    if (sample_count > 1) {
        std::cout << "\r\033[K" << std::flush; // Clear progress bar line
    }

    // Compute Windowed Attribution (Accumulating all intervals)
    auto report = engine.compute_windowed_attribution(hw_samples, proc_samples, top_n);

    // Modular Battery Optimization Feature Evaluation (REF-REQ-020 & REF-ARCH-009)
    bool on_batt = report.hardware.is_battery_discharging;
    double b_pct = static_cast<double>(report.hardware.battery_capacity_percent);
    feature_manager.evaluate_and_actuate(report, on_batt, b_pct);

    // Render Telemetry Outputs (REF-REQ-019, REF-REQ-020, REF-REQ-021)
    if (extreme_profile_mode) {
        // Part 4: Extreme 30s Physical Hardware Causation Profile for LLM Synthesis (REF-REQ-021)
        wattcurb::report::ReportGenerator::render_extreme_profile(report, std::cout);
    } else if (detail_mode) {
        // Part 2 (Developer): Detailed Terminal Dashboard Table
        wattcurb::report::ReportGenerator::render_terminal(report, std::cout);
    } else {
        // Part 1 (Human): High-Fidelity Executive & Physical Power Briefing (Default!)
        wattcurb::report::ReportGenerator::render_executive_briefing(report, std::cout);
    }

    if (dev_profile) {
        wattcurb::core::ScopedProfilerRegistry::instance().print_summary(std::cout);
    }

    return 0;
}
