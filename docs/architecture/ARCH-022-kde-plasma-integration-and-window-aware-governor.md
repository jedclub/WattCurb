# [REF-ARCH-022] KDE Plasma Integration & Window-Aware Desktop Governor Architecture

## 1. Architectural Overview & Design Motivation
- **Ref-ID**: `REF-ARCH-022`
- **Related Requirements**: [`REF-REQ-032`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-032-kde-plasma-desktop-mitigation.md)
- **Related Research**: [`REF-RES-014`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-014-kde-plasma-safe-desktop-optimization.md)
- **Namespace**: `wattcurb::policy::desktop`

Traditional power daemons treat the desktop environment either as a black box or as a target for crude process signals. `wattcurb::policy::desktop::KdeDesktopGovernor` establishes a **non-intrusive, protocol-driven bridge** between WattCurb's hardware telemetry engine and KDE Plasma 6 Wayland.

```
┌────────────────────────────────────────────────────────────────────────┐
│                        WattCurb Mitigation Engine                      │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ Evaluates Battery Profile & State
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│               wattcurb::policy::desktop::KdeDesktopGovernor            │
├────────────────────────────────────────────────────────────────────────┤
│  • Monitors DBus Signals (PowerDevil, UPower, KWin)                    │
│  • Maintains 128-byte In-Memory Rollback State                         │
│  • Dispatches Non-Blocking Commands to KWin & Baloo Interfaces         │
└───────┬───────────────────────────┬────────────────────────────┬───────┘
        │                           │                            │
        ▼ (D-Bus /Effects)          ▼ (cgroup.freeze)            ▼ (D-Bus /indexer)
┌───────────────┐           ┌───────────────────┐        ┌───────────────┐
│  KWin Blur /  │           │ Minimized Client  │        │ Baloo Indexer │
│  Shader Gates │           │ Background Freeze │        │ Pause/Resume  │
└───────────────┘           └───────────────────┘        └───────────────┘
```

---

## 2. Component Subsystems & C++23 Layout

### 2.1 In-Memory Rollback Journal (`KdeRollbackJournal`)
To strictly adhere to WattCurb's zero-heap design principle, the previous state of KDE settings is stored in a fixed-size POD struct:

```cpp
namespace wattcurb::policy::desktop {

struct alignas(64) KdeRollbackJournal {
    uint32_t magic = 0x4B44455F; // "KDE_"
    bool blur_was_loaded = false;
    bool contrast_was_loaded = false;
    bool baloo_was_suspended = false;
    uint32_t original_refresh_rate_mhz = 0; // e.g. 144000
    uint32_t original_animation_factor = 100; // 100%
    uint32_t frozen_client_pids[16] = {0};
    uint8_t frozen_client_count = 0;
};

} // namespace wattcurb::policy::desktop
```

### 2.2 KWin Effect Actuation Pipeline
Actuating KWin effects does not require spawning a shell or invoking Python. WattCurb uses a lightweight POSIX Unix domain socket or direct asynchronous `sd-bus` method call:

```cpp
class KWinEffectActuator {
public:
    static bool unload_effect(const char* effect_name) noexcept;
    static bool load_effect(const char* effect_name) noexcept;
    static bool is_effect_loaded(const char* effect_name) noexcept;
};
```

1. **Safety Boundary**: The method targets exclusively `/Effects` on `org.kde.KWin`.
2. **Failure Isolation**: If KWin crashes (independent of WattCurb) or the D-Bus call times out, the call safely returns `false` without blocking the main telemetry loop.

### 2.3 Window-Aware Suppression Loop
Rather than scanning `/proc` continuously, the governor evaluates window minimization state changes:
1. When a client window transition occurs (e.g. `minimized = true`), the client PID is queued with a timestamp.
2. If the client remains minimized beyond the hysteresis window ($T_{\text{hyst}} = 30\text{s}$) and the current profile is `BatterySaver` or `UltraSaver`:
   - Locate the client's cgroup: `/sys/fs/cgroup/user.slice/user-1000.slice/app.slice/...`
   - Write `"1"` to `cgroup.freeze`.
   - Write `"64M"` to `memory.reclaim`.
3. Upon user focus recovery (`minimized = false` or active window changed to PID):
   - Write `"0"` to `cgroup.freeze` immediately.

---

## 3. Power State Integration Matrix

| WattCurb Power Profile | KWin Blur Shader | Baloo Indexer | DRRS (eDP) | Minimized App Freezing |
| :--- | :--- | :--- | :--- | :--- |
| **AC / Performance** | Enabled (Native) | Enabled | High (144Hz/165Hz)| Disabled |
| **Battery (Balanced)** | Enabled | Suspended | 60Hz | Disabled |
| **Battery (Saver)** | **Unloaded (Off)** | Suspended | 60Hz | Enabled ($>30\text{s}$ idle)|
| **Battery (UltraSaver)**| **Unloaded (Off)** | Suspended | 48Hz / 60Hz | Aggressive ($>15\text{s}$ idle)|

---

## 4. Verification & Testing Standards (`REF-TEST-015`)

1. **Zero-Crash Verification**:
   - Issue 100 sequential `unloadEffect` and `loadEffect` cycles against `kwin_wayland`.
   - Verify that `kwin_wayland` process remains stable, memory does not leak, and no D-Bus timeouts occur.
2. **Rollback Determinism**:
   - Transition from `AC` $\rightarrow$ `UltraSaver` $\rightarrow$ `AC`.
   - Assert that `KdeRollbackJournal` restores all effects, refresh rate, and thawed PIDs with 100% fidelity.
3. **Latency Benchmarking**:
   - Verify that D-Bus dispatch and client cgroup thaw latency is strictly $\le 1.0\text{ms}$.
