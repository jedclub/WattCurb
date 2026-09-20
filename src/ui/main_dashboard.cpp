#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include <QFileInfo>
#include <iostream>
#include "ui/dashboard_backend.hpp"
#include "core/singleton_lock.hpp"

int main(int argc, char* argv[]) {
    bool benchmark_mode = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--benchmark") == 0 || std::strcmp(argv[i], "-B") == 0) {
            benchmark_mode = true;
        }
    }

    // REF-REQ-075 & REF-ARCH-052: Headless benchmark mode for PGO profile generation
    if (benchmark_mode) {
        std::cout << "[WattCurb-Dashboard] Running multi-faceted PGO benchmark training workload (5,000 iterations)...\n";
        qputenv("QT_QPA_PLATFORM", "offscreen");
        int fake_argc = 1;
        char fake_name[] = "wattcurb-dashboard";
        char* fake_argv[] = { fake_name, nullptr };
        QGuiApplication app(fake_argc, fake_argv);

        wattcurb::ui::DashboardBackend backend;

        // Multi-Scale Payload 1: Light laptop (5 processes)
        const std::string light_json = R"({
  "system_watts": 8.50, "battery_pct": 85, "battery_state": 1, "battery_voltage_v": 12.10, "battery_current_a": 0.70,
  "battery_health_pct": 95, "battery_cycles": 80, "time_to_empty_min": 320, "cpu_core_w": 1.50, "cpu_uncore_w": 0.80,
  "cpu_dram_w": 0.90, "cpu_freq_mhz": 1400, "cpu_governor": "powersave", "cstate_c0": 2.0, "cstate_c1": 5.0,
  "cstate_c2": 10.0, "cstate_c3": 83.0, "gpu_load": 2, "display_w": 1.20, "display_brightness": 40.0, "nvme_w": 0.30,
  "disk_read_mb_s": 0.1, "disk_write_mb_s": 0.1, "pmu_ipc": 0.95, "pmu_instructions": 12000000, "pmu_cycles": 12600000,
  "pmu_llc_misses": 400, "pmu_branch_misses": 1100, "pmu_ewr": 4.2, "aspm_policy": "powersave",
  "processes": [
    {"pid": 1001, "comm": "kwin_wayland", "uid": 1000, "total_w": 0.80, "cpu_w": 0.50, "gpu_w": 0.30, "rss_mb": 50.0, "pss_mb": 30.0, "wakeups_sec": 45, "io_rate_mb_s": 0.0, "is_immune": true, "safety_tier": "Immune", "action": "None"},
    {"pid": 1002, "comm": "systemd", "uid": 0, "total_w": 0.10, "cpu_w": 0.10, "gpu_w": 0.0, "rss_mb": 12.0, "pss_mb": 8.0, "wakeups_sec": 5, "io_rate_mb_s": 0.0, "is_immune": true, "safety_tier": "Immune", "action": "None"},
    {"pid": 1003, "comm": "pipewire", "uid": 1000, "total_w": 0.25, "cpu_w": 0.25, "gpu_w": 0.0, "rss_mb": 20.0, "pss_mb": 12.0, "wakeups_sec": 30, "io_rate_mb_s": 0.0, "is_immune": true, "safety_tier": "AudioCritical", "action": "None"},
    {"pid": 1004, "comm": "kitty", "uid": 1000, "total_w": 0.35, "cpu_w": 0.20, "gpu_w": 0.15, "rss_mb": 35.0, "pss_mb": 22.0, "wakeups_sec": 20, "io_rate_mb_s": 0.0, "is_immune": false, "safety_tier": "InteractiveForeground", "action": "None"},
    {"pid": 1005, "comm": "wattcurb", "uid": 0, "total_w": 0.05, "cpu_w": 0.05, "gpu_w": 0.0, "rss_mb": 4.0, "pss_mb": 3.0, "wakeups_sec": 2, "io_rate_mb_s": 0.0, "is_immune": true, "safety_tier": "Immune", "action": "None"}
  ]
})";

        // Multi-Scale Payload 2: Standard typical desktop (25 processes)
        const std::string standard_json = R"({
  "system_watts": 16.50, "battery_pct": 55, "battery_state": 1, "battery_voltage_v": 11.40, "battery_current_a": 1.45,
  "battery_health_pct": 94, "battery_cycles": 108, "time_to_empty_min": 140, "cpu_core_w": 4.20, "cpu_uncore_w": 1.20,
  "cpu_dram_w": 1.50, "cpu_freq_mhz": 2400, "cpu_governor": "schedutil", "cstate_c0": 6.5, "cstate_c1": 10.0,
  "cstate_c2": 15.5, "cstate_c3": 68.0, "gpu_load": 12, "display_w": 2.30, "display_brightness": 65.0, "nvme_w": 0.85,
  "disk_read_mb_s": 0.8, "disk_write_mb_s": 2.1, "pmu_ipc": 1.45, "pmu_instructions": 36000000, "pmu_cycles": 24800000,
  "pmu_llc_misses": 1200, "pmu_branch_misses": 2900, "pmu_ewr": 6.5, "aspm_policy": "powersave",
  "processes": [
    {"pid": 1001, "comm": "kwin_wayland", "uid": 1000, "total_w": 2.80, "cpu_w": 1.80, "gpu_w": 1.00, "rss_mb": 140.0, "pss_mb": 75.0, "wakeups_sec": 110, "io_rate_mb_s": 0.1, "is_immune": true, "safety_tier": "Immune", "action": "None"},
    {"pid": 1002, "comm": "firefox", "uid": 1000, "total_w": 3.40, "cpu_w": 2.50, "gpu_w": 0.90, "rss_mb": 520.0, "pss_mb": 380.0, "wakeups_sec": 90, "io_rate_mb_s": 1.2, "is_immune": false, "safety_tier": "InteractiveForeground", "action": "None"},
    {"pid": 1003, "comm": "pipewire", "uid": 1000, "total_w": 0.40, "cpu_w": 0.40, "gpu_w": 0.00, "rss_mb": 25.0, "pss_mb": 15.0, "wakeups_sec": 45, "io_rate_mb_s": 0.0, "is_immune": true, "safety_tier": "AudioCritical", "action": "None"},
    {"pid": 1004, "comm": "plasmashell", "uid": 1000, "total_w": 1.30, "cpu_w": 0.90, "gpu_w": 0.40, "rss_mb": 220.0, "pss_mb": 135.0, "wakeups_sec": 65, "io_rate_mb_s": 0.3, "is_immune": false, "safety_tier": "InteractiveForeground", "action": "None"},
    {"pid": 1005, "comm": "baloo_file", "uid": 1000, "total_w": 1.80, "cpu_w": 1.65, "gpu_w": 0.15, "rss_mb": 170.0, "pss_mb": 105.0, "wakeups_sec": 80, "io_rate_mb_s": 5.2, "is_immune": false, "safety_tier": "GreedyBackground", "action": "Throttle"},
    {"pid": 1006, "comm": "code", "uid": 1000, "total_w": 1.20, "cpu_w": 0.90, "gpu_w": 0.30, "rss_mb": 310.0, "pss_mb": 210.0, "wakeups_sec": 40, "io_rate_mb_s": 0.5, "is_immune": false, "safety_tier": "InteractiveForeground", "action": "None"},
    {"pid": 1007, "comm": "node", "uid": 1000, "total_w": 0.95, "cpu_w": 0.80, "gpu_w": 0.15, "rss_mb": 160.0, "pss_mb": 115.0, "wakeups_sec": 35, "io_rate_mb_s": 0.2, "is_immune": false, "safety_tier": "BackgroundService", "action": "None"},
    {"pid": 1008, "comm": "rustc", "uid": 1000, "total_w": 2.10, "cpu_w": 2.10, "gpu_w": 0.00, "rss_mb": 420.0, "pss_mb": 350.0, "wakeups_sec": 150, "io_rate_mb_s": 8.5, "is_immune": false, "safety_tier": "Runaway", "action": "AntiStarvationCap"},
    {"pid": 1009, "comm": "kitty", "uid": 1000, "total_w": 0.30, "cpu_w": 0.15, "gpu_w": 0.15, "rss_mb": 40.0, "pss_mb": 25.0, "wakeups_sec": 18, "io_rate_mb_s": 0.0, "is_immune": false, "safety_tier": "InteractiveForeground", "action": "None"},
    {"pid": 1010, "comm": "ksystemstats", "uid": 1000, "total_w": 0.20, "cpu_w": 0.20, "gpu_w": 0.00, "rss_mb": 15.0, "pss_mb": 10.0, "wakeups_sec": 12, "io_rate_mb_s": 0.0, "is_immune": false, "safety_tier": "BackgroundService", "action": "None"}
  ]
})";

        // Multi-Scale Payload 3: Heavy workstation under active charging (35W)
        const std::string heavy_json = R"({
  "system_watts": 35.20, "battery_pct": 98, "battery_state": 0, "battery_voltage_v": 12.80, "battery_current_a": 2.75,
  "battery_health_pct": 94, "battery_cycles": 108, "time_to_empty_min": 0, "cpu_core_w": 18.50, "cpu_uncore_w": 2.40,
  "cpu_dram_w": 3.10, "cpu_freq_mhz": 3800, "cpu_governor": "performance", "cstate_c0": 35.0, "cstate_c1": 20.0,
  "cstate_c2": 20.0, "cstate_c3": 25.0, "gpu_load": 85, "display_w": 3.80, "display_brightness": 100.0, "nvme_w": 1.95,
  "disk_read_mb_s": 15.5, "disk_write_mb_s": 42.0, "pmu_ipc": 1.85, "pmu_instructions": 85000000, "pmu_cycles": 46000000,
  "pmu_llc_misses": 4500, "pmu_branch_misses": 7800, "pmu_ewr": 9.2, "aspm_policy": "performance",
  "processes": [
    {"pid": 2001, "comm": "gcc", "uid": 1000, "total_w": 8.50, "cpu_w": 8.50, "gpu_w": 0.0, "rss_mb": 850.0, "pss_mb": 720.0, "wakeups_sec": 450, "io_rate_mb_s": 25.0, "is_immune": false, "safety_tier": "Runaway", "action": "AntiStarvationCap"},
    {"pid": 2002, "comm": "ninja", "uid": 1000, "total_w": 4.20, "cpu_w": 4.20, "gpu_w": 0.0, "rss_mb": 120.0, "pss_mb": 95.0, "wakeups_sec": 300, "io_rate_mb_s": 15.0, "is_immune": false, "safety_tier": "Runaway", "action": "Throttle"},
    {"pid": 2003, "comm": "kwin_wayland", "uid": 1000, "total_w": 3.50, "cpu_w": 2.00, "gpu_w": 1.50, "rss_mb": 180.0, "pss_mb": 90.0, "wakeups_sec": 180, "io_rate_mb_s": 0.2, "is_immune": true, "safety_tier": "Immune", "action": "None"},
    {"pid": 2004, "comm": "firefox", "uid": 1000, "total_w": 5.10, "cpu_w": 3.20, "gpu_w": 1.90, "rss_mb": 980.0, "pss_mb": 750.0, "wakeups_sec": 220, "io_rate_mb_s": 3.0, "is_immune": false, "safety_tier": "InteractiveForeground", "action": "None"},
    {"pid": 2005, "comm": "steam", "uid": 1000, "total_w": 2.20, "cpu_w": 1.20, "gpu_w": 1.00, "rss_mb": 340.0, "pss_mb": 250.0, "wakeups_sec": 85, "io_rate_mb_s": 1.0, "is_immune": false, "safety_tier": "InteractiveForeground", "action": "None"}
  ]
})";

        for (int i = 0; i < 5000; ++i) {
            // Alternating workload scales
            if ((i % 3) == 0) {
                backend.ingestTelemetryJson(light_json);
            } else if ((i % 3) == 1) {
                backend.ingestTelemetryJson(standard_json);
            } else {
                backend.ingestTelemetryJson(heavy_json);
            }

            // Periodic edge-case JSON ingestion (empty, corrupted)
            if ((i % 250) == 0) {
                backend.ingestTelemetryJson("{}");
                backend.ingestTelemetryJson("{invalid json content}");
            }

            // Periodic state polling
            backend.runPollIteration();

            // Periodic full getter sweep (exercising Qt bridge accessors)
            if ((i % 5) == 0) {
                [[maybe_unused]] auto sys_w = backend.systemDrainWatts();
                [[maybe_unused]] auto cpu_w = backend.cpuDrainWatts();
                [[maybe_unused]] auto gpu_w = backend.gpuDrainWatts();
                [[maybe_unused]] auto bat_pct = backend.batteryPercent();
                [[maybe_unused]] auto c3_pct = backend.cstateC3Percent();
                [[maybe_unused]] auto ps = backend.processPowerShares();
                [[maybe_unused]] auto ds = backend.devicePowerShares();
                [[maybe_unused]] auto pl = backend.processList();
                [[maybe_unused]] auto tw = backend.totalDeviceWatts();
                [[maybe_unused]] auto pw = backend.totalProcessWatts();
                [[maybe_unused]] auto pmu_ipc = backend.pmuIpc();
                [[maybe_unused]] auto lut = backend.lastUpdateTime();
            }

            // Periodic user action simulation
            if ((i % 50) == 0) {
                backend.setProfile(i % 4);
                backend.refreshNow();
            }
        }

        std::cout << "[WattCurb-Dashboard] Benchmark training complete. Profile counters flushed.\n";
        return 0;
    }

    // Enforce singleton dashboard window
    wattcurb::core::SingletonLock dashboard_lock("wattcurb-dashboard.lock");
    if (!dashboard_lock.is_locked()) {
        std::cerr << "[!] WattCurb dashboard is already running. Exiting.\n";
        return 0;
    }

    // Zero-Wakeup GUI setup: Set environment hints for KDE Wayland/X11
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");

    QGuiApplication app(argc, argv);
    app.setApplicationName("wattcurb-dashboard");
    app.setApplicationDisplayName("WattCurb 전력 소비 정밀 분석 매트릭");
    app.setDesktopFileName("wattcurb-dashboard");
    app.setWindowIcon(QIcon::fromTheme("utilities-system-monitor"));

    wattcurb::ui::DashboardBackend backend;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("backend", &backend);

    // Try QRC first, fallback to filesystem
    QUrl qmlUrl("qrc:/qml/DashboardWindow.qml");
    
    // Check if running from dev directory directly
    if (QFileInfo::exists("/home/jedclub/Develop/WattCurb/src/ui/qml/DashboardWindow.qml")) {
        qmlUrl = QUrl::fromLocalFile("/home/jedclub/Develop/WattCurb/src/ui/qml/DashboardWindow.qml");
    }

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [qmlUrl](QObject *obj, const QUrl &objUrl) {
        if (!obj && qmlUrl == objUrl) {
            std::cerr << "[!] Failed to load QML dashboard window!\n";
            QCoreApplication::exit(-1);
        }
    }, Qt::QueuedConnection);

    engine.load(qmlUrl);

    return app.exec();
}
