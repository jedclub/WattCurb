#include "policy/process_classifier.hpp"
#include <algorithm>

namespace wattcurb::policy {

// Implements REF-RES-008 & REF-ARCH-008: Process Knowledge Base
ProcessClassification ProcessClassifierDB::classify(std::string_view comm) noexcept {
    // 1. Tier 0: Critical Kernel, Audio & Init Services (CRITICAL_IMMUNE)
    // REF-REQ-049: Absolute Realtime Immunity for PipeWire, PulseAudio, and Audio Stack
    // REF-REQ-051: Daemon Self-Immunity (WattCurb components must never throttle themselves)
    // REF-ARCH-045: IME (fcitx5, ibus) and Desktop Core Daemons (kded6) Absolute Immunity
    if (comm == "systemd" || comm == "init" || comm == "kthreadd" ||
        comm.starts_with("kworker") || comm.starts_with("pipewire") || comm.starts_with("wireplumber") ||
        comm == "pulseaudio" || comm.starts_with("jackd") || comm == "jackdbus" ||
        comm == "alsactl" || comm == "rtkit-daemon" || comm == "sndiod" ||
        comm == "dbus-broker" || comm == "dbus-daemon" || comm == "seatd" ||
        comm == "polkitd" || comm == "udevd" || comm == "systemd-journal" ||
        comm == "systemd-resolve" || comm == "systemd-logind" || comm == "NetworkManager" ||
        comm == "wpa_supplicant" || comm == "iwd" || comm == "bluetoothd" ||
        comm == "upowerd" || comm == "acpid" || comm == "auditd" ||
        comm == "fcitx5" || comm == "fcitx" || comm == "ibus-daemon" || comm == "ibus" ||
        comm == "kded6" || comm == "kded5" || comm == "ksmserver" ||
        comm.starts_with("wattcurb")) {
        return ProcessClassification{
            .tier = ProcessSafetyTier::CriticalImmune,
            .default_action = MitigationAction::None,
            .category = "Critical System",
            .can_throttle_scheduler = false,
            .can_reclaim_memory = false,
            .can_freeze = false
        };
    }

    // 2. Tier 1: Desktop Compositor, Core Terminals & Development Tools (DESKTOP_CORE)
    // REF-ARCH-045: Interactive Terminals, IDEs, and Developer Tools are immune from scheduler throttling
    if (comm == "kwin_wayland" || comm == "kwin_x11" || comm == "kwin" ||
        comm == "mutter" || comm == "sway" || comm == "hyprland" ||
        comm == "Xorg" || comm == "Xwayland" || comm == "weston" ||
        comm == "foot" || comm == "kitty" || comm == "alacritty" ||
        comm == "konsole" || comm == "wezterm" || comm == "gnome-terminal" ||
        comm == "opencode" || comm == "agy" || comm == "code" ||
        comm == "cursor" || comm == "zed" || comm == "nvim" || comm == "emacs") {
        return ProcessClassification{
            .tier = ProcessSafetyTier::DesktopCore,
            .default_action = MitigationAction::None,
            .category = "Desktop Core / Terminal",
            .can_throttle_scheduler = false,
            .can_reclaim_memory = false,
            .can_freeze = false
        };
    }

    // 3. Tier 2: Desktop Shell & System Panels (DESKTOP_SHELL)
    if (comm == "plasmashell" || comm == "gnome-shell" || comm == "xfce4-panel" ||
        comm == "krunner" || comm == "polybar" || comm == "waybar") {
        return ProcessClassification{
            .tier = ProcessSafetyTier::DesktopShell,
            .default_action = MitigationAction::MemoryReclaim,
            .category = "Desktop Shell",
            .can_throttle_scheduler = false,
            .can_reclaim_memory = true,
            .can_freeze = false
        };
    }

    // 4. Tier 4: Background Indexers & Sync Workers (BACKGROUND_WORKER)
    // Check background workers before user apps to catch baloo/tracker immediately
    if (comm.starts_with("baloo_file") || comm.starts_with("tracker-miner") ||
        comm.starts_with("tracker-extract") || comm == "updatedb" || comm == "locate" ||
        comm == "packagekitd" || comm == "nextcloud" || comm == "dropbox" ||
        comm == "rclone" || comm == "syncthing" || comm == "ksystemstats" ||
        comm.starts_with("apt-daily") || comm == "mandb") {
        return ProcessClassification{
            .tier = ProcessSafetyTier::BackgroundWorker,
            .default_action = MitigationAction::SchedIdle,
            .category = "Background Indexer/Sync",
            .can_throttle_scheduler = true,
            .can_reclaim_memory = true,
            .can_freeze = false // REF-REQ-044: Freezing strictly disabled
        };
    }

    // 5. Tier 3: Heavy User Interactive Applications (USER_INTERACTIVE)
    if (comm == "chrome" || comm == "firefox" || comm == "zen-browser" ||
        comm == "brave" || comm == "edge" || comm == "chromium" ||
        comm == "claude" || comm == "clion" || comm == "pycharm" ||
        comm == "slack" || comm == "discord" || comm == "telegram-deskto" ||
        comm == "teams" || comm == "spotify" || comm == "steam" || comm == "obs") {
        return ProcessClassification{
            .tier = ProcessSafetyTier::UserInteractive,
            .default_action = MitigationAction::RelaxTimerSlack,
            .category = "Interactive App",
            .can_throttle_scheduler = true,
            .can_reclaim_memory = true,
            .can_freeze = false // REF-REQ-044: Freezing strictly disabled
        };
    }

    // 6. Tier 5: Ephemeral Daemons or Runaway Candidates (RUNAWAY_CANDIDATE)
    return ProcessClassification{
        .tier = ProcessSafetyTier::RunawayCandidate,
        .default_action = MitigationAction::SchedIdle,
        .category = "General / Worker",
        .can_throttle_scheduler = true,
        .can_reclaim_memory = true,
        .can_freeze = false // REF-REQ-044: Freezing strictly disabled
    };
}

const char* ProcessClassifierDB::tier_name(ProcessSafetyTier tier) noexcept {
    switch (tier) {
        case ProcessSafetyTier::CriticalImmune: return "Tier 0 (Immune)";
        case ProcessSafetyTier::DesktopCore:    return "Tier 1 (Compositor)";
        case ProcessSafetyTier::DesktopShell:   return "Tier 2 (Shell)";
        case ProcessSafetyTier::UserInteractive:return "Tier 3 (User App)";
        case ProcessSafetyTier::BackgroundWorker:return "Tier 4 (Bg Worker)";
        case ProcessSafetyTier::RunawayCandidate:return "Tier 5 (Runaway)";
    }
    return "Unknown";
}

const char* ProcessClassifierDB::action_name(MitigationAction action) noexcept {
    switch (action) {
        case MitigationAction::None:           return "None";
        case MitigationAction::SchedIdle:      return "SCHED_IDLE";
        case MitigationAction::RelaxTimerSlack:return "Relax Timer Slack";
        case MitigationAction::MemoryReclaim:  return "Reclaim Memory";
        case MitigationAction::CgroupFreeze:   return "Freeze Cgroup (Disabled)";
        case MitigationAction::Terminate:      return "Terminate (Prohibited)";
        case MitigationAction::AffinityCap:    return "AffinityCap (Headroom Guard)";
    }
    return "Unknown";
}

} // namespace wattcurb::policy
