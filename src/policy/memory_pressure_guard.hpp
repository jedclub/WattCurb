#pragma once

#include "core/types.hpp"
#include "core/custom_containers.hpp"

#include <cstdint>

namespace wattcurb::policy {

// Implements REF-REQ-112, REF-ARCH-072: Non-Halting Memory Pressure Guard
//
// The kernel's last resort under memory exhaustion is SIGKILL. It fires from
// kswapd with no warning and no appeal, and on 2026-09-22 13:28:02 it took a
// running desktop application on this machine with `Free swap = 200kB` of a
// 24 GB total. WattCurb's Zero-Kill guarantee (REF-REQ-044) covers only what
// WattCurb itself does; it says nothing about the kernel killing on WattCurb's
// watch while the daemon measures power and notices nothing.
//
// This guard closes that gap from the only side userspace can: it makes the
// exhaustion less likely to be reached, by slowing the allocator down before
// the last page is gone. It NEVER terminates, and per the operator's decision
// it never stops a process either - the ladder ends at CPU throttling. That
// bound is honest about its own limit: a single thread allocating faster than
// the guard can throttle can still outrun it. Preventing the kill outright
// also needs backing store, which is why REF-REQ-112 pairs this code with the
// swap topology it documents.
enum class MemoryPressureTier : uint8_t {
    Normal = 0,
    // Stop feeding swap and reclaim FILE-backed pages from background work.
    Advisory = 1,
    // Cap the CPU of the largest non-protected resident processes. A process
    // that is not running cannot allocate, so a CPU quota is an allocation-rate
    // brake that costs nothing when the machine is idle.
    Throttle = 2,
};

// Everything the ladder decides on, in one cache line. Parsed from
// /proc/meminfo and /proc/pressure/memory.
struct alignas(64) MemoryPressureSample {
    uint64_t mem_total_kb{0};
    uint64_t mem_available_kb{0};
    uint64_t swap_total_kb{0};
    uint64_t swap_free_kb{0};
    double psi_some_avg10{0.0};
    double psi_full_avg10{0.0};
};

class MemoryPressureGuard {
public:
    // Escalation thresholds. Swap is the leading indicator on a machine with
    // swap: MemAvailable stays comfortable right up until the swap tier is
    // full, which is exactly how the 13:28 kill arrived without warning.
    static constexpr double ADVISORY_SWAP_FREE_PCT = 30.0;
    static constexpr double THROTTLE_SWAP_FREE_PCT = 12.0;
    static constexpr double ADVISORY_MEM_AVAIL_PCT = 15.0;
    static constexpr double THROTTLE_MEM_AVAIL_PCT = 8.0;
    static constexpr double ADVISORY_PSI_FULL_AVG10 = 10.0;
    static constexpr double THROTTLE_PSI_FULL_AVG10 = 25.0;

    // De-escalation band. Deliberately far above the escalation thresholds:
    // releasing a throttle at the same level that applied it makes the guard
    // oscillate against its own effect.
    static constexpr double RELEASE_SWAP_FREE_PCT = 45.0;
    static constexpr double RELEASE_MEM_AVAIL_PCT = 25.0;
    static constexpr double RELEASE_PSI_FULL_AVG10 = 5.0;

    // 20 ms of every 100 ms. Enough to keep a process responsive and finishing
    // work, little enough to cut its page-fault rate by roughly five.
    static constexpr uint32_t THROTTLE_QUOTA_US = 20000;
    static constexpr uint32_t THROTTLE_PERIOD_US = 100000;

    // Only processes above this hold enough memory to be worth acting on.
    static constexpr uint32_t MIN_TARGET_RSS_KIB = 256u * 1024u; // 256 MiB
    static constexpr size_t MAX_THROTTLED = 16;
    static constexpr size_t MAX_RECLAIMED_PER_CYCLE = 8;

    // PSI trigger: wake when memory stalls exceed 150 ms inside any 2 s window.
    // This is what keeps the guard inside the Zero-Wakeup contract - the fd is
    // silent on a healthy machine and the daemon never polls for pressure.
    //
    // The parameters are not free. Measured against kernel 7.2.5 on this host:
    //   - the window must be a multiple of 2,000,000 us (a 1 s window is
    //     rejected with EINVAL even for root);
    //   - the threshold may not exceed one tenth of the window
    //     ("some 500000 2000000" is rejected, "some 200000 2000000" is not);
    //   - creating a trigger at all requires CAP_SYS_RESOURCE - every
    //     unprivileged attempt returns EINVAL.
    // A first attempt with a 1 s window was silently declined at runtime and
    // the guard fell back to tick sampling, which is exactly the failure this
    // ladder is written to avoid.
    static constexpr const char* PSI_TRIGGER = "some 150000 2000000";
    // Tried in order if the primary is refused, so a kernel with different
    // limits still gets an event-driven guard rather than the tick fallback.
    static constexpr const char* PSI_TRIGGER_FALLBACKS[] = {
        "some 200000 2000000",
        "some 400000 4000000",
        "some 1000000 10000000",
    };

    MemoryPressureGuard() = default;
    ~MemoryPressureGuard();

    MemoryPressureGuard(const MemoryPressureGuard&) = delete;
    MemoryPressureGuard& operator=(const MemoryPressureGuard&) = delete;

    // Opens /proc/meminfo for cached pread() and arms the PSI trigger. Returns
    // false only if /proc/meminfo is unreadable; a missing or unwritable PSI
    // trigger degrades to tick-driven sampling rather than failing.
    bool initialize() noexcept;

    // EPOLLPRI-armed descriptor, or -1 when PSI triggers are unavailable.
    [[nodiscard]] int psi_fd() const noexcept { return m_psi_fd; }

    [[nodiscard]] bool sample(MemoryPressureSample& out) const noexcept;

    // Pure decision function. Split out so the Oracle Gate can prove the whole
    // ladder, including the hysteresis, without a machine that is actually out
    // of memory. A zero swap_total disables the swap criterion entirely instead
    // of reading as "0% free" and pinning a swapless machine at Throttle.
    [[nodiscard]] static MemoryPressureTier classify(const MemoryPressureSample& s,
                                                     MemoryPressureTier previous) noexcept;

    // Samples, classifies, and actuates. Returns the tier now in force.
    // protected_pid is the focused window (REF-REQ-085): the one process the
    // user is watching is never the one slowed down.
    MemoryPressureTier evaluate_and_actuate(const AnalysisReportData& report,
                                            int32_t protected_pid = 0) noexcept;

    // Pure selection predicate, exposed so the Oracle Gate can prove WHICH
    // processes the Throttle tier is willing to slow down without needing a
    // machine under real pressure. The risk this change introduces is throttling
    // something that must never be throttled, and this is what falsifies it.
    [[nodiscard]] static bool is_throttle_candidate(const ProcessAttributedPower& p,
                                                    int32_t protected_pid) noexcept;

    // Pure parsers over kernel text. Public so the Oracle Gate can feed them
    // truncated and malformed buffers directly - REF-RES-029 listed unfuzzed
    // procfs parsers as an open audit gap.
    [[nodiscard]] static bool parse_meminfo(const char* buf, size_t len, MemoryPressureSample& out) noexcept;
    [[nodiscard]] static bool parse_psi(const char* buf, size_t len, MemoryPressureSample& out) noexcept;

    [[nodiscard]] MemoryPressureTier tier() const noexcept { return m_tier; }
    [[nodiscard]] size_t throttled_count() const noexcept { return m_throttled.size(); }
    [[nodiscard]] const MemoryPressureSample& last_sample() const noexcept { return m_last; }

    // Restores every throttled process. Called on de-escalation and on shutdown;
    // a cgroup CPU quota is cgroup state and outlives the daemon, so leaving one
    // behind would cap a user's application for the rest of the session.
    void release_all() noexcept;

    void shutdown() noexcept;

    // True while the guard wants the power-side reclaim ladder to stop pushing
    // anonymous pages into swap. REF-REQ-112.3.
    [[nodiscard]] static bool swap_feeding_suspended() noexcept { return s_suspend_swap_feed; }

private:
    struct ThrottledProcess {
        int32_t pid{0};
        bool quota_applied{false};
        bool sched_idle_applied{false};
    };

    void actuate_advisory(const AnalysisReportData& report) noexcept;
    void actuate_throttle(const AnalysisReportData& report, int32_t protected_pid) noexcept;

    int m_meminfo_fd{-1};
    int m_psi_fd{-1};
    MemoryPressureTier m_tier{MemoryPressureTier::Normal};
    MemoryPressureSample m_last{};
    core::FixedVector<ThrottledProcess, MAX_THROTTLED> m_throttled{};

    static inline bool s_suspend_swap_feed{false};
};

} // namespace wattcurb::policy
