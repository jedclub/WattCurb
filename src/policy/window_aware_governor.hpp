#pragma once

#include "core/custom_containers.hpp"
#include "policy/process_classifier.hpp"
#include "policy/mitigation_engine.hpp"
#include <cstdint>

namespace wattcurb::policy {

// Implements REF-REQ-032, REF-REQ-033, REF-ARCH-022, REF-ARCH-023
// Window-Aware Dynamic Suppression Ladder & Desktop Governor for KDE Plasma 6 (Wayland)
enum class WindowSuppressionState : uint8_t {
    ActiveForeground = 0, // Uninhibited (SCHED_OTHER, 50µs timerslack)
    Stage1Throttled  = 1, // SCHED_IDLE, 100ms timerslack, uclamp capped
    Stage2Frozen     = 2  // cgroup.freeze = 1, memory reclaimed
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
    static constexpr uint64_t STAGE2_HYSTERESIS_SEC = 20; // 20 seconds before freezing

    WindowAwareGovernor() noexcept = default;

    // Ingests window state events from KWin Scripting / D-Bus
    void on_window_state_changed(
        int32_t pid, 
        bool minimized, 
        bool active, 
        uint64_t now_sec,
        bool is_audio_active = false
    ) noexcept;

    // Evaluates hysteresis for all minimized windows (escalation to Stage 2 freezing)
    void evaluate_hysteresis(uint64_t now_sec) noexcept;

    // Instantaneous thaw and scheduler restoration (< 1.0ms)
    bool thaw_immediate(int32_t pid) noexcept;

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
