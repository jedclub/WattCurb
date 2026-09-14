#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <cstdint>
#include "ipc/tray_shared_state.hpp"

namespace wattcurb::ui {

// REF-REQ-036, REF-ARCH-026: C++23 QObject Bridge for KDE Plasma 6 Dashboard
class DashboardBackend : public QObject {
    Q_OBJECT

    // High-Level Electrical Telemetry
    Q_PROPERTY(double systemDrainWatts READ systemDrainWatts NOTIFY telemetryChanged)
    Q_PROPERTY(int batteryPercent READ batteryPercent NOTIFY telemetryChanged)
    Q_PROPERTY(int batteryHealth READ batteryHealth NOTIFY telemetryChanged)
    Q_PROPERTY(int batteryState READ batteryState NOTIFY telemetryChanged)
    Q_PROPERTY(QString batteryStateString READ batteryStateString NOTIFY telemetryChanged)
    Q_PROPERTY(int timeToEmptyMin READ timeToEmptyMin NOTIFY telemetryChanged)
    Q_PROPERTY(QString timeToEmptyString READ timeToEmptyString NOTIFY telemetryChanged)

    // Domain Breakdowns
    Q_PROPERTY(double cpuDrainWatts READ cpuDrainWatts NOTIFY telemetryChanged)
    Q_PROPERTY(double gpuDrainWatts READ gpuDrainWatts NOTIFY telemetryChanged)
    Q_PROPERTY(double displayDrainWatts READ displayDrainWatts NOTIFY telemetryChanged)
    Q_PROPERTY(double nvmeDrainWatts READ nvmeDrainWatts NOTIFY telemetryChanged)
    Q_PROPERTY(double otherDrainWatts READ otherDrainWatts NOTIFY telemetryChanged)

    // Hardware Telemetry
    Q_PROPERTY(int cpuTempC READ cpuTempC NOTIFY telemetryChanged)
    Q_PROPERTY(int fanRpm READ fanRpm NOTIFY telemetryChanged)
    Q_PROPERTY(int cstateC3Percent READ cstateC3Percent NOTIFY telemetryChanged)
    Q_PROPERTY(int wakeupsPerSec READ wakeupsPerSec NOTIFY telemetryChanged)
    Q_PROPERTY(int activeMitigations READ activeMitigations NOTIFY telemetryChanged)
    Q_PROPERTY(int displayBrightnessPct READ displayBrightnessPct NOTIFY telemetryChanged)

    // Power Profile
    Q_PROPERTY(int powerProfileMode READ powerProfileMode NOTIFY profileChanged)
    Q_PROPERTY(QString powerProfileName READ powerProfileName NOTIFY profileChanged)

    // Process Culprits
    Q_PROPERTY(QString top1Comm READ top1Comm NOTIFY telemetryChanged)
    Q_PROPERTY(int top1Pid READ top1Pid NOTIFY telemetryChanged)
    Q_PROPERTY(int top1DrainMw READ top1DrainMw NOTIFY telemetryChanged)
    Q_PROPERTY(int top1Tier READ top1Tier NOTIFY telemetryChanged)

    Q_PROPERTY(QString top2Comm READ top2Comm NOTIFY telemetryChanged)
    Q_PROPERTY(int top2Pid READ top2Pid NOTIFY telemetryChanged)
    Q_PROPERTY(int top2DrainMw READ top2DrainMw NOTIFY telemetryChanged)
    Q_PROPERTY(int top2Tier READ top2Tier NOTIFY telemetryChanged)

    // Status / Metadata
    Q_PROPERTY(bool isRescanning READ isRescanning NOTIFY rescanStatusChanged)
    Q_PROPERTY(QString lastUpdateTime READ lastUpdateTime NOTIFY telemetryChanged)

public:
    explicit DashboardBackend(QObject* parent = nullptr);
    ~DashboardBackend() override;

    // Getters
    double systemDrainWatts() const noexcept;
    int batteryPercent() const noexcept;
    int batteryHealth() const noexcept;
    int batteryState() const noexcept;
    QString batteryStateString() const;
    int timeToEmptyMin() const noexcept;
    QString timeToEmptyString() const;

    double cpuDrainWatts() const noexcept;
    double gpuDrainWatts() const noexcept;
    double displayDrainWatts() const noexcept;
    double nvmeDrainWatts() const noexcept;
    double otherDrainWatts() const noexcept;

    int cpuTempC() const noexcept;
    int fanRpm() const noexcept;
    int cstateC3Percent() const noexcept;
    int wakeupsPerSec() const noexcept;
    int activeMitigations() const noexcept;
    int displayBrightnessPct() const noexcept;

    int powerProfileMode() const noexcept;
    QString powerProfileName() const;

    QString top1Comm() const;
    int top1Pid() const noexcept;
    int top1DrainMw() const noexcept;
    int top1Tier() const noexcept;

    QString top2Comm() const;
    int top2Pid() const noexcept;
    int top2DrainMw() const noexcept;
    int top2Tier() const noexcept;

    bool isRescanning() const noexcept { return is_rescanning_; }
    QString lastUpdateTime() const { return last_update_time_; }

    // Q_INVOKABLE Actions for QML UI
    Q_INVOKABLE void setProfile(int mode);
    Q_INVOKABLE void triggerRescan();
    Q_INVOKABLE void openSystemMonitor();
    Q_INVOKABLE void refreshNow();

signals:
    void telemetryChanged();
    void profileChanged();
    void rescanStatusChanged();

private slots:
    void onPollTimer();

private:
    void mapSharedMemory() noexcept;
    void unmapSharedMemory() noexcept;
    void sendDaemonCommand(const char* cmd) noexcept;
    void updateAuxiliaryTelemetry() noexcept;

    int shm_fd_{-1};
    const ipc::WattCurbSharedState* shm_state_{nullptr};
    ipc::WattCurbSharedState latest_state_{};
    int local_override_mode_{-1};

    QTimer* poll_timer_{nullptr};
    bool is_rescanning_{false};
    QString last_update_time_{"Just now"};

    int display_brightness_pct_{50};
    double display_drain_w_{1.8};
    double nvme_drain_w_{0.8};
};

} // namespace wattcurb::ui
