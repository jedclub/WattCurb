#include "core/daemon_runner.hpp"
#include "core/singleton_lock.hpp"
#include "hw/hardware_probe.hpp"
#include "proc/process_analyzer.hpp"
#include "policy/attribution_engine.hpp"
#include "policy/battery_feature.hpp"
#include "report/report_generator.hpp"
#include "report/battery_history_analyzer.hpp"
#include "core/scoped_profiler.hpp"
#include "ipc/tray_shared_state.hpp"
#include "ipc/history_ring_buffer.hpp"
#include "core/event_logger.hpp"
#include "core/l10n.hpp"

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
    std::cout <<
R"(WattCurb — Ultra-Low-Overhead Linux Power Profiler & Modular Battery Mitigation Daemon

USAGE
  )" << prog << R"( [MODE] [OPTIONS]

  With no arguments, WattCurb runs one observation window and prints the
  developer detail table. Every mode below is a single-shot query unless it is
  marked PERSISTENT.

WHEN TO USE WHICH
  Just checking the machine now ............ -b  (briefing)
  Machine-parsable current state ........... -s  (status)
  Why is the battery draining .............. -R  (battery report)
  What has the daemon been doing ........... -L  (logs)
  Watch it live in a terminal .............. -l  (live)
  Run it as a background service ........... -d  (daemon)

DISPLAY & REPORT MODES
  -b, --briefing
        High-fidelity executive briefing: physical hardware power domains
        (CPU RAPL, GPU, display, NVMe, fan, platform loss), battery chemistry
        and health, and the top drain culprits with their causation mechanism.
        Queries the running daemon over IPC (zero disk I/O); falls back to the
        cached briefing file, then to a one-shot local observation.
        Default window: )" << "10.0s" << R"( (override with -w).

      --detail
        Comprehensive engineering table: per-process attributed watts, WDI
        ranking, wakeups, cgroup/tier classification and scheduler state.
        This is the default when no mode flag is given.

  -R, --battery-report
        Deep battery drain audit over ALL accumulated history (7-day in-memory
        ring buffer plus persisted logs). Reports discharge energy, average and
        peak power, per-profile efficiency comparisons, C3 residency and
        thermal correlation. Read-only; writes nothing.

  -l, --live
        PERSISTENT. Continuous interactive terminal dashboard, refreshed every
        -i seconds until Ctrl+C. Low cost, but it is a full-screen redraw loop.

  -F, --features
        Print the catalog of every modular optimization feature: what it does,
        which kernel interface it writes, and its power-saving rationale.
        No observation is performed.

  -X, --extreme-profile
        Run a 30 s extreme battery-drain observation for feature synthesis /
        LLM analysis. Forces -w 30 unless you pass -w explicitly.
        NOTE: this is a measurement profile, not a power profile.

  -H, --history
        Print the in-memory telemetry history (last ~30 minutes) from the
        running daemon. Zero disk I/O.

  -L, --logs
        Print recent event-driven audit records (mitigations, rollbacks,
        repairs, alerts) from journald / audit.log.

DAEMON MODES
  -d, --daemon
        PERSISTENT. Run the background daemon: it samples hardware, evaluates
        policy, actuates mitigation, and publishes a 128-byte Seqlock POD state
        into )" << "/dev/shm" << R"( for the tray and dashboard to read.
        Sleep between windows: --period (default 60.0s).
        This is what the systemd unit runs.

  -s, --status
        One-shot read of the live daemon state from shared memory (no daemon
        round-trip): total/CPU/GPU drain, temperature, battery %, wakeups,
        fan RPM, active mitigations, and the top culprit.

OBSERVATION TUNING
      --period <sec>       Daemon sleep between observation windows (min 1.0,
                           default 60.0). Only meaningful with -d.
      --window <sec>       Length of one observation window (min 0.5,
                           default 5.0). Longer = steadier numbers, slower.
  -i, --interval <sec>     Sampling interval within a window (min 0.5,
                           default 2.0).
  -w, --duration <sec>     Total duration to observe (min 0.5). Overrides the
                           per-mode default (-b 10.0, -X 30.0).
  -c, --count <n>          Fixed number of samples instead of a duration
                           (min 1). Mutually exclusive with -w in practice.
  -n, --top <n>            How many processes to list (min 1, default 15).

DIAGNOSTICS
      --dev-profile        Append a fine-grained subsystem execution-cost
                           breakdown (per-scope latency) to the output.
                           Development aid; it adds measurement overhead.
  -h, --help               Show this help and exit.

POWER PROFILES
  Profiles are selected from the tray icon's context menu or the dashboard,
  never by a CLI flag. The daemon persists the choice and reapplies it.
    Performance    unrestricted CPU ceiling, boost on, all cores, SMU limit
                   raised; no process throttling at all (REF-REQ-104).
    Balanced       default. Unrestricted ceiling, schedutil, SMU limit raised;
                   conservative process mitigation only.
    PowerSaver     low-power platform profile, boost off, 1.7 GHz cap,
                   moderate mitigation.
    UltraEndurance powersave governor, boost off, 1.4 GHz cap, GPU capped,
                   progressive mitigation. Below 45 C the fan is stopped
                   (REF-REQ-118.3).

  On battery, dropping to <= 30% demotes Performance to Balanced once, and
  <= 20% to PowerSaver. AC connection never changes the profile on its own.

ENVIRONMENT
  WATTCURB_ACTUATION_SANDBOX=1   Make every actuation a no-op (safe for
                                 development and benchmarking).
  WATTCURB_QML_DEV_ROOT=<dir>    Dev builds only: load QML from a directory
                                 instead of the compiled-in resources.

EXIT STATUS
  0   Success (including "daemon not running" fallbacks where handled).
  1   Invalid usage or a required resource was unavailable.
  255 QML window failed to load (dashboard/report).

EXAMPLES
  )" << prog << R"(                       # default detail table for one window
  )" << prog << R"( -b -w 30              # 30-second executive briefing
  )" << prog << R"( -R                    # deep battery drain audit
  )" << prog << R"( -d --period 10       # daemon, 10s between windows
  )" << prog << R"( -s                    # instant status from the daemon
  )" << prog << R"( -l -i 1               # live terminal dashboard, 1s refresh

NOTES
  * The CLI is read-only except for -d and -X. Profile changes and per-feature
    toggles live in the tray / dashboard UI.
  * Timings are wall-clock minimums; the kernel's timer coalescing can add a
    few milliseconds by design (Zero-Wakeup principle).
  * See docs/INDEX.md for the requirement behind every behaviour.
)";
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
                const char* bat_status_str = state.battery_state == 1 ? wattcurb::core::l10n::tr(wattcurb::core::l10n::StringId::STATUS_DISCHARGING) :
                                             (state.battery_state == 2 ? wattcurb::core::l10n::tr(wattcurb::core::l10n::StringId::STATUS_AC_PASSTHROUGH) :
                                              wattcurb::core::l10n::tr(wattcurb::core::l10n::StringId::STATUS_AC_CONNECTED));

                std::cout << "\033[1m[" << wattcurb::core::l10n::tr(wattcurb::core::l10n::StringId::CLI_STATUS_HEADER) << "]\033[0m\n"
                          << "  - " << wattcurb::core::l10n::tr(wattcurb::core::l10n::StringId::CLI_TOTAL_DRAIN) << " : "
                          << std::fixed << std::setprecision(2) << (state.system_drain_mw / 1000.0) << " W\n"
                          << "  - " << wattcurb::core::l10n::tr(wattcurb::core::l10n::StringId::CLI_CPU_DRAIN) << "  : "
                          << (state.cpu_drain_mw / 1000.0) << " W (" << state.cpu_temp_c << "°C)\n"
                          << "  - " << wattcurb::core::l10n::tr(wattcurb::core::l10n::StringId::CLI_GPU_DRAIN) << "  : "
                          << (state.gpu_drain_mw / 1000.0) << " W\n"
                          << "  - " << wattcurb::core::l10n::tr(wattcurb::core::l10n::StringId::CLI_BATTERY_LEVEL) << "      : "
                          << static_cast<int>(state.battery_percent) << "% (" << bat_status_str << ")\n"
                          << "  - " << wattcurb::core::l10n::tr(wattcurb::core::l10n::StringId::CLI_WAKEUPS) << "     : "
                          << state.wakeups_per_sec << " wakeups/sec\n"
                          << "  - " << wattcurb::core::l10n::tr(wattcurb::core::l10n::StringId::CLI_COOLING_FAN) << "        : "
                          << state.fan_rpm << " RPM\n"
                          << "  - " << wattcurb::core::l10n::tr(wattcurb::core::l10n::StringId::CLI_ACTIVE_MITIGATIONS) << " : "
                          << state.active_mitigations << " features active\n";
                if (state.culprits[0].pid > 0) {
                    std::cout << "  - " << wattcurb::core::l10n::tr(wattcurb::core::l10n::StringId::CLI_TOP_CULPRIT)
                              << "  : PID " << state.culprits[0].pid << " (" << state.culprits[0].comm
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

int query_daemon_history() {
    int fd = ::open(wattcurb::ipc::HISTORY_SHM_PATH, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        std::cerr << "[!] Error: History shared memory (/dev/shm/wattcurb_history.shm) not accessible. Is wattcurb running?\n";
        return 1;
    }
    void* ptr = ::mmap(nullptr, sizeof(wattcurb::ipc::HistoryRingBufferShm), PROT_READ, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        ::close(fd);
        std::cerr << "[!] Error: Failed to mmap history buffer.\n";
        return 1;
    }

    auto* shm = static_cast<const wattcurb::ipc::HistoryRingBufferShm*>(ptr);
    static std::vector<wattcurb::ipc::HistoryPoint> entries(wattcurb::ipc::HistoryRingBufferShm::CAPACITY);
    uint32_t count = 0;

    if (!shm->read_snapshot(entries.data(), wattcurb::ipc::HistoryRingBufferShm::CAPACITY, count) || count == 0) {
        std::cout << "[*] No history entries recorded yet. Waiting for observation cycles...\n";
        ::munmap(ptr, sizeof(wattcurb::ipc::HistoryRingBufferShm));
        ::close(fd);
        return 0;
    }

    std::cout << "\033[1m[WattCurb In-Memory Telemetry History (Last " << std::max(1u, count * 10 / 60) << " min / "
              << std::fixed << std::setprecision(1) << (static_cast<double>(count * 10) / 3600.0) << " hours, REF-REQ-070)]\033[0m\n"
              << std::left << std::setw(10) << "TIME"
              << std::right << std::setw(12) << "SYSTEM(W)"
              << std::setw(10) << "CPU(W)"
              << std::setw(10) << "GPU(W)"
              << std::setw(10) << "TEMP(°C)"
              << std::setw(12) << "CLOCK(MHz)"
              << std::setw(10) << "BATTERY"
              << std::setw(16) << "PROFILE"
              << "\n"
              << std::string(80, '-') << "\n";

    uint32_t start_idx = (count > 20) ? (count - 20) : 0;
    const char* profile_names[] = {"Performance", "Balanced", "PowerSaver", "UltraEndurance"};

    for (uint32_t i = start_idx; i < count; ++i) {
        const auto& pt = entries[i];
        std::time_t t = static_cast<std::time_t>(pt.timestamp_sec);
        struct std::tm tm_buf{};
        ::localtime_r(&t, &tm_buf);
        char time_str[16];
        std::snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d",
                      tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);

        const char* p_name = (pt.power_profile_mode <= 3) ? profile_names[pt.power_profile_mode] : "Unknown";

        std::cout << std::left << std::setw(10) << time_str
                  << std::right << std::fixed << std::setprecision(2)
                  << std::setw(12) << (pt.total_system_mw / 1000.0)
                  << std::setw(10) << (pt.cpu_package_mw / 1000.0)
                  << std::setw(10) << (pt.gpu_mw / 1000.0)
                  << std::setw(10) << pt.cpu_temp_c
                  << std::setw(12) << pt.cpu_freq_mhz
                  << std::setw(9) << static_cast<int>(pt.battery_percent) << "%"
                  << std::setw(16) << p_name
                  << "\n";
    }

    ::munmap(ptr, sizeof(wattcurb::ipc::HistoryRingBufferShm));
    ::close(fd);
    return 0;
}

int query_daemon_logs() {
    std::cout << "\033[1m[WattCurb Event-Driven Audit Journal (REF-REQ-059)]\033[0m\n";
    int ret = ::system("journalctl -u wattcurb.service -n 25 --no-pager 2>/dev/null");
    if (ret != 0) {
        int log_fd = ::open("/var/log/wattcurb/audit.log", O_RDONLY | O_CLOEXEC);
        if (log_fd >= 0) {
            char buf[4096];
            ssize_t n;
            while ((n = ::read(log_fd, buf, sizeof(buf))) > 0) {
                std::cout.write(buf, n);
            }
            ::close(log_fd);
        } else {
            std::cout << "[*] No local audit log found. Daemon may be logging exclusively to journald.\n";
        }
    }
    return 0;
}

int main(int argc, char* argv[]) {
    wattcurb::core::l10n::init_from_system();
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
    bool history_query = false;
    bool logs_query = false;
    bool live_mode = false;
    bool dev_profile = false;
    bool feature_catalog_mode = false;
    bool extreme_profile_mode = false;
    bool battery_report_mode = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            return 0;
        } else if (arg == "-F" || arg == "--features") {
            feature_catalog_mode = true;
        } else if (arg == "-R" || arg == "--battery-report") {
            battery_report_mode = true;
        } else if (arg == "-X" || arg == "--extreme-profile") {
            extreme_profile_mode = true;
            if (duration_sec == 0.0) duration_sec = 30.0;
        } else if (arg == "-d" || arg == "--daemon") {
            daemon_mode = true;
        } else if (arg == "-s" || arg == "--status") {
            status_query = true;
        } else if (arg == "-H" || arg == "--history") {
            history_query = true;
        } else if (arg == "-L" || arg == "--logs") {
            logs_query = true;
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

    if (history_query) {
        return query_daemon_history();
    }

    if (logs_query) {
        return query_daemon_logs();
    }

    if (battery_report_mode) {
        std::vector<wattcurb::ProcessAttributedPower> top_procs;
        std::string resp;
        if (wattcurb::core::SingletonLock::query_daemon("FULL_TELEMETRY\n", resp, "wattcurb.lock", 300)) {
            size_t proc_pos = resp.find("\"processes\":");
            if (proc_pos != std::string::npos) {
                size_t p = proc_pos;
                while ((p = resp.find("\"comm\": \"", p)) != std::string::npos) {
                    p += 9;
                    size_t end_comm = resp.find("\"", p);
                    if (end_comm == std::string::npos) break;
                    std::string comm = resp.substr(p, end_comm - p);

                    double total_w = 0.5;
                    size_t w_pos = resp.find("\"total_w\":", p);
                    if (w_pos != std::string::npos && w_pos < p + 300) {
                        total_w = std::strtod(resp.c_str() + w_pos + 10, nullptr);
                    }

                    int pid = 0;
                    size_t pid_pos = resp.find("\"pid\":", p - 60);
                    if (pid_pos != std::string::npos && pid_pos < p) {
                        pid = std::atoi(resp.c_str() + pid_pos + 6);
                    }

                    std::string domain = "CPU Compute";
                    size_t d_pos = resp.find("\"domain\": \"", p);
                    if (d_pos != std::string::npos && d_pos < p + 500) {
                        d_pos += 11;
                        size_t end_d = resp.find("\"", d_pos);
                        if (end_d != std::string::npos) domain = resp.substr(d_pos, end_d - d_pos);
                    }

                    std::string mech = "Active Load";
                    size_t m_pos = resp.find("\"mechanism\": \"", p);
                    if (m_pos != std::string::npos && m_pos < p + 600) {
                        m_pos += 14;
                        size_t end_m = resp.find("\"", m_pos);
                        if (end_m != std::string::npos) mech = resp.substr(m_pos, end_m - m_pos);
                    }

                    wattcurb::ProcessAttributedPower pap{};
                    pap.pid = pid;
                    pap.comm = comm.c_str();
                    pap.total_attributed_watts = total_w;
                    pap.primary_hw_domain = domain.c_str();
                    pap.hardware_mechanism = mech.c_str();
                    top_procs.push_back(pap);
                    if (top_procs.size() >= 12) break;
                }
            }
        }
        auto report = wattcurb::report::BatteryHistoryAnalyzer::analyze_shm(top_procs);
        std::cout << report.to_markdown() << "\n";
        return 0;
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
