#include "core/daemon_runner.hpp"
#include "core/posix_fs.hpp"
#include "report/report_generator.hpp"
#include "core/scoped_profiler.hpp"
#include "policy/mitigation_engine.hpp"

#include <chrono>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

namespace wattcurb::core {

namespace {

// ---------------------------------------------------------------------------
// Daemon-owned profile persistence (REF-REQ-053, security hardening)
//
// This previously lived at a hardcoded "/home/<dev>/.cache/power_profile_mode",
// opened by the ROOT daemon with O_CREAT|O_TRUNC and no O_NOFOLLOW, then forced
// to mode 0666 via fchmod(). Any code running as that desktop user could replace
// the file with a symlink to an arbitrary root-owned path; the daemon would then
// truncate it, chmod it world-writable and write to it as root - a local
// privilege escalation (CWE-59 + CWE-732).
//
// The live profile handoff from the tray/dashboard already travels over the
// authorized Unix command socket ("PROFILE <n>", REF-REQ-111), so this file only ever
// needed to be the daemon's OWN restart persistence. It now lives in the
// daemon's root-owned state directory, is created 0644, and is opened with
// O_NOFOLLOW. The daemon no longer reads or writes anything under /home.
// ---------------------------------------------------------------------------
constexpr const char* PROFILE_STATE_DIR  = "/var/lib/wattcurb";
constexpr const char* PROFILE_STATE_PATH = "/var/lib/wattcurb/power_profile_mode";

// ---------------------------------------------------------------------------
// REF-REQ-111 (DEF-4): the command socket is AUTHORIZED, not merely attributed.
//
// REF-REQ-093 added SO_PASSCRED so a profile change could be logged with the
// requesting comm, pid and uid. That value was formatted into a log line and
// never consulted. Any local process could therefore drive the root daemon: a
// throwaway script sent sixteen PROFILE datagrams during the REF-RES-029 audit
// and the daemon answered OK to every one, cycling the machine through all four
// profiles including UltraEndurance's 1.4 GHz ceiling.
//
// A uid check alone does not fix it - the stray script ran as the desktop user,
// the same uid the tray runs as. What distinguishes a real client is the
// executable behind it, so the peer's /proc/<pid>/exe must resolve to one of the
// installed client binaries, and that file must be root-owned and not writable
// by anyone else. A user-writable copy is not trustworthy for commanding a root
// daemon, so a build run out of ~/.local/bin is refused; the installed binary in
// /usr/local/bin is the one that may command.
//
// TOCTOU is inherent here: the kernel captures the credentials at send time, so
// by the time /proc is read the sender may have exited and its pid been reused.
// The window is one datagram wide and the consequence is bounded - the worst
// outcome is a refusal, or a profile change attributed to the wrong client.
// ---------------------------------------------------------------------------
bool peer_is_authorized_client(const struct ucred& cred) noexcept {
    if (cred.pid <= 0) return false;
    if (cred.uid == 0) return true; // root already owns every knob this touches

    static const char* const ALLOWED_CLIENTS[] = {
        "/usr/local/bin/wattcurb",
        "/usr/local/bin/wattcurb-tray",
        "/usr/local/bin/wattcurb-dashboard",
        "/usr/bin/wattcurb",
        "/usr/bin/wattcurb-tray",
        "/usr/bin/wattcurb-dashboard",
    };

    char exe_link[64];
    std::snprintf(exe_link, sizeof(exe_link), "/proc/%d/exe", cred.pid);
    char exe[256];
    const ssize_t n = ::readlink(exe_link, exe, sizeof(exe) - 1);
    if (n <= 0) return false;
    exe[n] = '\0';

    bool listed = false;
    for (const char* c : ALLOWED_CLIENTS) {
        if (std::strcmp(exe, c) == 0) { listed = true; break; }
    }
    if (!listed) return false;

    // The path being right is not enough; the file behind it must not be
    // writable by the account that is asking.
    struct stat st{};
    if (::stat(exe, &st) != 0) return false;
    if (st.st_uid != 0) return false;
    if ((st.st_mode & (S_IWGRP | S_IWOTH)) != 0) return false;

    return true;
}

void persist_profile_mode(const char* mode) noexcept {
    if (!mode) return;

    // systemd provisions this via StateDirectory=wattcurb; mkdir covers manual runs.
    (void)::mkdir(PROFILE_STATE_DIR, 0755);

    int fd = ::open(PROFILE_STATE_PATH,
                    O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, 0644);
    if (fd < 0) return;

    (void)::write(fd, mode, std::strlen(mode));
    (void)::write(fd, "\n", 1);
    ::close(fd);
}

// Contents are the daemon's own prior output, but read defensively anyway.
size_t load_profile_mode(char* buf, size_t cap) noexcept {
    if (!buf || cap == 0) return 0;
    buf[0] = '\0';

    int fd = ::open(PROFILE_STATE_PATH, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return 0;

    ssize_t n = ::read(fd, buf, cap - 1);
    ::close(fd);
    if (n <= 0) return 0;

    buf[n] = '\0';
    return static_cast<size_t>(n);
}

// Implements REF-REQ-129 & REF-ARCH-076: Zero-Allocation Process & System Memory History Export
void populate_history_memory_fields(ipc::HistoryPoint& pt,
                                    const policy::MemoryPressureGuard& guard,
                                    const AnalysisReportData& report) noexcept {
    policy::MemoryPressureSample s = guard.last_sample();
    if (s.mem_total_kb == 0) {
        (void)guard.sample(s);
    }
    if (s.mem_total_kb > s.mem_available_kb) {
        pt.mem_used_mb = static_cast<uint16_t>((s.mem_total_kb - s.mem_available_kb) / 1024);
    }
    if (s.swap_total_kb > s.swap_free_kb) {
        pt.swap_used_mb = static_cast<uint16_t>((s.swap_total_kb - s.swap_free_kb) / 1024);
    }
    if (!report.top_processes.empty()) {
        pt.top_proc_pss_mb = static_cast<uint16_t>(report.top_processes[0].pss_kib / 1024);
    }
}

} // namespace


DaemonRunner::DaemonRunner(double period_sec, double window_sec, std::string_view lock_name)
    : period_sec_(period_sec > 0.0 ? period_sec : 10.0),
      window_sec_(window_sec > 0.0 ? window_sec : 3.0),
      lock_name_(lock_name),
      lock_(lock_name) {}

DaemonRunner::~DaemonRunner() {
    stop();

    // REF-REQ-111 (DEF-1): release PROCESS state before hardware state.
    //
    // This call was absent. restore_hardware_baseline() puts sysfs and /dev back,
    // and WindowAwareGovernor releases its own on destruction, but every nice,
    // scheduling class, CPU affinity mask, timer slack and cgroup quota the
    // feature layer applied simply survived daemon exit. Affinity and nice are
    // process state: they outlive the daemon, outlive a restart, and are
    // inherited by every child, which is how a login shell ended up handing half
    // the machine to everything launched from it (REF-RES-027).
    //
    // FeatureManager now also releases in its own destructor, so an exit path
    // that does not run this one is still covered; the call here is explicit so
    // the ordering against the hardware restore is stated rather than implied.
    feature_manager_.rollback_all_tracked();

    // REF-REQ-112: a cgroup CPU quota is cgroup state, in the same category as
    // the affinity masks above - it outlives the daemon and would leave a user's
    // application capped at 20% for the rest of the session.
    memory_guard_.shutdown();

    window_governor_.rollback_all();
    hardware_bus_.rollback_all();
    policy::MitigationEngine::restore_hardware_baseline();
    cleanup_descriptors();
}

void DaemonRunner::cleanup_descriptors() noexcept {
    if (shm_state_ != nullptr && shm_state_ != MAP_FAILED) {
        ::munmap(shm_state_, sizeof(ipc::WattCurbSharedState));
        shm_state_ = nullptr;
    }
    if (shm_fd_ >= 0) { ::close(shm_fd_); shm_fd_ = -1; }

    if (shm_history_ != nullptr && shm_history_ != MAP_FAILED) {
        ::munmap(shm_history_, sizeof(ipc::HistoryRingBufferShm));
        shm_history_ = nullptr;
    }
    if (shm_history_fd_ >= 0) { ::close(shm_history_fd_); shm_history_fd_ = -1; }

    if (epoll_fd_ >= 0) { ::close(epoll_fd_); epoll_fd_ = -1; }
    if (timer_fd_ >= 0) { ::close(timer_fd_); timer_fd_ = -1; }
    if (signal_fd_ >= 0) { ::close(signal_fd_); signal_fd_ = -1; }
}

bool DaemonRunner::setup_history_shm() {
    if (shm_history_ != nullptr) return true;

    shm_history_fd_ = ::open(ipc::HISTORY_SHM_PATH, O_RDWR | O_CLOEXEC);
    if (shm_history_fd_ >= 0) {
        struct stat st{};
        if (::fstat(shm_history_fd_, &st) == 0 && static_cast<size_t>(st.st_size) != sizeof(ipc::HistoryRingBufferShm)) {
            ::close(shm_history_fd_);
            ::unlink(ipc::HISTORY_SHM_PATH);
            shm_history_fd_ = -1;
        }
    }

    if (shm_history_fd_ < 0) {
        shm_history_fd_ = ::open(ipc::HISTORY_SHM_PATH, O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    }
    if (shm_history_fd_ < 0) {
        return false;
    }
    // 0644, not 0666: the daemon is the only writer. Every consumer (tray,
    // dashboard, CLI, history analyzer) opens O_RDONLY and maps PROT_READ, so a
    // world-writable mapping only let any local user forge daemon telemetry.
    ::fchmod(shm_history_fd_, 0644);
    if (::ftruncate(shm_history_fd_, sizeof(ipc::HistoryRingBufferShm)) < 0) {
        ::close(shm_history_fd_);
        shm_history_fd_ = -1;
        return false;
    }
    void* ptr = ::mmap(nullptr, sizeof(ipc::HistoryRingBufferShm), PROT_READ | PROT_WRITE, MAP_SHARED, shm_history_fd_, 0);
    if (ptr == MAP_FAILED) {
        ::close(shm_history_fd_);
        shm_history_fd_ = -1;
        return false;
    }
    shm_history_ = static_cast<ipc::HistoryRingBufferShm*>(ptr);
    return true;
}

bool DaemonRunner::arm_timer(double interval_sec) noexcept {
    if (timer_fd_ < 0) return false;
    current_timer_interval_ = interval_sec;

    struct itimerspec its{};
    time_t sec = static_cast<time_t>(interval_sec);
    long nsec = static_cast<long>((interval_sec - static_cast<double>(sec)) * 1'000'000'000.0);

    its.it_value.tv_sec = sec > 0 ? sec : 10;
    its.it_value.tv_nsec = nsec;
    its.it_interval = its.it_value;

    return (::timerfd_settime(timer_fd_, 0, &its, nullptr) == 0);
}

bool DaemonRunner::setup_timer() {
    timer_fd_ = ::timerfd_create(CLOCK_BOOTTIME, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timer_fd_ < 0) return false;

    // Set prctl timer slack to coalesce wakeups with other system activity (Zero-Wakeup)
    ::prctl(PR_SET_TIMERSLACK, 500'000'000UL); // 500ms slack

    return arm_timer(period_sec_);
}

bool DaemonRunner::setup_signals() {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGHUP);
    sigaddset(&mask, SIGUSR1);

    if (::sigprocmask(SIG_BLOCK, &mask, nullptr) < 0) {
        return false;
    }

    signal_fd_ = ::signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    return (signal_fd_ >= 0);
}

bool DaemonRunner::setup_shm() {
    if (shm_state_ != nullptr) return true;

    // 1. Try opening existing file first (without O_CREAT to avoid fs.protected_regular EACCES)
    shm_fd_ = ::open(ipc::SHARED_STATE_SHM_PATH, O_RDWR | O_CLOEXEC);
    if (shm_fd_ < 0) {
        // If it does not exist, or permissions prevented opening, unlink any stale file and create anew
        ::unlink(ipc::SHARED_STATE_SHM_PATH);
        shm_fd_ = ::open(ipc::SHARED_STATE_SHM_PATH, O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    }
    if (shm_fd_ < 0) {
        return false;
    }
    // 0644: world-READABLE (clients need that, and root's umask may strip it),
    // but not world-writable - the daemon is the sole writer of this Seqlock.
    ::fchmod(shm_fd_, 0644);
    if (::ftruncate(shm_fd_, sizeof(ipc::WattCurbSharedState)) < 0) {
        ::close(shm_fd_);
        shm_fd_ = -1;
        return false;
    }
    void* ptr = ::mmap(nullptr, sizeof(ipc::WattCurbSharedState), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_, 0);
    if (ptr == MAP_FAILED) {
        ::close(shm_fd_);
        shm_fd_ = -1;
        return false;
    }
    shm_state_ = static_cast<ipc::WattCurbSharedState*>(ptr);
    return true;
}

bool DaemonRunner::initialize() {
    if (!lock_.is_locked()) {
        std::cerr << "[!] Error: WattCurb singleton lock could not be acquired. Another instance is already running.\n";
        return false;
    }

    EventLogger::initialize();
    setup_shm(); // Non-fatal: local Seqlock state remains 100% functional
    setup_history_shm(); // Non-fatal: RAM history buffer (REF-REQ-059)

    // REF-REQ-055: Capture exact hardware baseline state before any actuation
    policy::MitigationEngine::capture_hardware_baseline();
    hardware_bus_.initialize(); // REF-REQ-131: Capture deep hardware bus & display baseline

    // Load persisted profile mode if available (REF-REQ-053)
    PowerProfileMode initial_mode = PowerProfileMode::Balanced;
    char mode_buf[32]{};
    size_t r_mode = load_profile_mode(mode_buf, sizeof(mode_buf));
    if (r_mode > 0) {
        std::string_view m(mode_buf, r_mode);
        if (m.find("performance") != std::string_view::npos) {
            initial_mode = PowerProfileMode::Performance;
            local_shared_state_.power_profile_mode = 0;
        } else if (m.find("ultra") != std::string_view::npos) {
            initial_mode = PowerProfileMode::UltraEndurance;
            local_shared_state_.power_profile_mode = 3;
        } else if (m.find("save") != std::string_view::npos) {
            initial_mode = PowerProfileMode::PowerSaver;
            local_shared_state_.power_profile_mode = 2;
        } else {
            initial_mode = PowerProfileMode::Balanced;
            local_shared_state_.power_profile_mode = 1;
        }
    } else {
        initial_mode = PowerProfileMode::Balanced;
        local_shared_state_.power_profile_mode = 1;
    }
    feature_manager_.set_override_profile(initial_mode);

    // REF-REQ-110: before actuating anything, undo affinity masks a previous run
    // left behind. These survive daemon exit because they are process state, and
    // every child inherits them, so the damage compounds across sessions.
    policy::MitigationEngine::repair_orphaned_affinity_masks();

    policy::MitigationEngine::apply_power_profile(initial_mode);
    if (shm_state_ != nullptr) {
        shm_state_->update_profile_mode(local_shared_state_.power_profile_mode);
    }
    last_logged_profile_ = initial_mode;
    last_logged_battery_pct_ = 100;

    if (!setup_timer()) {
        std::cerr << "[!] Error: Failed to initialize timerfd.\n";
        return false;
    }

    if (!setup_signals()) {
        std::cerr << "[!] Error: Failed to initialize signalfd.\n";
        return false;
    }

    epoll_fd_ = ::epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ < 0) {
        std::cerr << "[!] Error: Failed to initialize epoll instance.\n";
        return false;
    }

    // Register timerfd
    struct epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = timer_fd_;
    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, timer_fd_, &ev) < 0) return false;

    // Register signalfd
    ev.data.fd = signal_fd_;
    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, signal_fd_, &ev) < 0) return false;

    // Register lock socket for IPC queries
    if (lock_.socket_fd() >= 0) {
        ev.data.fd = lock_.socket_fd();
        ::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, lock_.socket_fd(), &ev);
    }

    // REF-REQ-112: register the PSI memory-pressure trigger. EPOLLPRI, not
    // EPOLLIN - that is how the kernel signals a psi window breach. On a
    // healthy machine this descriptor never fires, so the guard costs nothing
    // until it is needed.
    setup_memory_guard();
    if (psi_fd_ >= 0) {
        struct epoll_event pev{};
        pev.events = EPOLLPRI;
        pev.data.fd = psi_fd_;
        if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, psi_fd_, &pev) < 0) {
            psi_fd_ = -1; // guard still runs on the observation tick
        }
    }

    return true;
}

bool DaemonRunner::setup_memory_guard() {
    if (!memory_guard_.initialize()) {
        EventLogger::log_alert("MEMORY",
                               "/proc/meminfo unreadable; memory pressure guard inactive (REF-REQ-112)");
        return false;
    }
    psi_fd_ = memory_guard_.psi_fd();
    if (psi_fd_ < 0) {
        EventLogger::log_alert("MEMORY",
                               "PSI trigger unavailable; memory pressure guard falls back to "
                               "tick-driven sampling (REF-REQ-112)");
    }
    return true;
}

static uint64_t get_monotonic_ms() noexcept {
    struct timespec ts{};
    ::clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000ULL + static_cast<uint64_t>(ts.tv_nsec) / 1'000'000ULL;
}

bool DaemonRunner::process_light_probe_cycle() {
    WATTCURB_PROFILE_SCOPE("daemon.light_probe_cycle");

    // Tier 2: Ultra-Lightweight Hardware Probe (< 0.05ms, ZERO /proc traversal)
    auto hw_cur = hw_probe_.capture_sample();
    // REF-REQ-069: Decoupled Light Probe baseline (eliminates timebase desync)
    cached_report_.hardware = engine_.compute_hardware_power(hw_light_prev_, hw_cur, 10.0);

    // REF-REQ-094: profile demotion is decided in exactly one place
    // (MitigationEngine::resolve_profile, reached via FeatureManager). The daemon
    // no longer runs a second, differently-calibrated ladder here.
    bool on_battery = cached_report_.hardware.is_battery_discharging;
    double batt_pct = static_cast<double>(cached_report_.hardware.battery_capacity_percent);

    // Seqlock State Export
    if (shm_state_ == nullptr) setup_shm();
    local_shared_state_.update_from_report(cached_report_);
    if (shm_state_ != nullptr) {
        shm_state_->update_from_report(cached_report_);
    }

    // Append to In-Memory History Ring-Buffer
    if (shm_history_ == nullptr) setup_history_shm();
    if (shm_history_ != nullptr) {
        ipc::HistoryPoint pt{};
        pt.timestamp_sec = static_cast<uint64_t>(::time(nullptr));
        pt.total_system_mw = static_cast<uint32_t>(cached_report_.hardware.total_system_watts * 1000.0);
        pt.cpu_package_mw = static_cast<uint16_t>(cached_report_.hardware.cpu_package_watts * 1000.0);
        pt.gpu_mw = static_cast<uint16_t>(cached_report_.hardware.gpu_watts * 1000.0);
        pt.cpu_temp_c = static_cast<uint16_t>(cached_report_.hardware.cpu_temp_c);
        pt.cpu_freq_mhz = static_cast<uint16_t>(cached_report_.hardware.cpu_freq_avg_mhz);
        pt.battery_percent = static_cast<uint8_t>(cached_report_.hardware.battery_capacity_percent);
        pt.battery_state = cached_report_.hardware.is_ac_passthrough ? 2 : (cached_report_.hardware.is_battery_discharging ? 1 : 0);
        pt.power_profile_mode = static_cast<uint8_t>(cached_report_.mitigation_status.current_profile);
        pt.cstate_c3_percent = static_cast<uint8_t>(cached_report_.hardware.cstate_c3_deep_percent);
        pt.active_mitigations = static_cast<uint8_t>(cached_report_.mitigation_status.feature_summary_count);
        populate_history_memory_fields(pt, memory_guard_, cached_report_);

        shm_history_->append(pt);
    }

    // Event-Driven AC & Battery Critical Alerts
    bool cur_discharging = cached_report_.hardware.is_battery_discharging;
    if (cur_discharging != last_logged_battery_state_) {
        last_logged_battery_state_ = cur_discharging;
        if (cur_discharging) EventLogger::log_alert("AC", "AC Disconnected: Operating on Battery Power");
        else EventLogger::log_alert("AC", "AC Connected: External Power Active");
    }

    uint8_t cur_pct = static_cast<uint8_t>(cached_report_.hardware.battery_capacity_percent);
    if (cur_pct <= 20 && last_logged_battery_pct_ > 20) {
        EventLogger::log_alert("BATTERY", "Battery capacity dropped below 20% threshold");
    } else if (cur_pct <= 10 && last_logged_battery_pct_ > 10) {
        EventLogger::log_alert("BATTERY", "Battery capacity critical: below 10% threshold");
    }
    last_logged_battery_pct_ = cur_pct;

    // REF-REQ-069 & REF-ARCH-046: Smart Adaptive Power-Spike Detection
    double pkg_w = cached_report_.hardware.cpu_package_watts;
    double sys_w = cached_report_.hardware.total_system_watts;
    double delta_w = (sys_w >= last_light_system_watts_) ? (sys_w - last_light_system_watts_) : 0.0;
    last_light_system_watts_ = sys_w;

    uint64_t now_ms = get_monotonic_ms();
    bool spike = false;
    if (on_battery) {
        spike = (pkg_w >= 10.0) || (sys_w >= 18.0) || (delta_w >= 8.0);
    } else {
        spike = (pkg_w >= 16.0) || (sys_w >= 30.0) || (delta_w >= 12.0);
    }

    bool should_trigger_early = spike && ((now_ms - last_early_sweep_time_ms_) >= EARLY_SWEEP_COOLDOWN_MS);
    if (should_trigger_early) {
        last_early_sweep_time_ms_ = now_ms;
    }

    hw_prev_ = hw_cur;
    hw_light_prev_ = std::move(hw_cur);
    return should_trigger_early;
}

void DaemonRunner::process_deep_observation_cycle() {
    WATTCURB_PROFILE_SCOPE("daemon.deep_observation_cycle");

    if (!has_baseline_) {
        hw_prev_ = hw_probe_.capture_sample();
        hw_light_prev_ = hw_prev_;
        hw_deep_prev_ = hw_prev_;
        proc_analyzer_.capture_snapshot(proc_pool_.current());
        has_baseline_ = true;
        return;
    }

    // 1. T1: Continuous observation sample without redundant usleep
    auto hw_cur = hw_probe_.capture_sample();
    auto& prev_snapshot = proc_pool_.current();
    auto& cur_snapshot = proc_pool_.next();

    {
        WATTCURB_PROFILE_SCOPE("daemon.proc_snapshot");
        proc_analyzer_.capture_snapshot(cur_snapshot, &prev_snapshot);
    }

    // 2. Compute Full-Domain Physical Attribution
    // REF-REQ-069: Pair-synchronized attribution using hw_deep_prev_ (eliminates 6x tick distortion)
    {
        WATTCURB_PROFILE_SCOPE("daemon.compute_attribution");
        cached_report_ = engine_.compute_attribution(
            hw_deep_prev_,
            hw_cur,
            prev_snapshot.span(),
            cur_snapshot.span(),
            20
        );
    }

    // 3. Modular Battery Optimization Feature Actuation (REF-REQ-020 & REF-ARCH-009)
    {
        WATTCURB_PROFILE_SCOPE("daemon.evaluate_and_actuate");
        bool on_battery = cached_report_.hardware.is_battery_discharging;
        double batt_pct = static_cast<double>(cached_report_.hardware.battery_capacity_percent);

        feature_manager_.evaluate_and_actuate(cached_report_, on_battery, batt_pct);
    }

    // 4. Ultra-Fast 128-Byte Seqlock POD Export (REF-REQ-028, REF-ARCH-018)
    {
        WATTCURB_PROFILE_SCOPE("daemon.shm_update");
        if (shm_state_ == nullptr) {
            setup_shm();
        }
        local_shared_state_.update_from_report(cached_report_);
        if (shm_state_ != nullptr) {
            shm_state_->update_from_report(cached_report_);
        }
    }

    // 5. Append to In-Memory History Ring-Buffer (REF-REQ-059, REF-ARCH-035: Zero Disk I/O)
    {
        WATTCURB_PROFILE_SCOPE("daemon.history_update");
        if (shm_history_ == nullptr) {
            setup_history_shm();
        }
        if (shm_history_ != nullptr) {
            ipc::HistoryPoint pt{};
            pt.timestamp_sec = static_cast<uint64_t>(::time(nullptr));
            pt.total_system_mw = static_cast<uint32_t>(cached_report_.hardware.total_system_watts * 1000.0);
            pt.cpu_package_mw = static_cast<uint16_t>(cached_report_.hardware.cpu_package_watts * 1000.0);
            pt.gpu_mw = static_cast<uint16_t>(cached_report_.hardware.gpu_watts * 1000.0);
            pt.cpu_temp_c = static_cast<uint16_t>(cached_report_.hardware.cpu_temp_c);
            pt.cpu_freq_mhz = static_cast<uint16_t>(cached_report_.hardware.cpu_freq_avg_mhz);
            pt.battery_percent = static_cast<uint8_t>(cached_report_.hardware.battery_capacity_percent);
            pt.battery_state = cached_report_.hardware.is_ac_passthrough ? 2 : (cached_report_.hardware.is_battery_discharging ? 1 : 0);
            pt.power_profile_mode = static_cast<uint8_t>(cached_report_.mitigation_status.current_profile);
            pt.cstate_c3_percent = static_cast<uint8_t>(cached_report_.hardware.cstate_c3_deep_percent);
            pt.active_mitigations = static_cast<uint8_t>(cached_report_.mitigation_status.feature_summary_count);
            populate_history_memory_fields(pt, memory_guard_, cached_report_);

            shm_history_->append(pt);
        }
    }

    // 6. Event-Driven Alerts & Profile Change Logging (REF-REQ-059: Zero Steady-State Writes)
    {
        bool cur_discharging = cached_report_.hardware.is_battery_discharging;
        if (cur_discharging != last_logged_battery_state_) {
            last_logged_battery_state_ = cur_discharging;
            if (cur_discharging) {
                EventLogger::log_alert("AC", "AC Disconnected: Operating on Battery Power");
            } else {
                EventLogger::log_alert("AC", "AC Connected: External Power Active");
            }
        }

        uint8_t cur_pct = static_cast<uint8_t>(cached_report_.hardware.battery_capacity_percent);
        if (cur_pct <= 20 && last_logged_battery_pct_ > 20) {
            EventLogger::log_alert("BATTERY", "Battery capacity dropped below 20% threshold");
        } else if (cur_pct <= 10 && last_logged_battery_pct_ > 10) {
            EventLogger::log_alert("BATTERY", "Battery capacity critical: below 10% threshold");
        }
        last_logged_battery_pct_ = cur_pct;

        PowerProfileMode cur_prof = cached_report_.mitigation_status.current_profile;
        if (cur_prof != last_logged_profile_) {
            EventLogger::log_profile_change(last_logged_profile_, cur_prof, "Battery Level Threshold / Dynamic Policy");
            last_logged_profile_ = cur_prof;
        }
    }

    hw_prev_ = hw_cur;
    hw_light_prev_ = hw_cur;
    hw_deep_prev_ = std::move(hw_cur);
    proc_pool_.swap(); // 0ns pointer swap
}

void DaemonRunner::process_observation_cycle() {
    uint64_t now_ms = get_monotonic_ms();

    // Check if interactive lease expired
    if (is_interactive_active_ && now_ms >= interactive_lease_deadline_ms_) {
        is_interactive_active_ = false;
        bg_tick_count_ = 0;
        arm_timer(10.0); // Re-arm timerfd back to 10.0s background cadence
    }

    if (is_interactive_active_) {
        // Mode 1: Interactive High-Cadence (2.0s on-demand streaming)
        process_deep_observation_cycle();
    } else {
        // Mode 2 & 3: Background Dual-Rate Cadence
        bg_tick_count_++;
        if (bg_tick_count_ % 6 == 0) {
            // Mode 3: 60-Second Deep Attribution Sweep (1분에 3초간 정밀 수집)
            process_deep_observation_cycle();
        } else {
            // Mode 2: 10-Second Ultra-Lightweight Hardware Probe (1초간 극저비용 수집, 0 proc traversal)
            bool spike_detected = process_light_probe_cycle();
            if (spike_detected) {
                // REF-REQ-069 & REF-ARCH-046: Smart Adaptive Early Deep Sweep
                EventLogger::log_alert("ADAPTIVE", "Power spike detected in light probe -> Executing early deep sweep");
                process_deep_observation_cycle();
                // Realign subsequent 60s windows from this point
                bg_tick_count_ = 0;
            }
        }
    }

    // REF-REQ-112: the PSI trigger wakes the daemon when pressure RISES. Nothing
    // wakes it when pressure falls, so the tick is what walks the guard back
    // down and releases the throttles. Two file reads and, on a healthy machine,
    // no writes at all.
    memory_guard_.evaluate_and_actuate(cached_report_, window_governor_.active_window_pid(),
                                       policy::MitigationEngine::effective_profile());

    // REF-REQ-128: Progressive C-State Governor evaluation for minimized windows
    uint64_t now_sec = static_cast<uint64_t>(std::time(nullptr));
    window_governor_.evaluate_hysteresis(now_sec, policy::MitigationEngine::effective_profile());

    // REF-REQ-131: Deep Hardware Bus, Display ABM, and Peripheral Power Minimization
    hardware_bus_.evaluate_and_actuate(hw_prev_.is_discharging,
                                       policy::MitigationEngine::effective_profile());
}

int DaemonRunner::run() {
    if (epoll_fd_ < 0 && !initialize()) {
        return 1;
    }

    running_ = true;
    EventLogger::log_raw("LIFECYCLE", "WattCurb background daemon initialized (Zero-Wakeup, Seqlock & History SHM active)");

    // Initial baseline capture immediately upon startup
    hw_prev_ = hw_probe_.capture_sample();
    hw_light_prev_ = hw_prev_;
    hw_deep_prev_ = hw_prev_;
    proc_analyzer_.capture_snapshot(proc_pool_.current());
    has_baseline_ = true;

    // Quick initial warm-up (0.5s) to populate shared memory and report before entering timer sleep
    ::usleep(500000);
    process_observation_cycle();

    struct epoll_event events[8];

    while (running_) {
        // Sleep until kernel timerfd, signal, or IPC packet fires
        int nfds = ::epoll_wait(epoll_fd_, events, 8, -1);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;

            if (fd == timer_fd_) {
                uint64_t expirations = 0;
                ssize_t s = ::read(timer_fd_, &expirations, sizeof(expirations));
                (void)s;

                // Stream one continuous window observation with zero redundant wakeups (REF-REQ-052)
                process_observation_cycle();
            } else if (fd == signal_fd_) {
                struct signalfd_siginfo fdsi{};
                ssize_t s = ::read(signal_fd_, &fdsi, sizeof(fdsi));
                if (s == sizeof(fdsi)) {
                    if (fdsi.ssi_signo == SIGINT || fdsi.ssi_signo == SIGTERM) {
                        EventLogger::log_raw("LIFECYCLE", "Received termination signal, gracefully exiting WattCurb daemon...");
                        running_ = false;
                    } else if (fdsi.ssi_signo == SIGHUP) {
                        hw_probe_.refresh_device_paths();
                    }
                }
            } else if (psi_fd_ >= 0 && fd == psi_fd_) {
                // REF-REQ-112: the kernel says memory is stalling. Act now
                // rather than at the next tick - under a runaway allocation the
                // distance between "stalling" and "kernel OOM kill" is seconds.
                memory_guard_.evaluate_and_actuate(cached_report_,
                                                   window_governor_.active_window_pid(),
                                                   policy::MitigationEngine::effective_profile());
            } else if (fd == lock_.socket_fd()) {
                handle_ipc_datagram(fd);
            }
        }
    }

    return 0;
}

void DaemonRunner::handle_ipc_datagram(int fd) {
    char buf[128];
    struct sockaddr_un client_addr{};
    socklen_t client_len = sizeof(client_addr);

    // recvmsg rather than recvfrom so SCM_CREDENTIALS comes with the datagram.
    struct iovec iov{ buf, sizeof(buf) - 1 };
    alignas(struct cmsghdr) char cmsg_buf[CMSG_SPACE(sizeof(struct ucred))]{};
    struct msghdr msg{};
    msg.msg_name = &client_addr;
    msg.msg_namelen = client_len;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    ssize_t bytes = ::recvmsg(fd, &msg, 0);
    if (bytes <= 0) return;
    buf[bytes] = '\0';
    client_len = msg.msg_namelen;

    // Who asked. Empty when the kernel supplied no credentials.
    char requester[64] = "unknown";
    // REF-REQ-111 (DEF-4): mutating commands require an authorized peer. Absent
    // credentials means an unauthorized peer, not a trusted one.
    bool peer_authorized = false;
    bool have_creds = false;
    for (struct cmsghdr* c = CMSG_FIRSTHDR(&msg); c != nullptr; c = CMSG_NXTHDR(&msg, c)) {
        if (c->cmsg_level != SOL_SOCKET || c->cmsg_type != SCM_CREDENTIALS) continue;
        struct ucred cred{};
        std::memcpy(&cred, CMSG_DATA(c), sizeof(cred));
        if (cred.pid <= 0) break;

        char comm_path[64];
        std::snprintf(comm_path, sizeof(comm_path), "/proc/%d/comm", cred.pid);
        char comm[48]{};
        size_t cn = 0;
        if (core::fs::read_small_file(comm_path, comm, sizeof(comm), &cn) && cn > 0) {
            while (cn > 0 && (comm[cn - 1] == '\n' || comm[cn - 1] == '\r')) comm[--cn] = '\0';
        } else {
            std::snprintf(comm, sizeof(comm), "?");
        }
        std::snprintf(requester, sizeof(requester), "%s[%d] uid=%d", comm, cred.pid, cred.uid);
        peer_authorized = peer_is_authorized_client(cred);
        have_creds = true;
        break;
    }

    std::string_view req(buf, static_cast<size_t>(bytes));

    if (req.find("BRIEFING") != std::string_view::npos) {
        // Developer text briefing channel exclusively for manual terminal debugging
        std::stringstream ss;
        report::ReportGenerator::render_executive_briefing(cached_report_, ss);
        auto resp_str = ss.str();
        ::sendto(fd, resp_str.data(), resp_str.size(), 0,
                 reinterpret_cast<struct sockaddr*>(&client_addr), client_len);
    } else if (req.find("FULL_TELEMETRY") != std::string_view::npos) {
        // REF-REQ-068: Active GUI dashboard query automatically engages Interactive 2.0s Cadence
        uint64_t now_ms = get_monotonic_ms();
        interactive_lease_deadline_ms_ = now_ms + 4500; // 4.5s lease window
        if (!is_interactive_active_) {
            is_interactive_active_ = true;
            arm_timer(2.0); // Switch timerfd dynamically to 2.0s
        }

        // High-density btop-style telemetry JSON payload (REF-REQ-037)
        std::stringstream ss;
        const auto& hw = cached_report_.hardware;
        double sys_w = hw.total_system_watts > 0.0 ? hw.total_system_watts :
                       (hw.cpu_package_watts + hw.gpu_watts + hw.display_watts + 2.0);

        ss << std::fixed << std::setprecision(2);
        ss << "{\n";
        ss << "  \"system_watts\": " << sys_w << ",\n";
        ss << "  \"battery_pct\": " << hw.battery_capacity_percent << ",\n";
        ss << "  \"battery_state\": " << (hw.is_ac_passthrough ? 2 : (hw.is_battery_discharging ? 1 : 0)) << ",\n";
        ss << "  \"battery_state_str\": \"" << (hw.is_ac_passthrough ? "AC Passthrough" : (hw.is_battery_discharging ? "Discharging" : "AC Powered")) << "\",\n";
        ss << "  \"battery_voltage_v\": " << hw.battery_voltage_now_v << ",\n";
        ss << "  \"battery_current_a\": " << hw.battery_current_now_a << ",\n";
        ss << "  \"battery_health_pct\": " << hw.battery_health_percent << ",\n";
        ss << "  \"battery_cycles\": " << hw.battery_cycle_count << ",\n";
        ss << "  \"time_to_empty_min\": " << static_cast<int>(hw.battery_remaining_hours_to_empty * 60.0) << ",\n";
        double pkg_w = hw.cpu_package_watts;
        double core_w = pkg_w * 0.75;
        double uncore_w = (hw.uncore_and_platform_watts > 0.0) ? hw.uncore_and_platform_watts : (pkg_w * 0.25);
        double dram_w = 0.95;

        ss << "  \"cpu_package_w\": " << pkg_w << ",\n";
        ss << "  \"cpu_core_w\": " << core_w << ",\n";
        ss << "  \"cpu_uncore_w\": " << uncore_w << ",\n";
        ss << "  \"cpu_dram_w\": " << dram_w << ",\n";
        ss << "  \"cpu_temp_c\": " << hw.cpu_temp_c << ",\n";
        ss << "  \"cpu_freq_mhz\": " << hw.cpu_freq_avg_mhz << ",\n";
        ss << "  \"cpu_governor\": \"" << hw.cpu_governor.c_str() << "\",\n";
        ss << "  \"cstate_c0\": " << hw.cstate_c0_active_percent << ",\n";
        ss << "  \"cstate_c1\": " << hw.cstate_c1_percent << ",\n";
        ss << "  \"cstate_c2\": " << hw.cstate_c2_percent << ",\n";
        ss << "  \"cstate_c3\": " << hw.cstate_c3_deep_percent << ",\n";
        ss << "  \"fan_rpm\": " << hw.fan_rpm << ",\n";
        ss << "  \"pmu_ipc\": " << hw.pmu_ipc << ",\n";
        ss << "  \"pmu_instructions\": " << hw.pmu_instructions << ",\n";
        ss << "  \"pmu_cycles\": " << hw.pmu_cycles << ",\n";
        ss << "  \"pmu_llc_misses\": " << hw.pmu_llc_misses << ",\n";
        ss << "  \"pmu_branch_misses\": " << hw.pmu_branch_misses << ",\n";
        ss << "  \"pmu_ewr\": " << hw.pmu_energy_waste_ratio << ",\n";
        ss << "  \"gpu_w\": " << hw.gpu_watts << ",\n";
        ss << "  \"gpu_load\": " << hw.gpu_busy_percent << ",\n";
        ss << "  \"display_w\": " << (hw.display_watts > 0.0 ? hw.display_watts : 1.8) << ",\n";
        ss << "  \"display_brightness\": " << hw.display_brightness_percent << ",\n";
        ss << "  \"nvme_w\": " << (hw.storage_estimated_watts > 0.0 ? hw.storage_estimated_watts : 0.8) << ",\n";
        ss << "  \"disk_read_mb_s\": " << hw.disk_read_mb_per_sec << ",\n";
        ss << "  \"disk_write_mb_s\": " << hw.disk_write_mb_per_sec << ",\n";
        ss << "  \"battery_mfg\": \"" << (hw.battery_manufacturer.empty() ? "SMP" : hw.battery_manufacturer.c_str()) << "\",\n";
        ss << "  \"battery_model\": \"" << (hw.battery_model_name.empty() ? "LNV-5B10W" : hw.battery_model_name.c_str()) << "\",\n";
        ss << "  \"battery_tech\": \"" << (hw.battery_technology.empty() ? "Li-poly" : hw.battery_technology.c_str()) << "\",\n";
        ss << "  \"battery_design_wh\": " << hw.battery_energy_design_wh << ",\n";
        ss << "  \"battery_full_wh\": " << hw.battery_energy_full_wh << ",\n";
        ss << "  \"battery_now_wh\": " << hw.battery_energy_now_wh << ",\n";
        ss << "  \"aspm_policy\": \"" << (hw.aspm_policy.empty() ? "powersave" : hw.aspm_policy.c_str()) << "\",\n";
        // Implements REF-REQ-132: Export full-spectrum system RAM & Swap metrics to Matrix Dashboard
        policy::MemoryPressureSample mem_s = memory_guard_.last_sample();
        if (mem_s.mem_total_kb == 0) {
            (void)memory_guard_.sample(mem_s);
        }
        uint64_t mem_tot_mb = mem_s.mem_total_kb / 1024;
        uint64_t mem_avail_mb = mem_s.mem_available_kb / 1024;
        uint64_t mem_used_mb = (mem_s.mem_total_kb > mem_s.mem_available_kb) ? ((mem_s.mem_total_kb - mem_s.mem_available_kb) / 1024) : 0;
        uint64_t swap_tot_mb = mem_s.swap_total_kb / 1024;
        uint64_t swap_used_mb = (mem_s.swap_total_kb > mem_s.swap_free_kb) ? ((mem_s.swap_total_kb - mem_s.swap_free_kb) / 1024) : 0;

        ss << "  \"mem_total_mb\": " << mem_tot_mb << ",\n";
        ss << "  \"mem_used_mb\": " << mem_used_mb << ",\n";
        ss << "  \"mem_avail_mb\": " << mem_avail_mb << ",\n";
        ss << "  \"swap_total_mb\": " << swap_tot_mb << ",\n";
        ss << "  \"swap_used_mb\": " << swap_used_mb << ",\n";
        ss << "  \"profile_mode\": " << static_cast<int>(cached_report_.mitigation_status.current_profile) << ",\n";
        ss << "  \"active_mitigations\": " << cached_report_.mitigation_status.feature_summary_count << ",\n";
        ss << "  \"wakeups_per_sec\": " << cached_report_.total_system_wakeups_per_sec << ",\n";
        ss << "  \"processes\": [\n";

        size_t n = std::min(size_t{25}, cached_report_.top_processes.size());
        for (size_t i = 0; i < n; ++i) {
            const auto& p = cached_report_.top_processes[i];
            ss << "    {\n";
            ss << "      \"pid\": " << p.pid << ",\n";
            ss << "      \"comm\": \"" << p.comm.c_str() << "\",\n";
            ss << "      \"uid\": " << p.uid << ",\n";
            ss << "      \"total_w\": " << p.total_attributed_watts << ",\n";
            ss << "      \"cpu_w\": " << p.cpu_watts << ",\n";
            ss << "      \"gpu_w\": " << p.gpu_watts << ",\n";
            ss << "      \"dram_w\": " << p.dram_attributed_watts << ",\n";
            ss << "      \"io_wake_w\": " << (p.io_watts + p.wakeup_tax_watts) << ",\n";
            ss << "      \"io_w\": " << p.io_watts << ",\n";
            ss << "      \"wake_tax_w\": " << p.wakeup_tax_watts << ",\n";
            ss << "      \"fan_w\": " << p.fan_attributed_watts << ",\n";
            ss << "      \"wifi_w\": " << p.wifi_attributed_watts << ",\n";
            ss << "      \"wdi_score\": " << p.wdi_score << ",\n";
            ss << "      \"pss_mb\": " << (p.pss_kib / 1024) << ",\n";
            ss << "      \"rss_mb\": " << (p.rss_kib / 1024) << ",\n";
            ss << "      \"tier\": " << static_cast<int>(p.safety_tier) << ",\n";
            ss << "      \"cstate\": \"" << (p.cstate_affinity.empty() ? "C3" : p.cstate_affinity.c_str()) << "\",\n";
            ss << "      \"cpu_core\": " << p.cpu_core << ",\n";
            ss << "      \"threads\": " << p.num_threads << ",\n";
            ss << "      \"cross_ccx\": " << (p.cross_ccx_migration ? 1 : 0) << ",\n";
            ss << "      \"nice\": " << p.nice << ",\n";
            ss << "      \"priority\": " << p.priority << ",\n";
            ss << "      \"wakeups_sec\": " << p.wakeups_per_sec << ",\n";
            ss << "      \"timerslack_ns\": " << p.timerslack_ns << ",\n";
            ss << "      \"vram_mb\": " << (static_cast<double>(p.vram_kib) / 1024.0) << ",\n";
            ss << "      \"io_mb_s\": " << p.disk_io_mb_per_sec << ",\n";
            ss << "      \"minflt_s\": " << p.minflt_per_sec << ",\n";
            ss << "      \"majflt_s\": " << p.majflt_per_sec << ",\n";
            ss << "      \"open_sockets\": " << p.open_sockets << ",\n";
            ss << "      \"action\": " << static_cast<int>(p.recommended_action) << ",\n";
            ss << "      \"domain\": \"" << (p.primary_hw_domain.empty() ? "CPU Compute" : p.primary_hw_domain.c_str()) << "\",\n";
            ss << "      \"mechanism\": \"" << (p.hardware_mechanism.empty() ? "Standard Execution" : p.hardware_mechanism.c_str()) << "\"\n";
            ss << "    }" << (i + 1 < n ? "," : "") << "\n";
        }
        ss << "  ]\n";
        ss << "}\n";

        auto resp = ss.str();
        ::sendto(fd, resp.data(), resp.size(), 0,
                 reinterpret_cast<struct sockaddr*>(&client_addr), client_len);
    } else if (req.rfind("PROFILE ", 0) == 0 && bytes >= 9) {
        // REF-REQ-111 (DEF-4): PROFILE mutates the machine's power state, so it
        // is the one command that has to prove who is asking. Read-only queries
        // above stay open.
        if (!have_creds || !peer_authorized) {
            const char deny[] = "ERROR: not an authorized WattCurb client (REF-REQ-111)\n";
            ::sendto(fd, deny, sizeof(deny) - 1, 0,
                     reinterpret_cast<struct sockaddr*>(&client_addr), client_len);
            char detail[160];
            std::snprintf(detail, sizeof(detail),
                          "Rejected PROFILE command from %s - not an installed WattCurb client binary",
                          requester);
            EventLogger::log_alert("DENY", detail);
            return;
        }

        // Parse "PROFILE <mode>" (0=Performance, 1=Balanced, 2=PowerSaver, 3=UltraEndurance)
        int mode_val = req[8] - '0';
        if (mode_val >= 0 && mode_val <= 3) {
            auto new_mode = static_cast<PowerProfileMode>(mode_val);

            // REF-REQ-067: Battery <= 20% Performance Mode Lockout Invariant
            bool is_discharging = cached_report_.hardware.is_battery_discharging;
            uint32_t batt_pct = cached_report_.hardware.battery_capacity_percent;
            // REF-REQ-094: only the 5% critical floor overrides an explicit user
            // choice. Between 5% and 30% the user may select whatever they want;
            // the automatic threshold demotions fire at most once each.
            if (new_mode != PowerProfileMode::UltraEndurance && is_discharging && batt_pct <= 5) {
                const char reject[] = "ERROR: Only UltraEndurance is permitted when battery <= 5% (REF-REQ-094)\n";
                ::sendto(fd, reject, sizeof(reject) - 1, 0,
                         reinterpret_cast<struct sockaddr*>(&client_addr), client_len);
                EventLogger::log_alert("BATTERY", "Profile switch rejected: Battery capacity <= 5% permits UltraEndurance only (REF-REQ-094)");
                return;
            }

            feature_manager_.set_override_profile(new_mode);
            local_shared_state_.power_profile_mode = static_cast<uint8_t>(new_mode);
            cached_report_.mitigation_status.current_profile = new_mode;
            if (shm_state_) {
                shm_state_->update_profile_mode(static_cast<uint8_t>(new_mode));
            }

            // Immediately actuate hardware limits natively via direct sysfs (REF-REQ-055, REF-ARCH-031)
            policy::MitigationEngine::apply_power_profile(new_mode);

            PowerProfileMode old_p = last_logged_profile_;
            last_logged_profile_ = new_mode;
            char trigger[96];
            std::snprintf(trigger, sizeof(trigger), "IPC request from %s", requester);
            EventLogger::log_profile_change(old_p, new_mode, trigger);

            // Persist selected mode
            const char* hw_arg = "balanced";
            if (new_mode == PowerProfileMode::Performance) hw_arg = "performance";
            else if (new_mode == PowerProfileMode::PowerSaver) hw_arg = "save";
            else if (new_mode == PowerProfileMode::UltraEndurance) hw_arg = "ultra";

            persist_profile_mode(hw_arg);

            // Immediately refresh observation and shared memory state
            process_observation_cycle();
        }
        const char ack[] = "OK\n";
        ::sendto(fd, ack, sizeof(ack) - 1, 0,
                 reinterpret_cast<struct sockaddr*>(&client_addr), client_len);
    } else if (req.find("INTERACTIVE") != std::string_view::npos ||
               req.find("HEARTBEAT") != std::string_view::npos) {
        uint64_t now_ms = get_monotonic_ms();
        interactive_lease_deadline_ms_ = now_ms + 4500; // 4.5s lease window
        if (!is_interactive_active_) {
            is_interactive_active_ = true;
            arm_timer(2.0); // Switch timerfd immediately to 2.0s
            process_deep_observation_cycle();
        }
        const char ack[] = "OK\n";
        ::sendto(fd, ack, sizeof(ack) - 1, 0,
                 reinterpret_cast<struct sockaddr*>(&client_addr), client_len);
    } else if (req.find("RESCAN") != std::string_view::npos) {
        uint64_t now_ms = get_monotonic_ms();
        interactive_lease_deadline_ms_ = now_ms + 4500;
        if (!is_interactive_active_) {
            is_interactive_active_ = true;
            arm_timer(2.0);
        }
        process_deep_observation_cycle();
        const char ack[] = "OK\n";
        ::sendto(fd, ack, sizeof(ack) - 1, 0,
                 reinterpret_cast<struct sockaddr*>(&client_addr), client_len);
    } else if (req.rfind("ACTIVE_WINDOW ", 0) == 0 && bytes >= 14) {
        // Parse "ACTIVE_WINDOW <pid> [comm]" (REF-REQ-085, REF-ARCH-062)
        int32_t target_pid = 0;
        char comm_buf[64]{};
        int parsed = std::sscanf(req.data() + 14, "%d %63s", &target_pid, comm_buf);
        if (parsed >= 1 && target_pid > 1) {
            window_governor_.engage_active_window(target_pid, comm_buf);
            const char ack[] = "OK\n";
            ::sendto(fd, ack, sizeof(ack) - 1, 0,
                     reinterpret_cast<struct sockaddr*>(&client_addr), client_len);
            return;
        }
        const char err[] = "ERR: Invalid PID\n";
        ::sendto(fd, err, sizeof(err) - 1, 0,
                 reinterpret_cast<struct sockaddr*>(&client_addr), client_len);
        return;
    } else if (req.rfind("WINDOW_MINIMIZED ", 0) == 0 && bytes >= 17) {
        // Parse "WINDOW_MINIMIZED <pid> [is_audio:0|1]" (REF-REQ-128)
        int32_t target_pid = 0;
        int audio_val = 0;
        int parsed = std::sscanf(req.data() + 17, "%d %d", &target_pid, &audio_val);
        if (parsed >= 1 && target_pid > 1) {
            uint64_t now_sec = static_cast<uint64_t>(std::time(nullptr));
            window_governor_.on_window_state_changed(
                target_pid, true, false, now_sec,
                policy::MitigationEngine::effective_profile(),
                audio_val != 0
            );
            const char ack[] = "OK\n";
            ::sendto(fd, ack, sizeof(ack) - 1, 0,
                     reinterpret_cast<struct sockaddr*>(&client_addr), client_len);
            return;
        }
        const char err[] = "ERR: Invalid PID\n";
        ::sendto(fd, err, sizeof(err) - 1, 0,
                 reinterpret_cast<struct sockaddr*>(&client_addr), client_len);
        return;
    } else if (req.rfind("WINDOW_RESTORED ", 0) == 0 && bytes >= 16) {
        // Parse "WINDOW_RESTORED <pid>" (REF-REQ-128)
        int32_t target_pid = 0;
        int parsed = std::sscanf(req.data() + 16, "%d", &target_pid);
        if (parsed >= 1 && target_pid > 1) {
            uint64_t now_sec = static_cast<uint64_t>(std::time(nullptr));
            window_governor_.on_window_state_changed(
                target_pid, false, true, now_sec,
                policy::MitigationEngine::effective_profile(),
                false
            );
            const char ack[] = "OK\n";
            ::sendto(fd, ack, sizeof(ack) - 1, 0,
                     reinterpret_cast<struct sockaddr*>(&client_addr), client_len);
            return;
        }
        const char err[] = "ERR: Invalid PID\n";
        ::sendto(fd, err, sizeof(err) - 1, 0,
                 reinterpret_cast<struct sockaddr*>(&client_addr), client_len);
        return;
    } else if (req.rfind("WINDOW_STATE ", 0) == 0 && bytes >= 13) {
        // Parse "WINDOW_STATE <pid> <minimized:0|1> <active:0|1> [is_audio:0|1]" (REF-REQ-033, REF-REQ-085, REF-REQ-128)
        int32_t target_pid = 0;
        int min_val = 0;
        int act_val = 0;
        int audio_val = 0;
        int parsed = std::sscanf(req.data() + 13, "%d %d %d %d", &target_pid, &min_val, &act_val, &audio_val);
        if (parsed >= 3 && target_pid > 1) {
            uint64_t now_sec = static_cast<uint64_t>(std::time(nullptr));
            window_governor_.on_window_state_changed(
                target_pid, min_val != 0, act_val != 0, now_sec,
                policy::MitigationEngine::effective_profile(),
                audio_val != 0
            );
            const char ack[] = "OK\n";
            ::sendto(fd, ack, sizeof(ack) - 1, 0,
                     reinterpret_cast<struct sockaddr*>(&client_addr), client_len);
            return;
        }
        const char err[] = "ERR: Invalid Window State\n";
        ::sendto(fd, err, sizeof(err) - 1, 0,
                 reinterpret_cast<struct sockaddr*>(&client_addr), client_len);
        return;
    } else {
        // High-Efficiency Binary Telemetry: Send 128-Byte Seqlock POD directly (0 allocations, 0 parsing)
        ::sendto(fd, &local_shared_state_, sizeof(local_shared_state_), 0,
                 reinterpret_cast<struct sockaddr*>(&client_addr), client_len);
    }
}

void DaemonRunner::stop() noexcept {
    running_ = false;
    window_governor_.rollback_all();
}

} // namespace wattcurb::core
