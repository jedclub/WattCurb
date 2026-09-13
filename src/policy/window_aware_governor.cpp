#include "policy/window_aware_governor.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>

namespace wattcurb::policy {

// Implements REF-REQ-033 & REF-ARCH-023:
// Non-Halting Graceful Throttle Engine
// Invariant: Background applications are NEVER halted or frozen.
// They execute normally on the lowest CFS runqueue tier (SCHED_IDLE)
// with coalesced wakeup timers, eliminating system-wide UI pauses.
void WindowAwareGovernor::on_window_state_changed(
    int32_t pid, 
    bool minimized, 
    bool active, 
    uint64_t now_sec,
    bool is_audio_active
) noexcept {
    // Self-Safety Invariant: Ignore daemon self and parent process
    if (pid <= 1 || pid == ::getpid() || pid == ::getppid()) return;

    // Fast-path: Foreground focus or unminimized window immediately unthrottles
    if (active || !minimized) {
        unthrottle_immediate(pid);
        return;
    }

    // Minimized path:
    auto* entry = find_entry_mut(pid);
    if (!entry) {
        if (m_windows.size() >= MAX_TRACKED_WINDOWS) {
            return; // Capacity saturated safely
        }
        WindowStateEntry new_entry{};
        new_entry.pid = pid;
        new_entry.minimized_timestamp_sec = now_sec;
        new_entry.state = WindowSuppressionState::ActiveForeground;
        new_entry.has_active_audio = is_audio_active;
        m_windows.push_back(new_entry);
        entry = &m_windows.back();
    } else {
        entry->minimized_timestamp_sec = now_sec;
        entry->has_active_audio = is_audio_active;
    }

    // Non-Halting Graceful Throttle:
    // Process is NEVER halted. It runs under SCHED_IDLE (only utilizing spare CPU cycles)
    // with timers relaxed to 50ms to prevent high-frequency CPU package wakeups.
    if (entry->state == WindowSuppressionState::ActiveForeground) {
        MitigationEngine::apply_sched_idle(pid);
        MitigationEngine::apply_timer_slack(pid, 50'000'000ULL); // 50ms graceful timer slack
        entry->state = WindowSuppressionState::GracefulIdleThrottled;
    }
}

void WindowAwareGovernor::evaluate_hysteresis(uint64_t /*now_sec*/) noexcept {
    // Non-Halting Invariant: No escalation to hard freeze.
    // Applications remain active and responsive in GracefulIdleThrottled state.
}

bool WindowAwareGovernor::unthrottle_immediate(int32_t pid) noexcept {
    auto* entry = find_entry_mut(pid);
    if (!entry) return false;

    if (entry->state == WindowSuppressionState::ActiveForeground) {
        return true;
    }

    // 1. Restore CFS scheduler (SCHED_OTHER, normal CFS weight)
    MitigationEngine::restore_sched_normal(pid);

    // 2. Restore standard 50µs timer slack for high frame-rate rendering
    MitigationEngine::apply_timer_slack(pid, 50'000ULL);

    entry->state = WindowSuppressionState::ActiveForeground;
    return true;
}

void WindowAwareGovernor::rollback_all() noexcept {
    for (size_t i = 0; i < m_windows.size(); ++i) {
        auto& entry = m_windows[i];
        if (entry.state != WindowSuppressionState::ActiveForeground) {
            MitigationEngine::restore_sched_normal(entry.pid);
            MitigationEngine::apply_timer_slack(entry.pid, 50'000ULL);
            entry.state = WindowSuppressionState::ActiveForeground;
        }
    }
    m_windows.clear();
}

} // namespace wattcurb::policy
