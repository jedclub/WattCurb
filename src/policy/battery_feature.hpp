#pragma once

#include "core/custom_containers.hpp"
#include "core/types.hpp"
#include "policy/process_classifier.hpp"
#include <cstdint>
#include <string_view>

namespace wattcurb::policy {

// Implements REF-REQ-020 & REF-ARCH-009: Modular Battery Optimization Features
enum class FeatureId : uint8_t {
    SchedIdleThrottle = 0,
    TimerSlackCoalescing = 1,
    ProactiveMemoryReclaim = 2,
    CgroupFreezer = 3,
    ZenCcxAffinityPinning = 4,
    DisplayBacklightFloor = 5,
    PcieAspmEnforcer = 6,
    AntiStarvationHeadroom = 7,
    Count = 8
};

struct FeatureDescriptor {
    FeatureId id{FeatureId::SchedIdleThrottle};
    core::FixedString<16> feature_code;              // e.g. "FEAT-001"
    core::FixedString<32> name;                      // e.g. "SchedIdleThrottle"
    core::FixedString<32> target_domain;             // e.g. "CPU / Scheduler"
    core::FixedString<128> kernel_mechanism;         // e.g. "sched_setscheduler(SCHED_IDLE) + ioprio_set"
    core::FixedString<256> power_saving_rationale;   // Physical hardware power reduction theory
    core::FixedString<192> safety_constraints;       // Immunity guarantees (T0/T1 immune)
    core::FixedString<128> description;              // Human-readable summary
    bool default_enabled{true};
};

struct FeatureMetrics {
    bool enabled{true};
    size_t actions_taken{0};
    uint64_t resource_reclaimed_bytes{0};
    double estimated_power_saved_watts{0.0};
    std::array<int32_t, 16> targeted_pids{};
    size_t targeted_pid_count{0};
    core::FixedString<128> detail_summary{};
};

static_assert(std::is_trivially_copyable_v<FeatureMetrics>, "FeatureMetrics must be TriviallyCopyable for Zero-Cost In-Memory Pipeline");

// Pure Zero-Allocation Battery Feature Manager
class FeatureManager {
public:
    FeatureManager() noexcept;

    // Feature Toggles & Profile Override
    void set_feature_enabled(FeatureId id, bool enabled) noexcept;
    [[nodiscard]] bool is_feature_enabled(FeatureId id) const noexcept;
    void set_override_profile(std::optional<PowerProfileMode> mode) noexcept { m_profile_override = mode; }
    [[nodiscard]] std::optional<PowerProfileMode> override_profile() const noexcept { return m_profile_override; }

    // Get live metrics for all features (Zero-String)
    [[nodiscard]] const std::array<FeatureMetrics, static_cast<size_t>(FeatureId::Count)>& metrics() const noexcept {
        return m_metrics;
    }

    [[nodiscard]] static FeatureDescriptor descriptor(FeatureId id) noexcept;

    // Evaluates all active features against structural telemetry and applies actuations
    ActiveMitigationStatus evaluate_and_actuate(
        AnalysisReportData& report,
        bool on_battery,
        double battery_pct
    ) noexcept;

    // Individual Feature Actuation Primitives
    static bool actuate_sched_idle(int32_t pid) noexcept;
    static bool actuate_timer_slack(int32_t pid, uint64_t slack_ns) noexcept;
    static bool actuate_memory_reclaim(int32_t pid, uint64_t bytes) noexcept;
    static bool actuate_cgroup_freeze(int32_t pid, bool freeze) noexcept;
    static bool actuate_ccx_affinity(int32_t pid, int32_t target_core) noexcept;
    static bool actuate_anti_starvation_cap(int32_t pid, PowerProfileMode mode = PowerProfileMode::Balanced, const char* comm = "runaway-task") noexcept;
    static bool actuate_anti_starvation_restore(int32_t pid, const cpu_set_t* target_affinity = nullptr, int orig_policy = 0, int orig_nice = 0, const char* comm = "runaway-task") noexcept;

    // REF-REQ-102: Releases every tracked mitigation - scheduling class, nice,
    // timer slack, CPU affinity and cgroup freeze - back to what was recorded
    // before it was applied.
    void rollback_all_tracked() noexcept;

private:
    std::array<FeatureMetrics, static_cast<size_t>(FeatureId::Count)> m_metrics{};

    static constexpr size_t MAX_TRACKED_MITIGATIONS = 128;
    struct TrackedMitigation {
        int32_t pid{0};
        char comm[16]{0};
        FeatureId applied_feature{FeatureId::SchedIdleThrottle};
        uint64_t timestamp_sec{0};
        int original_nice{0};
        int original_sched_policy{0};
        uint64_t original_timerslack_ns{50000};
        cpu_set_t original_affinity{};
    };
    core::FixedVector<TrackedMitigation, MAX_TRACKED_MITIGATIONS> m_tracked{};
    std::optional<PowerProfileMode> m_profile_override{};
    ProfileDemotionLatch m_demotion_latch{};
    // REF-REQ-102: Profile in force at the end of the previous cycle. A change
    // must release everything the old profile applied.
    std::optional<PowerProfileMode> m_last_profile{};
};

} // namespace wattcurb::policy
