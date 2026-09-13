# [REF-ARCH-023] KWin Scripting & Progressive Window-Aware Actuation Architecture

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
│  • Microsecond Monotonic Timestamps for Hysteresis Tracking            │
│  • PipeWire Audio Stream Bitmask Check                                 │
└─────────┬──────────────────────────────────────────────────────────────┘
          │
     ┌────┴──────────────────────────────┐
     ▼ (t = 0s)                          ▼ (t = 20s, no audio)
┌───────────────────────────┐       ┌───────────────────────────┐
│ Stage 1: Soft Throttling  │       │ Stage 2: Hard Freeze      │
├───────────────────────────┤       ├───────────────────────────┤
│ • sched_setscheduler      │       │ • /cgroup.freeze = 1      │
│   (SCHED_IDLE)            │       │ • /memory.reclaim = 64M   │
│ • timerslack_ns = 100ms   │       │                           │
└───────────────────────────┘       └───────────────────────────┘
```

---

## 2. In-Memory Data Structures (Zero-Heap POD)

```cpp
namespace wattcurb::policy::desktop {

enum class WindowSuppressionState : uint8_t {
    ActiveForeground = 0, // Uninhibited (SCHED_OTHER, normal timerslack)
    Stage1Throttled  = 1, // SCHED_IDLE, 100ms timerslack, uclamp capped
    Stage2Frozen     = 2  // cgroup.freeze = 1, memory reclaimed
};

struct alignas(32) WindowStateEntry {
    uint32_t pid{0};
    uint64_t minimized_timestamp_ns{0};
    WindowSuppressionState state{WindowSuppressionState::ActiveForeground};
    bool has_active_audio{false};
    bool is_terminal{false};
    uint8_t cgroup_path_len{0};
    char cgroup_path[128]{0};
};

class WindowAwareGovernor {
public:
    static void on_window_state_changed(uint32_t pid, bool minimized, bool active) noexcept;
    static void evaluate_hysteresis(uint64_t current_time_ns) noexcept;
    static void thaw_immediate(uint32_t pid) noexcept;

private:
    static inline FixedVector<WindowStateEntry, 64> s_tracking_table{};
};

} // namespace wattcurb::policy::desktop
```

---

## 3. Kernel Actuation Fast Path

### 3.1 Immediate Stage 1 (Soft Throttling)
```cpp
void apply_stage1(WindowStateEntry& entry) noexcept {
    // 1. Demote to SCHED_IDLE
    struct sched_param sp{.sched_priority = 0};
    sched_setscheduler(entry.pid, SCHED_IDLE, &sp);

    // 2. Coalesce timerslack to 100ms
    char path[64];
    snprintf(path, sizeof(path), "/proc/%u/timerslack_ns", entry.pid);
    int fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd >= 0) {
        write(fd, "100000000", 9);
        close(fd);
    }
    entry.state = WindowSuppressionState::Stage1Throttled;
}
```

### 3.2 Instantaneous Thaw (< 1ms)
```cpp
void WindowAwareGovernor::thaw_immediate(uint32_t pid) noexcept {
    auto* entry = find_entry(pid);
    if (!entry || entry->state == WindowSuppressionState::ActiveForeground) return;

    if (entry->state == WindowSuppressionState::Stage2Frozen) {
        char freeze_path[160];
        snprintf(freeze_path, sizeof(freeze_path), "%s/cgroup.freeze", entry->cgroup_path);
        int fd = open(freeze_path, O_WRONLY | O_CLOEXEC);
        if (fd >= 0) {
            write(fd, "0", 1);
            close(fd);
        }
    }

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
}
```

---

## 4. Verification & Testing Standards (`REF-TEST-016`)

1. **Thaw Latency Assertions**:
   - In automated tests, measure total elapsed time for `thaw_immediate()` execution.
   - Must execute in $\le 500\mu\text{s}$ (well below the 1.0ms threshold).
2. **Audio Immunity Verification**:
   - Simulate a minimized process with `has_active_audio = true`.
   - Run the hysteresis loop for 60 seconds; assert that the process remains at `Stage1Throttled` and is **never** escalated to `Stage2Frozen`.
3. **State Machine Idempotency**:
   - Redundant calls to `on_window_state_changed` must result in no duplicate syscalls.
