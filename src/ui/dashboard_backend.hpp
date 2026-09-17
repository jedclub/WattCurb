#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <cstdint>
#include "ipc/tray_shared_state.hpp"

namespace wattcurb::ui {

// REF-REQ-036, REF-REQ-037: High-Density btop-Style Backend Bridge for KDE Plasma 6
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
    Q_PROPERTY(double batteryVoltageV READ batteryVoltageV NOTIFY telemetryChanged)
    Q_PROPERTY(double batteryCurrentA READ batteryCurrentA NOTIFY telemetryChanged)
    Q_PROPERTY(int batteryCycles READ batteryCycles NOTIFY telemetryChanged)

    // CPU Domain Detailed Telemetry
    Q_PROPERTY(double cpuDrainWatts READ cpuDrainWatts NOTIFY telemetryChanged)
    Q_PROPERTY(double cpuCoreWatts READ cpuCoreWatts NOTIFY telemetryChanged)
    Q_PROPERTY(double cpuUncoreWatts READ cpuUncoreWatts NOTIFY telemetryChanged)
    Q_PROPERTY(double cpuDramWatts READ cpuDramWatts NOTIFY telemetryChanged)
    Q_PROPERTY(int cpuTempC READ cpuTempC NOTIFY telemetryChanged)
    Q_PROPERTY(int cpuFreqMhz READ cpuFreqMhz NOTIFY telemetryChanged)
    Q_PROPERTY(double cstateC0Percent READ cstateC0Percent NOTIFY telemetryChanged)
    Q_PROPERTY(double cstateC1Percent READ cstateC1Percent NOTIFY telemetryChanged)
    Q_PROPERTY(double cstateC2Percent READ cstateC2Percent NOTIFY telemetryChanged)
    Q_PROPERTY(double cstateC3Percent READ cstateC3Percent NOTIFY telemetryChanged)

    // GPU & Display Subsystems
    Q_PROPERTY(double gpuDrainWatts READ gpuDrainWatts NOTIFY telemetryChanged)
    Q_PROPERTY(int gpuLoadPercent READ gpuLoadPercent NOTIFY telemetryChanged)
    Q_PROPERTY(double displayDrainWatts READ displayDrainWatts NOTIFY telemetryChanged)
    Q_PROPERTY(int displayBrightnessPct READ displayBrightnessPct NOTIFY telemetryChanged)

    // Storage, Cooling, Wireless
    Q_PROPERTY(double nvmeDrainWatts READ nvmeDrainWatts NOTIFY telemetryChanged)
    Q_PROPERTY(double diskReadMbPerSec READ diskReadMbPerSec NOTIFY telemetryChanged)
    Q_PROPERTY(double diskWriteMbPerSec READ diskWriteMbPerSec NOTIFY telemetryChanged)
    Q_PROPERTY(int fanRpm READ fanRpm NOTIFY telemetryChanged)
    Q_PROPERTY(int wakeupsPerSec READ wakeupsPerSec NOTIFY telemetryChanged)
    Q_PROPERTY(int activeMitigations READ activeMitigations NOTIFY telemetryChanged)

    // Power Profile
    Q_PROPERTY(int powerProfileMode READ powerProfileMode NOTIFY profileChanged)
    Q_PROPERTY(QString powerProfileName READ powerProfileName NOTIFY profileChanged)

    // Deep Hardware & PMU Telemetry (REF-REQ-038)
    Q_PROPERTY(QString cpuGovernor READ cpuGovernor NOTIFY telemetryChanged)
    Q_PROPERTY(double pmuIpc READ pmuIpc NOTIFY telemetryChanged)
    Q_PROPERTY(qlonglong pmuInstructions READ pmuInstructions NOTIFY telemetryChanged)
    Q_PROPERTY(qlonglong pmuCycles READ pmuCycles NOTIFY telemetryChanged)
    Q_PROPERTY(qlonglong pmuLlcMisses READ pmuLlcMisses NOTIFY telemetryChanged)
    Q_PROPERTY(qlonglong pmuBranchMisses READ pmuBranchMisses NOTIFY telemetryChanged)
    Q_PROPERTY(double pmuEwr READ pmuEwr NOTIFY telemetryChanged)
    Q_PROPERTY(QString batteryMfg READ batteryMfg NOTIFY telemetryChanged)
    Q_PROPERTY(QString batteryModel READ batteryModel NOTIFY telemetryChanged)
    Q_PROPERTY(QString batteryTech READ batteryTech NOTIFY telemetryChanged)
    Q_PROPERTY(double batteryDesignWh READ batteryDesignWh NOTIFY telemetryChanged)
    Q_PROPERTY(double batteryFullWh READ batteryFullWh NOTIFY telemetryChanged)
    Q_PROPERTY(double batteryNowWh READ batteryNowWh NOTIFY telemetryChanged)
    Q_PROPERTY(QString aspmPolicy READ aspmPolicy NOTIFY telemetryChanged)

    // Real-Time Hardware Telemetry History Streams (REF-REQ-040)
    Q_PROPERTY(QVariantList systemHistory READ systemHistory NOTIFY historyChanged)
    Q_PROPERTY(QVariantList cpuHistory READ cpuHistory NOTIFY historyChanged)
    Q_PROPERTY(QVariantList gpuHistory READ gpuHistory NOTIFY historyChanged)
    Q_PROPERTY(double peakSystemWatts READ peakSystemWatts NOTIFY historyChanged)

    // btop-Style Detailed Process List (Top 12 with Exhaustive Hover Telemetry)
    Q_PROPERTY(QVariantList processList READ processList NOTIFY processListChanged)

    // Circular Power Share Breakdown (REF-REQ-060, REF-ARCH-036)
    Q_PROPERTY(QVariantList devicePowerShares READ devicePowerShares NOTIFY powerSharesChanged)
    Q_PROPERTY(QVariantList processPowerShares READ processPowerShares NOTIFY powerSharesChanged)
    Q_PROPERTY(double totalDeviceWatts READ totalDeviceWatts NOTIFY powerSharesChanged)
    Q_PROPERTY(double totalProcessWatts READ totalProcessWatts NOTIFY powerSharesChanged)

    // Status / Metadata
    Q_PROPERTY(bool isRescanning READ isRescanning NOTIFY rescanStatusChanged)
    Q_PROPERTY(QString lastUpdateTime READ lastUpdateTime NOTIFY telemetryChanged)

public:
    explicit DashboardBackend(QObject* parent = nullptr);
    ~DashboardBackend() override;

    QVariantList systemHistory() const { return system_history_; }
    QVariantList cpuHistory() const { return cpu_history_; }
    QVariantList gpuHistory() const { return gpu_history_; }
    double peakSystemWatts() const noexcept { return peak_system_w_; }

    // Getters
    double systemDrainWatts() const noexcept;
    int batteryPercent() const noexcept;
    int batteryHealth() const noexcept;
    int batteryState() const noexcept;
    QString batteryStateString() const;
    int timeToEmptyMin() const noexcept;
    QString timeToEmptyString() const;
    double batteryVoltageV() const noexcept { return battery_voltage_v_; }
    double batteryCurrentA() const noexcept { return battery_current_a_; }
    int batteryCycles() const noexcept { return battery_cycles_; }

    double cpuDrainWatts() const noexcept;
    double cpuCoreWatts() const noexcept { return cpu_core_w_; }
    double cpuUncoreWatts() const noexcept { return cpu_uncore_w_; }
    double cpuDramWatts() const noexcept { return cpu_dram_w_; }
    int cpuTempC() const noexcept;
    int cpuFreqMhz() const noexcept { return cpu_freq_mhz_; }
    double cstateC0Percent() const noexcept { return cstate_c0_; }
    double cstateC1Percent() const noexcept { return cstate_c1_; }
    double cstateC2Percent() const noexcept { return cstate_c2_; }
    double cstateC3Percent() const noexcept;

    double gpuDrainWatts() const noexcept;
    int gpuLoadPercent() const noexcept { return gpu_load_pct_; }
    double displayDrainWatts() const noexcept;
    int displayBrightnessPct() const noexcept;

    double nvmeDrainWatts() const noexcept;
    double diskReadMbPerSec() const noexcept { return disk_read_mb_s_; }
    double diskWriteMbPerSec() const noexcept { return disk_write_mb_s_; }
    int fanRpm() const noexcept;
    int wakeupsPerSec() const noexcept;
    int activeMitigations() const noexcept;

    int powerProfileMode() const noexcept;
    QString powerProfileName() const;

    QString cpuGovernor() const { return cpu_governor_; }
    double pmuIpc() const noexcept { return pmu_ipc_; }
    qlonglong pmuInstructions() const noexcept { return pmu_instructions_; }
    qlonglong pmuCycles() const noexcept { return pmu_cycles_; }
    qlonglong pmuLlcMisses() const noexcept { return pmu_llc_misses_; }
    qlonglong pmuBranchMisses() const noexcept { return pmu_branch_misses_; }
    double pmuEwr() const noexcept { return pmu_ewr_; }
    QString batteryMfg() const { return battery_mfg_; }
    QString batteryModel() const { return battery_model_; }
    QString batteryTech() const { return battery_tech_; }
    double batteryDesignWh() const noexcept { return battery_design_wh_; }
    double batteryFullWh() const noexcept { return battery_full_wh_; }
    double batteryNowWh() const noexcept { return battery_now_wh_; }
    QString aspmPolicy() const { return aspm_policy_; }

    QVariantList processList() const { return process_list_; }
    QVariantList devicePowerShares() const { return device_power_shares_; }
    QVariantList processPowerShares() const { return process_power_shares_; }
    double totalDeviceWatts() const noexcept { return total_device_w_; }
    double totalProcessWatts() const noexcept { return total_process_w_; }

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
    void processListChanged();
    void rescanStatusChanged();
    void historyChanged();
    void powerSharesChanged();

private slots:
    void onPollTimer();

private:
    void mapSharedMemory() noexcept;
    void unmapSharedMemory() noexcept;
    void sendDaemonCommand(const char* cmd) noexcept;
    bool queryDaemonTelemetry() noexcept;
    void updateFallbackTelemetry() noexcept;

    int shm_fd_{-1};
    const ipc::WattCurbSharedState* shm_state_{nullptr};
    ipc::WattCurbSharedState latest_state_{};
    int local_override_mode_{-1};

    QTimer* poll_timer_{nullptr};
    bool is_rescanning_{false};
    QString last_update_time_{"Just now"};

    // Telemetry History Window (REF-REQ-040)
    QVariantList system_history_{};
    QVariantList cpu_history_{};
    QVariantList gpu_history_{};
    double peak_system_w_{15.0};

    // Extended Telemetry Cache
    double battery_voltage_v_{11.49};
    double battery_current_a_{0.85};
    int battery_cycles_{99};
    QString battery_mfg_{"SMP"};
    QString battery_model_{"LNV-5B10W13895"};
    QString battery_tech_{"Li-poly"};
    double battery_design_wh_{45.28};
    double battery_full_wh_{42.65};
    double battery_now_wh_{31.91};

    double cpu_core_w_{2.45};
    double cpu_uncore_w_{0.82};
    double cpu_dram_w_{0.95};
    int cpu_freq_mhz_{2400};
    QString cpu_governor_{"powersave"};
    double cstate_c0_{3.0};
    double cstate_c1_{14.0};
    double cstate_c2_{20.0};
    double cstate_c3_{63.0};

    double pmu_ipc_{1.45};
    qlonglong pmu_instructions_{45000000};
    qlonglong pmu_cycles_{31000000};
    qlonglong pmu_llc_misses_{1200};
    qlonglong pmu_branch_misses_{4500};
    double pmu_ewr_{8.5};

    int gpu_load_pct_{0};
    double display_drain_w_{1.8};
    int display_brightness_pct_{50};
    double nvme_drain_w_{0.8};
    double disk_read_mb_s_{0.0};
    double disk_write_mb_s_{0.1};
    QString aspm_policy_{"powersave"};

    QVariantList process_list_{};

    // Power Share Decomposition (REF-REQ-060, REF-ARCH-036)
    QVariantList device_power_shares_{};
    QVariantList process_power_shares_{};
    double total_device_w_{0.0};
    double total_process_w_{0.0};
    void update_power_shares();
};

} // namespace wattcurb::ui
