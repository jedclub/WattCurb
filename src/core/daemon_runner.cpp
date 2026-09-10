#include "core/daemon_runner.hpp"
#include "report/report_generator.hpp"

#include <chrono>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <sys/epoll.h>
#include <sys/prctl.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

namespace wattcurb::core {

DaemonRunner::DaemonRunner(double period_sec, double window_sec, std::string_view lock_name)
    : period_sec_(period_sec > 0.0 ? period_sec : 60.0),
      window_sec_(window_sec > 0.0 ? window_sec : 5.0),
      lock_name_(lock_name),
      lock_(lock_name) {}

DaemonRunner::~DaemonRunner() {
    stop();
    cleanup_descriptors();
}

void DaemonRunner::cleanup_descriptors() noexcept {
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

bool DaemonRunner::initialize() {
    if (!lock_.is_locked()) {
        std::cerr << "[!] Error: WattCurb singleton lock could not be acquired. Another instance is already running.\n";
        return false;
    }

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
    // 1. T0: Capture start baseline
    auto hw_start = hw_probe_.capture_sample();
    auto& start_snapshot = proc_pool_.current();
    proc_analyzer_.capture_snapshot(start_snapshot);

    // 2. Continuous Observation Window (~5 seconds)
    auto sleep_us = static_cast<useconds_t>(window_sec_ * 1'000'000.0);
    ::usleep(sleep_us);

    // 3. T1: Capture end state
    auto hw_end = hw_probe_.capture_sample();
    auto& end_snapshot = proc_pool_.next();
    proc_analyzer_.capture_snapshot(end_snapshot, &start_snapshot);

    // 4. Compute Full-Domain Physical Attribution
    cached_report_ = engine_.compute_attribution(
        hw_start,
        hw_end,
        start_snapshot.span(),
        end_snapshot.span(),
        20
    );

    // 5. Modular Battery Optimization Feature Actuation (REF-REQ-020 & REF-ARCH-009)
    bool on_battery = cached_report_.hardware.is_battery_discharging;
    double batt_pct = static_cast<double>(cached_report_.hardware.battery_capacity_percent);
    feature_manager_.evaluate_and_actuate(cached_report_, on_battery, batt_pct);

    // Pure In-Memory Struct Pipeline:
    // Zero string serialization or formatting is performed in the routine background loop!
    // Telemetry and feature mitigation states are retained 100% in memory structures.
    // Serialization executes on-demand exclusively upon receiving client IPC datagrams.

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
    std::stringstream ss;

    if (req.find("BRIEFING") != std::string_view::npos) {
        report::ReportGenerator::render_executive_briefing(cached_report_, ss);
    } else {
        report::ReportGenerator::render_json(cached_report_, ss);
    }

    auto resp_str = ss.str();
    ::sendto(fd, resp_str.data(), resp_str.size(), 0,
             reinterpret_cast<struct sockaddr*>(&client_addr), client_len);
}

void DaemonRunner::stop() noexcept {
    running_ = false;
}

} // namespace wattcurb::core
