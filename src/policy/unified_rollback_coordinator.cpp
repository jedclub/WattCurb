#include "policy/unified_rollback_coordinator.hpp"

namespace wattcurb::policy {

// Implements REF-REQ-034 & REF-ARCH-024:
// Unified Rapid Rollback Sweep Execution
bool UnifiedRollbackCoordinator::execute_rapid_rollback(
    MitigationEngine& mitigation,
    WindowAwareGovernor& window_gov,
    uint64_t now_ns
) noexcept {
    // 1. Process Domain (Window Governor): Unthrottle all minimized background applications
    window_gov.rollback_all();

    // 2. Process & Hardware Domain (Mitigation Engine):
    //    - Restore SCHED_OTHER, timerslack 50µs for all background workers
    //    - Restore PCIe ASPM policy to default
    //    - Restore display backlight to saved pre-mitigation level
    //    - Restore CPU EPP to balance_performance / performance
    mitigation.rollback_all();

    // 3. Update Unified Rollback State Journal (Lock-free POD)
    s_state.last_rollback_timestamp_ns = now_ns;
    s_state.total_rollbacks_executed++;
    s_state.is_clean_baseline = true;

    return true;
}

} // namespace wattcurb::policy
