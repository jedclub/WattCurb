# REQ-066: User Profile Sovereignty & Explicit Display Optimization Specification

- **Status**: Approved
- **Ref ID**: `REF-REQ-066`
- **Related Architecture**: `REF-ARCH-042`
- **Related Test**: `REF-TEST-031`
- **Created**: 2026-09-18
- **Category**: Power Profile State Machine, User Sovereignty, Display Power Optimization

---

## 1. Background & Problem Clarification

During background monitoring and daemon testing, the agent and internal daemon heuristics toggled between `UltraEndurance`, `Balanced`, and `PowerSaver` modes without explicit user command.
Whenever the power profile toggled, the system executed:
- `kscreen-doctor output.1.mode.2` (48Hz) / `mode.1` (60Hz), triggering a physical display hardware modeset (1~2 second screen blackout/flicker).

The user explicitly clarified the system invariant:
> *"What I took issue with was the system arbitrarily changing the power profile without my input. Changing the refresh rate or optimizing the screen for power efficiency when I intend it is completely fine."*

---

## 2. Core Directives & Functional Specifications

### 2.1 Absolute User Profile Sovereignty (Anti-Drift Guard)
- **User Choice is Final**: When a user explicitly selects a power profile via the UI Dashboard, System Tray, or CLI (`power-profile-manager` / `PROFILE <mode>`), that profile is **immutable** against background heuristics.
- **Daemon Override Synchronization**: The root daemon runner (`DaemonRunner`) must strictly register `m_profile_override` inside `MitigationEngine` simultaneously with `BatteryFeatureManager`.
- **Elimination of Autonomous Profile Flapping**: Under no circumstances may the daemon's battery threshold state machine override the user's manual selection unless the user explicitly resets to automatic mode or unsets the override.
- **Agent Operational Restraint**: Testing tools and autonomous verification loops must never execute active system profile switches on the user's live desktop. Live profile changes belong solely to the user.

### 2.2 Intentional Display Refresh Rate Optimization (DRRS 48Hz)
- When the user **explicitly chooses Ultra Save / UltraEndurance mode**:
  - The system legitimately applies 48Hz DRRS (`kscreen-doctor output.1.mode.2`) alongside backlight capping and KWin effect unloading to extract maximum battery endurance (~0.4W savings).
- When the user **switches back to Balanced, PowerSaver, or Performance mode**:
  - The system smoothly restores 60Hz (`kscreen-doctor output.1.mode.1`).
- Because the profile is strictly locked to the user's manual choice, modeset flicker occurs **only once upon deliberate user command**, and never spontaneously or unpredictably in the background.

---

## 3. Verification & Oracle Gate Standards (`REF-TEST-031`)

1. **Profile Override Persistence**:
   - Issue `PROFILE 3` (UltraEndurance). Verify `m_profile_override == UltraEndurance`.
   - Run 100 periodic cycles under varying mock battery levels (10% ~ 90%). Verify the active profile never drifts away from `UltraEndurance`.
2. **Intentional Actuation Execution**:
   - Verify `set_display_refresh_rate(48)` executes cleanly when transitioning to `UltraEndurance`.
   - Verify `set_display_refresh_rate(60)` executes upon restoration.
