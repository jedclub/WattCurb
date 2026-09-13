#pragma once

#include "policy/mitigation_engine.hpp"
#include "policy/window_aware_governor.hpp"
#include <cstdint>

namespace wattcurb::policy {

// Implements REF-REQ-034, REF-ARCH-024:
// Unified Rapid Rollback Coordinator
// Enforces the Zero-Residual Rapid Restoration Mandate across all 4 domains:
// (1) Process Scheduler & Timers, (2) KDE Compositor/Shaders/DRRS, 
// (3) Silicon CPU EPP & Boost, (4) Bus PCIe ASPM & Display Backlight.
struct alignas(64) UnifiedRollbackState {
    uint32_t rollback_magic{0x57415454}; // "WATT"
    uint64_t last_rollback_timestamp_ns{0};
    uint32_t total_rollbacks_executed{0};
    bool is_clean_baseline{true};
};

class UnifiedRollbackCoordinator {
public:
    // Atomic one-pass sweep across all software and hardware mitigations
    static bool execute_rapid_rollback(
        MitigationEngine& mitigation,
        WindowAwareGovernor& window_gov,
        uint64_t now_ns = 0
    ) noexcept;

    [[nodiscard]] static const UnifiedRollbackState& state() noexcept { return s_state; }

private:
    static inline UnifiedRollbackState s_state{};
};

} // namespace wattcurb::policy
