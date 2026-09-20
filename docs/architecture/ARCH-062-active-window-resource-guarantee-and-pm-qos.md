# REF-ARCH-062: Active Window Resource Guarantee & PM QoS Architecture

- **Status**: Approved
- **Ref ID**: `REF-ARCH-062`
- **Related Requirements**: [`REF-REQ-085`](../requirements/REQ-085-kde-active-window-resource-guarantee-and-c0-pinning.md), [`REF-REQ-084`](../requirements/REQ-084-adaptive-c1-c2-cluster-dispersion-and-terminal-shield.md)
- **Related Research**: [`REF-RES-021`](../research/RES-021-kde-active-window-c0-qos-and-resource-guarantee.md)
- **Created**: 2026-09-21
- **Category**: Linux PM QoS, CPU Scheduling Architecture, Active Window Management

---

## 1. Architectural Diagram

```
+---------------------------------------------------------------------------------+
|                        KDE Plasma 6 KWin / Desktop Session                      |
|                                                                                 |
|   [Active Window Event]                                                         |
|         |                                                                       |
|         v                                                                       |
|   IPC Datagram: "ACTIVE_WINDOW <pid> <comm>"                                    |
+---------------------------------------------------------------------------------+
                                      |
                                      v (/run/wattcurb.sock)
+---------------------------------------------------------------------------------+
|                         WattCurb Daemon (DaemonRunner)                          |
|                                                                                 |
|   1. Parse datagram -> WindowAwareGovernor::engage_active_window(pid)           |
|                                                                                 |
|   2. PmQosController:                                                           |
|      - Open /dev/cpu_dma_latency                                                |
|      - Write int32_t(0) -> Restricts cpuidle to C0/C1 (< 2us exit latency)      |
|                                                                                 |
|   3. Resource Guarantee Snapshot:                                               |
|      - Save current nice, cpuset affinity, timer slack                          |
|      - Setpriority(PRIO_PROCESS, pid, -10)                                      |
|      - Sched_setaffinity(pid, C1_Cluster_Mask)                                  |
|      - Set timerslack to 10us                                                   |
+---------------------------------------------------------------------------------+
                                      |
                           [Focus Loss / Window Switch]
                                      v
+---------------------------------------------------------------------------------+
|   WindowAwareGovernor::release_active_window()                                  |
|      - Restore previous nice, cpuset, timerslack                                |
|      - Close /dev/cpu_dma_latency FD -> Kernel automatically restores C2/C3/C6   |
+---------------------------------------------------------------------------------+
```

---

## 2. Component Specifications

### 2.1 `PmQosController`
Encapsulates safe interaction with `/dev/cpu_dma_latency`:
```cpp
class PmQosController {
public:
    PmQosController() = default;
    ~PmQosController() { release_latency_pin(); }

    bool pin_c0_latency(int32_t max_latency_us = 0);
    void release_latency_pin();
    [[nodiscard]] bool is_pinned() const noexcept { return fd_ >= 0; }

private:
    int fd_{-1};
};
```

### 2.2 `ActiveWindowResourceSnapshot`
Preserves baseline process state for clean non-intrusive rollback:
```cpp
struct ActiveWindowResourceSnapshot {
    int32_t pid{0};
    int original_nice{0};
    cpu_set_t original_affinity{};
    uint64_t original_timerslack_ns{50000};
    bool is_guarantee_active{false};
};
```

### 2.3 `WindowAwareGovernor` Integration
Manages the active window lifecycle alongside background window suppression:
- `engage_active_window(int32_t pid, const std::string& comm = "")`:
  - If another window is currently active, cleanly release it first.
  - Snapshot original nice and affinity.
  - Apply CFS `nice -10`.
  - Apply C1 affinity mask.
  - Pin C0 latency via `PmQosController`.
- `release_active_window()`:
  - Restore snapshot properties.
  - Close PM QoS FD.
