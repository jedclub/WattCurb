# REF-REQ-053: Native Root Systemd Service & Display Brightness Immunity Specification

## 1. Overview & Problem Statement

In previous iterations, two critical user experience anomalies were identified:
1. **Interactive Privilege Escalation Prompts (Polkit / `pkexec`)**:
   The `wattcurb` daemon and helper scripts were originally started under the user session (`systemctl --user`). Whenever power profiles were toggled (e.g. from Performance to Save or Ultra), the helper script `power-profile-manager` checked for root privileges (`EUID != 0`) and executed `pkexec`, causing repeated desktop password authentication dialogs to disrupt the user.
2. **Uncontrolled Display Brightness & Refresh Rate Degradation**:
   - `MitigationEngine::cap_display_backlight(50.0)` in C++ and `power-profile-manager` in Bash actively wrote to `/sys/class/backlight/.../brightness`, forcibly dimming the screen to arbitrary levels (20%, 35%, 50%) during profile transitions.
   - `power-profile-manager` invoked `kscreen-doctor output.1.mode.2` on power save modes, forcibly locking the display refresh rate to 48Hz and causing noticeable visual stutter.
   - Left-clicking the tray icon executed `cycle_power_profile()`, accidentally flipping profiles and dimming the display when the user only intended to interact with or inspect the tray.

---

## 2. Functional Requirements

### 2.1 Native Root Systemd Service Topology
- **Requirement 1**: The WattCurb daemon must run natively as an administrator (`root`) systemd service at `/etc/systemd/system/wattcurb.service`, starting at system boot (`multi-user.target`).
- **Requirement 2**: With `EUID == 0`, all hardware actuation operations—cgroup freezing, CPU scaling governors, sysfs powercap RAPL adjustments, and SMU profile toggles—must execute with native kernel privileges. Under no circumstances may `pkexec`, `sudo`, or Polkit dialogs ever be invoked.
- **Requirement 3**: The user session daemon unit (`~/.config/systemd/user/wattcurb.service`) must be disabled and removed.

### 2.2 Cross-User Zero-Copy Shared Memory Access
- **Requirement 4**: The root daemon creates the POSIX shared memory object `/dev/shm/wattcurb_shared_state`. It must explicitly call `fchmod(shm_fd, 0666)` immediately after `shm_open` and `ftruncate`.
- **Requirement 5**: Unprivileged desktop clients (e.g. `wattcurb-tray` and `wattcurb-dashboard` running under UID 1000) must map `/dev/shm/wattcurb_shared_state` read-only (`O_RDONLY`, `PROT_READ`) and ingest binary Seqlock POD state without requiring root privileges.

### 2.3 Strict Display Brightness & Panel Immunity
- **Requirement 6**: Display brightness is strictly out-of-bounds for autonomous daemon throttling. `MitigationEngine::cap_display_backlight()` and `MitigationEngine::restore_display_backlight()` must be no-ops.
- **Requirement 7**: All hard-coded brightness overrides (`echo ... > $bl/brightness`) in `power-profile-manager` must be removed.
- **Requirement 8**: All display refresh rate overrides (e.g. `kscreen-doctor` forcing 48Hz) must be purged to maintain fluid 60Hz+ refresh rates without visual stutter.

### 2.4 Accidental Trigger Elimination in UI / Tray
- **Requirement 9**: Left-clicking the `wattcurb-tray` icon must NOT cycle power profiles. Left-click must only trigger a safe telemetry re-read (`RESCAN`).
- **Requirement 10**: Power profile switches must only occur via deliberate user action in the context menu or dashboard.
- **Requirement 11**: `DashboardBackend::setProfile()` must delegate hardware profile actuation solely through the daemon's abstract UNIX domain socket (`PROFILE <mode>`), completely eliminating local user-level `system()` execution.

---

## 3. Verification & Acceptance Criteria

- **AC-1**: `systemctl status wattcurb.service` reports `active (running)` as user `root` (PID owned by root).
- **AC-2**: `systemctl --user status wattcurb.service` is inactive/disabled.
- **AC-3**: `systemctl --user status wattcurb-tray.service` reports `active (running)` as user `jedclub` (UID 1000) and continuously ingests live telemetry from `/dev/shm/wattcurb_shared_state`.
- **AC-4**: Toggling power profiles (Performance, Balanced, Save, Ultra) via tray context menu or dashboard causes 0 Polkit/pkexec prompts.
- **AC-5**: Toggling power profiles never modifies `/sys/class/backlight/.../brightness` or triggers screen refresh rate downclocking.
