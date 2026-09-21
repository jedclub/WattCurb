# REF-REQ-091: Bi-Directional Power Profile State Coherence & Seqlock Synchronization

## 1. Context & Problem Statement
WattCurb provides multiple interactive client interfaces to control hardware power profiles (`0=Performance`, `1=Balanced`, `2=SmartSave`, `3=UltraSave`):
1. The low-overhead desktop StatusNotifierItem system tray indicator (`wattcurb-tray`).
2. The detailed telemetry analysis and btop-style monitoring dashboard (`wattcurb-dashboard`).
3. Command-line control invocations over the root Unix domain socket (`PROFILE <mode>`).

Previously, when a user switched the power profile from the tray menu, the change was applied to hardware by the root daemon, but the precision consumption metrics window (`wattcurb-dashboard`) failed to update its active profile button state in sync. The dashboard remained visually pinned to its initial profile.

Detailed architectural root causes identified:
1. **Seqlock Version Stagnation**: When the daemon handled `PROFILE <mode>`, it directly assigned `shm_state_->power_profile_mode = new_mode` without updating the atomic seqlock sequence (`seq_version`). Consequently, seqlock consumers (`DashboardBackend`) saw no increment in `seq_version` and skipped state propagation.
2. **Sticky Local Override**: `DashboardBackend` initialized its `local_override_mode_` in the constructor to the current profile mode (e.g. `1`), permanently trapping `powerProfileMode()` in override mode. Even if SHM changed, `local_override_mode_ >= 0` always superseded the daemon state.
3. **Missing Profile Signal Emission**: `DashboardBackend::onPollTimer()` never compared `power_profile_mode` changes nor emitted `emit profileChanged()`, leaving QML property bindings completely unnotified.
4. **JSON Telemetry Omission**: Although `daemon_runner.cpp` serialized `"profile_mode"` in `FULL_TELEMETRY`, `DashboardBackend::ingestTelemetryJson` omitted parsing it.

---

## 2. Functional Requirements

### REQ-091.1: Atomic Seqlock Profile Update in Daemon
- The shared state structure [`WattCurbSharedState`](file:///home/jedclub/Develop/WattCurb/src/ipc/tray_shared_state.hpp) must provide an atomic helper `update_profile_mode(uint8_t mode) noexcept` that advances `seq_version` with release barriers (odd during write, even when stable).
- The daemon runner ([`daemon_runner.cpp`](file:///home/jedclub/Develop/WattCurb/src/core/daemon_runner.cpp)) must invoke `update_profile_mode()` on every `PROFILE` IPC command and initial state publication.

### REQ-091.2: Ephemeral Local Override & Automatic Clearance
- `DashboardBackend::local_override_mode_` must initialize to `-1` (inactive).
- When `setProfile(mode)` is invoked from the dashboard UI, `local_override_mode_` temporarily reflects the requested mode for immediate UI snappiness.
- As soon as `onPollTimer()` reads a matching mode from the daemon SHM (`cur.power_profile_mode == local_override_mode_`) OR detects an external profile change (`cur.power_profile_mode != prev_profile_mode_`), `local_override_mode_` must automatically reset to `-1`.

### REQ-091.3: Real-Time Profile Change Detection & Signal Emission
- `DashboardBackend` must track `prev_profile_mode_`.
- On every poll iteration, if the effective profile differs from `prev_profile_mode_`, `emit profileChanged()` must be triggered immediately, updating all QML button states and titles.

### REQ-091.4: Telemetry JSON Profile Ingestion
- `DashboardBackend::ingestTelemetryJson()` must parse `"profile_mode"`, update internal state, and notify QML if the profile changed.

---

## 3. Non-Functional & Verification Requirements
- **Zero-Allocation**: Synchronization and signal emission must involve zero heap allocations in the monitoring loop.
- **Sub-Microsecond Latency**: Seqlock read and coherence resolution must execute in $< 50\text{ ns/op}$.
- **Verification**: Verified by [`REF-TEST-055`](file:///home/jedclub/Develop/WattCurb/tests/test_units.cpp).
