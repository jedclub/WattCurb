#include "policy/window_aware_governor.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>

namespace wattcurb::policy {

// Implements REF-REQ-033 & REF-ARCH-023: Zero-Wakeup Progressive Suppression Ladder
void WindowAwareGovernor::on_window_state_changed(
    int32_t pid, 
    bool minimized, 
    bool active, 
    uint64_t now_sec,
    bool is_audio_active
) noexcept {
    // Self-Safety Invariant: Ignore daemon self and parent process
    if (pid <= 1 || pid == ::getpid() || pid == ::getppid()) return;

    // Fast-path: Foreground focus or unminimized window immediately thaws to 100% responsiveness
    if (active || !minimized) {
        thaw_immediate(pid);
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

    // Stage 1 Immediate Soft Throttling: SCHED_IDLE + Timer Slack relaxation (100ms)
    if (entry->state == WindowSuppressionState::ActiveForeground) {
        MitigationEngine::apply_sched_idle(pid);
        MitigationEngine::apply_timer_slack(pid, 100'000'000ULL); // 100ms
        entry->state = WindowSuppressionState::Stage1Throttled;
    }
}

void WindowAwareGovernor::evaluate_hysteresis(uint64_t now_sec) noexcept {
    for (size_t i = 0; i < m_windows.size(); ++i) {
        auto& entry = m_windows[i];
        if (entry.state == WindowSuppressionState::Stage1Throttled) {
            // Audio-playing and terminal processes are strictly immune from hard freezing
            if (entry.has_active_audio || entry.is_terminal) {
                continue;
            }

            // Hysteresis window check (e.g. 20s minimized)
            if (now_sec >= entry.minimized_timestamp_sec &&
                (now_sec - entry.minimized_timestamp_sec) >= STAGE2_HYSTERESIS_SEC) {
                
                // Stage 2 Escalation: Transparent cgroup v2 freeze & memory compaction
                MitigationEngine::apply_cgroup_freeze(entry.pid, true);
                MitigationEngine::apply_memory_reclaim(entry.pid, 64 * 1024 * 1024); // 64MB
                entry.state = WindowSuppressionState::Stage2Frozen;
            }
        }
    }
}

bool WindowAwareGovernor::thaw_immediate(int32_t pid) noexcept {
    auto* entry = find_entry_mut(pid);
    if (!entry) return false;

    if (entry->state == WindowSuppressionState::ActiveForeground) {
        return true;
    }

    // 1. Thaw cgroup if frozen
    if (entry->state == WindowSuppressionState::Stage2Frozen) {
        MitigationEngine::apply_cgroup_freeze(pid, false);
    }

    // 2. Restore CFS scheduler (SCHED_OTHER, nice 0)
    MitigationEngine::restore_sched_normal(pid);

    // 3. Restore standard 50µs timer slack for high frame-rate responsiveness
    MitigationEngine::apply_timer_slack(pid, 50'000ULL);

    entry->state = WindowSuppressionState::ActiveForeground;
    return true;
}

void WindowAwareGovernor::rollback_all() noexcept {
    for (size_t i = 0; i < m_windows.size(); ++i) {
        auto& entry = m_windows[i];
        if (entry.state != WindowSuppressionState::ActiveForeground) {
            if (entry.state == WindowSuppressionState::Stage2Frozen) {
                MitigationEngine::apply_cgroup_freeze(entry.pid, false);
            }
            MitigationEngine::restore_sched_normal(entry.pid);
            MitigationEngine::apply_timer_slack(entry.pid, 50'000ULL);
            entry.state = WindowSuppressionState::ActiveForeground;
        }
    }
    m_windows.clear();
}

} // namespace wattcurb::policy
