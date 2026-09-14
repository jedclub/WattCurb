#include "ui/dashboard_backend.hpp"
#include "core/singleton_lock.hpp"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <QDateTime>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <fstream>
#include <thread>
#include <algorithm>

namespace wattcurb::ui {

DashboardBackend::DashboardBackend(QObject* parent)
    : QObject(parent)
{
    mapSharedMemory();

    // Fetch initial sample
    if (shm_state_ && shm_state_->read_atomic(latest_state_)) {
        if (latest_state_.power_profile_mode <= 3) {
            local_override_mode_ = latest_state_.power_profile_mode;
        }
    }
    
    if (!queryDaemonTelemetry()) {
        updateFallbackTelemetry();
    }
    last_update_time_ = QDateTime::currentDateTime().toString("hh:mm:ss");

    // 1-second live telemetry poll timer (ultra-low overhead)
    poll_timer_ = new QTimer(this);
    connect(poll_timer_, &QTimer::timeout, this, &DashboardBackend::onPollTimer);
    poll_timer_->start(1000);
}

DashboardBackend::~DashboardBackend() {
    unmapSharedMemory();
}

void DashboardBackend::mapSharedMemory() noexcept {
    shm_fd_ = ::open(ipc::SHARED_STATE_SHM_PATH, O_RDONLY | O_CLOEXEC);
    if (shm_fd_ >= 0) {
        void* addr = ::mmap(nullptr, sizeof(ipc::WattCurbSharedState), PROT_READ, MAP_SHARED, shm_fd_, 0);
        if (addr != MAP_FAILED) {
            shm_state_ = static_cast<const ipc::WattCurbSharedState*>(addr);
        }
    }
}

void DashboardBackend::unmapSharedMemory() noexcept {
    if (shm_state_) {
        ::munmap(const_cast<void*>(static_cast<const void*>(shm_state_)), sizeof(ipc::WattCurbSharedState));
        shm_state_ = nullptr;
    }
    if (shm_fd_ >= 0) {
        ::close(shm_fd_);
        shm_fd_ = -1;
    }
}

void DashboardBackend::onPollTimer() {
    if (!shm_state_) {
        mapSharedMemory();
    }

    if (shm_state_) {
        ipc::WattCurbSharedState cur{};
        if (shm_state_->read_atomic(cur)) {
            latest_state_ = cur;
            if (local_override_mode_ >= 0) {
                latest_state_.power_profile_mode = static_cast<uint8_t>(local_override_mode_);
            }
        }
    }

    if (!queryDaemonTelemetry()) {
        updateFallbackTelemetry();
    }

    last_update_time_ = QDateTime::currentDateTime().toString("hh:mm:ss");

    emit telemetryChanged();
    emit processListChanged();
}

bool DashboardBackend::queryDaemonTelemetry() noexcept {
    std::string resp;
    if (!core::SingletonLock::query_daemon("FULL_TELEMETRY\n", resp, "wattcurb.lock", 80)) {
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(resp));
    if (!doc.isObject()) return false;

    QJsonObject obj = doc.object();
    battery_voltage_v_ = obj.value("battery_voltage_v").toDouble(11.49);
    battery_current_a_ = obj.value("battery_current_a").toDouble(0.85);
    battery_cycles_ = obj.value("battery_cycles").toInt(99);

    cpu_core_w_ = obj.value("cpu_core_w").toDouble(2.45);
    cpu_uncore_w_ = obj.value("cpu_uncore_w").toDouble(0.82);
    cpu_dram_w_ = obj.value("cpu_dram_w").toDouble(0.95);
    cpu_freq_mhz_ = static_cast<int>(obj.value("cpu_freq_mhz").toDouble(2400.0));
    cstate_c0_ = obj.value("cstate_c0").toDouble(3.0);
    cstate_c1_ = obj.value("cstate_c1").toDouble(14.0);
    cstate_c2_ = obj.value("cstate_c2").toDouble(20.0);
    cstate_c3_ = obj.value("cstate_c3").toDouble(63.0);

    gpu_load_pct_ = obj.value("gpu_load").toInt(0);
    display_drain_w_ = obj.value("display_w").toDouble(1.8);
    display_brightness_pct_ = static_cast<int>(obj.value("display_brightness").toDouble(50.0));
    nvme_drain_w_ = obj.value("nvme_w").toDouble(0.8);
    disk_read_mb_s_ = obj.value("disk_read_mb_s").toDouble(0.0);
    disk_write_mb_s_ = obj.value("disk_write_mb_s").toDouble(0.1);

    double total_sys_w = obj.value("system_watts").toDouble(systemDrainWatts());

    QJsonArray proc_arr = obj.value("processes").toArray();
    QVariantList new_list;
    new_list.reserve(proc_arr.size());

    for (const auto& item_val : proc_arr) {
        QJsonObject p = item_val.toObject();
        QVariantMap map;
        map["pid"] = p.value("pid").toInt();
        map["comm"] = p.value("comm").toString();
        double w = p.value("total_w").toDouble();
        map["totalWatts"] = w;
        map["cpuWatts"] = p.value("cpu_w").toDouble();
        map["gpuWatts"] = p.value("gpu_w").toDouble();
        map["dramWatts"] = p.value("dram_w").toDouble();
        map["ioWakeWatts"] = p.value("io_wake_w").toDouble();
        map["pssMb"] = p.value("pss_mb").toInt();
        map["tier"] = p.value("tier").toInt();
        map["domain"] = p.value("domain").toString();
        map["mechanism"] = p.value("mechanism").toString();
        
        double pct = (total_sys_w > 0.0) ? std::min(100.0, (w / total_sys_w) * 100.0) : 0.0;
        map["ratioPercent"] = pct;

        new_list.append(map);
    }

    if (!new_list.isEmpty()) {
        process_list_ = std::move(new_list);
    }

    return true;
}

void DashboardBackend::updateFallbackTelemetry() noexcept {
    // Read display backlight percentage
    int cur_b = 0, max_b = 0;
    std::ifstream cur_f("/sys/class/backlight/amdgpu_bl1/actual_brightness");
    if (!cur_f.is_open()) cur_f.open("/sys/class/backlight/amdgpu_bl0/actual_brightness");
    if (cur_f >> cur_b) {
        std::ifstream max_f("/sys/class/backlight/amdgpu_bl1/max_brightness");
        if (!max_f.is_open()) max_f.open("/sys/class/backlight/amdgpu_bl0/max_brightness");
        if (max_f >> max_b && max_b > 0) {
            display_brightness_pct_ = std::clamp((cur_b * 100) / max_b, 1, 100);
        }
    }

    double total_w = latest_state_.system_drain_mw / 1000.0;
    double cpu_w = latest_state_.cpu_drain_mw / 1000.0;
    double gpu_w = latest_state_.gpu_drain_mw / 1000.0;
    double rem_w = std::max(0.0, total_w - cpu_w - gpu_w);

    cpu_core_w_ = cpu_w * 0.75;
    cpu_uncore_w_ = cpu_w * 0.25;
    cpu_dram_w_ = std::clamp(rem_w * 0.20, 0.4, 2.0);
    display_drain_w_ = std::clamp(rem_w * 0.50, 0.8, 4.5);
    nvme_drain_w_ = std::clamp(rem_w * 0.15, 0.3, 1.5);
    cstate_c3_ = latest_state_.cstate_c3_percent;

    // Fallback process list from SHM culprits if socket query was not yet active
    if (process_list_.isEmpty()) {
        QVariantList fallback_list;
        for (int i = 0; i < 2; ++i) {
            if (latest_state_.culprits[i].comm[0] != '\0') {
                QVariantMap m;
                m["pid"] = latest_state_.culprits[i].pid;
                m["comm"] = QString::fromUtf8(latest_state_.culprits[i].comm);
                double w = latest_state_.culprits[i].drain_mw / 1000.0;
                m["totalWatts"] = w;
                m["cpuWatts"] = w * 0.7;
                m["gpuWatts"] = 0.0;
                m["dramWatts"] = w * 0.2;
                m["ioWakeWatts"] = w * 0.1;
                m["pssMb"] = 120 + i * 80;
                m["tier"] = latest_state_.culprits[i].tier;
                m["domain"] = QStringLiteral("CPU Compute");
                m["mechanism"] = QStringLiteral("Background Active Execution");
                m["ratioPercent"] = (total_w > 0.0) ? (w / total_w * 100.0) : 5.0;
                fallback_list.append(m);
            }
        }
        process_list_ = fallback_list;
    }
}

double DashboardBackend::systemDrainWatts() const noexcept {
    return latest_state_.system_drain_mw / 1000.0;
}

int DashboardBackend::batteryPercent() const noexcept {
    return latest_state_.battery_percent;
}

int DashboardBackend::batteryHealth() const noexcept {
    return latest_state_.battery_health_percent;
}

int DashboardBackend::batteryState() const noexcept {
    return latest_state_.battery_state;
}

QString DashboardBackend::batteryStateString() const {
    switch (latest_state_.battery_state) {
        case 1: return QStringLiteral("방전 중 (Discharging)");
        case 2: return QStringLiteral("AC 직결 (AC Passthrough)");
        default: return QStringLiteral("AC 연결 (AC Powered)");
    }
}

int DashboardBackend::timeToEmptyMin() const noexcept {
    return latest_state_.time_to_empty_min;
}

QString DashboardBackend::timeToEmptyString() const {
    if (latest_state_.battery_state != 1) {
        return QStringLiteral("전원 연결됨 (무제한)");
    }
    int mins = latest_state_.time_to_empty_min;
    if (mins <= 0) return QStringLiteral("계산 중...");
    int h = mins / 60;
    int m = mins % 60;
    if (h > 0) {
        return QStringLiteral("%1시간 %2분").arg(h).arg(m, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1분").arg(m);
}

double DashboardBackend::cpuDrainWatts() const noexcept {
    return latest_state_.cpu_drain_mw / 1000.0;
}

double DashboardBackend::gpuDrainWatts() const noexcept {
    return latest_state_.gpu_drain_mw / 1000.0;
}

double DashboardBackend::displayDrainWatts() const noexcept {
    return display_drain_w_;
}

double DashboardBackend::nvmeDrainWatts() const noexcept {
    return nvme_drain_w_;
}

int DashboardBackend::cpuTempC() const noexcept {
    return latest_state_.cpu_temp_c;
}

int DashboardBackend::fanRpm() const noexcept {
    return latest_state_.fan_rpm;
}

double DashboardBackend::cstateC3Percent() const noexcept {
    return cstate_c3_;
}

int DashboardBackend::wakeupsPerSec() const noexcept {
    return static_cast<int>(latest_state_.wakeups_per_sec);
}

int DashboardBackend::activeMitigations() const noexcept {
    return static_cast<int>(latest_state_.active_mitigations);
}

int DashboardBackend::displayBrightnessPct() const noexcept {
    return display_brightness_pct_;
}

int DashboardBackend::powerProfileMode() const noexcept {
    return (local_override_mode_ >= 0) ? local_override_mode_ : latest_state_.power_profile_mode;
}

QString DashboardBackend::powerProfileName() const {
    int m = powerProfileMode();
    switch (m) {
        case 0: return QStringLiteral("Performance (고성능 4.1GHz)");
        case 1: return QStringLiteral("Balanced (기본 균형)");
        case 2: return QStringLiteral("Smart Save (스마트 절전 1.7GHz)");
        case 3: return QStringLiteral("Ultra Save (울트라 절전 1.4GHz + 48Hz)");
        default: return QStringLiteral("Balanced");
    }
}

void DashboardBackend::sendDaemonCommand(const char* cmd) noexcept {
    std::string dummy;
    core::SingletonLock::query_daemon(cmd, dummy, "wattcurb.lock", 50);
}

void DashboardBackend::setProfile(int mode) {
    if (mode < 0 || mode > 3) return;
    local_override_mode_ = mode;

    char cmd[32];
    std::snprintf(cmd, sizeof(cmd), "PROFILE %d\n", mode);
    sendDaemonCommand(cmd);

    // Apply hardware changes via CLI script immediately
    const char* hw_mode = "balanced";
    if (mode == 0) hw_mode = "performance";
    else if (mode == 2) hw_mode = "save";
    else if (mode == 3) hw_mode = "ultra";

    std::string sys_cmd = "/home/jedclub/.local/bin/wattcurb --profile " + std::string(hw_mode) + " 2>/dev/null &";
    ::system(sys_cmd.c_str());

    emit profileChanged();
}

void DashboardBackend::triggerRescan() {
    if (is_rescanning_) return;

    is_rescanning_ = true;
    emit rescanStatusChanged();

    std::thread([this]() {
        sendDaemonCommand("RESCAN\n");
    }).detach();

    QTimer::singleShot(3500, this, [this]() {
        is_rescanning_ = false;
        onPollTimer();
        emit rescanStatusChanged();
    });
}

void DashboardBackend::openSystemMonitor() {
    QProcess::startDetached(QStringLiteral("plasma-systemmonitor"), {});
}

void DashboardBackend::refreshNow() {
    onPollTimer();
}

} // namespace wattcurb::ui
