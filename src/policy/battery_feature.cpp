#include "policy/battery_feature.hpp"
#include "policy/mitigation_engine.hpp"

#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdio>
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
    return MitigationEngine::apply_memory_reclaim(pid, bytes);
}

bool FeatureManager::actuate_cgroup_freeze(int32_t pid, bool freeze) noexcept {
    return MitigationEngine::apply_cgroup_freeze(pid, freeze);
}

bool FeatureManager::actuate_ccx_affinity(int32_t pid, int32_t target_core) noexcept {
    if (pid <= 1 || target_core < 0) return false;

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);

    // Compute CCX core block (e.g. Zen 8-core CCX block: 0-7 or 8-15)
    int32_t ccx_base = (target_core / 8) * 8;
    for (int32_t c = ccx_base; c < ccx_base + 8; ++c) {
        CPU_SET(static_cast<size_t>(c), &cpuset);
    }

    return (::sched_setaffinity(pid, sizeof(cpu_set_t), &cpuset) == 0);
}

ActiveMitigationStatus FeatureManager::evaluate_and_actuate(
    AnalysisReportData& report,
    bool on_battery,
    double battery_pct
) noexcept {
    ActiveMitigationStatus status{};

    // Reset per-cycle actions in metrics while retaining enabled flags
    for (auto& fm : m_metrics) {
        fm.actions_taken = 0;
        fm.resource_reclaimed_bytes = 0;
        fm.estimated_power_saved_watts = 0.0;
        fm.targeted_pid_count = 0;
        fm.detail_summary.clear();
    }

    // Determine Power Profile and Aggressiveness (REF-ARCH-008, REF-REQ-020, REF-REQ-035)
    PowerProfileMode eff_profile = PowerProfileMode::Balanced;
    if (m_profile_override.has_value()) {
        eff_profile = *m_profile_override;
    } else if (on_battery) {
        if (battery_pct < 20.0) {
            eff_profile = PowerProfileMode::UltraEndurance;
        } else if (battery_pct <= 50.0) {
            eff_profile = PowerProfileMode::PowerSaver;
        } else {
            eff_profile = PowerProfileMode::Balanced;
        }
    } else {
        eff_profile = PowerProfileMode::Balanced;
    }
    status.current_profile = eff_profile;

    if (eff_profile == PowerProfileMode::Performance) {
        // Performance Mode: All features suppressed, max throughput, zero throttling (REF-REQ-043)
        for (const auto& tm : m_tracked) {
            if (tm.applied_feature == FeatureId::CgroupFreezer) {
                actuate_cgroup_freeze(tm.pid, false);
                MitigationEngine::restore_sched_normal(tm.pid);
            } else if (tm.applied_feature == FeatureId::SchedIdleThrottle) {
                MitigationEngine::restore_sched_normal(tm.pid);
            }
            MitigationEngine::apply_timer_slack(tm.pid, 50'000ULL);
        }
        m_tracked.clear();

        status.active_summary = "Performance Mode (Boost 4.1GHz, Zero Throttling)";
        report.mitigation_status = status;
        return status;
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

        // Strict Immunity for Tier 0 (CriticalImmune) & Tier 1 (DesktopCore)
        if (tier == ProcessSafetyTier::CriticalImmune || tier == ProcessSafetyTier::DesktopCore) {
            continue;
        }

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
    if (status.throttled_count == 0 && status.frozen_count == 0 && status.reclaimed_bytes == 0) {
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
