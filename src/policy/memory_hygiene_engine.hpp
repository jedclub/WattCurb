#pragma once

#include "core/types.hpp"
#include "core/custom_containers.hpp"

#include <cstdint>
#include <string_view>
#include <sys/types.h>
#include <sys/stat.h>

namespace wattcurb::policy {

// Implements REF-REQ-134, REF-ARCH-081: Safe Memory Hygiene & Progressive Swap Recovery Engine
//
// Solves three critical real-world resource leaks:
// 1. Tmpfs Inflation: Dead orphaned directories in /tmp (e.g. claude-*, CMake*, PPM dumps)
//    permanently consuming RAM and swap slots.
// 2. Persistent Swap Retention (Lazy Swap-in): Stale swap pages trapped in ZRAM or disk swap
//    long after memory pressure has subsided, with zero risk of OOM via strict margin gates.
// 3. Runaway Maintenance Batch Workers: CPU/memory-heavy background tasks (e.g. git pack-objects)
//    throttled to SCHED_IDLE and Idle I/O without process termination (Zero-Kill).

enum class HygieneStrategy : uint8_t {
    HoldAndProtect = 0,    // Retain current state; do not touch swap or memory
    TmpfsOrphanEvict = 1,  // Safely evict dead tmpfs directories passing 5-layer gate
    BatchSoftClamp = 2,    // Demote runaway maintenance batch process to SCHED_IDLE
    SafeIdleDeswap = 3     // Phased evacuation of secondary disk swap into RAM/ZRAM
};

struct OrphanDirCandidate {
    char path[256]{0};
    uint64_t bytes{0};
    int32_t owner_pid{-1};
    uint64_t mtime_sec{0};
};

struct alignas(64) MemoryHygieneSample {
    uint64_t tmpfs_total_kb{0};
    uint64_t tmpfs_used_kb{0};
    uint64_t shm_used_kb{0};
    uint64_t zram_used_kb{0};
    uint64_t disk_swap_used_kb{0};
    uint64_t mem_available_kb{0};
    uint32_t stale_orphan_dirs_count{0};
    uint64_t stale_orphan_bytes{0};
    int32_t runaway_batch_pid{0};
    bool on_ac_power{true};
    bool user_is_idle{false};
};

class MemoryHygieneEngine {
public:
    // Detection & Safety Thresholds (REF-REQ-134)
    static constexpr double TMPFS_TRIGGER_USED_PCT = 70.0;
    static constexpr uint64_t TMPFS_TRIGGER_USED_BYTES = 2ULL * 1024 * 1024 * 1024; // 2 GiB
    static constexpr uint64_t ORPHAN_MIN_AGE_SEC = 3600ULL;                         // 1 hour
    static constexpr uint64_t MIN_SWAP_RECLAIM_BYTES = 2ULL * 1024 * 1024 * 1024;   // 2 GiB
    static constexpr uint64_t SWAP_HEADROOM_SAFETY_MARGIN_BYTES = 3ULL * 1024 * 1024 * 1024; // 3 GiB
    static constexpr double SWAP_HEADROOM_SAFETY_FACTOR = 2.0;

    // Cooldown & Quench Timers (REF-REQ-134.2)
    static constexpr uint64_t TMPFS_SCAN_COOLDOWN_BASE_SEC = 900ULL;   // 15 minutes
    static constexpr uint64_t SWAP_EVAL_COOLDOWN_BASE_SEC = 1800ULL;   // 30 minutes
    static constexpr uint64_t POST_RECLAIM_QUENCH_SEC = 3600ULL;       // 1 hour
    static constexpr uint64_t MAX_COOLDOWN_BACKOFF_SEC = 7200ULL;      // 2 hours

    // Traversal Bounds (REF-REQ-134.3)
    static constexpr size_t MAX_SCAN_DEPTH = 2;
    static constexpr size_t MAX_INSPECTED_ENTRIES = 64;
    static constexpr size_t MAX_CANDIDATES = 16;

    MemoryHygieneEngine() = default;
    ~MemoryHygieneEngine() = default;

    bool initialize() noexcept;

    // Level 1 Fast Filter (O(1), < 2 µs, zero heap allocation)
    [[nodiscard]] static bool check_tmpfs_fast_filter(
        const char* mount_point,
        uint64_t& out_total_kb,
        uint64_t& out_used_kb
    ) noexcept;

    // Level 2 Bounded Deep Scan (called only when fast filter triggers & cooldown expired)
    bool scan_tmpfs_orphans(
        core::FixedVector<OrphanDirCandidate, MAX_CANDIDATES>& out,
        uint64_t now_sec
    ) noexcept;

    // Pure Decision & Gate Functions (REF-TEST-088)
    [[nodiscard]] static HygieneStrategy select_strategy(const MemoryHygieneSample& sample) noexcept;
    [[nodiscard]] static bool is_system_whitelisted(std::string_view path) noexcept;
    [[nodiscard]] static bool is_file_type_immune(mode_t mode, std::string_view filename) noexcept;
    [[nodiscard]] static bool is_process_dead(int32_t pid) noexcept;
    [[nodiscard]] static bool assert_swap_margin(uint64_t mem_avail_kb, uint64_t swap_used_kb) noexcept;
    [[nodiscard]] static uint64_t compute_backoff(uint64_t current_cooldown, bool actionable) noexcept;

    // Tri-Stage Master Pipeline (Evaluate and Actuate)
    HygieneStrategy evaluate_and_actuate(
        const AnalysisReportData& report,
        bool on_ac_power,
        bool user_is_idle,
        uint64_t now_sec
    ) noexcept;

    // Stage 3 Actuators
    size_t evict_tmpfs_orphans(
        const core::FixedVector<OrphanDirCandidate, MAX_CANDIDATES>& candidates
    ) noexcept;

    bool actuate_safe_deswap(uint64_t mem_avail_kb, uint64_t swap_used_kb) noexcept;
    bool actuate_batch_soft_clamp(int32_t pid) noexcept;

    // Accessors for Telemetry & Tests
    [[nodiscard]] const MemoryHygieneSample& last_sample() const noexcept { return m_last_sample; }
    [[nodiscard]] HygieneStrategy last_strategy() const noexcept { return m_last_strategy; }
    [[nodiscard]] uint64_t tmpfs_cooldown() const noexcept { return m_tmpfs_cooldown; }
    [[nodiscard]] uint64_t swap_cooldown() const noexcept { return m_swap_cooldown; }
    [[nodiscard]] uint64_t total_evicted_bytes() const noexcept { return m_total_evicted_bytes; }
    [[nodiscard]] uint32_t total_evicted_count() const noexcept { return m_total_evicted_count; }

private:
    MemoryHygieneSample m_last_sample{};
    HygieneStrategy m_last_strategy{HygieneStrategy::HoldAndProtect};

    uint64_t m_last_tmpfs_scan_sec{0};
    uint64_t m_last_swap_eval_sec{0};
    uint64_t m_last_reclaim_sec{0};

    uint64_t m_tmpfs_cooldown{TMPFS_SCAN_COOLDOWN_BASE_SEC};
    uint64_t m_swap_cooldown{SWAP_EVAL_COOLDOWN_BASE_SEC};

    uint64_t m_total_evicted_bytes{0};
    uint32_t m_total_evicted_count{0};
};

} // namespace wattcurb::policy
