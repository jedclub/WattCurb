#pragma once

#include "core/singleton_lock.hpp"
#include "core/types.hpp"
#include "hw/hardware_probe.hpp"
#include "proc/process_analyzer.hpp"
#include "policy/attribution_engine.hpp"
#include "policy/battery_feature.hpp"
#include "policy/window_aware_governor.hpp"

#include <atomic>
#include <string>

#include "core/event_logger.hpp"
#include "ipc/history_ring_buffer.hpp"
#include "ipc/tray_shared_state.hpp"

namespace wattcurb::core {

// Implements REF-REQ-002, REF-REQ-007, REF-REQ-019, REF-REQ-020, REF-REQ-028, REF-ARCH-004, REF-ARCH-018
class DaemonRunner {
public:
    explicit DaemonRunner(double period_sec = 3.0, double window_sec = 1.0, std::string_view lock_name = "wattcurb.lock");
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
    [[nodiscard]] policy::WindowAwareGovernor& window_governor() noexcept { return window_governor_; }
    [[nodiscard]] const policy::WindowAwareGovernor& window_governor() const noexcept { return window_governor_; }

private:
    double period_sec_{3.0};
    double window_sec_{1.0};
    std::string lock_name_;
    SingletonLock lock_;
    std::atomic<bool> running_{false};

    hw::HardwareProbe hw_probe_;
    proc::ProcessAnalyzer proc_analyzer_;
    policy::AttributionEngine engine_;
    policy::FeatureManager feature_manager_;
    policy::WindowAwareGovernor window_governor_{};
    ProcessPool proc_pool_;
    AnalysisReportData cached_report_;
    HardwareSample hw_prev_{};
    HardwareSample hw_light_prev_{}; // REF-REQ-069: Decoupled Light Probe baseline
    HardwareSample hw_deep_prev_{};  // REF-REQ-069: Decoupled Deep Sweep baseline (pair-sync with proc_pool_)
    bool has_baseline_{false};

    ipc::WattCurbSharedState local_shared_state_{};
    ipc::WattCurbSharedState* shm_state_{nullptr};
    int shm_fd_{-1};

    ipc::HistoryRingBufferShm* shm_history_{nullptr};
    int shm_history_fd_{-1};
    PowerProfileMode last_logged_profile_{PowerProfileMode::Balanced};
    bool last_logged_battery_state_{false};
    uint8_t last_logged_battery_pct_{100};

    int epoll_fd_{-1};
    int timer_fd_{-1};
    int signal_fd_{-1};

    // REF-REQ-068 & REF-REQ-069: Adaptive 3-Tier Cadence & Smart Trigger
    uint64_t bg_tick_count_{0};
    uint64_t interactive_lease_deadline_ms_{0};
    bool is_interactive_active_{false};
    double current_timer_interval_{10.0};

    // REF-REQ-069 & REF-ARCH-046: Smart Adaptive Power-Spike State
    double last_light_system_watts_{0.0};
    uint64_t last_early_sweep_time_ms_{0};
    static constexpr uint64_t EARLY_SWEEP_COOLDOWN_MS = 15'000;

    bool arm_timer(double interval_sec) noexcept;
    bool setup_timer();
    bool setup_signals();
    bool setup_shm();
    bool setup_history_shm();
    void process_observation_cycle();
    bool process_light_probe_cycle();
    void process_deep_observation_cycle();
    void handle_ipc_datagram(int fd);
    void cleanup_descriptors() noexcept;
};

} // namespace wattcurb::core
