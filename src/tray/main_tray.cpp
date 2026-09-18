#include "tray/tray_client.hpp"
#include "core/singleton_lock.hpp"
#include "core/scoped_profiler.hpp"
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
    // Implements REF-REQ-035, REF-ARCH-025, REF-REQ-072:
    // Standalone Ultra-Low-Overhead SNI Desktop Tray Client for WattCurb with Fine-Grained Profiler

    bool profile_mode = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--profile") == 0 || std::strcmp(argv[i], "-p") == 0) {
            profile_mode = true;
        }
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
