#include "policy/memory_pressure_guard.hpp"

#include "core/event_logger.hpp"
#include "policy/mitigation_engine.hpp"
#include "policy/process_classifier.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace wattcurb::policy {

namespace {

// Locates "key" at the start of a line and returns the first integer after it.
// /proc/meminfo lines are "MemTotal:       15710204 kB".
[[nodiscard]] bool find_kb_field(const char* buf, size_t len, const char* key, uint64_t& out) noexcept {
    const size_t klen = std::strlen(key);
    size_t i = 0;
    while (i < len) {
        // i is at the start of a line here.
        if (i + klen <= len && std::memcmp(buf + i, key, klen) == 0) {
            size_t j = i + klen;
            while (j < len && (buf[j] == ' ' || buf[j] == '\t')) ++j;
            uint64_t v = 0;
            bool any = false;
            while (j < len && buf[j] >= '0' && buf[j] <= '9') {
                v = v * 10 + static_cast<uint64_t>(buf[j] - '0');
                ++j;
                any = true;
            }
            if (any) {
                out = v;
                return true;
            }
            return false;
        }
        while (i < len && buf[i] != '\n') ++i;
        ++i;
    }
    return false;
}

// Extracts the avg10 value of the named PSI row. /proc/pressure/memory is
// "some avg10=0.00 avg60=0.02 avg300=3.44 total=1113389582\nfull avg10=...".
[[nodiscard]] bool find_psi_avg10(const char* buf, size_t len, const char* row, double& out) noexcept {
    const size_t rlen = std::strlen(row);
    size_t i = 0;
    while (i < len) {
        if (i + rlen <= len && std::memcmp(buf + i, row, rlen) == 0) {
            const char* p = buf + i;
            const char* end = buf + len;
            const char* tag = nullptr;
            for (const char* q = p; q + 6 < end && *q != '\n'; ++q) {
                if (std::memcmp(q, "avg10=", 6) == 0) {
                    tag = q + 6;
                    break;
                }
            }
            if (tag == nullptr) return false;
            out = std::strtod(tag, nullptr);
            return true;
        }
        while (i < len && buf[i] != '\n') ++i;
        ++i;
    }
    return false;
}

[[nodiscard]] inline double pct(uint64_t part, uint64_t whole) noexcept {
    if (whole == 0) return 100.0;
    return (static_cast<double>(part) * 100.0) / static_cast<double>(whole);
}

} // namespace

MemoryPressureGuard::~MemoryPressureGuard() {
    shutdown();
}

bool MemoryPressureGuard::initialize() noexcept {
    if (m_meminfo_fd < 0) {
        m_meminfo_fd = ::open("/proc/meminfo", O_RDONLY | O_CLOEXEC);
    }
    if (m_meminfo_fd < 0) return false;

    // REF-REQ-113: adopt or reclaim dynamic swapfiles from a previous run
    // before any decision is taken on current capacity.
    m_expander.initialize();

    // The PSI trigger is what makes this guard event-driven instead of polled.
    // It is optional: a kernel without CONFIG_PSI, or a container without write
    // access, leaves m_psi_fd at -1 and the guard still runs on the daemon's
    // existing observation tick - later, but not never.
    if (m_psi_fd < 0) {
        // A refused trigger leaves the descriptor unusable, so each candidate
        // gets its own open.
        auto try_trigger = [](const char* spec) noexcept -> int {
            int fd = ::open("/proc/pressure/memory", O_RDWR | O_NONBLOCK | O_CLOEXEC);
            if (fd < 0) return -1;
            if (::write(fd, spec, std::strlen(spec)) < 0) {
                ::close(fd);
                return -1;
            }
            return fd;
        };

        m_psi_fd = try_trigger(PSI_TRIGGER);
        for (const char* spec : PSI_TRIGGER_FALLBACKS) {
            if (m_psi_fd >= 0) break;
            m_psi_fd = try_trigger(spec);
        }
    }
    return true;
}

void MemoryPressureGuard::shutdown() noexcept {
    release_all();
    if (m_psi_fd >= 0) {
        ::close(m_psi_fd);
        m_psi_fd = -1;
    }
    if (m_meminfo_fd >= 0) {
        ::close(m_meminfo_fd);
        m_meminfo_fd = -1;
    }
}

bool MemoryPressureGuard::parse_meminfo(const char* buf, size_t len, MemoryPressureSample& out) noexcept {
    if (buf == nullptr || len == 0) return false;
    uint64_t v = 0;
    if (!find_kb_field(buf, len, "MemTotal:", v)) return false;
    out.mem_total_kb = v;
    out.mem_available_kb = find_kb_field(buf, len, "MemAvailable:", v) ? v : 0;
    out.swap_total_kb = find_kb_field(buf, len, "SwapTotal:", v) ? v : 0;
    out.swap_free_kb = find_kb_field(buf, len, "SwapFree:", v) ? v : 0;
    return true;
}

bool MemoryPressureGuard::parse_psi(const char* buf, size_t len, MemoryPressureSample& out) noexcept {
    if (buf == nullptr || len == 0) return false;
    bool ok = find_psi_avg10(buf, len, "some ", out.psi_some_avg10);
    ok = find_psi_avg10(buf, len, "full ", out.psi_full_avg10) || ok;
    return ok;
}

bool MemoryPressureGuard::sample(MemoryPressureSample& out) const noexcept {
    if (m_meminfo_fd < 0) return false;

    char buf[2048];
    const ssize_t n = ::pread(m_meminfo_fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return false;
    buf[n] = '\0';
    if (!parse_meminfo(buf, static_cast<size_t>(n), out)) return false;

    // Read PSI by path rather than through m_psi_fd: that descriptor is the
    // armed trigger, and reading it is not how the averages are retrieved.
    char pbuf[256];
    int pfd = ::open("/proc/pressure/memory", O_RDONLY | O_CLOEXEC);
    if (pfd >= 0) {
        const ssize_t pn = ::read(pfd, pbuf, sizeof(pbuf) - 1);
        ::close(pfd);
        if (pn > 0) {
            pbuf[pn] = '\0';
            (void)parse_psi(pbuf, static_cast<size_t>(pn), out);
        }
    }
    return true;
}

MemoryPressureTier MemoryPressureGuard::classify(const MemoryPressureSample& s,
                                                 MemoryPressureTier previous) noexcept {
    const bool has_swap = (s.swap_total_kb > 0);
    const double swap_free_pct = has_swap ? pct(s.swap_free_kb, s.swap_total_kb) : 100.0;
    const double mem_avail_pct = pct(s.mem_available_kb, s.mem_total_kb);

    MemoryPressureTier raw = MemoryPressureTier::Normal;
    if ((has_swap && swap_free_pct < THROTTLE_SWAP_FREE_PCT) ||
        mem_avail_pct < THROTTLE_MEM_AVAIL_PCT ||
        s.psi_full_avg10 > THROTTLE_PSI_FULL_AVG10) {
        raw = MemoryPressureTier::Throttle;
    } else if ((has_swap && swap_free_pct < ADVISORY_SWAP_FREE_PCT) ||
               mem_avail_pct < ADVISORY_MEM_AVAIL_PCT ||
               s.psi_full_avg10 > ADVISORY_PSI_FULL_AVG10) {
        raw = MemoryPressureTier::Advisory;
    }

    // Escalation is immediate - the whole point is to act before the kernel does.
    if (static_cast<uint8_t>(raw) >= static_cast<uint8_t>(previous)) return raw;

    // De-escalation walks back one tier at a time, and only from inside the
    // release band. Anything looser lets the guard release a throttle the
    // moment the throttle starts working, then immediately re-apply it.
    const bool released =
        (!has_swap || swap_free_pct > RELEASE_SWAP_FREE_PCT) &&
        mem_avail_pct > RELEASE_MEM_AVAIL_PCT &&
        s.psi_full_avg10 < RELEASE_PSI_FULL_AVG10;
    if (!released) return previous;

    return (previous == MemoryPressureTier::Throttle) ? MemoryPressureTier::Advisory
                                                      : MemoryPressureTier::Normal;
}

void MemoryPressureGuard::actuate_advisory(const AnalysisReportData& report) noexcept {
    // File-backed reclaim only. Pushing anonymous pages out here would consume
    // the swap headroom this tier exists to defend.
    size_t acted = 0;
    for (size_t i = 0; i < report.top_processes.size() && acted < MAX_RECLAIMED_PER_CYCLE; ++i) {
        const auto& p = report.top_processes[i];
        if (p.pid <= 1) continue;
        if (p.pss_kib < MIN_TARGET_RSS_KIB) continue;
        const auto tier = static_cast<ProcessSafetyTier>(p.safety_tier);
        if (tier != ProcessSafetyTier::BackgroundWorker &&
            tier != ProcessSafetyTier::RunawayCandidate) {
            continue;
        }
        // A quarter of what it holds, file pages only.
        const uint64_t bytes = (static_cast<uint64_t>(p.pss_kib) * 1024ull) / 4ull;
        if (MitigationEngine::apply_memory_reclaim(p.pid, bytes, /*file_only=*/true)) {
            ++acted;
        }
    }
}

bool MemoryPressureGuard::is_throttle_candidate(const ProcessAttributedPower& p,
                                                int32_t protected_pid) noexcept {
    if (p.pid <= 1) return false;
    if (protected_pid != 0 && p.pid == protected_pid) return false;
    if (p.pss_kib < MIN_TARGET_RSS_KIB) return false;

    // Tiers 0..2 run the session itself. Slowing the compositor or the sound
    // server under memory pressure turns a memory problem into an unusable
    // desktop, and neither is ever the process that exhausted RAM.
    const auto tier = static_cast<ProcessSafetyTier>(p.safety_tier);
    if (tier == ProcessSafetyTier::CriticalImmune ||
        tier == ProcessSafetyTier::DesktopCore ||
        tier == ProcessSafetyTier::DesktopShell) {
        return false;
    }
    return true;
}

void MemoryPressureGuard::actuate_throttle(const AnalysisReportData& report,
                                           int32_t protected_pid) noexcept {
    for (size_t i = 0; i < report.top_processes.size(); ++i) {
        if (m_throttled.size() >= MAX_THROTTLED) break;
        const auto& p = report.top_processes[i];
        if (!is_throttle_candidate(p, protected_pid)) continue;

        bool already = false;
        for (size_t j = 0; j < m_throttled.size(); ++j) {
            if (m_throttled[j].pid == p.pid) {
                already = true;
                break;
            }
        }
        if (already) continue;

        ThrottledProcess t{};
        t.pid = p.pid;
        t.quota_applied = MitigationEngine::apply_cgroup_cpu_quota(p.pid, THROTTLE_QUOTA_US, THROTTLE_PERIOD_US);
        t.sched_idle_applied = MitigationEngine::apply_sched_idle(p.pid);
        if (!t.quota_applied && !t.sched_idle_applied) continue;

        m_throttled.push_back(t);

        char detail[192];
        std::snprintf(detail, sizeof(detail),
                      "PSS=%llu MiB, cpu.max=%u/%u us%s (REF-REQ-112)",
                      static_cast<unsigned long long>(p.pss_kib / 1024ull),
                      THROTTLE_QUOTA_US, THROTTLE_PERIOD_US,
                      t.sched_idle_applied ? " + SCHED_IDLE" : "");
        core::EventLogger::log_mitigation(p.pid, p.comm.c_str(), "MemoryPressureThrottle", detail);
    }
}

MemoryPressureTier MemoryPressureGuard::evaluate_and_actuate(const AnalysisReportData& report,
                                                             int32_t protected_pid,
                                                             PowerProfileMode profile) noexcept {
    MemoryPressureSample s{};
    if (!sample(s)) return m_tier;
    m_last = s;

    const MemoryPressureTier previous = m_tier;
    const MemoryPressureTier next = classify(s, previous);
    m_tier = next;
    s_suspend_swap_feed = (next != MemoryPressureTier::Normal);

    if (next != previous) {
        char detail[224];
        std::snprintf(detail, sizeof(detail),
                      "tier %u -> %u | MemAvailable=%llu MiB/%llu MiB, SwapFree=%llu MiB/%llu MiB, "
                      "PSI full avg10=%.2f (REF-REQ-112)",
                      static_cast<unsigned>(previous), static_cast<unsigned>(next),
                      static_cast<unsigned long long>(s.mem_available_kb / 1024ull),
                      static_cast<unsigned long long>(s.mem_total_kb / 1024ull),
                      static_cast<unsigned long long>(s.swap_free_kb / 1024ull),
                      static_cast<unsigned long long>(s.swap_total_kb / 1024ull),
                      s.psi_full_avg10);
        core::EventLogger::log_alert("MEMORY", detail);
    }

    // REF-REQ-113: a creation child started on an earlier tick may have finished.
    m_expander.poll_pending();

    if (profile == PowerProfileMode::Performance) {
        // Performance does not brake the workload. It buys room instead: the
        // backing store grows while there is disk to grow it into, and the
        // pages that get written out are the cost of staying alive, paid in
        // I/O rather than in CPU the user asked for.
        const bool growing = m_expander.ensure_headroom(s.swap_total_kb, s.swap_free_kb);

        if (next == MemoryPressureTier::Normal) {
            if (previous != MemoryPressureTier::Normal) release_all();
            m_expander.maybe_release(s.swap_total_kb, s.swap_free_kb);
            return next;
        }

        // File-backed reclaim only - it frees page cache, it does not slow the
        // foreground, and it does not consume the swap tier being defended.
        actuate_advisory(report);

        const bool exhausted = m_expander.budget_exhausted();
        if (next == MemoryPressureTier::Throttle &&
            throttle_permitted(profile, growing, exhausted)) {
            // No capacity left to add. The remaining choice is the brake or a
            // kernel SIGKILL, and REF-REQ-112 exists to avoid the latter.
            if (!m_logged_budget_exhausted) {
                m_logged_budget_exhausted = true;
                core::EventLogger::log_alert(
                    "SWAP",
                    "Performance mode: swap cannot be grown further (file ceiling or disk "
                    "floor reached); falling back to the CPU throttle to avoid a kernel "
                    "OOM kill (REF-REQ-113)");
            }
            actuate_throttle(report, protected_pid);
        } else {
            // While capacity can still be added, hold no CPU cap at all.
            m_logged_budget_exhausted = false;
            release_all();
        }
        return next;
    }

    switch (next) {
    case MemoryPressureTier::Normal:
        if (previous != MemoryPressureTier::Normal) release_all();
        m_expander.maybe_release(s.swap_total_kb, s.swap_free_kb);
        break;
    case MemoryPressureTier::Advisory:
        // Stepping down from Throttle releases the CPU caps but keeps reclaiming.
        if (previous == MemoryPressureTier::Throttle) release_all();
        actuate_advisory(report);
        break;
    case MemoryPressureTier::Throttle:
        actuate_advisory(report);
        actuate_throttle(report, protected_pid);
        break;
    }
    return next;
}

void MemoryPressureGuard::release_all() noexcept {
    for (size_t i = 0; i < m_throttled.size(); ++i) {
        const auto& t = m_throttled[i];
        if (t.quota_applied) {
            MitigationEngine::restore_cgroup_cpu_quota(t.pid);
        }
        if (t.sched_idle_applied) {
            MitigationEngine::restore_sched_normal(t.pid, 0, 0);
        }
        core::EventLogger::log_rollback(t.pid, "", "Memory pressure throttle released (REF-REQ-112)");
    }
    m_throttled.clear();
}

} // namespace wattcurb::policy
