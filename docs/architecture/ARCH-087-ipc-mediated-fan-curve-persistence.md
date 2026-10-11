# REF-ARCH-087: IPC-Mediated Fan Curve Persistence & Privilege Separation Architecture

## 1. System Architecture Overview

```
+-------------------------------------------------------------------------+
|                  Qt6 / QML Unified Matrix Dashboard                     |
|  (Unprivileged Process: UID 1000 / jedclub, Wayland / Desktop Session)  |
|                                                                         |
|  +-------------------------------------------------------------------+  |
|  |             Tab 2: FanCurveStudioView.qml                         |  |
|  |   - Interactive Monotone Hermite Spline Canvas (2~10 pts)         |  |
|  |   - Click "💾 커브 적용 (데몬 반영)"                              |  |
|  +---------------------------------+---------------------------------+  |
|                                    |                                    |
|                                    v                                    |
|  +-------------------------------------------------------------------+  |
|  |             DashboardBackend::applyFanCurves()                    |  |
|  |   - Serialize in-memory curves via FanCurveEngine                 |  |
|  |   - Send datagram: "SET_FAN_CURVES\n" + payload                   |  |
|  |   - Verify daemon reply == "OK"                                   |  |
|  +---------------------------------+---------------------------------+  |
+------------------------------------+------------------------------------+
                                     | UNIX Abstract Socket (@wattcurb.lock)
                                     | With SCM_CREDENTIALS (SO_PASSCRED)
                                     v
+-------------------------------------------------------------------------+
|                       WattCurb Root Daemon                              |
|  (Privileged System Service: UID 0 / root, systemd root slice)          |
|                                                                         |
|  +-------------------------------------------------------------------+  |
|  |             DaemonRunner::handle_client()                         |  |
|  |   1. Verify peer credentials via peer_is_authorized_client()      |  |
|  |   2. Parse and deserialize payload -> FanCurveEngine              |  |
|  |   3. Write /etc/wattcurb/fan_curves.conf (mode 0644, root-owned)  |  |
|  |   4. Trigger immediate fan actuation: apply_fan_for_temp()        |  |
|  |   5. Reply "OK\n"                                                 |  |
|  +---------------------------------+---------------------------------+  |
|                                    |                                    |
|                                    v                                    |
|  +-------------------------------------------------------------------+  |
|  |             Kernel /proc/acpi/ibm/fan Actuation                   |  |
|  |   - Instant EC register update to target level                    |  |
|  +-------------------------------------------------------------------+  |
+-------------------------------------------------------------------------+
```

---

## 2. IPC Wire Protocol (`SET_FAN_CURVES`)

* **Datagram Payload**:
  ```text
  SET_FAN_CURVES
  PROFILE_0_POINTS=5
  PROFILE_0_CUSTOM=1
  PT_0=30.0,20.0
  PT_1=40.0,40.0
  PT_2=50.0,65.0
  PT_3=60.0,85.0
  PT_4=70.0,100.0
  PROFILE_1_POINTS=5
  PROFILE_1_CUSTOM=0
  ...
  ```
* **Security Guardrails**:
  * Mandatory verification of `SCM_CREDENTIALS`.
  * Requester executable path must strictly match `/usr/local/bin/wattcurb-dashboard` or `/usr/local/bin/wattcurb-tray` (REF-REQ-111).
  * Payload size bounds: max 4096 bytes per datagram.
  * Point count validated against `MIN_POINTS` (2) and `MAX_POINTS` (10).
  * Temperature monotonically increasing clamped to $[30.0, 70.0]$.
  * Full-speed 100% safety ceiling clamped at $70.0^\circ\text{C}$ (REF-REQ-136.3).
