#pragma once

#include "core/types.hpp"
#include <cstdint>
#include <cstddef>

namespace wattcurb::policy {

// Implements REF-REQ-135, REF-ARCH-082:
// Targeted Memory Pressure Smart GC & Application Cgroup Reclaim with 20-min Cooldown
//
// Automatically frees accumulated V8 heaps, detached DOM trees, and unpurged browser caches
// from heavy desktop/Electron processes when the system encounters memory pressure.
//
// Invariants:
// 1. Zero-Wakeup: Evaluated strictly under memory pressure (PSI stall or low free memory).
// 2. Anti-Churn 20-min Cooldown: Once executed, locked for 1,200 seconds (20 minutes).
// 3. Non-Destructive Two-Stage:
//    - Stage 1: D-Bus broadcast LowMemoryWarning(LEVEL_MODERATE = 100) via LowMemoryNotifier.
//    - Stage 2: Bounded cgroup memory.reclaim (256 MiB per candidate, max 2 candidates).
// 4. Immunity Gate: Critical desktop components (Compositor, Audio, Shell) are strictly immune.

class LowMemoryNotifier;
struct MemoryPressureSample;
enum class MemoryPressureTier : uint8_t;

struct alignas(64) TargetedReclaimTelemetry {
    uint64_t last_reclaim_sec{0};
    uint64_t total_reclaim_bytes{0};
    uint32_t total_reclaim_count{0};
    int32_t last_reclaimed_pids[2]{0, 0};
};

class TargetedAppReclaimEngine {
public:
    // Core parameters (REF-REQ-135.2, REF-REQ-135.3)
    static constexpr uint64_t COOLDOWN_SEC = 1200ULL;                                // 20 minutes
    static constexpr uint64_t RECLAIM_BYTES_PER_PASS = 256ULL * 1024ULL * 1024ULL;   // 256 MiB
    static constexpr uint32_t MIN_CANDIDATE_PSS_KIB = 256u * 1024u;                 // 256 MiB
    static constexpr size_t MAX_CANDIDATES = 2;

    TargetedAppReclaimEngine() noexcept = default;
    ~TargetedAppReclaimEngine() noexcept = default;

    TargetedAppReclaimEngine(const TargetedAppReclaimEngine&) = delete;
    TargetedAppReclaimEngine& operator=(const TargetedAppReclaimEngine&) = delete;

    // Pure decision predicates (REF-TEST-089)
    [[nodiscard]] static bool is_eligible_candidate(const ProcessAttributedPower& p) noexcept;
    [[nodiscard]] static bool is_pressure_satisfied(const MemoryPressureSample& sample,
                                                    MemoryPressureTier tier) noexcept;
    [[nodiscard]] static bool is_cooldown_expired(uint64_t now_sec,
                                                  uint64_t last_reclaim_sec,
                                                  uint64_t cooldown_sec = COOLDOWN_SEC) noexcept;

    // Master execution entry point called by MemoryPressureGuard
    size_t evaluate_and_actuate(const AnalysisReportData& report,
                                const MemoryPressureSample& sample,
                                MemoryPressureTier tier,
                                LowMemoryNotifier& notifier,
                                uint64_t now_sec) noexcept;

    // Accessors for telemetry & tests
    [[nodiscard]] static constexpr uint64_t cooldown_sec() noexcept { return COOLDOWN_SEC; }
    [[nodiscard]] uint64_t last_reclaim_sec() const noexcept { return m_telemetry.last_reclaim_sec; }
    [[nodiscard]] uint64_t total_reclaim_bytes() const noexcept { return m_telemetry.total_reclaim_bytes; }
    [[nodiscard]] uint32_t total_reclaim_count() const noexcept { return m_telemetry.total_reclaim_count; }
    [[nodiscard]] const TargetedReclaimTelemetry& telemetry() const noexcept { return m_telemetry; }

    void reset_cooldown_for_test(uint64_t timestamp_sec = 0) noexcept {
        m_telemetry.last_reclaim_sec = timestamp_sec;
    }

private:
    TargetedReclaimTelemetry m_telemetry{};
};

} // namespace wattcurb::policy
