# [REF-ARCH-025] Zero-Allocation SNI Desktop Tray Client Architecture

## 1. Architectural Blueprint
- **Ref-ID**: `REF-ARCH-025`
- **Related Requirements**: [`REF-REQ-035`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-035-thinkpower-faithful-ultra-low-overhead-tray.md)
- **Related Research**: [`REF-RES-017`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-017-thinkpower-tray-ux-and-zero-overhead-client.md)
- **Namespace**: `wattcurb::tray`

```
┌────────────────────────────────────────────────────────────────────────┐
│                          KDE Plasma Panel / SNI Host                   │
│                                                                        │
│   [User moves mouse over tray icon]                                    │
│         │                                                              │
│         ▼ D-Bus org.freedesktop.DBus.Properties.Get("ToolTip")         │
└─────────┼──────────────────────────────────────────────────────────────┘
          │
          ▼ Unix domain socket (sd-bus session bus)
┌────────────────────────────────────────────────────────────────────────┐
│               wattcurb-tray (Zero-Allocation SNI Client)               │
├────────────────────────────────────────────────────────────────────────┤
│  • epoll loop wakes on D-Bus socket descriptor (< 20µs)                │
│  • Performs atomic Seqlock read from /dev/shm/wattcurb_state.shm (30ns)│
│  • Formats 512-byte Tooltip in fixed stack buffer                      │
│  • Replies to D-Bus Get call directly                                  │
│  • Re-enters epoll_wait sleep immediately (CPU drops to 0.0%)          │
└─────────┬──────────────────────────────────────────────────────────────┘
          │
          ▼ [Left/Right Click: Change Profile]
┌────────────────────────────────────────────────────────────────────────┐
│         Unix Domain Socket Control: /run/wattcurb.sock                 │
│         Sends: "SET_PROFILE powersaver\n"                              │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 2. In-Memory Data Flow & Zero-Heap Tooltip Formatter

```cpp
namespace wattcurb::tray {

class TrayController {
public:
    static bool init() noexcept;
    static void run() noexcept;

    // Called on-demand when KDE Plasma queries ToolTip over D-Bus
    static void render_tooltip_payload(
        const ipc::WattCurbSharedState& state,
        char* out_title, size_t title_cap,
        char* out_desc, size_t desc_cap
    ) noexcept {
        double watts = static_cast<double>(state.system_drain_mw) / 1000.0;
        const char* status_str = state.battery_state == 1 ? "Discharging" : 
                                (state.battery_state == 2 ? "AC Passthrough" : "AC / Charging");

        std::snprintf(out_title, title_cap, "WattCurb: %.1f W (%s)", watts, status_str);

        // Dense ThinkPower technical tooltip
        std::snprintf(out_desc, desc_cap,
            "Battery: %u%% (Health: %u%%) | Est: %u min\n"
            "CPU: %.1f W (%u°C, Fan %u RPM) | GPU: %.1f W\n"
            "Top 1: %s (%u mW, PID %d)\n"
            "Top 2: %s (%u mW, PID %d)\n"
            "Profile: %s | Active Gates: %u",
            state.battery_percent, state.battery_health_percent, state.time_to_empty_min,
            static_cast<double>(state.cpu_drain_mw) / 1000.0, state.cpu_temp_c, state.fan_rpm,
            static_cast<double>(state.gpu_drain_mw) / 1000.0,
            state.culprits[0].comm, state.culprits[0].drain_mw, state.culprits[0].pid,
            state.culprits[1].comm, state.culprits[1].drain_mw, state.culprits[1].pid,
            state.power_profile_mode == 2 ? "UltraEndurance" : (state.power_profile_mode == 1 ? "PowerSaver" : "Balanced"),
            state.active_mitigations
        );
    }
};

} // namespace wattcurb::tray
```

---

## 3. Optimization Telemetry & Footprint Targets

| Metric | Legacy ThinkPower (Python/Qt) | WattCurb Tray (`wattcurb-tray`) | Target Gain |
| :--- | :--- | :--- | :--- |
| **Resident Memory (RSS)** | 64 MB ~ 92 MB | **< 1.8 MB** | **> 97% reduction** |
| **Idle CPU Usage** | 0.2% ~ 0.8% (Polling) | **0.00% (Pure Event-driven)** | **100% elimination** |
| **Tooltip Latency** | 25 ms ~ 40 ms | **< 80 µs (0.08 ms)** | **300x faster** |
| **Binary Size** | ~15 MB runtime | **< 200 KB stripped ELF** | **98% reduction** |

---

## 4. Verification & Testing Standards (`REF-TEST-018`)

1. **Zero-Allocation Assertion**:
   - `render_tooltip_payload()` must execute strictly on stack buffers with zero calls to `malloc()` or `operator new`.
2. **Seqlock Integrity**:
   - Verify that concurrent writer updates during tooltip formatting are cleanly handled by the Seqlock retry loop without torn reads or corrupted strings.
3. **Sub-100µs Execution Latency**:
   - Measure 10,000 consecutive tooltip renders; assert that average latency is strictly $\le 50\mu\text{s}$.
