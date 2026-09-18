# REQ-066: Display Modeset Flicker Elimination & Refresh Rate Invariant

- **Status**: Approved
- **Ref ID**: `REF-REQ-066`
- **Related Architecture**: `REF-ARCH-042`
- **Related Test**: `REF-TEST-031`
- **Created**: 2026-09-18
- **Category**: Display Power Management, User Experience, Non-Intrusive Execution

---

## 1. Background & Problem Statement

During battery optimization experiments, the daemon attempted Dynamic Refresh Rate Switching (DRRS) between 60Hz and 48Hz using `kscreen-doctor output.1.mode.2`.
However, under Wayland/KWin on AMD Renoir APU with eDP panels:
1. Every invocation of `kscreen-doctor` triggers a hardware **Display Modeset**, causing the physical eDP panel to turn off and turn back on (a complete 1~2 second black screen flicker).
2. When profile transitions occurred (e.g., automated battery state changes or test suites running `UltraEndurance` $\leftrightarrow$ `Balanced`), the display repeatedly turned off and on.
3. This severely compromises desktop ergonomics and user predictability, violating the global non-intrusive directive (`AGENTS.md` Sec 7.3).

---

## 2. Requirements & Invariant Enforcement

### 2.1 Complete Elimination of Dynamic Modeset
- The daemon and helper scripts **must never** execute commands that trigger a physical modeset on the primary display (`kscreen-doctor output.*.mode.*`).
- `MitigationEngine::set_display_refresh_rate` is permanently deactivated into a no-op returning `false`.
- Profile actuation routines (`PowerProfileMode::UltraEndurance`, `PowerSaver`, `Balanced`, `Performance`) and hardware baseline restoration **must omit** all refresh rate modifications.
- The display refresh rate remains permanently anchored at the user's preferred baseline (60.06 Hz) with **zero black-screen flickering**.

---

## 3. Verification & Oracle Gate Standards (`REF-TEST-031`)

1. **Zero-Modeset Transition Latency**:
   - `UltraEndurance` profile transitions must complete within **< 50ms** without blocking on external Wayland display calls.
2. **Display Mode Invariance**:
   - Verify `kscreen-doctor -o` reports 60Hz permanently across profile shifts.
