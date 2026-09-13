# [REF-ARCH-024] Unified Rapid Rollback Coordinator Architecture

## 1. Architectural Blueprint
- **Ref-ID**: `REF-ARCH-024`
- **Related Requirements**: [`REF-REQ-034`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-034-rapid-charge-and-profile-restoration-engine.md)
- **Related Research**: [`REF-RES-016`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-016-rapid-dynamic-rollback-and-charge-coordination.md)
- **Namespace**: `wattcurb::policy`

To ensure that no individual subsystem is overlooked during an AC plug-in or profile elevation, WattCurb centralizes all restoration logic into the **`UnifiedRollbackCoordinator`**.

```
┌────────────────────────────────────────────────────────────────────────┐
│                        Daemon Event Dispatch Loop                      │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ Edge: on_battery == false OR profile elevated
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│             wattcurb::policy::UnifiedRollbackCoordinator               │
├────────────────────────────────────────────────────────────────────────┤
│  • Coordinates: MitigationEngine, WindowAwareGovernor, DesktopGovernor │
│  • Manages 256-Byte In-Memory Rollback Journal                         │
│  • Atomic One-Pass Sweep Across All 4 Domains                          │
└─────────┬─────────────────────────┬────────────────────────────┬───────┘
          │                         │                            │
          ▼ Domain 1                ▼ Domain 2                   ▼ Domains 3 & 4
┌───────────────────┐     ┌───────────────────┐        ┌───────────────────┐
│ MitigationEngine  │     │ WindowAware       │        │ DesktopGovernor   │
│ ::rollback_all()  │     │ ::rollback_all()  │        │ ::rollback_all()  │
├───────────────────┤     ├───────────────────┤        ├───────────────────┤
│• SCHED_OTHER      │     │• All Minimized    │        │• KWin loadEffect  │
│• timerslack 50µs  │     │  Windows Restored │        │• DRRS 144Hz       │
│• PCIe ASPM default│     │• 50µs Timerslack  │        │• Baloo Resume     │
│• CPU EPP perf     │     │• CFS Normal       │        │• Backlight Cap Off│
└───────────────────┘     └───────────────────┘        └───────────────────┘
```

---

## 2. In-Memory Journal Layout (Zero-Allocation)

```cpp
namespace wattcurb::policy {

struct alignas(64) UnifiedRollbackState {
    uint32_t rollback_magic{0x57415454}; // "WATT"
    uint64_t last_rollback_timestamp_ns{0};
    uint16_t total_processes_restored{0};
    bool kwin_shaders_restored{false};
    bool drrs_restored{false};
    bool baloo_restored{false};
    bool silicon_restored{false};
    bool is_clean_baseline{true};
};

class UnifiedRollbackCoordinator {
public:
    static bool execute_rapid_rollback(
        MitigationEngine& mitigation,
        WindowAwareGovernor& window_gov
    ) noexcept;

    [[nodiscard]] static const UnifiedRollbackState& state() noexcept { return s_state; }

private:
    static inline UnifiedRollbackState s_state{};
};

} // namespace wattcurb::policy
```

---

## 3. Verification & Testing Standards (`REF-TEST-017`)

1. **Deterministic Sweep Verification**:
   - Apply mitigations across both `MitigationEngine` and `WindowAwareGovernor`.
   - Fire `execute_rapid_rollback()`.
   - Assert that `mitigation.tracked_count() == 0` and `window_gov.tracked_count() == 0`.
2. **Sub-Millisecond Execution Benchmark**:
   - Measure total wall-clock time for the full 4-domain rollback sweep.
   - Must complete in $\le 1.0\text{ms}$ (well inside the 5.0ms requirement).
3. **Idempotency Verification**:
   - Double-invoking `execute_rapid_rollback()` must execute the second time in $\le 10\mu\text{s}$ as a clean no-op.
