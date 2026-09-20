#include "policy/window_aware_governor.hpp"
#include "core/event_logger.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <sys/resource.h>
#include <sched.h>
#include <sys/syscall.h>

namespace wattcurb::policy {

// Implements REF-REQ-033, REF-REQ-085 & REF-ARCH-062:
// Non-Halting Graceful Throttle Engine & Active Window Resource Guarantee
void WindowAwareGovernor::on_window_state_changed(
    int32_t pid, 
    bool minimized, 
    bool active, 
    uint64_t now_sec,
    bool is_audio_active
) noexcept {
    // Self-Safety Invariant: Ignore daemon self and parent process
    if (pid <= 1 || pid == ::getpid() || pid == ::getppid()) return;

    // Fast-path 1: Active foreground focus activates dedicated C0 PM QoS & C1 cluster guarantee
    if (active) {
        engage_active_window(pid);
        return;
    }

    // If this window was active but lost focus, release its active resource guarantee
    if (m_active_snapshot.is_guarantee_active && m_active_snapshot.pid == pid) {
        release_active_window();
    }

    // Fast-path 2: Unminimized window immediately unthrottles to normal CFS
    if (!minimized) {
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

// Implements REF-REQ-085 & REF-ARCH-062:
// Active Window Fixed Resource Guarantee & PM QoS C0 Pinning
bool WindowAwareGovernor::engage_active_window(int32_t pid, const char* comm) noexcept {
    if (pid <= 1 || pid == ::getpid() || pid == ::getppid()) return false;

    if (m_active_snapshot.is_guarantee_active) {
        if (m_active_snapshot.pid == pid) {
            // Already active, re-affirm PM QoS lock
            m_pm_qos.pin_c0_latency(0);
            return true;
        }
        // Focus changed: release previous window
        release_active_window();
    }

    // 1. Capture exact pre-guarantee baseline snapshot
    errno = 0;
    int cur_nice = ::getpriority(PRIO_PROCESS, static_cast<id_t>(pid));
    int cur_policy = ::sched_getscheduler(pid);
    cpu_set_t cur_aff{};
    CPU_ZERO(&cur_aff);
    int aff_ret = ::sched_getaffinity(pid, sizeof(cur_aff), &cur_aff);

    uint64_t cur_slack = 50'000ULL;
    char slack_path[64];
    std::snprintf(slack_path, sizeof(slack_path), "/proc/%d/timerslack_ns", pid);
    int sfd = ::open(slack_path, O_RDONLY | O_CLOEXEC);
    if (sfd >= 0) {
        char sbuf[32];
        ssize_t n = ::read(sfd, sbuf, sizeof(sbuf) - 1);
        ::close(sfd);
        if (n > 0) {
            sbuf[n] = '\0';
            cur_slack = std::strtoull(sbuf, nullptr, 10);
        }
    }

    m_active_snapshot.pid = pid;
    m_active_snapshot.original_nice = (errno == 0 ? cur_nice : 0);
    m_active_snapshot.original_sched_policy = (cur_policy >= 0 ? cur_policy : SCHED_OTHER);
    m_active_snapshot.original_affinity = cur_aff;
    m_active_snapshot.original_timerslack_ns = (cur_slack > 0 ? cur_slack : 50'000ULL);
    m_active_snapshot.has_original_state = (aff_ret == 0);
    m_active_snapshot.is_guarantee_active = true;

    // 2. Actuate CFS priority elevation (nice -10 for instantaneous preemption)
    ::setpriority(PRIO_PROCESS, static_cast<id_t>(pid), -10);

    // 3. Actuate Spatial Core Pinning to Cluster 1 (C1: Cores 0..7 or Headroom 0..3)
    cpu_set_t c1_mask = MitigationEngine::get_c1_cpuset();
    ::sched_setaffinity(pid, sizeof(c1_mask), &c1_mask);

    // 4. Actuate precision timer slack (10µs for zero-stutter frame delivery)
    MitigationEngine::apply_timer_slack(pid, 10'000ULL);

    // 5. Actuate Block I/O priority: Best-Effort Priority 0
#if defined(SYS_ioprio_set)
    ::syscall(SYS_ioprio_set, 1 /* IOPRIO_WHO_PROCESS */, pid, (2 << 13) | 0 /* BE class, prio 0 */);
#endif

    // 6. Actuate PM QoS C0 Latency Pinning (/dev/cpu_dma_latency -> 0µs exit latency)
    m_pm_qos.pin_c0_latency(0);

    // 7. Ensure uninhibited state if previously tracked
    unthrottle_immediate(pid);

    core::EventLogger::log_mitigation(pid, comm ? comm : "", "ACTIVE_WINDOW_C0_GUARANTEE",
                                      "Pinned to C1 cores, nice -10, PM QoS 0us C0 clamp");
    return true;
}

void WindowAwareGovernor::release_active_window() noexcept {
    if (!m_active_snapshot.is_guarantee_active) return;

    if (m_active_snapshot.has_original_state && m_active_snapshot.pid > 1) {
        // 1. Restore baseline nice
        ::setpriority(PRIO_PROCESS, static_cast<id_t>(m_active_snapshot.pid), m_active_snapshot.original_nice);

        // 2. Restore baseline scheduler policy
        struct sched_param sp{};
        sp.sched_priority = 0;
        ::sched_setscheduler(m_active_snapshot.pid, m_active_snapshot.original_sched_policy, &sp);

        // 3. Restore baseline core affinity
        ::sched_setaffinity(m_active_snapshot.pid, sizeof(cpu_set_t), &m_active_snapshot.original_affinity);

        // 4. Restore baseline timer slack
        MitigationEngine::apply_timer_slack(m_active_snapshot.pid, m_active_snapshot.original_timerslack_ns);
    }

    // 5. Close PM QoS file descriptor -> Kernel automatically restores C2/C3/C6 deep C-states
    m_pm_qos.release_latency_pin();

    // 6. Reset snapshot state
    m_active_snapshot = ActiveWindowResourceSnapshot{};
}

void WindowAwareGovernor::rollback_all() noexcept {
    release_active_window();
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
