#pragma once

#include "core/singleton_lock.hpp"
#include "core/types.hpp"
#include "hw/hardware_probe.hpp"
#include "proc/process_analyzer.hpp"
#include "policy/attribution_engine.hpp"
#include "policy/battery_feature.hpp"

#include <atomic>
#include <string>

#include "ipc/tray_shared_state.hpp"

namespace wattcurb::core {

// Implements REF-REQ-002, REF-REQ-007, REF-REQ-019, REF-REQ-020, REF-REQ-028, REF-ARCH-004, REF-ARCH-018
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
    [[nodiscard]] const ipc::WattCurbSharedState& shared_state() const noexcept { return local_shared_state_; }
    [[nodiscard]] policy::FeatureManager& feature_manager() noexcept { return feature_manager_; }
    [[nodiscard]] const policy::FeatureManager& feature_manager() const noexcept { return feature_manager_; }

private:
    double period_sec_{60.0};
    double window_sec_{5.0};
    std::string lock_name_;
    SingletonLock lock_;
    std::atomic<bool> running_{false};

    hw::HardwareProbe hw_probe_;
    proc::ProcessAnalyzer proc_analyzer_;
    policy::AttributionEngine engine_;
    policy::FeatureManager feature_manager_;
    ProcessPool proc_pool_;
    AnalysisReportData cached_report_;

    ipc::WattCurbSharedState local_shared_state_{};
    ipc::WattCurbSharedState* shm_state_{nullptr};
    int shm_fd_{-1};

    int epoll_fd_{-1};
    int timer_fd_{-1};
    int signal_fd_{-1};

    bool setup_timer();
    bool setup_signals();
    bool setup_shm();
    void collect_observation_window();
    void handle_ipc_datagram(int fd);
    void cleanup_descriptors() noexcept;
};

} // namespace wattcurb::core
