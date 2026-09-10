#pragma once

#include <string_view>
#include <cstdint>

namespace wattcurb::policy {

// Implements REF-REQ-019 & REF-RES-008: Process Safety Tiers
enum class ProcessSafetyTier : uint8_t {
    CriticalImmune = 0,    // Tier 0: systemd, kthreadd, pipewire, dbus (DO NOT TOUCH)
    DesktopCore = 1,       // Tier 1: kwin_wayland, mutter, Xorg (NEVER FREEZE/KILL)
    DesktopShell = 2,      // Tier 2: plasmashell, gnome-shell (Reclaim only)
    UserInteractive = 3,   // Tier 3: chrome, kitty, code, slack (Conditional throttle/reclaim)
    BackgroundWorker = 4,  // Tier 4: baloo, updatedb, tracker (Aggressive IDLE/freeze)
    RunawayCandidate = 5   // Tier 5: Orphaned daemons, runaway scripts (Freeze/Kill)
};

// Implements REF-REQ-019 & REF-ARCH-008: Progressive Mitigation Actions
enum class MitigationAction : uint8_t {
    None = 0,
    SchedIdle = 1,         // Stage 1: sched_setscheduler(SCHED_IDLE) + ionice(class 3)
    RelaxTimerSlack = 2,   // Stage 2: timerslack_ns -> 100ms ~ 500ms
    MemoryReclaim = 3,     // Stage 3: cgroup.memory.reclaim
    CgroupFreeze = 4,      // Stage 4: cgroup.freeze = 1
    Terminate = 5          // Stage 5: SIGTERM (Emergency runaway only)
};

struct ProcessClassification {
    ProcessSafetyTier tier{ProcessSafetyTier::RunawayCandidate};
    MitigationAction default_action{MitigationAction::None};
    const char* category{"General"};
    bool can_throttle_scheduler{false};
    bool can_reclaim_memory{false};
    bool can_freeze{false};
};

class ProcessClassifierDB {
public:
    [[nodiscard]] static ProcessClassification classify(std::string_view comm) noexcept;
    [[nodiscard]] static const char* tier_name(ProcessSafetyTier tier) noexcept;
    [[nodiscard]] static const char* action_name(MitigationAction action) noexcept;
};

} // namespace wattcurb::policy
