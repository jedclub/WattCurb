#pragma once

#include "core/custom_containers.hpp"
#include "policy/process_classifier.hpp"
#include "policy/mitigation_engine.hpp"
#include <cstdint>

namespace wattcurb::policy {

// Implements REF-REQ-033, REF-ARCH-023:
// Non-Halting Graceful Throttle & Window-Aware Desktop Governor for KDE Plasma 6 (Wayland)
// Core Invariant: NEVER freeze or halt background applications completely.
// Processes remain 100% alive for WebSockets, background notifications, and IPC,
// while yielding CPU runqueue priority (SCHED_IDLE) and coalescing wakeup timers.
enum class WindowSuppressionState : uint8_t {
    ActiveForeground     = 0, // Uninhibited (SCHED_OTHER, 50µs timerslack)
    GracefulIdleThrottled = 1  // Non-Halting Throttle (SCHED_IDLE, 50ms timerslack, IOPRIO_IDLE)
};

struct alignas(32) WindowStateEntry {
    int32_t pid{0};
    uint64_t minimized_timestamp_sec{0};
    WindowSuppressionState state{WindowSuppressionState::ActiveForeground};
    bool has_active_audio{false};
    bool is_terminal{false};
    uint64_t original_timerslack_ns{50000};
};

class WindowAwareGovernor {
public:
    static constexpr size_t MAX_TRACKED_WINDOWS = 64;

    WindowAwareGovernor() noexcept = default;

    // Ingests window state events from KWin Scripting / D-Bus
    void on_window_state_changed(
        int32_t pid, 
        bool minimized, 
        bool active, 
        uint64_t now_sec,
        bool is_audio_active = false
    ) noexcept;

    // Evaluates non-halting graceful throttle state
    void evaluate_hysteresis(uint64_t now_sec) noexcept;

    // Instantaneous unthrottle and scheduler restoration (< 50µs)
    bool unthrottle_immediate(int32_t pid) noexcept;

    // Global rollback (e.g. on AC reconnection or daemon shutdown)
    void rollback_all() noexcept;

    [[nodiscard]] size_t tracked_count() const noexcept { return m_windows.size(); }
    [[nodiscard]] const core::FixedVector<WindowStateEntry, MAX_TRACKED_WINDOWS>& entries() const noexcept {
        return m_windows;
    }

    [[nodiscard]] const WindowStateEntry* find_entry(int32_t pid) const noexcept {
        for (size_t i = 0; i < m_windows.size(); ++i) {
            if (m_windows[i].pid == pid) return &m_windows[i];
        }
        return nullptr;
    }

private:
    WindowStateEntry* find_entry_mut(int32_t pid) noexcept {
        for (size_t i = 0; i < m_windows.size(); ++i) {
            if (m_windows[i].pid == pid) return &m_windows[i];
        }
        return nullptr;
    }

    core::FixedVector<WindowStateEntry, MAX_TRACKED_WINDOWS> m_windows{};
};

} // namespace wattcurb::policy
