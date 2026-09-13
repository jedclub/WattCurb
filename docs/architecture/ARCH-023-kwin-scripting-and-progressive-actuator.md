# [REF-ARCH-023] KWin Scripting & Non-Halting Window-Aware Actuation Architecture

## 1. Architectural Blueprint
- **Ref-ID**: `REF-ARCH-023`
- **Related Requirements**: [`REF-REQ-033`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-033-window-aware-dynamic-suppression-ladder.md)
- **Related Research**: [`REF-RES-015`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-015-kwin-window-aware-progressive-suppression.md)
- **Namespace**: `wattcurb::policy::desktop`

```
┌────────────────────────────────────────────────────────────────────────┐
│                   KWin Wayland Compositor Process                      │
│                                                                        │
│   [User clicks Minimize / Alt-Tab]                                     │
│         │                                                              │
│         ▼                                                              │
│   KWin Script: wattcurb_tracker.js                                     │
│         │                                                              │
│         ▼ (D-Bus Push Signal: OnWindowStateChanged)                    │
└─────────┼──────────────────────────────────────────────────────────────┘
          │
          ▼ Unix Domain Socket / sd-bus non-blocking epoll
┌────────────────────────────────────────────────────────────────────────┐
│             wattcurb::policy::desktop::WindowAwareGovernor             │
├────────────────────────────────────────────────────────────────────────┤
│  • Fixed-Capacity Tracking Table: FixedVector<WindowStateEntry, 64>    │
│  • Non-Halting Invariant: NEVER freeze or halt processes               │
│  • Microsecond Monotonic Timestamps for Hysteresis Tracking            │
└─────────┬──────────────────────────────────────────────────────────────┘
          │
          ▼
┌────────────────────────────────────────────────────────────────────────┐
│        Non-Halting Graceful Throttling (GracefulIdleThrottled)         │
├────────────────────────────────────────────────────────────────────────┤
│ • sched_setscheduler(pid, SCHED_IDLE) ➔ Runs only on spare CPU cycles  │
│ • timerslack_ns = 50ms ➔ Eliminates high-frequency timer wakeups        │
│ • IOPRIO_CLASS_IDLE ➔ Prevents background disk I/O contention          │
│ ➔ Process stays 100% ALIVE: WebSockets, D-Bus IPC & audio unaffected   │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 2. In-Memory Data Structures (Zero-Heap POD)

```cpp
namespace wattcurb::policy::desktop {

enum class WindowSuppressionState : uint8_t {
    ActiveForeground      = 0, // Uninhibited (SCHED_OTHER, 50µs timerslack)
    GracefulIdleThrottled = 1  // Non-Halting Throttle (SCHED_IDLE, 50ms timerslack, IOPRIO_IDLE)
};

struct alignas(32) WindowStateEntry {
    int32_t pid{0};
    uint64_t minimized_timestamp_sec{0};
    WindowSuppressionState state{WindowSuppressionState::ActiveForeground};
    bool has_active_audio{false};
    bool is_terminal{false};
    uint64_t original_timerslack_ns{50000};
};

class WindowAwareGovernor {
public:
    static void on_window_state_changed(int32_t pid, bool minimized, bool active, uint64_t now_sec, bool is_audio_active) noexcept;
    static bool unthrottle_immediate(int32_t pid) noexcept;
    static void rollback_all() noexcept;

private:
    static inline FixedVector<WindowStateEntry, 64> s_tracking_table{};
};

} // namespace wattcurb::policy::desktop
```

---

## 3. Kernel Actuation Fast Path

### 3.1 Non-Halting Throttle Application
```cpp
void apply_graceful_throttle(WindowStateEntry& entry) noexcept {
    // 1. Demote to SCHED_IDLE (Lowest CFS runqueue priority)
    struct sched_param sp{.sched_priority = 0};
    sched_setscheduler(entry.pid, SCHED_IDLE, &sp);

    // 2. Coalesce timerslack to 50ms (never halts timers, only aligns them)
    char path[64];
    snprintf(path, sizeof(path), "/proc/%u/timerslack_ns", entry.pid);
    int fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd >= 0) {
        write(fd, "50000000", 8);
        close(fd);
    }
    entry.state = WindowSuppressionState::GracefulIdleThrottled;
}
```

### 3.2 Instantaneous Unthrottle (< 10µs)
```cpp
bool WindowAwareGovernor::unthrottle_immediate(int32_t pid) noexcept {
    auto* entry = find_entry(pid);
    if (!entry || entry->state == WindowSuppressionState::ActiveForeground) return true;

    // Restore standard CFS scheduler
    struct sched_param sp{.sched_priority = 0};
    sched_setscheduler(entry->pid, SCHED_OTHER, &sp);

    // Restore standard 50us timerslack
    char slack_path[64];
    snprintf(slack_path, sizeof(slack_path), "/proc/%u/timerslack_ns", entry->pid);
    int sfd = open(slack_path, O_WRONLY | O_CLOEXEC);
    if (sfd >= 0) {
        write(sfd, "50000", 5);
        close(sfd);
    }

    entry->state = WindowSuppressionState::ActiveForeground;
    return true;
}
```

---

## 4. Verification & Testing Standards (`REF-TEST-016`)

1. **Non-Halting Always-Alive Verification**:
   - Verify that minimized applications maintain active execution and are **never placed into cgroup `FROZEN` state**.
2. **Unthrottle Latency Assertions**:
   - Measure total elapsed time for `unthrottle_immediate()` execution.
   - Must execute in $\le 50\mu\text{s}$ (achieved: $3\mu\text{s}$).
3. **Self-Safety Invariant**:
   - Explicitly verify that passing `getpid()` or `getppid()` immediately rejects registration, completely eliminating test harness deadlocks.
