# [REF-ARCH-075] Window Minimized Progressive C-State Governor Architecture

- **Status**: Approved · **Date**: 2026-09-26
- **Ref ID**: `REF-ARCH-075`
- **Related Requirements**: [`REF-REQ-128`](../requirements/REQ-128-window-minimized-progressive-cstate-governor.md), [`REF-REQ-085`](../requirements/REQ-085-kde-active-window-resource-guarantee-and-c0-pinning.md), [`REF-ARCH-062`](ARCH-062-active-window-resource-guarantee-and-pm-qos.md)
- **Category**: CPU C-States, Linux CFS Scheduler, Timerslack Coalescing, Window State Management

---

## 1. Architectural State Machine Diagram

```
                        [Window Minimized Event: IPC / KWin]
                                         │
                                         ▼
            ┌────────────────────────────────────────────────────────┐
            │        Check Active Power Profile & Audio State        │
            └────────────────────────────────────────────────────────┘
                      │                                  │
      (PowerSaver / UltraEndurance)           (Performance or Balanced)
                      │                                  │
                      │                                  ▼
                      │                 ┌─────────────────────────────────┐
                      │                 │ Stage 1: Soft C1 Coalescing     │
                      │                 │ • CFS nice = min(cur_nice + 10) │
                      │                 │ • Timerslack = 100ms            │
                      │                 │ • Status: C1_SoftCoalesced      │
                      │                 └─────────────────────────────────┘
                      │                                  │
                      │                 [evaluate_hysteresis() Periodic Tick]
                      │                 • Performance: now - ts >= 600s (10 min)
                      │                 • Balanced   : now - ts >= 60s  (1 min)
                      │                 • Audio Active: Inhibit escalation
                      │                                  │
                      ▼                                  ▼
            ┌─────────────────────────────────────────────────────────────┐
            │ Stage 2: Deep C2 Package Sleep                              │
            │ • Scheduler = SCHED_IDLE                                    │
            │ • I/O Priority = IOPRIO_CLASS_IDLE                          │
            │ • Timerslack = 1,000ms (1.0 sec)                            │
            │ • Status: C2_DeepIdleThrottled                              │
            │ • Non-Halting Invariant: Never frozen, runnable on idle     │
            └─────────────────────────────────────────────────────────────┘
                                         │
                        [Window Un-minimized / Restored / Activated]
                                         │
                                         ▼
            ┌─────────────────────────────────────────────────────────────┐
            │ Instant Zero-Latency Rollback (unthrottle_immediate)         │
            │ • Restore baseline nice & original scheduler policy         │
            │ • Restore standard 50µs timerslack                          │
            │ • Status: ActiveForeground                                  │
            └─────────────────────────────────────────────────────────────┘
```

---

## 2. Component Structures & Logic

### 2.1 State Enum & Entry
```cpp
enum class WindowSuppressionState : uint8_t {
    ActiveForeground     = 0, // Uninhibited (SCHED_OTHER, 50µs timerslack)
    C1_SoftCoalesced    = 1, // Stage 1 (CFS nice +10, 100ms timerslack)
    C2_DeepIdleThrottled = 2  // Stage 2 (SCHED_IDLE, 1,000ms timerslack, IOPRIO_IDLE)
};

struct alignas(32) WindowStateEntry {
    int32_t pid{0};
    uint64_t minimized_timestamp_sec{0};
    WindowSuppressionState state{WindowSuppressionState::ActiveForeground};
    bool has_active_audio{false};
    bool is_terminal{false};
    int original_nice{0};
    int original_sched_policy{0};
    uint64_t original_timerslack_ns{50000};
};
```

### 2.2 Profile Threshold Calculation
```cpp
static constexpr uint64_t get_c2_escalation_threshold_sec(PowerProfileMode mode) noexcept {
    switch (mode) {
        case PowerProfileMode::Performance:
            return 600; // 10 minutes
        case PowerProfileMode::Balanced:
            return 60;  // 1 minute
        case PowerProfileMode::PowerSaver:
        case PowerProfileMode::UltraEndurance:
        default:
            return 0;   // Immediate
    }
}
```
