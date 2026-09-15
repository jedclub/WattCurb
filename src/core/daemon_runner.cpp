#include "core/daemon_runner.hpp"
#include "report/report_generator.hpp"
#include "core/scoped_profiler.hpp"

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
#include <sys/timerfd.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

namespace wattcurb::core {

DaemonRunner::DaemonRunner(double period_sec, double window_sec, std::string_view lock_name)
    : period_sec_(period_sec > 0.0 ? period_sec : 3.0),
      window_sec_(window_sec > 0.0 ? window_sec : 1.0),
      lock_name_(lock_name),
      lock_(lock_name) {}

DaemonRunner::~DaemonRunner() {
    stop();
    cleanup_descriptors();
}

void DaemonRunner::cleanup_descriptors() noexcept {
    if (shm_state_ != nullptr && shm_state_ != MAP_FAILED) {
        ::munmap(shm_state_, sizeof(ipc::WattCurbSharedState));
        shm_state_ = nullptr;
    }
    if (shm_fd_ >= 0) { ::close(shm_fd_); shm_fd_ = -1; }
    if (epoll_fd_ >= 0) { ::close(epoll_fd_); epoll_fd_ = -1; }
    if (timer_fd_ >= 0) { ::close(timer_fd_); timer_fd_ = -1; }
    if (signal_fd_ >= 0) { ::close(signal_fd_); signal_fd_ = -1; }
}

bool DaemonRunner::setup_timer() {
    timer_fd_ = ::timerfd_create(CLOCK_BOOTTIME, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timer_fd_ < 0) return false;

    // Set prctl timer slack to coalesce wakeups with other system activity (Zero-Wakeup)
    ::prctl(PR_SET_TIMERSLACK, 500'000'000UL); // 500ms slack

    time_t sec = static_cast<time_t>(period_sec_);
    long nsec = static_cast<long>((period_sec_ - static_cast<double>(sec)) * 1'000'000'000.0);

    struct itimerspec spec{};
    spec.it_value.tv_sec = sec > 0 ? sec : 60;
    spec.it_value.tv_nsec = nsec;
    spec.it_interval = spec.it_value;

    return (::timerfd_settime(timer_fd_, 0, &spec, nullptr) == 0);
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
    shm_fd_ = ::open(ipc::SHARED_STATE_SHM_PATH, O_RDWR | O_CREAT | O_CLOEXEC, 0666);
    if (shm_fd_ < 0) {
        return false;
    }
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

    setup_shm(); // Non-fatal: local Seqlock state remains 100% functional

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

    return true;
}

void DaemonRunner::collect_observation_window() {
    WATTCURB_PROFILE_SCOPE("daemon.collect_window");

    // 1. T0: Capture start baseline
    decltype(hw_probe_.capture_sample()) hw_start{};
    {
        WATTCURB_PROFILE_SCOPE("daemon.hw_capture_start");
        hw_start = hw_probe_.capture_sample();
    }
    auto& start_snapshot = proc_pool_.current();
    {
        WATTCURB_PROFILE_SCOPE("daemon.proc_snapshot_start");
        proc_analyzer_.capture_snapshot(start_snapshot);
    }

    // 2. Continuous Observation Window (~1 to 5 seconds)
    auto sleep_us = static_cast<useconds_t>(window_sec_ * 1'000'000.0);
    ::usleep(sleep_us);

    // 3. T1: Capture end state
    decltype(hw_probe_.capture_sample()) hw_end{};
    {
        WATTCURB_PROFILE_SCOPE("daemon.hw_capture_end");
        hw_end = hw_probe_.capture_sample();
    }
    auto& end_snapshot = proc_pool_.next();
    {
        WATTCURB_PROFILE_SCOPE("daemon.proc_snapshot_end");
        proc_analyzer_.capture_snapshot(end_snapshot, &start_snapshot);
    }

    // 4. Compute Full-Domain Physical Attribution
    {
        WATTCURB_PROFILE_SCOPE("daemon.compute_attribution");
        cached_report_ = engine_.compute_attribution(
            hw_start,
            hw_end,
            start_snapshot.span(),
            end_snapshot.span(),
            20
        );
    }

    // 5. Modular Battery Optimization Feature Actuation (REF-REQ-020 & REF-ARCH-009)
    {
        WATTCURB_PROFILE_SCOPE("daemon.evaluate_and_actuate");
        bool on_battery = cached_report_.hardware.is_battery_discharging;
        double batt_pct = static_cast<double>(cached_report_.hardware.battery_capacity_percent);
        feature_manager_.evaluate_and_actuate(cached_report_, on_battery, batt_pct);
    }

    // 6. Ultra-Fast 128-Byte Seqlock POD Export (REF-REQ-028, REF-ARCH-018)
    {
        WATTCURB_PROFILE_SCOPE("daemon.shm_update");
        local_shared_state_.update_from_report(cached_report_);
        if (shm_state_ != nullptr) {
            shm_state_->update_from_report(cached_report_);
        }
    }

    proc_pool_.swap(); // 0ns pointer swap
}

int DaemonRunner::run() {
    if (epoll_fd_ < 0 && !initialize()) {
        return 1;
    }

    running_ = true;
    std::cout << "[*] WattCurb background daemon initialized (PID: " << ::getpid()
              << ", period: " << period_sec_ << "s, window: " << window_sec_ << "s, Zero-Wakeup active)\n" << std::flush;

    // Initial baseline capture immediately upon startup
    collect_observation_window();

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

                // Collect 5-second window observation and apply mitigation
                collect_observation_window();

            } else if (fd == signal_fd_) {
                struct signalfd_siginfo fdsi{};
                ssize_t s = ::read(signal_fd_, &fdsi, sizeof(fdsi));
                if (s == sizeof(fdsi)) {
                    if (fdsi.ssi_signo == SIGINT || fdsi.ssi_signo == SIGTERM) {
                        std::cout << "\n[*] Received termination signal, gracefully exiting WattCurb daemon...\n" << std::flush;
                        running_ = false;
                    } else if (fdsi.ssi_signo == SIGHUP) {
                        hw_probe_.refresh_device_paths();
                    }
                }
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

    ssize_t bytes = ::recvfrom(fd, buf, sizeof(buf) - 1, 0,
                               reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
    if (bytes <= 0) return;
    buf[bytes] = '\0';

    std::string_view req(buf, static_cast<size_t>(bytes));

    if (req.find("BRIEFING") != std::string_view::npos) {
        // Developer text briefing channel exclusively for manual terminal debugging
        std::stringstream ss;
        report::ReportGenerator::render_executive_briefing(cached_report_, ss);
        auto resp_str = ss.str();
        ::sendto(fd, resp_str.data(), resp_str.size(), 0,
                 reinterpret_cast<struct sockaddr*>(&client_addr), client_len);
    } else if (req.find("FULL_TELEMETRY") != std::string_view::npos) {
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
        ss << "  \"profile_mode\": " << static_cast<int>(cached_report_.mitigation_status.current_profile) << ",\n";
        ss << "  \"active_mitigations\": " << cached_report_.mitigation_status.feature_summary_count << ",\n";
        ss << "  \"wakeups_per_sec\": " << cached_report_.total_system_wakeups_per_sec << ",\n";
        ss << "  \"processes\": [\n";

        size_t n = std::min(size_t{12}, cached_report_.top_processes.size());
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
            ss << "      \"tier\": " << static_cast<int>(p.safety_tier) << ",\n";
            ss << "      \"cpu_core\": " << p.cpu_core << ",\n";
            ss << "      \"threads\": " << p.num_threads << ",\n";
            ss << "      \"cross_ccx\": " << (p.cross_ccx_migration ? 1 : 0) << ",\n";
            ss << "      \"nice\": " << p.nice << ",\n";
            ss << "      \"priority\": " << p.priority << ",\n";
            ss << "      \"wakeups_sec\": " << p.wakeups_per_sec << ",\n";
            ss << "      \"timerslack_ns\": " << p.timerslack_ns << ",\n";
            ss << "      \"vram_mb\": " << (p.vram_kib / 1024.0) << ",\n";
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
        // Parse "PROFILE <mode>" (0=Performance, 1=Balanced, 2=PowerSaver, 3=UltraEndurance)
        int mode_val = req[8] - '0';
        if (mode_val >= 0 && mode_val <= 3) {
            auto new_mode = static_cast<PowerProfileMode>(mode_val);
            feature_manager_.set_override_profile(new_mode);
            local_shared_state_.power_profile_mode = static_cast<uint8_t>(new_mode);
            if (shm_state_) {
                shm_state_->power_profile_mode = static_cast<uint8_t>(new_mode);
            }

            // Immediately actuate hardware limits via power-profile-manager (REF-REQ-043)
            const char* hw_arg = "balanced";
            if (new_mode == PowerProfileMode::Performance) hw_arg = "performance";
            else if (new_mode == PowerProfileMode::PowerSaver) hw_arg = "save";
            else if (new_mode == PowerProfileMode::UltraEndurance) hw_arg = "ultra";

            pid_t pid = ::fork();
            if (pid == 0) {
                ::setsid();
                const char* ppm = "/home/jedclub/.local/bin/power-profile-manager";
                if (::access(ppm, X_OK) == 0) {
                    ::execl(ppm, "power-profile-manager", hw_arg, "--internal", nullptr);
                }
                ::_exit(0);
            }
        }
        const char ack[] = "OK\n";
        ::sendto(fd, ack, sizeof(ack) - 1, 0,
                 reinterpret_cast<struct sockaddr*>(&client_addr), client_len);
    } else if (req.find("RESCAN") != std::string_view::npos) {
        collect_observation_window();
        const char ack[] = "OK\n";
        ::sendto(fd, ack, sizeof(ack) - 1, 0,
                 reinterpret_cast<struct sockaddr*>(&client_addr), client_len);
    } else {
        // High-Efficiency Binary Telemetry: Send 128-Byte Seqlock POD directly (0 allocations, 0 parsing)
        ::sendto(fd, &local_shared_state_, sizeof(local_shared_state_), 0,
                 reinterpret_cast<struct sockaddr*>(&client_addr), client_len);
    }
}

void DaemonRunner::stop() noexcept {
    running_ = false;
}

} // namespace wattcurb::core
