# REF-REQ-140: Privilege-Separated Fan Curve IPC Transmission & Atomic Persistence

## 1. Problem Statement & Root Cause

Under the implementation of REF-REQ-136, the Qt6/QML precision dashboard (`wattcurb-dashboard`) provides an interactive curve tuner (`FanCurveStudioView.qml`) allowing users to modify control points and click **[💾 커브 적용 (데몬 반영)]**.

However, fan curves were completely failing to be recorded and persisted due to two architectural defects:
1. **Directory Non-Existence**:
   - `/etc/wattcurb/` was not created by `install.sh`, nor did `FanCurveEngine::save_to_file` create parent directories via `mkdir`.
   - Any call to `std::fopen("/etc/wattcurb/fan_curves.conf", "w")` failed immediately with `ENOENT`.
2. **Privilege Boundary Inversion**:
   - The dashboard runs as the unprivileged desktop user (`UID 1000`), whereas `/etc` is owned by `root:root (0755)`.
   - The unprivileged dashboard attempted to write directly to `/etc/wattcurb/fan_curves.conf`. Because unprivileged users cannot write to `/etc`, `save_to_file()` failed with `EACCES`.
   - Although `DashboardBackend::applyFanCurves()` sent `RELOAD_FAN_CURVES` to the root daemon, the daemon attempted to read the nonexistent file and returned immediately without setting custom splines (`is_custom` remained `false`).
   - Consequently, the host EC fan continued executing the hardcoded factory default curve, while the UI displayed an inaccurate status.

---

## 2. Functional Requirements

### REF-REQ-140.1: Privilege-Separated IPC Fan Curve Synchronization (`SET_FAN_CURVES`)
* Instead of requiring unprivileged client processes to directly write to `/etc`, `DashboardBackend::applyFanCurves()` shall serialize the 4-profile curve configuration and transmit it over the authorized abstract domain socket (`@wattcurb.lock`) via the `SET_FAN_CURVES` datagram command.
* The root daemon (`DaemonRunner`) shall verify caller credentials via `SO_PASSCRED` (`peer_authorized`, REF-REQ-111). Unauthorized senders shall be rejected immediately.
* Upon validation, the root daemon shall deserialize the curves, update its active `FanCurveEngine` in-memory structures, write the configuration atomically to `/etc/wattcurb/fan_curves.conf` (ensuring parent directories exist), immediately actuate the fan hardware for current temperatures, and reply `OK\n`.

### REF-REQ-140.2: Bidirectional Serialization & Deserialization Engine
* `FanCurveEngine` shall implement zero-overhead `serialize_curves()` and `deserialize_curves()` methods:
  * Profile-specific custom status (`PROFILE_<id>_CUSTOM=1` vs `0`) must be explicitly recorded, allowing factory default curves and custom curves to coexist independently across profiles.
  * Point count and control points `(temp_c, speed_pct)` must be accurately restored.
  * Spline lookup tables (`m_fast_levels`, `m_fast_pcts`) must be automatically recomputed upon deserialization using AVX2 SIMD splines.
* `save_to_file()` and `load_from_file()` shall delegate directly to the unified serialization engine to eliminate parsing divergence.

### REF-REQ-140.3: Directory & Installation Safeguards
* `FanCurveEngine::ensure_dir_exists()` shall automatically create missing parent directories (`0755`) before opening files.
* `install.sh` shall explicitly provision `/etc/wattcurb` (`0755`) during installation.
* `FanCurveStudioView.qml` status toast shall accurately reflect daemon ACK success (`OK`) versus failure.

---

## 3. Verification & Oracle Gate Standards (`REF-TEST-094`)

* **Unit Test**: `test_fan_curve_serialization_and_ipc_persistence()`
  * Verifies custom flag retention (`is_profile_custom`).
  * Verifies exact round-trip equivalence of points and fast lookup tables.
  * Verifies file saving and directory creation resilience.
