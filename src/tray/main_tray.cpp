#include "tray/tray_client.hpp"
#include <csignal>
#include <cstdio>
#include <unistd.h>

namespace {
wattcurb::tray::TrayClient* g_tray_client = nullptr;

void signal_handler(int sig) noexcept {
    if (g_tray_client) {
        g_tray_client->stop();
    }
}
} // anonymous namespace

int main(int argc, char* argv[]) {
    // Implements REF-REQ-035 & REF-ARCH-025:
    // Standalone Ultra-Low-Overhead SNI Desktop Tray Client for WattCurb

    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGHUP, &sa, nullptr);

    wattcurb::tray::TrayClient client;
    g_tray_client = &client;

    if (!client.initialize()) {
        std::fprintf(stderr, "[WattCurb-Tray] Failed to register StatusNotifierItem over session D-Bus.\n");
        return 1;
    }

    int rc = client.run();
    g_tray_client = nullptr;
    return rc;
}
