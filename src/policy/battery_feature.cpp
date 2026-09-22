#include "policy/battery_feature.hpp"
#include "policy/memory_pressure_guard.hpp"
#include "policy/process_classifier.hpp"
#include "core/posix_fs.hpp"
#include "policy/mitigation_engine.hpp"
#include "core/event_logger.hpp"

#include <sched.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace wattcurb::policy {

FeatureDescriptor FeatureManager::descriptor(FeatureId id) noexcept {
    switch (id) {
        case FeatureId::SchedIdleThrottle:
            return FeatureDescriptor{
                .id = FeatureId::SchedIdleThrottle,
                .feature_code = "FEAT-001",
                .name = "SchedIdleThrottle",
                .target_domain = "CPU / Scheduler",
                .kernel_mechanism = "sched_setscheduler(pid, SCHED_IDLE) + syscall(SYS_ioprio_set, IOPRIO_CLASS_IDLE)",
                .power_saving_rationale = "Drops thread weight to zero against CFS; reduces core frequency scaling up-steps and prevents boost residency on non-interactive loads, saving 30-50% CPU watts.",
                .safety_constraints = "Tier 0 (CriticalImmune) & Tier 1 (DesktopCore) 100% immune; applied only to Tier 3/4.",
                .description = "Throttles background workers/runaways to SCHED_IDLE and idle I/O priority",
                .default_enabled = true
            };
        case FeatureId::TimerSlackCoalescing:
            return FeatureDescriptor{
                .id = FeatureId::TimerSlackCoalescing,
                .feature_code = "FEAT-002",
                .name = "TimerSlackCoalescing",
                .target_domain = "Kernel Timers",
                .kernel_mechanism = "prctl(PR_SET_TIMERSLACK, 100ms..500ms) or /proc/[pid]/timerslack_ns write",
                .power_saving_rationale = "Aligns uncoordinated process wakeups with global tick periods, extending CPU Package C6/C10 sleep duration and preventing deep package state exit penalties (~0.8W).",
                .safety_constraints = "Tier 0/1 immune. Audio/compositor exempt to prevent buffer underrun/stutter.",
                .description = "Coalesces timer slack up to 100~500ms to group wakeups and maximize CPU C-state residency",
                .default_enabled = true
            };
        case FeatureId::ProactiveMemoryReclaim:
            return FeatureDescriptor{
                .id = FeatureId::ProactiveMemoryReclaim,
                .feature_code = "FEAT-003",
                .name = "ProactiveMemoryReclaim",
                .target_domain = "Memory / ZRAM",
                .kernel_mechanism = "cgroup.reclaim write or madvise(MADV_PAGEOUT) on background anonymous inactive pages",
                .power_saving_rationale = "Compresses inactive background pages to ZRAM/swap, reducing physical DRAM page activation, refresh current, and memory controller energy (~50-100mW per GB reclaimed).",
                .safety_constraints = "Tier 0/1 immune. Only triggered when battery < 50% or high memory pressure.",
                .description = "Reclaims inactive anonymous pages from idle background processes to save DRAM power",
                .default_enabled = true
            };
        case FeatureId::CgroupFreezer:
            return FeatureDescriptor{
                .id = FeatureId::CgroupFreezer,
                .feature_code = "FEAT-004",
                .name = "CgroupFreezer",
                .target_domain = "Cgroup Subsystem",
                .kernel_mechanism = "Strictly Disabled by REF-REQ-044 (Zero-Kill & Non-Halting Invariant). Fallback to SchedIdle.",
                .power_saving_rationale = "Disabled to prevent desktop hangs and process killing. Background tasks throttled via SCHED_IDLE instead.",
                .safety_constraints = "Prohibited under all profiles to preserve system stability and user responsiveness.",
                .description = "Disabled: Process freezing prohibited by REF-REQ-044 Zero-Kill invariant",
                .default_enabled = false
            };
        case FeatureId::ZenCcxAffinityPinning:
            return FeatureDescriptor{
                .id = FeatureId::ZenCcxAffinityPinning,
                .feature_code = "FEAT-005",
                .name = "ZenCcxAffinityPinning",
                .target_domain = "AMD Zen IF / CCX",
                .kernel_mechanism = "sched_setaffinity(pid, ccx_cpuset) restricting cross-CCX thread ping-pong",
                .power_saving_rationale = "Eliminates high-power Infinity Fabric (Data Fabric) cache line transfers between remote CCX complexes, localizing L3 cache hits and saving 150-300mW interconnect power.",
                .safety_constraints = "Only applied to multi-CCX Zen CPUs when cross-CCX migration thrashing is detected.",
                .description = "Pins multi-threaded workloads within a single CCX to eliminate Infinity Fabric interconnect energy",
                .default_enabled = true
            };
        case FeatureId::DisplayBacklightFloor:
            return FeatureDescriptor{
                .id = FeatureId::DisplayBacklightFloor,
                .feature_code = "FEAT-006",
                .name = "DisplayBacklightFloor",
                .target_domain = "Display Panel",
                .kernel_mechanism = "/sys/class/backlight/[card]/brightness sysfs write or advisory caps",
                .power_saving_rationale = "Display backlight power scales linearly to quadratically with brightness. Capping excessive brightness on battery yields direct 1.0W~3.5W immediate power reduction.",
                .safety_constraints = "Advisory mode or soft cap; user interactive brightness adjustment overrides.",
                .description = "Monitors and caps peak panel backlight brightness under low battery states",
                .default_enabled = true
            };
        case FeatureId::PcieAspmEnforcer:
            return FeatureDescriptor{
                .id = FeatureId::PcieAspmEnforcer,
                .feature_code = "FEAT-007",
                .name = "PcieAspmEnforcer",
                .target_domain = "PCIe Bus",
                .kernel_mechanism = "/sys/module/pcie_aspm/parameters/policy = powersave & device runtime_pm auto",
                .power_saving_rationale = "Enforces PCIe Active State Power Management L1/L1.1/L1.2 low-power substates and NVMe APST, reducing idle bus link transceiver power by 300-800mW.",
                .safety_constraints = "Restores previous policy when on AC; skips blacklisted faulty PCIe bridge devices.",
                .description = "Enforces PCIe ASPM powersave policy and NVMe autonomous power state transitions on battery",
                .default_enabled = true
            };
        case FeatureId::AntiStarvationHeadroom:
            return FeatureDescriptor{
                .id = FeatureId::AntiStarvationHeadroom,
                .feature_code = "FEAT-008",
                .name = "AntiStarvationHeadroom",
                .target_domain = "CPU Scheduler / Core Affinity",
                .kernel_mechanism = "sched_setaffinity(task, allowed_mask) + sched_setscheduler(task, SCHED_BATCH)",
                .power_saving_rationale = "Reserves clean physical headroom cores for real-time interactive audio (PipeWire) and display compositor (KWin Wayland). Restricts greedy multi-threaded compute workloads (>150% CPU) to remaining cores and enforces CFS batch preemption, eliminating audio dropouts and desktop freezes.",
                .safety_constraints = "Tier 0 (CriticalImmune) & Tier 1 (DesktopCore) strictly immune. Never terminates or freezes processes (REF-REQ-044).",
                .description = "Guarantees clean CPU headroom cores for audio & compositor, capping greedy batch processes to prevent system freezes",
                .default_enabled = true
            };
        case FeatureId::Count:
            break;
    }
    return FeatureDescriptor{
        .id = FeatureId::Count,
        .feature_code = "FEAT-UNK",
        .name = "Unknown",
        .target_domain = "None",
        .kernel_mechanism = "None",
        .power_saving_rationale = "None",
        .safety_constraints = "None",
        .description = "",
        .default_enabled = false
    };
}

FeatureManager::FeatureManager() noexcept {
    for (size_t i = 0; i < static_cast<size_t>(FeatureId::Count); ++i) {
        FeatureMetrics fm{};
        fm.enabled = descriptor(static_cast<FeatureId>(i)).default_enabled;
        m_metrics[i] = fm;
    }
}

void FeatureManager::set_feature_enabled(FeatureId id, bool enabled) noexcept {
    auto idx = static_cast<size_t>(id);
    if (idx < m_metrics.size()) {
        m_metrics[idx].enabled = enabled;
    }
}

bool FeatureManager::is_feature_enabled(FeatureId id) const noexcept {
    auto idx = static_cast<size_t>(id);
    if (idx < m_metrics.size()) {
        return m_metrics[idx].enabled;
    }
    return false;
}

bool FeatureManager::actuate_sched_idle(int32_t pid) noexcept {
    return MitigationEngine::apply_sched_idle(pid);
}

bool FeatureManager::actuate_timer_slack(int32_t pid, uint64_t slack_ns) noexcept {
    return MitigationEngine::apply_timer_slack(pid, slack_ns);
}

bool FeatureManager::actuate_memory_reclaim(int32_t pid, uint64_t bytes) noexcept {
    // REF-REQ-112: under memory pressure this reclaim must not be allowed to
    // convert a power optimisation into swap consumption.
    return MitigationEngine::apply_memory_reclaim(pid, bytes,
                                                  MemoryPressureGuard::swap_feeding_suspended());
}

bool FeatureManager::actuate_cgroup_freeze(int32_t pid, bool freeze) noexcept {
    return MitigationEngine::apply_cgroup_freeze(pid, freeze);
}

bool FeatureManager::actuate_ccx_affinity(int32_t pid, int32_t target_core) noexcept {
    if (pid <= 1 || target_core < 0) return false;

    const auto& topo = MitigationEngine::get_cluster_topology();
    if (topo.cluster_count >= 2) {
        const cpu_set_t& target_set = CPU_ISSET(static_cast<size_t>(target_core), &topo.c1_cpuset) 
                                      ? topo.c1_cpuset : topo.c2_cpuset;
        return (::sched_setaffinity(pid, sizeof(cpu_set_t), &target_set) == 0);
    }

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);

    // Compute CCX core block (e.g. Zen 8-core CCX block: 0-7 or 8-15)
    int32_t ccx_base = (target_core / 8) * 8;
    for (int32_t c = ccx_base; c < ccx_base + 8; ++c) {
        CPU_SET(static_cast<size_t>(c), &cpuset);
    }

    return (::sched_setaffinity(pid, sizeof(cpu_set_t), &cpuset) == 0);
}

namespace {

// REF-REQ-111 (DEF-3): field 22 of /proc/<pid>/stat, the process start time in
// clock ticks since boot. Parsed from after the ')' that closes comm, because
// comm itself may contain spaces and parentheses.
uint64_t read_proc_start_ticks(int32_t pid) noexcept {
    if (pid <= 0) return 0;
    char path[64];
    std::snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    char buf[512];
    size_t n = 0;
    if (!core::fs::read_small_file(path, buf, sizeof(buf), &n) || n == 0) return 0;
    buf[n < sizeof(buf) ? n : sizeof(buf) - 1] = '\0';

    const char* p = std::strrchr(buf, ')');
    if (p == nullptr) return 0;
    ++p; // now at the space before state

    // Fields after comm: state(3) ppid(4) ... starttime(22). Skip 19 tokens.
    int to_skip = 19;
    while (to_skip-- > 0) {
        while (*p == ' ') ++p;
        if (*p == '\0') return 0;
        while (*p != ' ' && *p != '\0') ++p;
    }
    while (*p == ' ') ++p;
    if (*p == '\0') return 0;
    return std::strtoull(p, nullptr, 10);
}

// True when this pid still refers to the same process the mitigation was applied
// to. A zero recorded value means the start time could not be read when the
// mitigation was taken; such an entry is not trusted for restore.
bool same_process(int32_t pid, uint64_t recorded_ticks) noexcept {
    if (pid <= 1 || recorded_ticks == 0) return false;
    return read_proc_start_ticks(pid) == recorded_ticks;
}

// REF-REQ-117 (DEF-1): parent pid from field 4 of /proc/<pid>/stat, parsed after
// the ')' that closes comm for the same reason as the start ticks above.
int32_t read_proc_ppid(int32_t pid) noexcept {
    if (pid <= 1) return 0;
    char path[64];
    std::snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    char buf[512];
    size_t n = 0;
    if (!core::fs::read_small_file(path, buf, sizeof(buf), &n) || n == 0) return 0;
    buf[n < sizeof(buf) ? n : sizeof(buf) - 1] = '\0';

    const char* p = std::strrchr(buf, ')');
    if (p == nullptr) return 0;
    ++p; // at the space before state

    // Skip state(3), then ppid(4) is the next token.
    while (*p == ' ') ++p;
    while (*p != ' ' && *p != '\0') ++p; // consume state
    while (*p == ' ') ++p;
    if (*p == '\0') return 0;
    const long ppid = std::strtol(p, nullptr, 10);
    return (ppid > 1 && ppid < INT32_MAX) ? static_cast<int32_t>(ppid) : 0;
}

} // namespace

// REF-REQ-117 (DEF-3): a process belongs to a user desktop application when its
// ancestry reaches a session/desktop ancestor before it reaches init. This is the
// fallback the allowlist cannot provide: an Electron app spawns renderer, GPU and
// utility processes whose thread names ("MainThread", "ThreadPoolForegound", ...)
// are not in any name list, yet they carry the app's input and paint path. The
// daemon cannot learn the focused window (KWin's queryWindowInfo is an
// interactive, user-click API and Wayland exposes no focus protocol), so the app
// TREE is the self-contained signal it can actually observe.
//
// The walk is bounded and reads only /proc. It deliberately stops at any
// background-worker ancestor so an indexer's children are NOT protected by
// virtue of having been launched from a shell.
bool FeatureManager::is_user_app_tree(int32_t pid) noexcept {
    if (pid <= 1) return false;
    constexpr int MAX_DEPTH = 8;
    int32_t cur = pid;
    for (int depth = 0; depth < MAX_DEPTH; ++depth) {
        const int32_t ppid = read_proc_ppid(cur);
        if (ppid <= 1) return false;

        char comm_path[64];
        std::snprintf(comm_path, sizeof(comm_path), "/proc/%d/comm", ppid);
        char comm_buf[64]{};
        size_t n = 0;
        if (!core::fs::read_small_file(comm_path, comm_buf, sizeof(comm_buf) - 1, &n) || n == 0) {
            return false;
        }
        comm_buf[n] = '\0';
        while (n > 0 && (comm_buf[n - 1] == '\n' || comm_buf[n - 1] == '\r')) comm_buf[--n] = '\0';
        const std::string_view comm(comm_buf, n);

        // A background worker's descendants are background work, not an app.
        const auto cls = ProcessClassifierDB::classify(comm);
        if (cls.tier == ProcessSafetyTier::BackgroundWorker ||
            cls.tier == ProcessSafetyTier::RunawayCandidate) {
            return false;
        }
        // Reaching a user-app ancestor means everything under it is that app.
        if (cls.tier == ProcessSafetyTier::UserInteractive) {
            return true;
        }
        // init(1)/systemd is the stopping point; its direct children are not apps.
        if (comm == "systemd" || comm == "init") return false;
        cur = ppid;
    }
    return false;
}

bool FeatureManager::actuate_anti_starvation_cap(int32_t pid, PowerProfileMode mode, const char* comm) noexcept {
    // REF-REQ-104: Performance mode applies NO process throttling at all.
    //
    // This branch used to do the opposite of its name: in Performance mode it
    // confined the process to the C2 cluster - four of sixteen logical CPUs on
    // this machine - and applied nice +5. A desktop application was found pinned
    // to CPUs 8,10,12,14 while the machine was nominally in Performance mode. The
    // feature exists to keep greedy work off the cores reserved for audio and the
    // compositor; in a profile whose entire contract is "do not hold anything
    // back", the correct amount of capping is none.
    if (mode == PowerProfileMode::Performance) {
        return false;
    }

    const cpu_set_t allowed_set = MitigationEngine::get_headroom_allowed_cpuset(mode);
    const bool c2_dispersed = false;

    bool aff = MitigationEngine::apply_core_affinity_cap(pid, &allowed_set);
    int nice_val = 10;
    if (mode == PowerProfileMode::UltraEndurance) {
        nice_val = 15; // Balanced CFS deprioritization in UltraEndurance
    }
    bool batch = MitigationEngine::apply_sched_batch(pid, nice_val);
    if (mode == PowerProfileMode::UltraEndurance) {
        // Enforce hard cgroup CPU quota (400% = 4 cores max quota per 100ms, matching 50% core limit)
        MitigationEngine::apply_cgroup_cpu_quota(pid, 400000, 100000);
    }
    char det[128];
    std::snprintf(det, sizeof(det), "Nice=%d, %s Mask applied, cgroup quota=%s",
                  nice_val, (c2_dispersed ? "C2-Cluster" : "Headroom"), (mode == PowerProfileMode::UltraEndurance ? "400ms/100ms" : "none"));
    // REF-REQ-092: report only what was actually applied. Under the actuation
    // sandbox every primitive is a no-op, and a log line claiming "Headroom Mask
    // applied" would be a false record of a hardware change.
    const bool applied = (aff || batch);
    if (applied) {
        core::EventLogger::log_mitigation(pid, (comm && *comm) ? comm : "runaway-task",
                                          (c2_dispersed ? "C2ClusterDispersion" : "AntiStarvationCap"), det);
    }
    return applied;
}

bool FeatureManager::actuate_anti_starvation_restore(int32_t pid, const cpu_set_t* target_affinity, int orig_policy, int orig_nice, const char* comm) noexcept {
    bool aff = MitigationEngine::restore_core_affinity(pid, target_affinity);
    bool norm = MitigationEngine::restore_sched_normal(pid, orig_policy, orig_nice);
    MitigationEngine::restore_cgroup_cpu_quota(pid);
    const bool restored = (aff || norm);
    if (restored) {
        core::EventLogger::log_rollback(pid, (comm && *comm) ? comm : "runaway-task", "Restored baseline CFS nice/affinity/cgroup quota");
    }
    return restored;
}

void FeatureManager::rollback_all_tracked() noexcept {
    for (auto& tm : m_tracked) {
        if (tm.pid <= 1) continue;

        // REF-REQ-111 (DEF-3): the pid may have been recycled since the
        // mitigation was applied. Writing the saved nice, policy and affinity
        // onto a different process is worse than leaving the original throttled.
        if (!same_process(tm.pid, tm.start_time_ticks)) continue;

        // Thaw first: a frozen task cannot be re-scheduled.
        actuate_cgroup_freeze(tm.pid, false);
        MitigationEngine::restore_sched_normal(tm.pid, tm.original_sched_policy, tm.original_nice);
        MitigationEngine::apply_timer_slack(
            tm.pid, tm.original_timerslack_ns > 0 ? tm.original_timerslack_ns : 50'000ULL);

        // Affinity was previously never restored on a profile change, so a core
        // mask applied in a saving profile survived into Performance - the
        // reported case had a desktop application left at SCHED_IDLE on four of
        // sixteen cores while the machine was nominally in Performance mode.
        MitigationEngine::restore_process_affinity(tm.pid, tm.original_affinity);
    }
    m_tracked.clear();
}

ActiveMitigationStatus FeatureManager::evaluate_and_actuate(
    AnalysisReportData& report,
    bool on_battery,
    double battery_pct
) noexcept {
    // Implements REF-REQ-049 & REF-REQ-054: Active audio stack immunity and healing every cycle
    MitigationEngine::audit_and_heal_audio_stack();

    ActiveMitigationStatus status{};

    // Reset per-cycle actions in metrics while retaining enabled flags
    for (auto& fm : m_metrics) {
        fm.actions_taken = 0;
        fm.resource_reclaimed_bytes = 0;
        fm.estimated_power_saved_watts = 0.0;
        fm.targeted_pid_count = 0;
        fm.detail_summary.clear();
    }

    // Determine Power Profile (REF-REQ-094). This is the production policy path,
    // and it defers entirely to MitigationEngine::resolve_profile() so the daemon
    // cannot end up with two ladders disagreeing about the user's profile - the
    // defect that made the selected profile change on its own.
    PowerProfileMode eff_profile = MitigationEngine::resolve_profile(
        m_profile_override.value_or(PowerProfileMode::Balanced),
        on_battery, battery_pct, m_demotion_latch);

    // A threshold demotion becomes the new baseline, so the next cycle does not
    // silently restore the profile the battery rule just moved away from.
    if (m_profile_override.has_value() && eff_profile != *m_profile_override) {
        m_profile_override = eff_profile;
    }
    status.current_profile = eff_profile;

    // REF-REQ-099: Publish it so the static actuators (which are reached from
    // here, not from MitigationEngine's own instance) can see which profile is
    // in force.
    MitigationEngine::set_effective_profile(eff_profile);

    // REF-REQ-112.4: re-assert the unrestricted CPU ceiling every cycle while an
    // unrestricted profile is in force. apply_power_profile() runs only on a
    // TRANSITION, so anything that caps the CPU behind WattCurb's back - a
    // competing tool, a suspend/resume that resets cpufreq, a leftover from a
    // previous run that outlived a restart - otherwise stays in force
    // indefinitely and the user sees a CPU that never boosts.
    //
    // This lives here, not in MitigationEngine::evaluate_and_actuate(), because
    // that function has no production caller: the daemon and the CLI both enter
    // through THIS path. A first implementation put the re-assertion there and
    // it never executed outside the test suite.
    //
    // The call reads before it writes, so a healthy machine pays N reads and no
    // writes.
    if (eff_profile == PowerProfileMode::Performance ||
        eff_profile == PowerProfileMode::Balanced) {
        if (MitigationEngine::assert_unrestricted_cpu_ceiling()) {
            core::EventLogger::log_alert(
                "REPAIR",
                "CPU frequency ceiling had drifted below the hardware maximum in an "
                "unrestricted profile; restored (REF-REQ-112)");
        }
        // REF-REQ-112.9: the EC platform profile is the other half of "can this
        // CPU boost at all". A competitor (ppd) can reset it between our cycles,
        // and on a host left on "balanced" the all-core clock collapses under
        // load. Re-assert it here: set_platform_profile() is a read when the node
        // already matches and a ppd request only when it has drifted.
        const char* want_pp = (eff_profile == PowerProfileMode::Performance) ? "performance"
                                                                              : "balanced";
        MitigationEngine::set_platform_profile(want_pp);

        // REF-REQ-112.10: load-aware frequency watchdog. Every knob can read
        // correct while the EC pins the CPU near its idle clock - the 2026-09-22
        // all-core collapse to ~400 MHz looked exactly like that. If the machine
        // is loaded and even the highest core clock this cycle is far below the
        // hardware maximum for several consecutive cycles, treat it as a stuck
        // condition: re-assert the ceiling and the EC profile, and say so once.
        static uint32_t s_freq_starved_streak = 0;
        static bool s_freq_starved_logged = false;

        uint64_t hw_max_khz = MitigationEngine::hardware_baseline().hw_max_freq_khz;
        if (hw_max_khz == 0) {
            char fbuf[32];
            size_t fn = 0;
            if (core::fs::read_small_file("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq",
                                          fbuf, sizeof(fbuf) - 1, &fn) && fn > 0) {
                fbuf[fn] = '\0';
                hw_max_khz = static_cast<uint64_t>(std::strtoul(fbuf, nullptr, 10));
            }
        }
        const uint64_t observed_max_khz =
            static_cast<uint64_t>(report.hardware.cpu_freq_max_mhz * 1000.0);

        double load1 = 0.0;
        {
            char lbuf[32];
            size_t ln = 0;
            if (core::fs::read_small_file("/proc/loadavg", lbuf, sizeof(lbuf) - 1, &ln) && ln > 0) {
                lbuf[ln] = '\0';
                load1 = std::strtod(lbuf, nullptr);
            }
        }

        if (MitigationEngine::is_frequency_starved(observed_max_khz, hw_max_khz, load1,
                                                   MitigationEngine::get_total_online_cpus())) {
            if (++s_freq_starved_streak >= MitigationEngine::FREQ_STARVED_TRIP_CYCLES) {
                (void)MitigationEngine::assert_unrestricted_cpu_ceiling();
                (void)MitigationEngine::set_platform_profile(want_pp);

                // REF-REQ-115.2 / REF-REQ-112.11: on the observed defect (2026-09-22,
                // ThinkPad, Ryzen 4750U) BOTH knobs above were already correct -
                // scaling_max_freq was at the driver ceiling and platform_profile was
                // already "performance" - and the CPU still sat at 620-680 MHz under
                // load 11-17. The binding limit was the SMU STAPM, which is set by the
                // EC (here 6 W, its lap-mode thermal table) and is below every knob
                // this daemon owns. Re-asserting the same two values could therefore
                // never repair anything; it only produced a log line that read like a
                // successful repair.
                //
                // The one OS-side lever that reaches the STAPM is ryzenadj, so the
                // repair now re-applies the SMU performance limits for the profiles
                // that are supposed to be unrestricted. Measured on the defect host:
                // STAPM 6 W -> 550-600 MHz at 56 C, STAPM 25 W -> 2.6-2.77 GHz at
                // 56-57 C under the same 8-thread load. Tctl protection is untouched
                // by the raise (it is set to SMU_TCTL_PERF_C, not disabled).
                const bool smu_tool_present = MitigationEngine::ryzenadj_available();
                const bool smu_perf_profile = (eff_profile == PowerProfileMode::Performance ||
                                               eff_profile == PowerProfileMode::Balanced);
                const bool smu_applied =
                    (smu_tool_present && smu_perf_profile)
                        ? MitigationEngine::apply_smu_performance_limits()
                        : false;

                if (!s_freq_starved_logged) {
                    s_freq_starved_logged = true;
                    const char* smu_note =
                        !smu_tool_present
                            ? "ryzenadj NOT installed on a system path - the EC power cap cannot be lifted"
                            : (!smu_perf_profile
                                   ? "saving profile - EC power cap intentionally left in place"
                                   : (smu_applied ? "re-applied SMU performance limits (REF-REQ-115)"
                                                  : "SMU re-apply refused (baseline not captured)"));
                    char detail[320];
                    std::snprintf(detail, sizeof(detail),
                                  "CPU frequency starved under load: max core %llu MHz vs ceiling "
                                  "%llu MHz, load1=%.2f, profile=%s; ceiling/profile were already "
                                  "at target, %s; SMU baseline STAPM %u mW "
                                  "(REF-REQ-112.10, REF-REQ-115.2)",
                                  static_cast<unsigned long long>(observed_max_khz / 1000ull),
                                  static_cast<unsigned long long>(hw_max_khz / 1000ull),
                                  load1,
                                  (eff_profile == PowerProfileMode::Performance) ? "Performance"
                                                                                 : "Balanced",
                                  smu_note,
                                  MitigationEngine::hardware_baseline().smu_stapm_mw);
                    core::EventLogger::log_alert("WARN", detail);
                }
                s_freq_starved_streak = 0; // re-arm after a repair attempt
            }
        } else {
            // REF-REQ-112.12: report the recovery once, with the clock that was
            // actually reached. Without this the log only ever records the
            // starvation; a reader cannot tell whether a repair worked.
            if (s_freq_starved_logged) {
                s_freq_starved_logged = false;
                char detail[224];
                std::snprintf(detail, sizeof(detail),
                              "CPU frequency recovered: max core %llu MHz vs ceiling %llu MHz, "
                              "load1=%.2f (REF-REQ-112.12)",
                              static_cast<unsigned long long>(observed_max_khz / 1000ull),
                              static_cast<unsigned long long>(hw_max_khz / 1000ull),
                              load1);
                core::EventLogger::log_alert("INFO", detail);
            }
            s_freq_starved_streak = 0;
        }
    }

    // REF-REQ-118: the fan curve is a thermal-safety mapping and applies in EVERY
    // power profile, not only the two that raise the SMU limit. It is driven here,
    // every cycle, because apply_power_profile() only runs on a profile
    // TRANSITION and the curve has to track temperature. Two rules ride on top of
    // the common curve:
    //   * >= 70 C is full speed in all profiles (the part must never be cooked to
    //     save fan power);
    //   * in UltraEndurance only, <= 45 C stops the fan (REF-REQ-118.3).
    // apply_fan_for_temp() writes only on a level change, so a steady temperature
    // costs no sysfs write. Restore/exit returns the captured baseline.
    (void)MitigationEngine::apply_fan_for_temp(report.hardware.cpu_temp_c, eff_profile);

    // REF-REQ-102: A profile change releases everything the previous profile
    // applied, before the new one decides anything. Restrictions must not
    // outlive the profile that imposed them.
    if (m_last_profile.has_value() && *m_last_profile != eff_profile) {
        rollback_all_tracked();
    }
    m_last_profile = eff_profile;

    bool is_perf_mode = (eff_profile == PowerProfileMode::Performance);

    if (is_perf_mode) {
        // Performance Mode: Throttling & freezer features suppressed, but AntiStarvationHeadroom preserved! (REF-REQ-057)
        for (size_t i = 0; i < m_tracked.size(); ) {
            if (m_tracked[i].applied_feature == FeatureId::CgroupFreezer) {
                actuate_cgroup_freeze(m_tracked[i].pid, false);
                MitigationEngine::restore_sched_normal(m_tracked[i].pid, m_tracked[i].original_sched_policy, m_tracked[i].original_nice);
                MitigationEngine::apply_timer_slack(m_tracked[i].pid, m_tracked[i].original_timerslack_ns > 0 ? m_tracked[i].original_timerslack_ns : 50'000ULL);
                m_tracked[i] = m_tracked.back();
                m_tracked.pop_back();
            } else if (m_tracked[i].applied_feature == FeatureId::SchedIdleThrottle) {
                MitigationEngine::restore_sched_normal(m_tracked[i].pid, m_tracked[i].original_sched_policy, m_tracked[i].original_nice);
                MitigationEngine::apply_timer_slack(m_tracked[i].pid, m_tracked[i].original_timerslack_ns > 0 ? m_tracked[i].original_timerslack_ns : 50'000ULL);
                m_tracked[i] = m_tracked.back();
                m_tracked.pop_back();
            } else {
                ++i;
            }
        }
    }

    enum class Aggressiveness {
        Conservative,
        Moderate,
        Progressive
    } profile = Aggressiveness::Conservative;

    if (eff_profile == PowerProfileMode::UltraEndurance) {
        profile = Aggressiveness::Progressive;
    } else if (eff_profile == PowerProfileMode::PowerSaver) {
        profile = Aggressiveness::Moderate;
    } else {
        profile = Aggressiveness::Conservative;
    }

    // Evaluate Process-Targeted Features
    for (const auto& proc : report.top_processes) {
        if (proc.pid <= 1) continue;

        auto tier = static_cast<ProcessSafetyTier>(proc.safety_tier);

        // REF-REQ-117 (DEF-2): a Tier 3 (UserInteractive) application is a user
        // input consumer. Putting any of its threads on SCHED_IDLE, raising its
        // nice, or masking its CPUs adds latency directly to typing and scrolling,
        // and a "memory reclaim" on a renderer costs it its page cache. The
        // allowlist in ProcessClassifierDB is matched on comm, which for an
        // Electron app covers only some of its processes, so the ancestry check
        // closes the gap. Performance mode already imposes nothing (REF-REQ-104);
        // this makes Balanced/PowerSaver equally hands-off for user apps.
        if (tier == ProcessSafetyTier::UserInteractive || is_user_app_tree(proc.pid)) {
            continue;
        }

        // Strict Immunity for Tier 0 (CriticalImmune) & Proactive Latency Shield for Tier 1 (DesktopCore)
        if (tier == ProcessSafetyTier::DesktopCore) {
            MitigationEngine::shield_interactive_process(proc.pid, proc.comm.view());
            continue;
        }
        if (tier == ProcessSafetyTier::CriticalImmune) {
            continue;
        }

        if (!is_perf_mode) {
            // Feature 3: Proactive Memory Reclaim on Desktop Shell (Tier 2)
            if (tier == ProcessSafetyTier::DesktopShell && is_feature_enabled(FeatureId::ProactiveMemoryReclaim)) {
                if (profile == Aggressiveness::Progressive && proc.pss_kib > 250 * 1024) {
                    uint64_t reclaim_target = 64ULL * 1024 * 1024; // 64 MB
                    if (actuate_memory_reclaim(proc.pid, reclaim_target)) {
                        auto& m = m_metrics[static_cast<size_t>(FeatureId::ProactiveMemoryReclaim)];
                        ++m.actions_taken;
                        m.resource_reclaimed_bytes += reclaim_target;
                        m.estimated_power_saved_watts += 0.05;
                        if (m.targeted_pid_count < m.targeted_pids.size()) {
                            m.targeted_pids[m.targeted_pid_count++] = proc.pid;
                        }
                        status.reclaimed_bytes += reclaim_target;
                        status.estimated_savings_watts += 0.05;
                    }
                }
                continue;
            }

            // Feature 1: SchedIdleThrottle
            if (is_feature_enabled(FeatureId::SchedIdleThrottle)) {
                bool should_throttle = false;
                if (tier == ProcessSafetyTier::BackgroundWorker) {
                    should_throttle = (profile == Aggressiveness::Progressive) ||
                                      (profile == Aggressiveness::Moderate && (proc.wdi_score > 3.0 || proc.wakeups_per_sec > 50)) ||
                                      (proc.wdi_score > 10.0 || proc.cpu_watts > 1.5);
                } else if (tier == ProcessSafetyTier::RunawayCandidate) {
                    should_throttle = (proc.is_runaway_candidate || proc.wdi_score > 10.0);
                }

                if (should_throttle) {
                    if (actuate_sched_idle(proc.pid)) {
                        auto& m = m_metrics[static_cast<size_t>(FeatureId::SchedIdleThrottle)];
                        ++m.actions_taken;
                        double savings = proc.cpu_watts * 0.4;
                        m.estimated_power_saved_watts += savings;
                        if (m.targeted_pid_count < m.targeted_pids.size()) {
                            m.targeted_pids[m.targeted_pid_count++] = proc.pid;
                        }
                        ++status.throttled_count;
                        status.estimated_savings_watts += savings;
                    }
                }
            }

            // Feature 2: TimerSlackCoalescing
            if (is_feature_enabled(FeatureId::TimerSlackCoalescing)) {
                bool should_relax = false;
                if (tier == ProcessSafetyTier::BackgroundWorker && proc.wakeups_per_sec > 80) {
                    should_relax = true;
                } else if (tier == ProcessSafetyTier::UserInteractive && profile == Aggressiveness::Progressive && proc.wakeups_per_sec > 250) {
                    should_relax = true;
                } else if (tier == ProcessSafetyTier::RunawayCandidate && proc.wakeups_per_sec > 100) {
                    should_relax = true;
                }

                if (should_relax && proc.timerslack_ns < 100'000'000ULL) {
                    if (actuate_timer_slack(proc.pid, 100'000'000ULL)) {
                        auto& m = m_metrics[static_cast<size_t>(FeatureId::TimerSlackCoalescing)];
                        ++m.actions_taken;
                        double savings = proc.wakeup_tax_watts * 0.5;
                        m.estimated_power_saved_watts += savings;
                        if (m.targeted_pid_count < m.targeted_pids.size()) {
                            m.targeted_pids[m.targeted_pid_count++] = proc.pid;
                        }
                        status.estimated_savings_watts += savings;
                    }
                }
            }

            // Feature 3: ProactiveMemoryReclaim for user apps & workers
            if (is_feature_enabled(FeatureId::ProactiveMemoryReclaim)) {
                bool should_reclaim = false;
                if (tier == ProcessSafetyTier::BackgroundWorker && proc.pss_kib > 80 * 1024) {
                    should_reclaim = true;
                } else if (tier == ProcessSafetyTier::UserInteractive && profile != Aggressiveness::Conservative &&
                           proc.pss_kib > 400 * 1024 && proc.cpu_watts < 0.2) {
                    should_reclaim = true;
                }

                if (should_reclaim) {
                    uint64_t reclaim_bytes = std::min(proc.pss_kib * 1024ULL / 2, 128ULL * 1024 * 1024);
                    if (reclaim_bytes > 0 && actuate_memory_reclaim(proc.pid, reclaim_bytes)) {
                        auto& m = m_metrics[static_cast<size_t>(FeatureId::ProactiveMemoryReclaim)];
                        ++m.actions_taken;
                        m.resource_reclaimed_bytes += reclaim_bytes;
                        m.estimated_power_saved_watts += 0.04;
                        if (m.targeted_pid_count < m.targeted_pids.size()) {
                            m.targeted_pids[m.targeted_pid_count++] = proc.pid;
                        }
                        status.reclaimed_bytes += reclaim_bytes;
                        status.estimated_savings_watts += 0.04;
                    }
                }
            }

            // Feature 4: CgroupFreezer (Disabled & Replaced by Non-Halting Graceful Throttle per REF-REQ-044)
            if (is_feature_enabled(FeatureId::CgroupFreezer)) {
                // REF-REQ-044: Processes must NEVER be frozen or killed!
                // Graceful non-halting throttle fallback only
                if (profile == Aggressiveness::Progressive &&
                    (tier == ProcessSafetyTier::BackgroundWorker || tier == ProcessSafetyTier::RunawayCandidate)) {
                    actuate_sched_idle(proc.pid);
                    actuate_timer_slack(proc.pid, 100'000'000ULL);
                }
            }

            // Feature 5: ZenCcxAffinityPinning
            if (is_feature_enabled(FeatureId::ZenCcxAffinityPinning)) {
                if (proc.cross_ccx_migration && proc.cpu_core >= 0) {
                    if (actuate_ccx_affinity(proc.pid, proc.cpu_core)) {
                        auto& m = m_metrics[static_cast<size_t>(FeatureId::ZenCcxAffinityPinning)];
                        ++m.actions_taken;
                        m.estimated_power_saved_watts += 0.15; // Infinity Fabric cache transfer savings
                        if (m.targeted_pid_count < m.targeted_pids.size()) {
                            m.targeted_pids[m.targeted_pid_count++] = proc.pid;
                        }
                        status.estimated_savings_watts += 0.15;
                    }
                }
            }
        }

        // Feature 8: AntiStarvationHeadroom (REF-REQ-054, REF-ARCH-030, REF-REQ-057)
        // Runs across ALL 4 power profiles (including Performance mode) to prevent 100% all-core starvation!
        if (is_feature_enabled(FeatureId::AntiStarvationHeadroom)) {
            bool should_cap = false;
            if (tier != ProcessSafetyTier::CriticalImmune && tier != ProcessSafetyTier::DesktopCore && !MitigationEngine::is_immune_process(proc.pid)) {
                double cpu_w_threshold = 1.2;
                if (eff_profile == PowerProfileMode::UltraEndurance) {
                    cpu_w_threshold = 0.20; // Tightened threshold for 1.4GHz UltraEndurance
                } else if (eff_profile == PowerProfileMode::PowerSaver) {
                    cpu_w_threshold = 0.70; // Scaled to 10W TDP limit
                } else if (eff_profile == PowerProfileMode::Performance) {
                    cpu_w_threshold = 2.0;
                }

                if (proc.cpu_watts > cpu_w_threshold) {
                    should_cap = true;
                } else if (proc.num_threads >= 4 && proc.cpu_watts > 0.25) {
                    // Multi-threaded parallel workload attempting to saturate cores
                    should_cap = true;
                } else if (eff_profile == PowerProfileMode::UltraEndurance && proc.num_threads >= 2 && proc.cpu_watts > 0.12) {
                    // Multi-threaded workload in UltraEndurance capped immediately
                    should_cap = true;
                } else if (proc.wdi_score > (eff_profile == PowerProfileMode::UltraEndurance ? 4.0 : 6.0) || proc.is_runaway_candidate) {
                    should_cap = true;
                } else if (tier == ProcessSafetyTier::BackgroundWorker && proc.cpu_watts > (eff_profile == PowerProfileMode::UltraEndurance ? 0.15 : 0.20)) {
                    should_cap = true;
                } else if (tier == ProcessSafetyTier::RunawayCandidate && proc.cpu_watts > (eff_profile == PowerProfileMode::UltraEndurance ? 0.15 : 0.25)) {
                    should_cap = true;
                }
            }

            if (should_cap) {
                // Check if already tracked
                bool already_tracked = false;
                for (const auto& tm : m_tracked) {
                    if (tm.pid == proc.pid && tm.applied_feature == FeatureId::AntiStarvationHeadroom) {
                        already_tracked = true;
                        break;
                    }
                }

                // REF-REQ-111 (DEF-2): capacity is checked BEFORE acting, not after.
                // Previously the cap was applied and only then recorded
                // 'if (m_tracked.size() < MAX_TRACKED_MITIGATIONS)', so past the
                // 128th concurrent mitigation a process was masked and reniced
                // with no record - leaving no path by which it could ever be
                // restored, in any profile, while status.throttled_count still
                // counted it. An un-undoable mitigation is not worth its saving.
                if (!already_tracked && m_tracked.size() >= MAX_TRACKED_MITIGATIONS) {
                    already_tracked = true; // suppress the actuation entirely
                }

                if (!already_tracked) {
                    int orig_nice = ::getpriority(PRIO_PROCESS, static_cast<id_t>(proc.pid));
                    int orig_sched = ::sched_getscheduler(proc.pid);
                    cpu_set_t orig_aff;
                    CPU_ZERO(&orig_aff);
                    ::sched_getaffinity(proc.pid, sizeof(cpu_set_t), &orig_aff);

                    if (actuate_anti_starvation_cap(proc.pid, eff_profile, proc.comm.c_str())) {
                        auto& m = m_metrics[static_cast<size_t>(FeatureId::AntiStarvationHeadroom)];
                        ++m.actions_taken;
                        double savings = proc.cpu_watts * 0.15; // Energy reduction from mitigating SMT cross-thread thrashing
                        m.estimated_power_saved_watts += savings;
                        if (m.targeted_pid_count < m.targeted_pids.size()) {
                            m.targeted_pids[m.targeted_pid_count++] = proc.pid;
                        }
                        // Guaranteed by the capacity check above; kept as a bound.
                        if (m_tracked.size() < MAX_TRACKED_MITIGATIONS) {
                            TrackedMitigation tm{};
                            tm.pid = proc.pid;
                            std::strncpy(tm.comm, proc.comm.c_str(), sizeof(tm.comm) - 1);
                            tm.applied_feature = FeatureId::AntiStarvationHeadroom;
                            tm.timestamp_sec = static_cast<uint64_t>(::time(nullptr));
                            tm.original_nice = orig_nice;
                            tm.original_sched_policy = (orig_sched >= 0) ? orig_sched : SCHED_OTHER;
                            tm.original_timerslack_ns = proc.timerslack_ns;
                            tm.original_affinity = orig_aff;
                            tm.start_time_ticks = read_proc_start_ticks(proc.pid);
                            m_tracked.push_back(tm);
                        }
                        ++status.throttled_count;
                        status.estimated_savings_watts += savings;
                    }
                }
            }
        }
    }

    // Dynamic De-escalation: uncap any process whose CPU consumption has subsided
    double deescalate_w = 0.40;
    if (eff_profile == PowerProfileMode::UltraEndurance) deescalate_w = 0.10;
    else if (eff_profile == PowerProfileMode::PowerSaver) deescalate_w = 0.25;
    else if (eff_profile == PowerProfileMode::Performance) deescalate_w = 0.80;

    uint64_t now_sec = static_cast<uint64_t>(::time(nullptr));

    for (size_t i = 0; i < m_tracked.size(); ) {
        if (m_tracked[i].applied_feature == FeatureId::AntiStarvationHeadroom) {
            bool still_greedy = false;
            // REF-ARCH-045: 15-second minimum cooldown hold window to prevent zero-hysteresis flapping
            if (now_sec >= m_tracked[i].timestamp_sec && (now_sec - m_tracked[i].timestamp_sec < 15)) {
                still_greedy = true;
            } else {
                for (const auto& proc : report.top_processes) {
                    if (proc.pid == m_tracked[i].pid) {
                        if (proc.cpu_watts > deescalate_w || proc.wdi_score > 3.5 || (proc.num_threads >= 4 && proc.cpu_watts > 0.15)) {
                            still_greedy = true;
                        }
                        break;
                    }
                }
            }
            if (!still_greedy) {
                // REF-REQ-111 (DEF-3): same identity check as the bulk rollback.
                // This path fires on a 15 s grace timer, so it is the one most
                // likely to reach a pid that has since been recycled.
                if (same_process(m_tracked[i].pid, m_tracked[i].start_time_ticks)) {
                    actuate_anti_starvation_restore(m_tracked[i].pid, &m_tracked[i].original_affinity, m_tracked[i].original_sched_policy, m_tracked[i].original_nice, m_tracked[i].comm);
                }
                m_tracked[i] = m_tracked.back();
                m_tracked.pop_back();
                continue;
            }
        }
        ++i;
    }

    // Feature 6: DisplayBacklightFloor Advisory
    if (is_feature_enabled(FeatureId::DisplayBacklightFloor) && on_battery && battery_pct < 30.0) {
        if (report.hardware.display_brightness_percent > 50.0) {
            auto& m = m_metrics[static_cast<size_t>(FeatureId::DisplayBacklightFloor)];
            m.actions_taken = 1;
            m.estimated_power_saved_watts = report.hardware.display_watts * 0.35;
        }
    }

    // Feature 7: PcieAspmEnforcer
    if (is_feature_enabled(FeatureId::PcieAspmEnforcer) && on_battery) {
        auto& m = m_metrics[static_cast<size_t>(FeatureId::PcieAspmEnforcer)];
        if (report.hardware.aspm_policy != "powersave") {
            m.actions_taken = 1;
            m.estimated_power_saved_watts = 0.30; // PCIe L1/L1.1/L1.2 substate link power saving
        }
    }

    // Populate individual feature summaries in ActiveMitigationStatus
    for (size_t i = 0; i < m_metrics.size(); ++i) {
        auto fid = static_cast<FeatureId>(i);
        auto desc = descriptor(fid);
        const auto& m = m_metrics[i];

        if (m.actions_taken > 0 || m.estimated_power_saved_watts > 0.0) {
            char buf[96];
            if (fid == FeatureId::ProactiveMemoryReclaim) {
                std::snprintf(buf, sizeof(buf), "[%s] Reclaimed %luMB (~%.2fW)",
                              desc.name.c_str(),
                              static_cast<unsigned long>(m.resource_reclaimed_bytes / (1024 * 1024)),
                              m.estimated_power_saved_watts);
            } else if (fid == FeatureId::DisplayBacklightFloor) {
                std::snprintf(buf, sizeof(buf), "[%s] Backlight floor advisory active (~%.2fW)",
                              desc.name.c_str(), m.estimated_power_saved_watts);
            } else if (fid == FeatureId::AntiStarvationHeadroom) {
                std::snprintf(buf, sizeof(buf), "[%s] Headroom preserved, capped %zu greedy PID(s) (~%.2fW)",
                              desc.name.c_str(), m.actions_taken, m.estimated_power_saved_watts);
            } else {
                std::snprintf(buf, sizeof(buf), "[%s] %zu action(s) on target PIDs (~%.2fW)",
                              desc.name.c_str(), m.actions_taken, m.estimated_power_saved_watts);
            }
            if (status.feature_summary_count < status.feature_summaries.size()) {
                status.feature_summaries[status.feature_summary_count++] = core::FixedString<96>(buf);
            }
        }
    }

    // Summary string
    char summary_buf[128];
    if (is_perf_mode) {
        if (status.throttled_count > 0) {
            std::snprintf(summary_buf, sizeof(summary_buf),
                          "Performance Mode (Boost 4.1GHz, Anti-Starvation capped %zu greedy PID(s))",
                          status.throttled_count);
        } else {
            std::snprintf(summary_buf, sizeof(summary_buf), "Performance Mode (Boost 4.1GHz, Headroom Clear)");
        }
    } else if (status.throttled_count == 0 && status.frozen_count == 0 && status.reclaimed_bytes == 0) {
        std::snprintf(summary_buf, sizeof(summary_buf), "Optimal baseline (No intrusive throttling needed)");
    } else {
        std::snprintf(summary_buf, sizeof(summary_buf),
                      "%zu throttled, %zu frozen, %luMB reclaimed (~%.2fW saved across %zu features)",
                      status.throttled_count,
                      status.frozen_count,
                      static_cast<unsigned long>(status.reclaimed_bytes / (1024 * 1024)),
                      status.estimated_savings_watts,
                      status.feature_summary_count);
    }
    status.active_summary = summary_buf;

    report.mitigation_status = status;
    return status;
}

} // namespace wattcurb::policy
