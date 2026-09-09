#pragma once

#include "core/singleton_lock.hpp"
#include "core/types.hpp"
#include "hw/hardware_probe.hpp"
#include "proc/process_analyzer.hpp"
#include "policy/attribution_engine.hpp"

#include <atomic>
#include <string>

namespace wattcurb::core {

// Implements REF-REQ-002, REF-REQ-007, REF-ARCH-004
class DaemonRunner {
public:
    explicit DaemonRunner(double interval_sec = 5.0, std::string_view lock_name = "wattcurb.lock");
    ~DaemonRunner();

    DaemonRunner(const DaemonRunner&) = delete;
    DaemonRunner& operator=(const DaemonRunner&) = delete;

    [[nodiscard]] bool initialize();
    int run();
    void stop() noexcept;

    [[nodiscard]] const AnalysisReportData& latest_report() const noexcept { return cached_report_; }

private:
    double interval_sec_{5.0};
    std::string lock_name_;
    SingletonLock lock_;
    std::atomic<bool> running_{false};

    hw::HardwareProbe hw_probe_;
    proc::ProcessAnalyzer proc_analyzer_;
    policy::AttributionEngine engine_;
    AnalysisReportData cached_report_;

    int epoll_fd_{-1};
    int timer_fd_{-1};
    int signal_fd_{-1};

    bool setup_timer();
    bool setup_signals();
    void handle_ipc_datagram(int fd);
    void cleanup_descriptors() noexcept;
};

} // namespace wattcurb::core
