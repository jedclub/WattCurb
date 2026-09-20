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
        std::cout << "[WattCurb-Dashboard] Running PGO benchmark training workload...\n";
        qputenv("QT_QPA_PLATFORM", "offscreen");
        int fake_argc = 1;
        char fake_name[] = "wattcurb-dashboard";
        char* fake_argv[] = { fake_name, nullptr };
        QGuiApplication app(fake_argc, fake_argv);

        wattcurb::ui::DashboardBackend backend;

        std::string sample_json = R"({
  "system_watts": 14.85,
  "battery_pct": 72,
  "battery_state": 1,
  "battery_voltage_v": 11.82,
  "battery_current_a": 1.25,
  "battery_health_pct": 94,
  "battery_cycles": 108,
  "time_to_empty_min": 185,
  "cpu_core_w": 3.50,
  "cpu_uncore_w": 1.10,
  "cpu_dram_w": 1.20,
  "cpu_freq_mhz": 2200,
  "cpu_governor": "schedutil",
  "cstate_c0": 4.5,
  "cstate_c1": 12.0,
  "cstate_c2": 18.5,
  "cstate_c3": 65.0,
  "gpu_load": 15,
  "display_w": 2.10,
  "display_brightness": 60.0,
  "nvme_w": 0.95,
  "disk_read_mb_s": 0.5,
  "disk_write_mb_s": 1.2,
  "pmu_ipc": 1.62,
  "pmu_instructions": 48000000,
  "pmu_cycles": 29000000,
  "pmu_llc_misses": 1500,
  "pmu_branch_misses": 3800,
  "pmu_ewr": 7.8,
  "aspm_policy": "powersave",
  "processes": [
    {"pid": 1001, "comm": "kwin_wayland", "uid": 1000, "total_w": 3.20, "cpu_w": 2.10, "gpu_w": 1.10, "rss_mb": 150.0, "pss_mb": 80.0, "wakeups_sec": 120, "io_rate_mb_s": 0.1, "is_immune": true, "safety_tier": "Immune", "action": "None"},
    {"pid": 1002, "comm": "firefox", "uid": 1000, "total_w": 2.80, "cpu_w": 2.00, "gpu_w": 0.80, "rss_mb": 450.0, "pss_mb": 320.0, "wakeups_sec": 85, "io_rate_mb_s": 0.5, "is_immune": false, "safety_tier": "InteractiveForeground", "action": "None"},
    {"pid": 1003, "comm": "pipewire", "uid": 1000, "total_w": 0.45, "cpu_w": 0.45, "gpu_w": 0.00, "rss_mb": 25.0, "pss_mb": 15.0, "wakeups_sec": 40, "io_rate_mb_s": 0.0, "is_immune": true, "safety_tier": "AudioCritical", "action": "None"},
    {"pid": 1004, "comm": "plasmashell", "uid": 1000, "total_w": 1.10, "cpu_w": 0.80, "gpu_w": 0.30, "rss_mb": 210.0, "pss_mb": 130.0, "wakeups_sec": 60, "io_rate_mb_s": 0.2, "is_immune": false, "safety_tier": "InteractiveForeground", "action": "None"},
    {"pid": 1005, "comm": "baloo_file", "uid": 1000, "total_w": 1.95, "cpu_w": 1.80, "gpu_w": 0.15, "rss_mb": 180.0, "pss_mb": 110.0, "wakeups_sec": 95, "io_rate_mb_s": 4.5, "is_immune": false, "safety_tier": "GreedyBackground", "action": "Throttle"}
  ]
})";

        for (int i = 0; i < 2000; ++i) {
            backend.ingestTelemetryJson(sample_json);
            backend.runPollIteration();
            [[maybe_unused]] auto ps = backend.processPowerShares();
            [[maybe_unused]] auto ds = backend.devicePowerShares();
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
