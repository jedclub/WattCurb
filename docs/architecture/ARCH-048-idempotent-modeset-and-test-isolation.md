# [REF-ARCH-048] Idempotent Display Modeset & Test Desktop Isolation Architecture

## 1. Architectural Overview

This architecture implements the idempotent display actuation and test session isolation defined in [`REF-REQ-071`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-071-modeset-flapping-elimination-and-test-isolation.md).

```
[ Call to set_display_refresh_rate(hz) ]
                 |
        target = (hz <= 50)
                 |
     (current_drrs == target)?
      /                     \
    YES                      NO
    /                         \
[ Return true (0-Overhead) ]  [ Update state: current_drrs = target ]
(Zero modeset flicker)                  |
                               [ execute_user_desktop_cmd ]
                                        |
                          (WATTCURB_TEST_MOCK_DESKTOP set)?
                            /                           \
                          YES                            NO
                          /                               \
               [ Return true (Mock) ]         [ Execute kscreen-doctor ]
               (Zero user session impact)      (Genuine Intentional Modeset)
```

---

## 2. Implementation in `src/policy/mitigation_engine.cpp`

```cpp
static bool execute_user_desktop_cmd(const char* cmd_body) noexcept {
    if (!cmd_body) return false;
    // REF-REQ-071-3: Strict Test Desktop Isolation
    if (::getenv("WATTCURB_TEST_MOCK_DESKTOP") != nullptr) {
        return true; // Isolate automated test suites from user screen
    }
    ...
}

bool MitigationEngine::set_display_refresh_rate(uint32_t hz) noexcept {
    bool target_drrs = (hz <= 50);
    // REF-REQ-071-1: Idempotency Guard - Prevent redundant DRM modeset blackout
    if (s_hardware_baseline.drrs_applied == target_drrs) {
        return true;
    }
    s_hardware_baseline.drrs_applied = target_drrs;
    if (target_drrs) {
        return execute_user_desktop_cmd("kscreen-doctor output.1.mode.2");
    } else {
        return execute_user_desktop_cmd("kscreen-doctor output.1.mode.1");
    }
}

bool MitigationEngine::set_kwin_effects_suspended(bool suspend) noexcept {
    // REF-REQ-071-2: Idempotency Guard - Prevent redundant KWin shader pipeline reloads
    if (s_hardware_baseline.kwin_blur_unloaded == suspend) {
        return true;
    }
    s_hardware_baseline.kwin_blur_unloaded = suspend;
    if (suspend) {
        return execute_user_desktop_cmd("qdbus6 org.kde.KWin /Effects unloadEffect blur");
    } else {
        return execute_user_desktop_cmd("qdbus6 org.kde.KWin /Effects loadEffect blur");
    }
}
```

---

## 3. Test Suite Integration in `tests/test_units.cpp`

```cpp
int main() {
    // REF-REQ-071: Isolate test suite from physical display modeset
    ::setenv("WATTCURB_TEST_MOCK_DESKTOP", "1", 1);
    ...
}
```
