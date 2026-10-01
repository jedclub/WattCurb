#include "policy/targeted_app_reclaim.hpp"
#include "policy/memory_pressure_guard.hpp"
#include "policy/low_memory_notifier.hpp"
#include "policy/mitigation_engine.hpp"
#include "core/event_logger.hpp"

#include <cstdio>

namespace wattcurb::policy {

bool TargetedAppReclaimEngine::is_eligible_candidate(const ProcessAttributedPower& p) noexcept {
    if (p.pid <= 1) return false;
    if (p.pss_kib < MIN_CANDIDATE_PSS_KIB) return false;

    const auto tier = static_cast<ProcessSafetyTier>(p.safety_tier);
    if (tier == ProcessSafetyTier::CriticalImmune ||
        tier == ProcessSafetyTier::DesktopCore ||
        tier == ProcessSafetyTier::DesktopShell) {
        return false;
    }
    return true;
}

bool TargetedAppReclaimEngine::is_pressure_satisfied(const MemoryPressureSample& sample,
                                                    MemoryPressureTier tier) noexcept {
    if (tier != MemoryPressureTier::Normal) return true;
    if (sample.psi_full_avg10 > 5.0) return true;

    if (sample.mem_total_kb > 0) {
        const double avail_pct = (static_cast<double>(sample.mem_available_kb) * 100.0) /
                                 static_cast<double>(sample.mem_total_kb);
        if (avail_pct < 20.0) return true;
    }

    if (sample.swap_total_kb > 0) {
        const double swap_free_pct = (static_cast<double>(sample.swap_free_kb) * 100.0) /
                                     static_cast<double>(sample.swap_total_kb);
        if (swap_free_pct < 35.0) return true;
    }

    return false;
}

bool TargetedAppReclaimEngine::is_cooldown_expired(uint64_t now_sec,
                                                  uint64_t last_reclaim_sec,
                                                  uint64_t cooldown_sec) noexcept {
    if (last_reclaim_sec == 0) return true;
    if (now_sec < last_reclaim_sec) return true; // Clock skew protection
    return (now_sec - last_reclaim_sec) >= cooldown_sec;
}

size_t TargetedAppReclaimEngine::evaluate_and_actuate(const AnalysisReportData& report,
                                                      const MemoryPressureSample& sample,
                                                      MemoryPressureTier tier,
                                                      LowMemoryNotifier& notifier,
                                                      uint64_t now_sec) noexcept {
    if (!is_pressure_satisfied(sample, tier)) {
        return 0;
    }

    if (!is_cooldown_expired(now_sec, m_telemetry.last_reclaim_sec)) {
        return 0;
    }

    // Stage 1 (REF-REQ-135.3): Cooperative D-Bus LowMemoryWarning broadcast (LEVEL_MODERATE = 100)
    notifier.notify(LowMemoryNotifier::LEVEL_MODERATE, now_sec);

    // Stage 2 (REF-REQ-135.3): Bounded cgroup memory.reclaim for up to MAX_CANDIDATES
    size_t acted = 0;
    m_telemetry.last_reclaimed_pids[0] = 0;
    m_telemetry.last_reclaimed_pids[1] = 0;

    for (size_t i = 0; i < report.top_processes.size() && acted < MAX_CANDIDATES; ++i) {
        const auto& p = report.top_processes[i];
        if (!is_eligible_candidate(p)) continue;

        if (MitigationEngine::apply_memory_reclaim(p.pid, RECLAIM_BYTES_PER_PASS, /*file_only=*/false)) {
            m_telemetry.last_reclaimed_pids[acted] = p.pid;
            m_telemetry.total_reclaim_bytes += RECLAIM_BYTES_PER_PASS;
            ++acted;

            char detail[192];
            std::snprintf(detail, sizeof(detail),
                          "Targeted Smart GC & cgroup reclaim 256 MiB (PSS=%llu MiB, 20m cooldown, REF-REQ-135)",
                          static_cast<unsigned long long>(p.pss_kib / 1024ull));
            core::EventLogger::log_mitigation(p.pid, p.comm.c_str(), "TargetedAppReclaim", detail);
        }
    }

    m_telemetry.last_reclaim_sec = now_sec;
    ++m_telemetry.total_reclaim_count;

    char summary[192];
    std::snprintf(summary, sizeof(summary),
                  "Targeted app reclaim pass executed: %zu process(es) reclaimed, 20-min cooldown locked (REF-REQ-135)",
                  acted);
    core::EventLogger::log_alert("MEMORY_HYGIENE", summary);

    return acted;
}

} // namespace wattcurb::policy
