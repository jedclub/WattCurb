#pragma once

#include "core/types.hpp"
#include <cstdint>
#include <cstddef>
#include <string_view>

namespace wattcurb::policy {

// Implements REF-REQ-137, REF-ARCH-084:
// Safe Progressive Disk Pressure Guard & Storage Full Prevention Engine
//
// Periodically inspects root storage utilization via statvfs.
// When disk usage >= 90.0% (DISK_PRESSURE_THRESHOLD_PCT), progressively vacates
// safe, disposable system logs and caches in rate-limited batches to prevent ENOSPC.
//
// Invariants:
// 1. Zero-Kill / Non-Destructive: Never touches user home directories ($HOME) or live sockets/locks.
// 2. Anti-Churn Cooldown: Mandatory 300-second lockout between cleanup cycles to prevent I/O thrashing.
// 3. Rate-Limited Batch Budget: At most 512 MiB reclaimed per actuation pass.

struct alignas(64) DiskPressureSample {
    uint64_t total_bytes{0};
    uint64_t avail_bytes{0};
    uint64_t used_bytes{0};
    double used_pct{0.0};
    bool is_pressure_satisfied{false};
};

class DiskPressureGuard {
public:
    static constexpr double DISK_PRESSURE_THRESHOLD_PCT = 90.0;
    static constexpr double DISK_RELEASE_THRESHOLD_PCT = 88.0;
    static constexpr uint64_t COOLDOWN_SEC = 300ULL; // 5 minutes
    static constexpr uint64_t MAX_BATCH_RECLAIM_BYTES = 512ULL * 1024ULL * 1024ULL; // 512 MiB budget
    static constexpr uint64_t TMP_FILE_MAX_AGE_SEC = 7ULL * 86400ULL; // 7 days

    DiskPressureGuard() noexcept = default;
    ~DiskPressureGuard() noexcept = default;

    // Evaluates current root disk pressure and actuates tiered cleanup if threshold exceeded
    bool evaluate_and_actuate() noexcept;

    // Sample storage metrics for a specific mount path (default: "/")
    [[nodiscard]] static DiskPressureSample sample_storage(const char* mount_path = "/") noexcept;

    // Direct tiered reclaim entrypoint with configurable budget
    uint64_t reclaim_tiered_storage(uint64_t max_bytes = MAX_BATCH_RECLAIM_BYTES) noexcept;

    [[nodiscard]] uint64_t total_reclaimed_bytes() const noexcept { return m_total_reclaimed_bytes; }
    [[nodiscard]] uint64_t last_reclaim_time() const noexcept { return m_last_reclaim_time; }
    [[nodiscard]] uint32_t reclaim_event_count() const noexcept { return m_reclaim_events; }
    [[nodiscard]] bool is_cooldown_expired(uint64_t now_sec) const noexcept;

    void reset_metrics() noexcept;

private:
    uint64_t sweep_journal_and_dumps(uint64_t budget_bytes) noexcept;
    uint64_t sweep_package_caches(uint64_t budget_bytes) noexcept;
    uint64_t sweep_old_tmp_files(const char* dir_path, uint64_t budget_bytes, uint64_t now_sec) noexcept;

    uint64_t m_last_sample_time{0};
    uint64_t m_last_reclaim_time{0};
    uint64_t m_total_reclaimed_bytes{0};
    uint32_t m_reclaim_events{0};
};

} // namespace wattcurb::policy
