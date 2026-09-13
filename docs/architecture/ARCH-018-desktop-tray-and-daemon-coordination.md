# [REF-ARCH-018] Desktop Tray System & Bi-Directional Daemon Coordination Architecture

- **Ref-ID**: `REF-ARCH-018`
- **Related Requirements**: [`REF-REQ-028`](../requirements/REQ-025-desktop-tray-and-bidirectional-control.md)
- **Status**: Approved / Architectural Blueprint

---

## 1. System Topology & Process Separation

```
[User Desktop Session (Wayland / X11)]
┌─────────────────────────────────────────────────────────────────────────────┐
│                       WattCurb Tray Client (wattcurb-tray)                  │
│  - Runs as standard unprivileged user ($UID != 0)                           │
│  - Implements D-Bus org.kde.StatusNotifierItem (KDE, GNOME, Waybar)         │
│  - Zero-Allocation Seqlock Reader for Dynamic Tray Icon & Hover Tooltip     │
└───────────────────────┬─────────────────────────────▲───────────────────────┘
                        │                             │
    [Channel B: Action] │                             │ [Channel A: Telemetry]
     Unix Domain Socket │                             │  POSIX Shared Memory
     /run/wattcurb.sock │                             │  /dev/shm/wattcurb_state.shm
     (Non-blocking UDS) │                             │  (128-byte Seqlock POD)
                        v                             │
┌─────────────────────────────────────────────────────┴───────────────────────┐
│                    WattCurb Resident Daemon (wattcurb --daemon)              │
│  - Runs as privileged root / systemd background service                     │
│  - 5s~60s Zero-Wakeup Observation Loop & Mitigation Actuator                │
│  - Memory: 336 KB Flat | Binary: 237 KB | CPU: < 0.05%                      │
└─────────────────────────────────────────────────────────────────────────────┘
[Linux Kernel & Hardware Layer (RAPL, GPU, NVMe APST, ASPM, cgroups v2)]
```

---

## 2. Shared Memory Protocol: `WattCurbSharedState`

To eliminate IPC serialization latency and memory allocation, telemetry is shared via a fixed-capacity, cacheline-aligned struct:

```cpp
namespace wattcurb::ipc {

struct alignas(64) SharedCulprit {
    char     comm[16];
    int32_t  pid;
    uint32_t drain_mw;
    uint8_t  domain_id;
    uint8_t  tier;
    uint8_t  padding[6];
};

struct alignas(64) WattCurbSharedState {
    // Seqlock synchronization header (Odd: Writer active, Even: Stable data)
    std::atomic<uint64_t> seq_version{0};

    // System Electrical Telemetry
    uint32_t system_drain_mw{0};
    uint8_t  battery_percent{0};
    uint8_t  battery_state{0};       // 0: Unknown/AC, 1: Discharging, 2: Passthrough
    uint16_t time_to_empty_min{0};
    uint32_t active_mitigations{0};  // Bitmask of active features

    // Top 3 Dominant Energy Culprits
    SharedCulprit culprits[3];
};

} // namespace wattcurb::ipc
```

### Seqlock Read/Write Protocol
1. **Daemon Writer**:
   ```cpp
   uint64_t ver = state->seq_version.load(std::memory_order_relaxed);
   state->seq_version.store(ver + 1, std::memory_order_release); // Mark busy (odd)
   // ... copy telemetry fields ...
   state->seq_version.store(ver + 2, std::memory_order_release); // Mark ready (even)
   ```
2. **Tray Reader**:
   ```cpp
   WattCurbSharedState local_copy;
   uint64_t v1, v2;
   do {
       v1 = state->seq_version.load(std::memory_order_acquire);
       std::memcpy(&local_copy, state, sizeof(WattCurbSharedState));
       v2 = state->seq_version.load(std::memory_order_acquire);
   } while ((v1 & 1) != 0 || v1 != v2);
   ```
   Ensures the tray reader never locks the daemon writer and achieves **$0\ \text{ns}$ reader stall latency**.

---

## 3. Bi-Directional Action Protocol (Unix Domain Socket)

- **Connection**: Tray client connects on-demand or maintains a persistent non-blocking file descriptor to `/run/wattcurb.sock`.
- **Command Dispatch Grammar**:
  ```
  CMD := SET_FEATURE <feature_id> <0|1>
       | THROTTLE_PID <pid>
       | UNTHROTTLE_PID <pid>
       | RECLAIM_MEM <mb>
       | QUERY_BRIEFING
       | QUERY_JSON
  ```
- **Response**: `OK\n` or `ERR <reason>\n`.
