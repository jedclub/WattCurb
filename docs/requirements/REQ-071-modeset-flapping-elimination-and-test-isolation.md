# [REF-REQ-071] Desktop Modeset Flapping Elimination & Test Isolation Invariant Specification

## 1. Executive Summary & Problem Analysis

Users experienced transient display flicker and black screens during daemon operations and automated test runs. Empirical investigation revealed two critical causes:

1. **Non-Idempotent Display & Compositor Actuation**:
   - In [`mitigation_engine.cpp`](file:///home/jedclub/Develop/WattCurb/src/policy/mitigation_engine.cpp), `set_display_refresh_rate` and `set_kwin_effects_suspended` unconditionally executed `kscreen-doctor` and `qdbus6` desktop commands without checking whether the display or compositor was already in the target state.
   - Calling `set_display_refresh_rate(60)` when the display was already running at 60Hz triggered a redundant hardware DRM modeset, forcing the eDP display panel to go black for ~0.5 seconds.
   - Calling `set_kwin_effects_suspended(false)` when blur was already loaded forced KWin to reconstruct shader pipelines, producing visible window redraw flickers.

2. **Test Suite Desktop Escape**:
   - Automated unit tests (`wattcurb_tests`) executing `test_ultra_endurance_extensions` invoked real desktop actuation commands (`kscreen-doctor`, `qdbus6`), disrupting the active user session during agent test passes.

This specification formalizes **Idempotent Display Modeset Protection** and **Strict Test Desktop Isolation**.

---

## 2. Functional Requirements

### 2.1 Idempotent Refresh Rate State Guard (`REF-REQ-071-1`)
- Before invoking `kscreen-doctor` or issuing any display modeset command, `MitigationEngine::set_display_refresh_rate(uint32_t hz)` must evaluate whether the current state matches the target state:
  $$\text{target\_drrs} = (hz \le 50)$$
- If `s_hardware_baseline.drrs_applied == target_drrs`, the method must return `true` immediately without executing external desktop commands.

### 2.2 Idempotent KWin Compositor Effect Guard (`REF-REQ-071-2`)
- `MitigationEngine::set_kwin_effects_suspended(bool suspend)` must check `s_hardware_baseline.kwin_blur_unloaded == suspend`.
- If already matching, return `true` immediately without touching KWin D-Bus interfaces.

### 2.3 Strict Test Desktop Isolation Invariant (`REF-REQ-071-3`)
- `execute_user_desktop_cmd` must inspect the environment variable `WATTCURB_TEST_MOCK_DESKTOP`.
- If set, the function must simulate execution success (`return true`) without spawning any subshell or modifying the physical display.
- `wattcurb_tests` entry point (`main()`) must initialize `WATTCURB_TEST_MOCK_DESKTOP=1`.

---

## 3. Verification & Oracle Gate Standards (`REF-TEST-036`)

1. **Idempotence Assertion**:
   - Successive calls to `set_display_refresh_rate(60)` when baseline is 60Hz must execute with 0 syscalls and zero command spawns.
2. **Test Isolation Validation**:
   - Complete execution of `wattcurb_tests` must produce 0 `kscreen-doctor` and 0 `qdbus6` system commands.
