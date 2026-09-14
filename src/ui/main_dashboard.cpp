#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include <QFileInfo>
#include <iostream>
#include "ui/dashboard_backend.hpp"

int main(int argc, char* argv[]) {
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
