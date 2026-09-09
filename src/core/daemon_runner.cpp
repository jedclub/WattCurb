#include "core/daemon_runner.hpp"
#include "report/report_generator.hpp"

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
#include <unistd.h>

namespace wattcurb::core {

DaemonRunner::DaemonRunner(double interval_sec, std::string_view lock_name)
    : interval_sec_(interval_sec),
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

    time_t sec = static_cast<time_t>(interval_sec_);
    long nsec = static_cast<long>((interval_sec_ - static_cast<double>(sec)) * 1'000'000'000.0);

    struct itimerspec spec{};
    spec.it_value.tv_sec = sec > 0 ? sec : 1;
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

int DaemonRunner::run() {
    if (epoll_fd_ < 0 && !initialize()) {
        return 1;
    }

    running_ = true;
    std::cout << "[*] WattCurb background daemon initialized (PID: " << ::getpid()
              << ", interval: " << interval_sec_ << "s, Zero-Wakeup active)\n" << std::flush;

    // Initial baseline capture
    auto hw_prev = hw_probe_.capture_sample();
    auto proc_prev = proc_analyzer_.capture_active_processes();

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

                auto hw_cur = hw_probe_.capture_sample();
                auto proc_cur = proc_analyzer_.capture_active_processes(&proc_prev);

                cached_report_ = engine_.compute_attribution(hw_prev, hw_cur, proc_prev, proc_cur, 20);

                // Update live file in /tmp/wattcurb_live.json atomically
                {
                    std::stringstream ss;
                    report::ReportGenerator::render_json(cached_report_, ss);
                    auto json_str = ss.str();
                    int out_fd = ::open("/tmp/wattcurb_live.json.tmp", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
                    if (out_fd >= 0) {
                        ssize_t w = ::write(out_fd, json_str.data(), json_str.size());
                        (void)w;
                        ::close(out_fd);
                        ::rename("/tmp/wattcurb_live.json.tmp", "/tmp/wattcurb_live.json");
                    }
                }

                hw_prev = std::move(hw_cur);
                proc_prev = std::move(proc_cur);

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

    std::stringstream ss;
    report::ReportGenerator::render_json(cached_report_, ss);
    auto json_str = ss.str();

    ::sendto(fd, json_str.data(), json_str.size(), 0,
             reinterpret_cast<struct sockaddr*>(&client_addr), client_len);
}

void DaemonRunner::stop() noexcept {
    running_ = false;
}

} // namespace wattcurb::core
