#include "policy/mitigation_engine.hpp"
#include "core/posix_fs.hpp"
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <algorithm>

namespace wattcurb::policy {

namespace {

#ifndef IOPRIO_CLASS_IDLE
#define IOPRIO_CLASS_IDLE 3
#endif

#ifndef IOPRIO_CLASS_BE
#define IOPRIO_CLASS_BE 2
#endif

#ifndef IOPRIO_WHO_PROCESS
#define IOPRIO_WHO_PROCESS 1
#endif

#ifndef IOPRIO_PRIO_VALUE
#define IOPRIO_PRIO_VALUE(class_val, data_val) (((class_val) << 13) | ((data_val) & 0x1fff))
#endif

// Saved state for backlight restoration
static uint32_t s_saved_backlight_level{0};
static char s_saved_backlight_device[64]{0};

} // anonymous namespace

PowerProfileMode MitigationEngine::determine_profile(bool on_battery, double battery_pct) const noexcept {
    if (m_profile_override.has_value()) {
        return *m_profile_override;
    }

    if (!on_battery) {
        return PowerProfileMode::Balanced;
    }

    // REF-REQ-031 Sec 2.2: Hysteresis & Anti-Flapping Guards
    switch (m_current_profile) {
    case PowerProfileMode::Performance:
        if (battery_pct < 20.0) {
            return PowerProfileMode::UltraEndurance;
        }
        if (battery_pct <= 50.0) {
            return PowerProfileMode::PowerSaver;
        }
        return PowerProfileMode::Balanced;

    case PowerProfileMode::Balanced:
        if (battery_pct < 20.0) {
            return PowerProfileMode::UltraEndurance;
        }
        if (battery_pct <= 50.0) {
            return PowerProfileMode::PowerSaver;
        }
        return PowerProfileMode::Balanced;

    case PowerProfileMode::PowerSaver:
        if (battery_pct < 20.0) {
            return PowerProfileMode::UltraEndurance;
        }
        if (battery_pct > 55.0) { // 55% Hysteresis recovery
            return PowerProfileMode::Balanced;
        }
        return PowerProfileMode::PowerSaver;

    case PowerProfileMode::UltraEndurance:
        if (battery_pct > 55.0) {
            return PowerProfileMode::Balanced;
        }
        if (battery_pct >= 25.0) { // 25% Hysteresis recovery
            return PowerProfileMode::PowerSaver;
        }
        return PowerProfileMode::UltraEndurance;

    default:
        return PowerProfileMode::Balanced;
    }
}

bool MitigationEngine::resolve_cgroup_path(int32_t pid, char* out_buf, size_t out_cap) noexcept {
    if (pid <= 1 || !out_buf || out_cap < 32) return false;

    char proc_path[64];
    std::snprintf(proc_path, sizeof(proc_path), "/proc/%d/cgroup", pid);

    int fd = ::open(proc_path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;

    char read_buf[512];
    ssize_t bytes_read = ::read(fd, read_buf, sizeof(read_buf) - 1);
    ::close(fd);

    if (bytes_read <= 0) return false;
    read_buf[bytes_read] = '\0';

    // cgroup v2 format: "0::<path>\n"
    const char* ptr = read_buf;
    const char* end = read_buf + bytes_read;

    while (ptr < end) {
        if (ptr[0] == '0' && ptr[1] == ':' && ptr[2] == ':') {
            const char* path_start = ptr + 3;
            const char* line_end = path_start;
            while (line_end < end && *line_end != '\n' && *line_end != '\r') {
                ++line_end;
            }

            size_t path_len = static_cast<size_t>(line_end - path_start);
            // Construct full sysfs path: /sys/fs/cgroup + path
            constexpr const char base[] = "/sys/fs/cgroup";
            constexpr size_t base_len = sizeof(base) - 1;

            if (base_len + path_len + 1 >= out_cap) {
                return false; // Truncation guard
            }

            std::memcpy(out_buf, base, base_len);
            if (path_len > 0 && path_start[0] == '/') {
                std::memcpy(out_buf + base_len, path_start, path_len);
                out_buf[base_len + path_len] = '\0';
            } else if (path_len > 0) {
                out_buf[base_len] = '/';
                std::memcpy(out_buf + base_len + 1, path_start, path_len);
                out_buf[base_len + 1 + path_len] = '\0';
            } else {
                out_buf[base_len] = '\0';
            }
            return true;
        }

        // Advance to next line
        while (ptr < end && *ptr != '\n') {
            ++ptr;
        }
        if (ptr < end && *ptr == '\n') {
            ++ptr;
        }
    }

    return false;
}

bool MitigationEngine::apply_sched_idle(int32_t pid) noexcept {
    if (pid <= 1) return false;

    // 1. Set CPU scheduler to SCHED_IDLE
    struct sched_param sp{};
    sp.sched_priority = 0;
    int sched_ret = ::sched_setscheduler(pid, SCHED_IDLE, &sp);

    // 2. Set Block I/O scheduler to IOPRIO_CLASS_IDLE
#ifdef SYS_ioprio_set
    int prio_val = IOPRIO_PRIO_VALUE(IOPRIO_CLASS_IDLE, 7);
    int io_ret = static_cast<int>(::syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, pid, prio_val));
    return (sched_ret == 0 || io_ret == 0);
#else
    return (sched_ret == 0);
#endif
}

bool MitigationEngine::restore_sched_normal(int32_t pid) noexcept {
    if (pid <= 1) return false;

    // 1. Restore CPU scheduler to SCHED_OTHER (CFS)
    struct sched_param sp{};
    sp.sched_priority = 0;
    int sched_ret = ::sched_setscheduler(pid, SCHED_OTHER, &sp);

    // 2. Restore Block I/O scheduler to Best-Effort (IOPRIO_CLASS_BE, priority 4)
#ifdef SYS_ioprio_set
    int prio_val = IOPRIO_PRIO_VALUE(IOPRIO_CLASS_BE, 4);
    int io_ret = static_cast<int>(::syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, pid, prio_val));
    return (sched_ret == 0 || io_ret == 0);
#else
    return (sched_ret == 0);
#endif
}

bool MitigationEngine::apply_timer_slack(int32_t pid, uint64_t slack_ns) noexcept {
    if (pid <= 1 || slack_ns == 0) return false;

    char proc_path[64];
    std::snprintf(proc_path, sizeof(proc_path), "/proc/%d/timerslack_ns", pid);

    int fd = ::open(proc_path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) return false;

    char num_buf[32];
    int len = std::snprintf(num_buf, sizeof(num_buf), "%lu\n", static_cast<unsigned long>(slack_ns));
    ssize_t written = ::write(fd, num_buf, static_cast<size_t>(len));
    ::close(fd);

    return (written > 0);
}

bool MitigationEngine::apply_memory_reclaim(int32_t pid, uint64_t bytes) noexcept {
    if (pid <= 1 || bytes == 0) return false;

    char cg_path[256];
    if (!resolve_cgroup_path(pid, cg_path, sizeof(cg_path))) {
        return false;
    }

    char reclaim_path[320];
    std::snprintf(reclaim_path, sizeof(reclaim_path), "%s/memory.reclaim", cg_path);

    int fd = ::open(reclaim_path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) return false;

    char num_buf[32];
    int len = std::snprintf(num_buf, sizeof(num_buf), "%lu\n", static_cast<unsigned long>(bytes));
    ssize_t written = ::write(fd, num_buf, static_cast<size_t>(len));
    ::close(fd);

    return (written > 0);
}

bool MitigationEngine::apply_cgroup_freeze(int32_t pid, bool freeze) noexcept {
    if (pid <= 1) return false;

    // REF-REQ-044 & REF-RES-015: Absolute Zero-Kill & Zero-Freeze Invariant
    // Freezing user/system processes (cgroup.freeze = 1) causes D-Bus IPC deadlocks,
    // tree-wide application freezes, and process termination. WattCurb strictly forbids
    // freezing processes under any circumstance!
    if (freeze) {
        // Fallback safely to non-halting graceful idle throttling
        apply_sched_idle(pid);
        apply_timer_slack(pid, 100'000'000ULL);
        return false; // Prohibit cgroup freeze
    }

    // Thawing (unfreezing) is safely executed to recover any previously frozen process
    char cg_path[256];
    if (!resolve_cgroup_path(pid, cg_path, sizeof(cg_path))) {
        return false;
    }

    char freeze_path[320];
    std::snprintf(freeze_path, sizeof(freeze_path), "%s/cgroup.freeze", cg_path);

    int fd = ::open(freeze_path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) return false;

    ssize_t written = ::write(fd, "0\n", 2);
    ::close(fd);

    return (written > 0);
}

bool MitigationEngine::set_pcie_aspm_policy(const char* policy) noexcept {
    if (!policy) return false;
    int fd = ::open("/sys/module/pcie_aspm/parameters/policy", O_WRONLY | O_CLOEXEC);
    if (fd < 0) return false;

    size_t len = std::strlen(policy);
    ssize_t written = ::write(fd, policy, len);
    ::close(fd);
    return (written > 0);
}

bool MitigationEngine::set_cpu_epp_policy(const char* policy) noexcept {
    if (!policy) return false;
    int fd = ::open("/sys/devices/system/cpu/cpu0/power/energy_performance_preference", O_WRONLY | O_CLOEXEC);
    if (fd < 0) return false;

    size_t len = std::strlen(policy);
    ssize_t written = ::write(fd, policy, len);
    ::close(fd);
    return (written > 0);
}

bool MitigationEngine::cap_display_backlight(double max_pct) noexcept {
    DIR* dir = ::opendir("/sys/class/backlight");
    if (!dir) return false;

    struct dirent* entry = nullptr;
    bool success = false;
    while ((entry = ::readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;

        char path[256];
        std::snprintf(path, sizeof(path), "/sys/class/backlight/%s/max_brightness", entry->d_name);
        char max_buf[32]{};
        if (core::fs::read_small_file(path, max_buf, sizeof(max_buf)) <= 0) continue;
        uint32_t max_b = static_cast<uint32_t>(std::strtoul(max_buf, nullptr, 10));
        if (max_b == 0) continue;

        std::snprintf(path, sizeof(path), "/sys/class/backlight/%s/brightness", entry->d_name);
        char cur_buf[32]{};
        if (core::fs::read_small_file(path, cur_buf, sizeof(cur_buf)) <= 0) continue;
        uint32_t cur_b = static_cast<uint32_t>(std::strtoul(cur_buf, nullptr, 10));

        // Save original level and device name if not already saved
        if (s_saved_backlight_level == 0) {
            s_saved_backlight_level = cur_b;
            std::strncpy(s_saved_backlight_device, entry->d_name, sizeof(s_saved_backlight_device) - 1);
            s_saved_backlight_device[sizeof(s_saved_backlight_device) - 1] = '\0';
        }

        uint32_t cap_target = static_cast<uint32_t>(max_b * (max_pct / 100.0));
        if (cur_b > cap_target) {
            int fd = ::open(path, O_WRONLY | O_CLOEXEC);
            if (fd >= 0) {
                char write_buf[32];
                int wlen = std::snprintf(write_buf, sizeof(write_buf), "%u\n", cap_target);
                ssize_t written = ::write(fd, write_buf, static_cast<size_t>(wlen));
                ::close(fd);
                if (written > 0) {
                    success = true;
                }
            }
        }
    }
    ::closedir(dir);
    return success;
}

bool MitigationEngine::restore_display_backlight() noexcept {
    if (s_saved_backlight_level == 0 || s_saved_backlight_device[0] == '\0') {
        return false;
    }

    char path[256];
    std::snprintf(path, sizeof(path), "/sys/class/backlight/%s/brightness", s_saved_backlight_device);
    int fd = ::open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) return false;

    char write_buf[32];
    int wlen = std::snprintf(write_buf, sizeof(write_buf), "%u\n", s_saved_backlight_level);
    ssize_t written = ::write(fd, write_buf, static_cast<size_t>(wlen));
    ::close(fd);

    s_saved_backlight_level = 0;
    s_saved_backlight_device[0] = '\0';
    return (written > 0);
}

void MitigationEngine::rollback_all() noexcept {
    // Implements REF-REQ-031 Sec 3.2: Restore all mitigated processes to baseline
    for (const auto& tm : m_tracked) {
        if (tm.current_action == MitigationAction::CgroupFreeze) {
            apply_cgroup_freeze(tm.pid, false);
            restore_sched_normal(tm.pid);
        } else if (tm.current_action == MitigationAction::SchedIdle) {
            restore_sched_normal(tm.pid);
        }
        if (tm.original_timerslack_ns > 0) {
            apply_timer_slack(tm.pid, tm.original_timerslack_ns);
        }
    }
    m_tracked.clear();

    if (m_aspm_modified) {
        set_pcie_aspm_policy("default");
        m_aspm_modified = false;
    }
    if (m_backlight_capped) {
        restore_display_backlight();
        m_backlight_capped = false;
    }
    set_cpu_epp_policy("balance_performance");
}

void MitigationEngine::thaw_all_frozen() noexcept {
    // Transition from UltraEndurance to PowerSaver: thaw cgroups and downgrade to SCHED_IDLE
    for (auto& tm : m_tracked) {
        if (tm.current_action == MitigationAction::CgroupFreeze) {
            apply_cgroup_freeze(tm.pid, false);
            apply_sched_idle(tm.pid);
            tm.current_action = MitigationAction::SchedIdle;
        }
    }
    if (m_backlight_capped) {
        restore_display_backlight();
        m_backlight_capped = false;
    }
}

ActiveMitigationStatus MitigationEngine::evaluate_and_actuate(
    AnalysisReportData& report,
    bool on_battery,
    double battery_pct
) noexcept {
    ActiveMitigationStatus status{};

    // 1. Determine profile governed by state machine & hysteresis (REF-REQ-031 Sec 2.1)
    PowerProfileMode target_profile = determine_profile(on_battery, battery_pct);
    PowerProfileMode old_profile = m_current_profile;

    if (old_profile != target_profile) {
        if (target_profile == PowerProfileMode::Performance) {
            rollback_all();
            set_cpu_epp_policy("performance");
        } else if (target_profile == PowerProfileMode::Balanced) {
            rollback_all();
        } else if (old_profile == PowerProfileMode::UltraEndurance && target_profile == PowerProfileMode::PowerSaver) {
            thaw_all_frozen();
            set_cpu_epp_policy("balance_power");
        } else if (target_profile == PowerProfileMode::PowerSaver) {
            set_pcie_aspm_policy("powersave");
            m_aspm_modified = true;
            set_cpu_epp_policy("balance_power");
        } else if (target_profile == PowerProfileMode::UltraEndurance) {
            set_pcie_aspm_policy("powersave");
            m_aspm_modified = true;
            set_cpu_epp_policy("power");
            cap_display_backlight(50.0);
            m_backlight_capped = true;
        }
        m_current_profile = target_profile;
    }

    status.current_profile = m_current_profile;

    // In Performance mode: zero throttling, zero freezes, maximum throughput
    if (m_current_profile == PowerProfileMode::Performance) {
        if (status.feature_summary_count < status.feature_summaries.size()) {
            status.feature_summaries[status.feature_summary_count++] = "CPU: 4.1GHz Boost (Performance)";
        }
        if (status.feature_summary_count < status.feature_summaries.size()) {
            status.feature_summaries[status.feature_summary_count++] = "Mitigations: All Off (Full Speed)";
        }
        status.active_summary = "Performance Mode (4.1GHz Boost, Unconstrained)";
        report.mitigation_status = status;
        return status;
    }

    // 2. Add hardware feature summaries
    if (m_current_profile == PowerProfileMode::PowerSaver) {
        if (status.feature_summary_count < status.feature_summaries.size()) {
            status.feature_summaries[status.feature_summary_count++] = "PCIe ASPM: powersave";
        }
        if (status.feature_summary_count < status.feature_summaries.size()) {
            status.feature_summaries[status.feature_summary_count++] = "CPU EPP: balance_power";
        }
    } else if (m_current_profile == PowerProfileMode::UltraEndurance) {
        if (status.feature_summary_count < status.feature_summaries.size()) {
            status.feature_summaries[status.feature_summary_count++] = "PCIe ASPM: powersave";
        }
        if (status.feature_summary_count < status.feature_summaries.size()) {
            status.feature_summaries[status.feature_summary_count++] = "CPU EPP: power";
        }
        if (status.feature_summary_count < status.feature_summaries.size()) {
            status.feature_summaries[status.feature_summary_count++] = "Display Panel: 50% Soft-Cap";
        }
    }

    // 3. Inspect top power culprits from attribution analysis
    for (auto& proc : report.top_processes) {
        if (proc.pid <= 1) continue;

        auto tier = static_cast<ProcessSafetyTier>(proc.safety_tier);

        // Tier 0 (CriticalImmune) & Tier 1 (DesktopCore) are strictly untouchable! (REF-REQ-031 Sec 3.1)
        if (tier == ProcessSafetyTier::CriticalImmune || tier == ProcessSafetyTier::DesktopCore) {
            continue;
        }

        // Tier 2 (DesktopShell): Only safe proactive memory reclaim allowed under progressive profile
        if (tier == ProcessSafetyTier::DesktopShell) {
            if (m_current_profile == PowerProfileMode::UltraEndurance && proc.pss_kib > 250 * 1024) {
                uint64_t reclaim_target = 64ULL * 1024 * 1024; // 64 MB
                if (apply_memory_reclaim(proc.pid, reclaim_target)) {
                    status.reclaimed_bytes += reclaim_target;
                    status.estimated_savings_watts += 0.05;
                }
            }
            continue;
        }

        // Check if already tracked
        bool already_tracked = false;
        for (const auto& tm : m_tracked) {
            if (tm.pid == proc.pid) {
                already_tracked = true;
                break;
            }
        }

        // Mitigation candidate evaluation based on profile and WDI / wakeup score
        // REF-REQ-044: Non-Halting & Zero-Kill Invariant. Processes are NEVER frozen or killed!
        bool should_throttle = false;
        bool should_relax_timer = false;
        bool should_reclaim = false;

        if (tier == ProcessSafetyTier::BackgroundWorker) {
            // Background indexing/sync tasks (baloo, tracker, updatedb, etc.)
            if (m_current_profile == PowerProfileMode::UltraEndurance) {
                should_throttle = true;
                should_relax_timer = true;
                should_reclaim = (proc.pss_kib > 50 * 1024);
            } else if (m_current_profile == PowerProfileMode::PowerSaver) {
                should_throttle = (proc.wdi_score > 3.0 || proc.wakeups_per_sec > 50);
                should_relax_timer = (proc.wakeups_per_sec > 100);
                should_reclaim = (proc.pss_kib > 100 * 1024);
            } else { // Balanced
                should_throttle = (proc.wdi_score > 10.0 || proc.cpu_watts > 1.5);
                should_relax_timer = (proc.wakeups_per_sec > 250);
            }
        } else if (tier == ProcessSafetyTier::RunawayCandidate) {
            // General worker or runaway candidate - graceful throttling only, never killed
            if (proc.is_runaway_candidate || proc.wdi_score > 10.0) {
                should_throttle = true;
                should_relax_timer = true;
                if (m_current_profile == PowerProfileMode::UltraEndurance) {
                    should_reclaim = true;
                }
            }
        } else if (tier == ProcessSafetyTier::UserInteractive) {
            // Interactive apps (browser, terminal, editor)
            if (m_current_profile == PowerProfileMode::UltraEndurance && proc.wakeups_per_sec > 300) {
                should_relax_timer = true;
            }
            if (m_current_profile != PowerProfileMode::Balanced && proc.pss_kib > 500 * 1024 && proc.cpu_watts < 0.2) {
                should_reclaim = true;
            }
        }

        // Apply Actuations (Zero-Freeze: Only SchedIdle, TimerSlack, MemoryReclaim)
        if (should_throttle && !already_tracked) {
            if (apply_sched_idle(proc.pid)) {
                ++status.throttled_count;
                status.estimated_savings_watts += (proc.cpu_watts * 0.4);
                if (m_tracked.size() < MAX_TRACKED_MITIGATIONS) {
                    m_tracked.push_back(TrackedMitigation{
                        .pid = proc.pid,
                        .tier = tier,
                        .current_action = MitigationAction::SchedIdle,
                        .applied_timestamp_sec = 0,
                        .original_timerslack_ns = proc.timerslack_ns
                    });
                }
            }
        }

        if (should_relax_timer && proc.timerslack_ns < 100'000'000ULL) { // Relax to 100ms
            if (apply_timer_slack(proc.pid, 100'000'000ULL)) {
                status.estimated_savings_watts += (proc.wakeup_tax_watts * 0.5);
            }
        }

        if (should_reclaim) {
            uint64_t reclaim_amount = std::min(proc.pss_kib * 1024ULL / 2, 128ULL * 1024 * 1024);
            if (reclaim_amount > 0 && apply_memory_reclaim(proc.pid, reclaim_amount)) {
                status.reclaimed_bytes += reclaim_amount;
                status.estimated_savings_watts += 0.04;
            }
        }
    }

    // 4. Build active summary string with profile prefix
    const char* profile_label = "[Balanced]";
    if (m_current_profile == PowerProfileMode::PowerSaver) {
        profile_label = "[PowerSaver]";
    } else if (m_current_profile == PowerProfileMode::UltraEndurance) {
        profile_label = "[UltraEndurance]";
    }

    char summary_buf[128];
    if (status.throttled_count == 0 && status.frozen_count == 0 && status.reclaimed_bytes == 0) {
        std::snprintf(summary_buf, sizeof(summary_buf), "%s Optimal (No throttling needed)", profile_label);
    } else {
        std::snprintf(summary_buf, sizeof(summary_buf),
                      "%s %zu throttled, %zu frozen, %luMB reclaimed (~%.2fW saved)",
                      profile_label,
                      status.throttled_count,
                      status.frozen_count,
                      static_cast<unsigned long>(status.reclaimed_bytes / (1024 * 1024)),
                      status.estimated_savings_watts);
    }
    status.active_summary = summary_buf;

    report.mitigation_status = status;
    return status;
}

} // namespace wattcurb::policy
