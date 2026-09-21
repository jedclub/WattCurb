#include "tray/tray_client.hpp"
#include "ipc/tray_shared_state.hpp"
#include "core/singleton_lock.hpp"
#include "core/scoped_profiler.hpp"
#include "core/l10n.hpp"
#include <csignal>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace {
wattcurb::tray::TrayClient* g_tray_client = nullptr;

void signal_handler(int) noexcept {
    if (g_tray_client) {
        g_tray_client->stop();
    }
}

void sigusr1_handler(int) noexcept {
    wattcurb::tray::TrayClient::print_profiler_summary();
}
} // anonymous namespace

int main(int argc, char* argv[]) {
    // Implements REF-REQ-035, REF-ARCH-025, REF-REQ-072, REF-REQ-076:
    // Standalone Ultra-Low-Overhead SNI Desktop Tray Client for WattCurb with L10n Auto-Detection
    wattcurb::core::l10n::init_from_system();

    bool profile_mode = false;
    bool benchmark_mode = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--profile") == 0 || std::strcmp(argv[i], "-p") == 0) {
            profile_mode = true;
        } else if (std::strcmp(argv[i], "--benchmark") == 0 || std::strcmp(argv[i], "-B") == 0) {
            benchmark_mode = true;
        }
    }

    // REF-REQ-075 & REF-ARCH-052: Headless benchmark mode for PGO profile generation
    if (benchmark_mode) {
        std::printf("[WattCurb-Tray] Running multi-faceted PGO benchmark training workload (50,000 iterations)...\n");
        wattcurb::ipc::WattCurbSharedState state{};
        char title[128]{};
        char desc[8192]{};
        char icon[64]{};

        const char* culprits_pool[] = {
            "firefox", "kwin_wayland", "baloo_file", "plasmashell", "pipewire",
            "code", "rustc", "clangd", "kitty", "systemd", "none"
        };
        constexpr size_t pool_size = sizeof(culprits_pool) / sizeof(culprits_pool[0]);

        for (int i = 0; i < 50000; ++i) {
            state.seq_version = static_cast<uint64_t>(i);
            // Scenario A: Discharging under varied load (0..20,000)
            if (i < 20000) {
                state.battery_state = 1; // Discharging
                state.battery_percent = static_cast<uint8_t>(5 + (i % 95));
                state.system_drain_mw = static_cast<uint32_t>(5000 + (i % 25000));
                state.cpu_drain_mw = static_cast<uint32_t>(1500 + (i % 18000));
                state.gpu_drain_mw = static_cast<uint32_t>((i % 5) == 0 ? 8000 : 200);
                state.cpu_temp_c = static_cast<uint8_t>(38 + (i % 55));
                state.fan_rpm = static_cast<uint16_t>((state.cpu_temp_c > 60) ? 3500 : 0);
            }
            // Scenario B: AC Charging under rapid charge (20,000..35,000)
            else if (i < 35000) {
                state.battery_state = 0; // AC Charging
                state.battery_percent = static_cast<uint8_t>(30 + ((i - 20000) % 70));
                state.system_drain_mw = static_cast<uint32_t>(20000 + (i % 45000));
                state.cpu_drain_mw = static_cast<uint32_t>(2000 + (i % 8000));
                state.gpu_drain_mw = 500;
                state.cpu_temp_c = static_cast<uint8_t>(45 + (i % 25));
                state.fan_rpm = 2500;
            }
            // Scenario C: AC Passthrough Full (35,000..50,000)
            else {
                state.battery_state = 2; // Full passthrough
                state.battery_percent = 100;
                state.system_drain_mw = static_cast<uint32_t>(3500 + (i % 12000));
                state.cpu_drain_mw = static_cast<uint32_t>(1200 + (i % 5000));
                state.gpu_drain_mw = 100;
                state.cpu_temp_c = 40;
                state.fan_rpm = 0;
            }

            // Cycle power profiles: Performance, Balanced, SmartSave, UltraSave
            state.power_profile_mode = static_cast<uint8_t>(i % 4);

            // Dynamic culprits
            const char* c0 = culprits_pool[static_cast<size_t>(i) % pool_size];
            const char* c1 = culprits_pool[static_cast<size_t>(i + 3) % pool_size];
            // strncpy with the full buffer size leaves the result unterminated for
            // any name >= sizeof(comm); mirror the production writer in
            // ipc/tray_shared_state.hpp and terminate explicitly.
            std::strncpy(state.culprits[0].comm, c0, sizeof(state.culprits[0].comm) - 1);
            state.culprits[0].comm[sizeof(state.culprits[0].comm) - 1] = '\0';
            state.culprits[0].drain_mw = static_cast<uint32_t>(500 + (i % 4000));
            std::strncpy(state.culprits[1].comm, c1, sizeof(state.culprits[1].comm) - 1);
            state.culprits[1].comm[sizeof(state.culprits[1].comm) - 1] = '\0';
            state.culprits[1].drain_mw = static_cast<uint32_t>(200 + (i % 2000));

            // 1. ToolTip rendering
            wattcurb::tray::TrayClient::render_tooltip(state, title, sizeof(title), desc, sizeof(desc));

            // 2. Burst cache hits (simulate rapid mouse hover in KDE Plasma)
            if ((i % 10) == 0) {
                for (int b = 0; b < 15; ++b) {
                    wattcurb::tray::TrayClient::render_tooltip(state, title, sizeof(title), desc, sizeof(desc));
                }
            }

            // 3. Icon name resolution across all LUT buckets
            wattcurb::tray::TrayClient::resolve_icon_name(state, icon, sizeof(icon));

            // 4. Hover sensor probe simulation
            if ((i % 5) == 0) {
                wattcurb::tray::TrayClient::probe_sensors_for_hover(state);
            }
        }
        std::printf("[WattCurb-Tray] Benchmark training complete. Profile counters flushed.\n");
        return 0;
    }

    // Enforce singleton instance: prevents duplicate tray icons on concurrent autostart & systemd launches
    wattcurb::core::SingletonLock tray_lock("wattcurb-tray.lock");
    if (!tray_lock.is_locked()) {
        std::fprintf(stderr, "[WattCurb-Tray] Another instance is already running. Exiting cleanly.\n");
        return 0;
    }

    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGHUP, &sa, nullptr);
    std::signal(SIGCHLD, SIG_IGN); // Automatically reap forked children

    // Implements REF-REQ-072: SIGUSR1 prints live ScopedProfiler summary on demand
    struct sigaction sa_usr1{};
    sa_usr1.sa_handler = sigusr1_handler;
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa_usr1, nullptr);

    wattcurb::tray::TrayClient client;
    g_tray_client = &client;

    if (!client.initialize()) {
        std::fprintf(stderr, "[WattCurb-Tray] Failed to register StatusNotifierItem over session D-Bus.\n");
        return 1;
    }

    if (profile_mode) {
        std::printf("[WattCurb-Tray] Running in fine-grained profiling mode (REF-REQ-072).\n");
        std::printf("                Send SIGUSR1 ('kill -USR1 %d') to print live hot-path breakdown.\n", ::getpid());
    }

    int rc = client.run();
    g_tray_client = nullptr;

    if (profile_mode) {
        wattcurb::tray::TrayClient::print_profiler_summary();
    }
    return rc;
}
