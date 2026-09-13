# [REF-ARCH-021] Closed-Loop Mitigation Engine & Kernel Actuation Architecture

## 1. System Architecture Diagram

```
                       ┌─────────────────────────────────────────┐
                       │        Hardware & Process Telemetry     │
                       │   (Observation Window Analysis Data)    │
                       └────────────────────┬────────────────────┘
                                            │
                                            ▼
                       ┌─────────────────────────────────────────┐
                       │    PowerProfileStateMachine (3-Tier)    │
                       │    [Balanced | Saver | UltraEndurance]  │
                       └────────────────────┬────────────────────┘
                                            │
                     ┌──────────────────────┴──────────────────────┐
                     ▼                                             ▼
       ┌───────────────────────────┐                 ┌───────────────────────────┐
       │   Process-Level Actuators │                 │  Hardware-Level Actuators │
       └─────────────┬─────────────┘                 └─────────────┬─────────────┘
                     │                                             │
      ┌──────────────┼──────────────┐                ┌─────────────┼─────────────┐
      ▼              ▼              ▼                ▼             ▼             ▼
 SCHED_IDLE    Timerslack    cgroup.freeze       PCIe ASPM     Display Cap    CPU EPP
(CPU/IO Prio)  (100ms align)  (Halt worker)     (powersave)    (Soft 50%)    (balance_pwr)
      │              │              │
      └──────────────┼──────────────┘
                     ▼
       ┌───────────────────────────┐
       │  Rollback & Tracking Ring │
       │  (Thaw / Unthrottle on AC)│
       └───────────────────────────┘
```

---

## 2. Core Class & State Structure Design

### 2.1 PowerProfileMode Definition

```cpp
enum class PowerProfileMode : uint8_t {
    Balanced = 0,        // AC or Battery > 50%: Normal CFS, runaways only
    PowerSaver = 1,      // Battery 20% ~ 50%: SCHED_IDLE on T4, timer 100ms, PCIe ASPM
    UltraEndurance = 2   // Battery < 20%: cgroup freeze on heavy T4, display cap, EPP power
};
```

### 2.2 Dynamic Rollback Architecture (`TrackedMitigation`)

To prevent lingering throttles or frozen states when system conditions improve (e.g. user plugs in AC adapter), the engine maintains a ring buffer of active mitigations:

```cpp
struct TrackedProcessMitigation {
    int32_t pid{0};
    ProcessSafetyTier tier{ProcessSafetyTier::BackgroundWorker};
    MitigationAction active_action{MitigationAction::None};
    uint64_t applied_timestamp_sec{0};
    uint64_t original_timerslack_ns{50000};
};

static constexpr size_t MAX_TRACKED = 128;
core::FixedVector<TrackedProcessMitigation, MAX_TRACKED> m_tracked{};
```

When conditions transition toward higher power availability:
1. `rollback_all()`:
   - For every PID in `m_tracked`:
     - If `active_action == MitigationAction::CgroupFreeze`: execute `apply_cgroup_freeze(pid, false)`.
     - If `active_action == MitigationAction::SchedIdle`: execute `restore_sched_normal(pid)`.
     - If timerslack was modified: restore to `original_timerslack_ns`.
   - Clear `m_tracked`.

---

## 3. Hardware Domain Actuation Primitives

### 3.1 PCIe ASPM Control
```cpp
static bool set_pcie_aspm_policy(const char* policy) noexcept {
    int fd = ::open("/sys/module/pcie_aspm/parameters/policy", O_WRONLY | O_CLOEXEC);
    if (fd < 0) return false;
    ssize_t n = ::write(fd, policy, std::strlen(policy));
    ::close(fd);
    return n > 0;
}
```

### 3.2 Display Backlight Soft-Cap
```cpp
static bool cap_display_backlight(double max_pct) noexcept;
```

---

## 4. IPC & Shared State Synchronization

The active `PowerProfileMode` is published directly into the 128-byte Seqlock POD (`WattCurbSharedState`):
- `power_profile_mode`: `0` (Balanced), `1` (PowerSaver), `2` (UltraEndurance).
- `active_mitigations`: Bitmask of currently active features and throttled PID count.
- Tray clients read this state in `< 15ns` without locking or waking the daemon.
