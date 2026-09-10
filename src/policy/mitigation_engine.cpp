#include "policy/mitigation_engine.hpp"
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cstdio>
#include <algorithm>

namespace wattcurb::policy {

namespace {

#ifndef IOPRIO_CLASS_IDLE
#define IOPRIO_CLASS_IDLE 3
#endif

#ifndef IOPRIO_WHO_PROCESS
#define IOPRIO_WHO_PROCESS 1
#endif

#ifndef IOPRIO_PRIO_VALUE
#define IOPRIO_PRIO_VALUE(class_val, data_val) (((class_val) << 13) | ((data_val) & 0x1fff))
#endif

} // anonymous namespace

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

    char cg_path[256];
    if (!resolve_cgroup_path(pid, cg_path, sizeof(cg_path))) {
        return false;
    }

    char freeze_path[320];
    std::snprintf(freeze_path, sizeof(freeze_path), "%s/cgroup.freeze", cg_path);

    int fd = ::open(freeze_path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) return false;

    const char* val = freeze ? "1\n" : "0\n";
    ssize_t written = ::write(fd, val, 2);
    ::close(fd);

    return (written > 0);
}

ActiveMitigationStatus MitigationEngine::evaluate_and_actuate(
    AnalysisReportData& report,
    bool on_battery,
    double battery_pct
) noexcept {
    ActiveMitigationStatus status{};

    // Determine Aggressiveness Profile (REF-ARCH-008 & REF-REQ-019)
    // - Battery > 50% or AC Power: Conservative (IDLE on runaway/heavy workers only)
    // - Battery 20% ~ 50%: Moderate (IDLE + TimerSlack + Reclaim on background & heavy interactive)
    // - Battery < 20%: Progressive (Aggressive IDLE, Reclaim, and Freeze on background workers)
    enum class Aggressiveness {
        Conservative,
        Moderate,
        Progressive
    } profile = Aggressiveness::Conservative;

    if (on_battery) {
        if (battery_pct < 20.0) {
            profile = Aggressiveness::Progressive;
        } else if (battery_pct <= 50.0) {
            profile = Aggressiveness::Moderate;
        } else {
            profile = Aggressiveness::Conservative;
        }
    } else {
        profile = Aggressiveness::Conservative;
    }

    // Inspect top power culprits from attribution analysis
    for (auto& proc : report.top_processes) {
        if (proc.pid <= 1) continue;

        auto tier = static_cast<ProcessSafetyTier>(proc.safety_tier);

        // Tier 0 (CriticalImmune) & Tier 1 (DesktopCore) are strictly untouchable!
        if (tier == ProcessSafetyTier::CriticalImmune || tier == ProcessSafetyTier::DesktopCore) {
            continue;
        }

        // Tier 2 (DesktopShell): Only safe proactive memory reclaim allowed under progressive profile
        if (tier == ProcessSafetyTier::DesktopShell) {
            if (profile == Aggressiveness::Progressive && proc.pss_kib > 250 * 1024) {
                uint64_t reclaim_target = 64ULL * 1024 * 1024; // 64 MB
                if (apply_memory_reclaim(proc.pid, reclaim_target)) {
                    status.reclaimed_bytes += reclaim_target;
                    status.estimated_savings_watts += 0.05; // Reduced DRAM refresh/dirty pages
                }
            }
            continue;
        }

        // Check if already mitigated
        bool already_tracked = false;
        for (const auto& tm : m_tracked) {
            if (tm.pid == proc.pid) {
                already_tracked = true;
                break;
            }
        }

        // Mitigation candidate evaluation based on profile and WDI / wakeup score
        bool should_throttle = false;
        bool should_relax_timer = false;
        bool should_reclaim = false;
        bool should_freeze = false;

        if (tier == ProcessSafetyTier::BackgroundWorker) {
            // Background indexing/sync tasks (baloo, tracker, updatedb, etc.)
            if (profile == Aggressiveness::Progressive) {
                should_throttle = true;
                should_relax_timer = true;
                should_reclaim = (proc.pss_kib > 50 * 1024);
                if (proc.wdi_score > 8.0) {
                    should_freeze = true;
                }
            } else if (profile == Aggressiveness::Moderate) {
                should_throttle = (proc.wdi_score > 3.0 || proc.wakeups_per_sec > 50);
                should_relax_timer = (proc.wakeups_per_sec > 100);
                should_reclaim = (proc.pss_kib > 100 * 1024);
            } else { // Conservative
                should_throttle = (proc.wdi_score > 10.0 || proc.cpu_watts > 1.5);
                should_relax_timer = (proc.wakeups_per_sec > 250);
            }
        } else if (tier == ProcessSafetyTier::RunawayCandidate) {
            // General worker or runaway candidate
            if (proc.is_runaway_candidate || proc.wdi_score > 12.0) {
                should_throttle = true;
                should_relax_timer = true;
                if (profile == Aggressiveness::Progressive) {
                    should_reclaim = true;
                    if (proc.wdi_score > 25.0) {
                        should_freeze = true;
                    }
                }
            }
        } else if (tier == ProcessSafetyTier::UserInteractive) {
            // Interactive apps (browser, terminal, editor)
            // Never freeze; relax timer slack or reclaim memory only if severely draining in background
            if (profile == Aggressiveness::Progressive && proc.wakeups_per_sec > 300) {
                should_relax_timer = true;
            }
            if (profile != Aggressiveness::Conservative && proc.pss_kib > 500 * 1024 && proc.cpu_watts < 0.2) {
                // Background idle browser tab/window holding massive RAM
                should_reclaim = true;
            }
        }

        // Apply Actuations
        if (should_freeze && !already_tracked) {
            if (apply_cgroup_freeze(proc.pid, true)) {
                ++status.frozen_count;
                status.estimated_savings_watts += (proc.total_attributed_watts * 0.9);
                if (m_tracked.size() < MAX_TRACKED_MITIGATIONS) {
                    m_tracked.push_back(TrackedMitigation{
                        .pid = proc.pid,
                        .current_action = MitigationAction::CgroupFreeze,
                        .applied_timestamp_sec = 0
                    });
                }
            }
        } else if (should_throttle && !already_tracked) {
            if (apply_sched_idle(proc.pid)) {
                ++status.throttled_count;
                status.estimated_savings_watts += (proc.cpu_watts * 0.4);
                if (m_tracked.size() < MAX_TRACKED_MITIGATIONS) {
                    m_tracked.push_back(TrackedMitigation{
                        .pid = proc.pid,
                        .current_action = MitigationAction::SchedIdle,
                        .applied_timestamp_sec = 0
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

    // Build active summary string
    char summary_buf[128];
    if (status.throttled_count == 0 && status.frozen_count == 0 && status.reclaimed_bytes == 0) {
        std::snprintf(summary_buf, sizeof(summary_buf), "Optimal (No throttling needed)");
    } else {
        std::snprintf(summary_buf, sizeof(summary_buf),
                      "%zu throttled, %zu frozen, %luMB reclaimed (~%.2fW saved)",
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
