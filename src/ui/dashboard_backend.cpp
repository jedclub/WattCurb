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

    // Initialize 35-sample history streams (REF-REQ-040)
    double init_sys = systemDrainWatts();
    double init_cpu = cpuDrainWatts();
    double init_gpu = gpuDrainWatts();
    peak_system_w_ = std::max(15.0, init_sys * 1.2);
    for (int i = 0; i < 35; ++i) {
        system_history_.append(init_sys);
        cpu_history_.append(init_cpu);
        gpu_history_.append(init_gpu);
    }

    // 1.5-second live telemetry poll timer (ultra-low overhead, Zero-Wakeup compliant)
    poll_timer_ = new QTimer(this);
    connect(poll_timer_, &QTimer::timeout, this, &DashboardBackend::onPollTimer);
    poll_timer_->start(1500);
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

    // Update live history sliding windows (35 samples)
    double cur_sys_w = systemDrainWatts();
    double cur_cpu_w = cpuDrainWatts();
    double cur_gpu_w = gpuDrainWatts();

    if (cur_sys_w > peak_system_w_) {
        peak_system_w_ = cur_sys_w;
    }

    system_history_.append(cur_sys_w);
    cpu_history_.append(cur_cpu_w);
    gpu_history_.append(cur_gpu_w);

    while (system_history_.size() > 35) system_history_.removeFirst();
    while (cpu_history_.size() > 35) cpu_history_.removeFirst();
    while (gpu_history_.size() > 35) gpu_history_.removeFirst();

    update_power_shares();

    emit telemetryChanged();
    emit processListChanged();
    emit historyChanged();
    emit powerSharesChanged();
}

bool DashboardBackend::queryDaemonTelemetry() noexcept {
    std::string resp;
    if (!core::SingletonLock::query_daemon("FULL_TELEMETRY\n", resp, "wattcurb.lock", 300)) {
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(resp));
    if (!doc.isObject()) return false;

    QJsonObject obj = doc.object();
    battery_voltage_v_ = obj.value("battery_voltage_v").toDouble(11.49);
    battery_current_a_ = obj.value("battery_current_a").toDouble(0.85);
    battery_cycles_ = obj.value("battery_cycles").toInt(99);
    battery_mfg_ = obj.value("battery_mfg").toString("SMP");
    battery_model_ = obj.value("battery_model").toString("LNV-5B10W");
    battery_tech_ = obj.value("battery_tech").toString("Li-poly");
    battery_design_wh_ = obj.value("battery_design_wh").toDouble(45.28);
    battery_full_wh_ = obj.value("battery_full_wh").toDouble(42.65);
    battery_now_wh_ = obj.value("battery_now_wh").toDouble(31.91);

    cpu_core_w_ = obj.value("cpu_core_w").toDouble(2.45);
    cpu_uncore_w_ = obj.value("cpu_uncore_w").toDouble(0.82);
    cpu_dram_w_ = obj.value("cpu_dram_w").toDouble(0.95);
    cpu_freq_mhz_ = static_cast<int>(obj.value("cpu_freq_mhz").toDouble(2400.0));
    cpu_governor_ = obj.value("cpu_governor").toString("powersave");
    cstate_c0_ = obj.value("cstate_c0").toDouble(3.0);
    cstate_c1_ = obj.value("cstate_c1").toDouble(14.0);
    cstate_c2_ = obj.value("cstate_c2").toDouble(20.0);
    cstate_c3_ = obj.value("cstate_c3").toDouble(63.0);

    pmu_ipc_ = obj.value("pmu_ipc").toDouble(1.45);
    pmu_instructions_ = obj.value("pmu_instructions").toInteger(45000000);
    pmu_cycles_ = obj.value("pmu_cycles").toInteger(31000000);
    pmu_llc_misses_ = obj.value("pmu_llc_misses").toInteger(1200);
    pmu_branch_misses_ = obj.value("pmu_branch_misses").toInteger(4500);
    pmu_ewr_ = obj.value("pmu_ewr").toDouble(8.5);

    gpu_load_pct_ = obj.value("gpu_load").toInt(0);
    display_drain_w_ = obj.value("display_w").toDouble(1.8);
    display_brightness_pct_ = static_cast<int>(obj.value("display_brightness").toDouble(50.0));
    nvme_drain_w_ = obj.value("nvme_w").toDouble(0.8);
    disk_read_mb_s_ = obj.value("disk_read_mb_s").toDouble(0.0);
    disk_write_mb_s_ = obj.value("disk_write_mb_s").toDouble(0.1);
    aspm_policy_ = obj.value("aspm_policy").toString("powersave");

    double total_sys_w = obj.value("system_watts").toDouble(systemDrainWatts());

    QJsonArray proc_arr = obj.value("processes").toArray();
    QVariantList new_list;
    new_list.reserve(std::min<qsizetype>(proc_arr.size(), 25));

    for (const auto& item_val : proc_arr) {
        if (new_list.size() >= 25) break; // Top 25 processes are sufficient for visible matrix
        QJsonObject p = item_val.toObject();
        QVariantMap map;
        map["pid"] = p.value("pid").toInt();
        map["comm"] = p.value("comm").toString();
        map["uid"] = p.value("uid").toInt();
        double w = p.value("total_w").toDouble();
        map["totalWatts"] = w;
        map["cpuWatts"] = p.value("cpu_w").toDouble();
        map["gpuWatts"] = p.value("gpu_w").toDouble();
        map["dramWatts"] = p.value("dram_w").toDouble();
        map["ioWakeWatts"] = p.value("io_wake_w").toDouble();
        map["ioWatts"] = p.value("io_w").toDouble();
        map["wakeTaxWatts"] = p.value("wake_tax_w").toDouble();
        map["fanWatts"] = p.value("fan_w").toDouble();
        map["wifiWatts"] = p.value("wifi_w").toDouble();
        map["wdiScore"] = p.value("wdi_score").toDouble();
        map["pssMb"] = p.value("pss_mb").toInt();
        map["tier"] = p.value("tier").toInt();
        map["cpuCore"] = p.value("cpu_core").toInt();
        map["threads"] = p.value("threads").toInt(1);
        map["crossCcx"] = p.value("cross_ccx").toInt(0);
        map["nice"] = p.value("nice").toInt(0);
        map["priority"] = p.value("priority").toInt(0);
        map["wakeupsSec"] = p.value("wakeups_sec").toInteger(0);
        map["timerslackNs"] = p.value("timerslack_ns").toInteger(50000);
        map["vramMb"] = p.value("vram_mb").toDouble(0.0);
        map["ioMbSec"] = p.value("io_mb_s").toDouble(0.0);
        map["minfltSec"] = p.value("minflt_s").toInteger(0);
        map["majfltSec"] = p.value("majflt_s").toInteger(0);
        map["openSockets"] = p.value("open_sockets").toInt(0);
        map["action"] = p.value("action").toInt(0);
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
        case 3: return QStringLiteral("Ultra Save (울트라 절전 1.4GHz 상한)");
        default: return QStringLiteral("Balanced");
    }
}

void DashboardBackend::sendDaemonCommand(const char* cmd) noexcept {
    std::string dummy;
    core::SingletonLock::query_daemon(cmd, dummy, "wattcurb.lock", 50);
}

void DashboardBackend::setProfile(int mode) {
    if (mode < 0 || mode > 3) return;

    // REF-REQ-067: Battery <= 20% Performance Mode Lockout Invariant
    if (mode == 0 && batteryState() == 1 && batteryPercent() <= 20) {
        return;
    }

    local_override_mode_ = mode;

    char cmd[32];
    std::snprintf(cmd, sizeof(cmd), "PROFILE %d\n", mode);
    sendDaemonCommand(cmd);

    // Hardware actuation is executed natively by root daemon upon receiving PROFILE command
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

void DashboardBackend::update_power_shares() {
    // 1. Compute Hardware Device Shares (REF-REQ-060, REF-ARCH-036)
    double sys_w = systemDrainWatts();
    double cpu_w = cpuDrainWatts();
    double gpu_w = gpuDrainWatts();
    double disp_w = displayDrainWatts();
    double nvme_w = nvmeDrainWatts();
    double fan_w = fanRpm() > 0 ? (fanRpm() / 4000.0) * 0.9 : 0.0;

    double known_w = cpu_w + gpu_w + disp_w + nvme_w + fan_w;
    double plat_w = (sys_w > known_w) ? (sys_w - known_w) : 0.0;
    double total_dev_w = std::max(known_w + plat_w, 0.1);
    total_device_w_ = total_dev_w;

    QVariantList dev_list;
    auto add_dev = [&](const QString& name, double w, const QString& col) {
        if (w < 0.001) return;
        QVariantMap m;
        m["name"] = name;
        m["watts"] = w;
        m["pct"] = std::min(100.0, (w / total_dev_w) * 100.0);
        m["color"] = col;
        dev_list.append(m);
    };

    add_dev(QStringLiteral("CPU Subsystem"), cpu_w, QStringLiteral("#00d2ff")); // Cyan
    add_dev(QStringLiteral("GPU Silicon"), gpu_w, QStringLiteral("#a855f7"));   // Purple
    add_dev(QStringLiteral("Display & Light"), disp_w, QStringLiteral("#f59e0b")); // Orange
    add_dev(QStringLiteral("NVMe Storage"), nvme_w, QStringLiteral("#10b981")); // Emerald
    if (fan_w > 0.05) {
        add_dev(QStringLiteral("Cooling Fan"), fan_w, QStringLiteral("#3b82f6"));   // Blue
    }
    if (plat_w > 0.05) {
        // Physical Constituent Decomposition of Platform & Loss (REF-REQ-064)
        // 1. VRM Conversion Loss (9% of system power due to DC-DC buck converter efficiency ~91%)
        double est_vrm = sys_w * 0.09;
        // 2. DRAM Memory (16GB LPDDR5 tREFI periodic cell refresh + command/data bus)
        double est_dram = 0.70 + std::min(0.25, (cpu_w * 0.05));
        // 3. Wireless (Wi-Fi 6 + Bluetooth baseband & RF front-end standby/beacon)
        double est_wifi = 0.35;
        // 4. Motherboard & IO (EC controller, I2C bus, audio codec, PCIe bridge)
        double est_mb = 0.15;

        double est_sum = est_vrm + est_dram + est_wifi + est_mb;
        double scale = (est_sum > 0.01) ? (plat_w / est_sum) : 1.0;

        double vrm_w = est_vrm * scale;
        double dram_w = est_dram * scale;
        double wifi_w = est_wifi * scale;
        double mb_w = plat_w - (vrm_w + dram_w + wifi_w);
        if (mb_w < 0.01) {
            mb_w = 0.01;
            double rem = std::max(0.01, plat_w - mb_w);
            double sub_sum = est_vrm + est_dram + est_wifi;
            vrm_w = rem * (est_vrm / sub_sum);
            dram_w = rem * (est_dram / sub_sum);
            wifi_w = rem - vrm_w - dram_w;
        }

        add_dev(QStringLiteral("DRAM Memory"), dram_w, QStringLiteral("#38bdf8"));     // Light blue
        add_dev(QStringLiteral("VRM Power Loss"), vrm_w, QStringLiteral("#f43f5e"));   // Rose red
        add_dev(QStringLiteral("Wireless (Wi-Fi)"), wifi_w, QStringLiteral("#818cf8")); // Indigo
        add_dev(QStringLiteral("Motherboard & IO"), mb_w, QStringLiteral("#94a3b8"));  // Slate
    }
    device_power_shares_ = dev_list;

    // 2. Compute Process Power Shares (REF-REQ-060, REF-ARCH-036, REF-REQ-064)
    QVariantList proc_list;
    double proc_sum = 0.0;

    if (!process_list_.isEmpty()) {
        for (const auto& item : process_list_) {
            proc_sum += item.toMap().value(QStringLiteral("totalWatts")).toDouble();
        }
    } else {
        if (latest_state_.culprits[0].pid > 0) {
            proc_sum += (latest_state_.culprits[0].drain_mw / 1000.0);
        }
        if (latest_state_.culprits[1].pid > 0) {
            proc_sum += (latest_state_.culprits[1].drain_mw / 1000.0);
        }
    }

    double total_proc_w = std::max(proc_sum, 0.1);
    total_process_w_ = total_proc_w;

    const QString proc_colors[] = {
        QStringLiteral("#ef4444"), // Red (Top 1)
        QStringLiteral("#f59e0b"), // Amber (Top 2)
        QStringLiteral("#00d2ff"), // Cyan (Top 3)
        QStringLiteral("#a855f7"), // Purple (Top 4)
        QStringLiteral("#10b981"), // Emerald (Top 5)
        QStringLiteral("#ec4899"), // Pink (Top 6)
        QStringLiteral("#3b82f6")  // Blue (Top 7)
    };

    double top_sum = 0.0;
    if (!process_list_.isEmpty()) {
        int count = std::min<int>(7, static_cast<int>(process_list_.size()));
        for (int i = 0; i < count; ++i) {
            QVariantMap p = process_list_[i].toMap();
            double w = p.value(QStringLiteral("totalWatts")).toDouble();
            if (w < 0.001) continue;
            top_sum += w;
            QVariantMap m;
            m["name"] = p.value(QStringLiteral("comm")).toString();
            m["pid"] = p.value(QStringLiteral("pid")).toInt();
            m["watts"] = w;
            m["pct"] = std::min(100.0, (w / total_proc_w) * 100.0);
            m["color"] = proc_colors[i];
            proc_list.append(m);
        }
    } else {
        for (int i = 0; i < 2; ++i) {
            if (latest_state_.culprits[i].pid > 0) {
                double w = latest_state_.culprits[i].drain_mw / 1000.0;
                top_sum += w;
                QVariantMap m;
                m["name"] = QString::fromUtf8(latest_state_.culprits[i].comm);
                m["pid"] = latest_state_.culprits[i].pid;
                m["watts"] = w;
                m["pct"] = std::min(100.0, (w / total_proc_w) * 100.0);
                m["color"] = proc_colors[i];
                proc_list.append(m);
            }
        }
    }

    double other_w = std::max(0.0, proc_sum - top_sum);
    if (other_w > 0.01) {
        QVariantMap m;
        m["name"] = QStringLiteral("기타 150+ 프로세스 (Other)");
        m["pid"] = 0;
        m["watts"] = other_w;
        m["pct"] = std::min(100.0, (other_w / total_proc_w) * 100.0);
        m["color"] = QStringLiteral("#64748b"); // Slate gray
        proc_list.append(m);
    }
    process_power_shares_ = proc_list;
}

} // namespace wattcurb::ui
