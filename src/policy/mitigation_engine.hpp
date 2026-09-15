#pragma once

#include "core/types.hpp"
#include "policy/process_classifier.hpp"
#include <string_view>
#include <cstdint>
#include <optional>

namespace wattcurb::policy {

// Implements REF-REQ-019, REF-REQ-031, REF-ARCH-008 & REF-ARCH-021
// Closed-Loop Adaptive Mitigation Engine with 3-Tier State Machine & Bidirectional Rollback
class MitigationEngine {
public:
    MitigationEngine() noexcept;

    // Evaluates current power report, determines profile via hysteresis, and applies closed-loop actuations
    ActiveMitigationStatus evaluate_and_actuate(
        AnalysisReportData& report,
        bool on_battery,
        double battery_pct
    ) noexcept;

    // Profile inspection and manual override
    [[nodiscard]] PowerProfileMode current_profile() const noexcept { return m_current_profile; }
    void set_profile_override(std::optional<PowerProfileMode> mode) noexcept { m_profile_override = mode; }
    [[nodiscard]] std::optional<PowerProfileMode> profile_override() const noexcept { return m_profile_override; }

    // Hysteresis-aware profile calculator (REF-REQ-031 Sec 2.2)
    [[nodiscard]] PowerProfileMode determine_profile(bool on_battery, double battery_pct) const noexcept;

    // Bidirectional Rollback (REF-REQ-031 Sec 3.2, REF-ARCH-021)
    void rollback_all() noexcept;
    void thaw_all_frozen() noexcept;
    [[nodiscard]] size_t tracked_count() const noexcept { return m_tracked.size(); }

    // Process-Level Actuation Primitives
    static bool apply_sched_idle(int32_t pid) noexcept;
    static bool restore_sched_normal(int32_t pid) noexcept;
    static bool apply_timer_slack(int32_t pid, uint64_t slack_ns) noexcept;
    static bool apply_memory_reclaim(int32_t pid, uint64_t bytes) noexcept;
    static bool apply_cgroup_freeze(int32_t pid, bool freeze) noexcept;

    // Hardware-Level Actuation Primitives (REF-REQ-031 Sec 3.3)
    static bool set_pcie_aspm_policy(const char* policy) noexcept;
    static bool set_cpu_epp_policy(const char* policy) noexcept;
    static bool cap_display_backlight(double max_pct) noexcept;
    static bool restore_display_backlight() noexcept;

    // Process Immunity & Audio Protection (REF-REQ-049)
    static bool is_immune_process(int32_t pid) noexcept;
    static void audit_and_heal_audio_stack() noexcept;

    // Fast resolution of cgroup v2 path for a given pid without heap allocations
    static bool resolve_cgroup_path(int32_t pid, char* out_buf, size_t out_cap) noexcept;

    // Internal tracking structure for rollback & idempotency
    static constexpr size_t MAX_TRACKED_MITIGATIONS = 128;
    struct TrackedMitigation {
        int32_t pid{0};
        ProcessSafetyTier tier{ProcessSafetyTier::BackgroundWorker};
        MitigationAction current_action{MitigationAction::None};
        uint64_t applied_timestamp_sec{0};
        uint64_t original_timerslack_ns{50000};
    };

    [[nodiscard]] const core::FixedVector<TrackedMitigation, MAX_TRACKED_MITIGATIONS>& tracked_mitigations() const noexcept {
        return m_tracked;
    }

private:
    PowerProfileMode m_current_profile{PowerProfileMode::Balanced};
    std::optional<PowerProfileMode> m_profile_override{std::nullopt};
    bool m_aspm_modified{false};
    bool m_backlight_capped{false};

    core::FixedVector<TrackedMitigation, MAX_TRACKED_MITIGATIONS> m_tracked{};
};

} // namespace wattcurb::policy
