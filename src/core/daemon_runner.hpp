#pragma once

#include "core/singleton_lock.hpp"
#include "core/types.hpp"
#include "hw/hardware_probe.hpp"
#include "proc/process_analyzer.hpp"
#include "policy/attribution_engine.hpp"
#include "policy/mitigation_engine.hpp"

#include <atomic>
#include <string>

namespace wattcurb::core {

// Implements REF-REQ-002, REF-REQ-007, REF-REQ-019, REF-ARCH-004, REF-ARCH-008
class DaemonRunner {
public:
    explicit DaemonRunner(double period_sec = 60.0, double window_sec = 5.0, std::string_view lock_name = "wattcurb.lock");
    ~DaemonRunner();

    DaemonRunner(const DaemonRunner&) = delete;
    DaemonRunner& operator=(const DaemonRunner&) = delete;

    [[nodiscard]] bool initialize();
    int run();
    void stop() noexcept;

    [[nodiscard]] const AnalysisReportData& latest_report() const noexcept { return cached_report_; }

private:
    double period_sec_{60.0};
    double window_sec_{5.0};
    std::string lock_name_;
    SingletonLock lock_;
    std::atomic<bool> running_{false};

    hw::HardwareProbe hw_probe_;
    proc::ProcessAnalyzer proc_analyzer_;
    policy::AttributionEngine engine_;
    policy::MitigationEngine mitigation_engine_;
    ProcessPool proc_pool_;
    AnalysisReportData cached_report_;

    int epoll_fd_{-1};
    int timer_fd_{-1};
    int signal_fd_{-1};

    bool setup_timer();
    bool setup_signals();
    void collect_observation_window();
    void handle_ipc_datagram(int fd);
    void cleanup_descriptors() noexcept;
};

} // namespace wattcurb::core
