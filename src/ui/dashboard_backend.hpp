#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <cstdint>
#include "ipc/tray_shared_state.hpp"
#include "report/battery_history_analyzer.hpp"

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

    // System Memory & Swap Telemetry (REF-REQ-132)
    Q_PROPERTY(int memTotalMb READ memTotalMb NOTIFY telemetryChanged)
    Q_PROPERTY(int memUsedMb READ memUsedMb NOTIFY telemetryChanged)
    Q_PROPERTY(int memAvailMb READ memAvailMb NOTIFY telemetryChanged)
    Q_PROPERTY(int swapTotalMb READ swapTotalMb NOTIFY telemetryChanged)
    Q_PROPERTY(int swapUsedMb READ swapUsedMb NOTIFY telemetryChanged)
    Q_PROPERTY(double memUsedPercent READ memUsedPercent NOTIFY telemetryChanged)
    Q_PROPERTY(QString memorySummaryString READ memorySummaryString NOTIFY telemetryChanged)

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

    // Deep Battery Drain Report Properties (REF-REQ-078, REF-ARCH-055, REF-REQ-086)
    Q_PROPERTY(QVariantMap batteryReportSummary READ batteryReportSummary NOTIFY batteryReportChanged)
    Q_PROPERTY(QVariantList batteryReportHardwareShares READ batteryReportHardwareShares NOTIFY batteryReportChanged)
    Q_PROPERTY(QVariantList batteryReportProcessCulprits READ batteryReportProcessCulprits NOTIFY batteryReportChanged)
    Q_PROPERTY(QVariantList batteryReportModeComparisons READ batteryReportModeComparisons NOTIFY batteryReportChanged)
    Q_PROPERTY(int reportFilterMode READ reportFilterMode WRITE setReportFilterMode NOTIFY reportFilterModeChanged)

    // Status / Metadata
    Q_PROPERTY(bool isRescanning READ isRescanning NOTIFY rescanStatusChanged)
    Q_PROPERTY(QString lastUpdateTime READ lastUpdateTime NOTIFY telemetryChanged)

    // Localization (REF-REQ-076, REF-ARCH-053)
    Q_PROPERTY(QString currentLanguage READ currentLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString currentLanguageCode READ currentLanguageCode NOTIFY languageChanged)

public:
    explicit DashboardBackend(QObject* parent = nullptr);
    ~DashboardBackend() override;

    // Localization bridge for QML
    Q_INVOKABLE QString tr(const QString& key) const;
    Q_INVOKABLE void setLanguage(const QString& code);
    QString currentLanguage() const;
    QString currentLanguageCode() const;

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

    // System Memory Getters (REF-REQ-132)
    int memTotalMb() const noexcept { return mem_total_mb_; }
    int memUsedMb() const noexcept { return mem_used_mb_; }
    int memAvailMb() const noexcept { return mem_avail_mb_; }
    int swapTotalMb() const noexcept { return swap_total_mb_; }
    int swapUsedMb() const noexcept { return swap_used_mb_; }
    double memUsedPercent() const noexcept {
        return (mem_total_mb_ > 0) ? (static_cast<double>(mem_used_mb_) / mem_total_mb_ * 100.0) : 0.0;
    }
    QString memorySummaryString() const {
        double used_gb = mem_used_mb_ / 1024.0;
        double tot_gb = mem_total_mb_ / 1024.0;
        return QString::asprintf("%.1fG / %.1fG (%.0f%%)", used_gb, tot_gb, memUsedPercent());
    }

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

    // Deep Battery Drain Report Getters (REF-REQ-078, REF-ARCH-055, REF-REQ-086)
    QVariantMap batteryReportSummary() const { return battery_report_summary_; }
    QVariantList batteryReportHardwareShares() const { return battery_report_hardware_shares_; }
    QVariantList batteryReportProcessCulprits() const { return battery_report_process_culprits_; }
    QVariantList batteryReportModeComparisons() const { return battery_report_mode_comparisons_; }
    int reportFilterMode() const noexcept { return report_filter_mode_; }

    bool isRescanning() const noexcept { return is_rescanning_; }
    QString lastUpdateTime() const { return last_update_time_; }

    // Q_INVOKABLE Actions for QML UI
    Q_INVOKABLE void setProfile(int mode);
    Q_INVOKABLE void triggerRescan();
    Q_INVOKABLE void openSystemMonitor();
    Q_INVOKABLE void refreshNow();

    // Battery Report Actions (REF-REQ-078, REF-REQ-086)
    Q_INVOKABLE void generateBatteryReport();
    Q_INVOKABLE void setReportFilterMode(int mode);
    Q_INVOKABLE void copyReportToClipboard();
    Q_INVOKABLE QString getReportMarkdown();
    Q_INVOKABLE void requestReportWindow();

    // Test & Benchmark helpers (REF-REQ-074, REF-TEST-039)
    void runPollIteration() noexcept;
    bool ingestTelemetryJson(const std::string& json_str) noexcept;

signals:
    void telemetryChanged();
    void profileChanged();
    void processListChanged();
    void rescanStatusChanged();
    void historyChanged();
    void powerSharesChanged();
    void languageChanged();
    void batteryReportChanged();
    void reportFilterModeChanged();
    void reportWindowRequested();

private slots:
    void onPollTimer();

private:
    void mapSharedMemory() noexcept;
    void unmapSharedMemory() noexcept;
    void sendDaemonCommand(const char* cmd) noexcept;
    // REF-REQ-121.4: same, but hands back the daemon's answer so a refusal can be
    // reported instead of silently painting a profile that was not applied.
    bool sendDaemonCommandQuery(const char* cmd, std::string& out_response) noexcept;
    bool queryDaemonTelemetry() noexcept;
    void updateFallbackTelemetry() noexcept;

    int shm_fd_{-1};
    const ipc::WattCurbSharedState* shm_state_{nullptr};
    ipc::WattCurbSharedState latest_state_{};
    int local_override_mode_{-1};
    int prev_profile_mode_{-1};

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
    double gpu_drain_w_fallback_{0.8};
    double system_drain_w_fallback_{12.5};
    double display_drain_w_{1.8};
    int display_brightness_pct_{50};
    double nvme_drain_w_{0.8};
    double disk_read_mb_s_{0.0};
    double disk_write_mb_s_{0.1};
    QString aspm_policy_{"powersave"};

    // System Memory & Swap Telemetry Cache (REF-REQ-132)
    int mem_total_mb_{16384};
    int mem_used_mb_{4096};
    int mem_avail_mb_{12288};
    int swap_total_mb_{8192};
    int swap_used_mb_{0};

    QVariantList process_list_{};

    // Power Share Decomposition (REF-REQ-060, REF-ARCH-036)
    struct ProcessShareSummary {
        int pid{0};
        QString comm;
        double total_watts{0.0};
    };
    std::vector<ProcessShareSummary> cached_proc_summaries_{};
    double cached_proc_sum_{0.0};
    uint64_t prev_seq_version_{0};
    // REF-REQ-074 & REF-ARCH-051: Explicit first-poll sentinel. prev_seq_version_
    // cannot serve as one: when no daemon is publishing to /dev/shm the observed
    // sequence stays 0 forever, so a "prev_seq_version_ == 0" test never becomes
    // false and the Seqlock delta gate degrades into an unconditional socket
    // query plus a full QML signal storm on every single poll tick.
    bool initial_poll_done_{false};

    QVariantList device_power_shares_{};
    QVariantList process_power_shares_{};
    double total_device_w_{0.0};
    double total_process_w_{0.0};
    void update_power_shares();

    // Deep Battery Drain Report Cache (REF-REQ-078, REF-ARCH-055, REF-REQ-086)
    int report_filter_mode_{-1}; // -1: All, 0: Perf, 1: Balanced, 2: Save, 3: Ultra
    report::BatteryDrainReportResult cached_report_result_{};
    QVariantMap battery_report_summary_{};
    QVariantList battery_report_hardware_shares_{};
    QVariantList battery_report_process_culprits_{};
    QVariantList battery_report_mode_comparisons_{};
    std::vector<ProcessAttributedPower> cached_top_procs_{};
};

} // namespace wattcurb::ui
