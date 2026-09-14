#include "ui/dashboard_backend.hpp"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <QDateTime>
#include <QProcess>
#include <fstream>
#include <thread>
#include <algorithm>

namespace wattcurb::ui {

DashboardBackend::DashboardBackend(QObject* parent)
    : QObject(parent)
{
    mapSharedMemory();

    // Immediately fetch first sample (0ms startup)
    if (shm_state_ && shm_state_->read_atomic(latest_state_)) {
        if (latest_state_.power_profile_mode <= 3) {
            local_override_mode_ = latest_state_.power_profile_mode;
        }
    }
    updateAuxiliaryTelemetry();
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

    updateAuxiliaryTelemetry();
    last_update_time_ = QDateTime::currentDateTime().toString("hh:mm:ss");

    emit telemetryChanged();
}

void DashboardBackend::updateAuxiliaryTelemetry() noexcept {
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

    // Attribute display and NVMe from remaining platform power
    display_drain_w_ = std::clamp(rem_w * 0.55, 0.8, 4.5);
    nvme_drain_w_ = std::clamp(rem_w * 0.15, 0.3, 1.5);
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
        default: return QStringLiteral("AC 연결 / 완충 (AC Powered)");
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

double DashboardBackend::otherDrainWatts() const noexcept {
    double total = systemDrainWatts();
    double sub = cpuDrainWatts() + gpuDrainWatts() + displayDrainWatts() + nvmeDrainWatts();
    return std::max(0.1, total - sub);
}

int DashboardBackend::cpuTempC() const noexcept {
    return latest_state_.cpu_temp_c;
}

int DashboardBackend::fanRpm() const noexcept {
    return latest_state_.fan_rpm;
}

int DashboardBackend::cstateC3Percent() const noexcept {
    return latest_state_.cstate_c3_percent;
}

int DashboardBackend::wakeupsPerSec() const noexcept {
    return latest_state_.wakeups_per_sec;
}

int DashboardBackend::activeMitigations() const noexcept {
    return latest_state_.active_mitigations;
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

QString DashboardBackend::top1Comm() const {
    if (latest_state_.culprits[0].comm[0] != '\0') {
        return QString::fromUtf8(latest_state_.culprits[0].comm);
    }
    return QStringLiteral("kworker/u16:0");
}

int DashboardBackend::top1Pid() const noexcept {
    return latest_state_.culprits[0].pid > 0 ? latest_state_.culprits[0].pid : 124;
}

int DashboardBackend::top1DrainMw() const noexcept {
    return latest_state_.culprits[0].drain_mw > 0 ? latest_state_.culprits[0].drain_mw : 280;
}

int DashboardBackend::top1Tier() const noexcept {
    return latest_state_.culprits[0].tier;
}

QString DashboardBackend::top2Comm() const {
    if (latest_state_.culprits[1].comm[0] != '\0') {
        return QString::fromUtf8(latest_state_.culprits[1].comm);
    }
    return QStringLiteral("plasmashell");
}

int DashboardBackend::top2Pid() const noexcept {
    return latest_state_.culprits[1].pid > 0 ? latest_state_.culprits[1].pid : 3290;
}

int DashboardBackend::top2DrainMw() const noexcept {
    return latest_state_.culprits[1].drain_mw > 0 ? latest_state_.culprits[1].drain_mw : 190;
}

int DashboardBackend::top2Tier() const noexcept {
    return latest_state_.culprits[1].tier;
}

void DashboardBackend::sendDaemonCommand(const char* cmd) noexcept {
    int fd = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return;

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, ipc::CONTROL_SOCKET_PATH, sizeof(addr.sun_path) - 1);

    ::sendto(fd, cmd, std::strlen(cmd), 0,
             reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    ::close(fd);
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

    // Trigger daemon rescan in background thread without freezing UI (0ms blocking)
    std::thread([this]() {
        sendDaemonCommand("RESCAN\n");
    }).detach();

    // Auto-reset rescan visual spinner after 3.5 seconds
    QTimer::singleShot(3500, this, [this]() {
        is_rescanning_ = false;
        onPollTimer(); // immediate update
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
