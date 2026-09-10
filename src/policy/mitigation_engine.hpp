#pragma once

#include "core/types.hpp"
#include "policy/process_classifier.hpp"
#include <string_view>
#include <cstdint>

namespace wattcurb::policy {

// Implements REF-REQ-019, REF-ARCH-008, REF-RES-008 & REF-RES-009
// Adaptive Closed-Loop Mitigation Engine with Zero-Wakeup & Minimal Syscall Overhead
class MitigationEngine {
public:
    MitigationEngine() noexcept = default;

    // Evaluates current power report and applies closed-loop mitigation actions
    // Returns the populated ActiveMitigationStatus
    ActiveMitigationStatus evaluate_and_actuate(
        AnalysisReportData& report,
        bool on_battery,
        double battery_pct
    ) noexcept;

    // Actuation primitives
    static bool apply_sched_idle(int32_t pid) noexcept;
    static bool apply_timer_slack(int32_t pid, uint64_t slack_ns) noexcept;
    static bool apply_memory_reclaim(int32_t pid, uint64_t bytes) noexcept;
    static bool apply_cgroup_freeze(int32_t pid, bool freeze) noexcept;

    // Fast resolution of cgroup v2 path for a given pid
    // Writes path to out_buf without heap allocation
    static bool resolve_cgroup_path(int32_t pid, char* out_buf, size_t out_cap) noexcept;

private:
    // Internal tracking to avoid redundant syscalls on already-mitigated processes
    static constexpr size_t MAX_TRACKED_MITIGATIONS = 128;
    struct TrackedMitigation {
        int32_t pid{0};
        MitigationAction current_action{MitigationAction::None};
        uint64_t applied_timestamp_sec{0};
    };

    core::FixedVector<TrackedMitigation, MAX_TRACKED_MITIGATIONS> m_tracked{};
};

} // namespace wattcurb::policy
