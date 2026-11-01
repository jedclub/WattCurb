# REF-REQ-147: Per-Profile Max Fan Speed Ceiling & 85°C Dual-Tier Thermal Failsafe

## 1. Operational Need & Motivation
Prior to this requirement (under `REF-REQ-136`), WattCurb strictly forced the cooling fan curve's 70°C endpoint to 100% full-speed (`FAN_LEVEL_FULL_SPEED`, ~5,400 RPM on ThinkPad EC) across all profiles, and unconditionally returned 100% full speed whenever the CPU package temperature reached or exceeded 70°C.

While designed as a conservative thermal protection invariant, this hardcoded ceiling caused severe acoustic disruption in quiet working environments (such as libraries, quiet offices, or lecture halls). Transient CPU load spikes (e.g., loading complex web pages, background indexing, short compilations) routinely push modern high-density processors (such as AMD Ryzen Rembrandt/Zen 3+) to 65°C~75°C for brief intervals of 1~3 seconds. Under the former rule, the cooling fan immediately ramped up to maximum RPM, causing intolerable fan noise even when operating under `PowerSaver` or `UltraEndurance` profiles.

Users require the ability to define a **custom Maximum Fan Speed Ceiling** (e.g., 40%~70%) for each profile via the interactive Fan Curve Studio. To maintain absolute hardware safety, the system must separate the normal acoustic regulation domain from the emergency thermal failsafe, raising the emergency full-speed trigger to **85°C**.

---

## 2. Functional Requirements

### REF-REQ-147.1: Unlocked Profile-Specific Maximum Fan Speed
* The Fan Curve Studio and `FanCurveEngine` must allow the endpoint control point ($T = 70^\circ\text{C}$) to be configured to any valid speed percentage:
  * For standard profiles (`Performance`, `Balanced`, `PowerSaver`): $S_{\max} \in [10\%, 100\%]$.
  * For `UltraEndurance`: $S_{\max} \in [0\%, 100\%]$.
* The hardcoded override (`c.points[count - 1].speed_pct = 100.0f;`) in `set_profile_curve` must be eliminated.
* In-memory editing, persistence serialization, and IPC transmission must faithfully preserve the configured endpoint speed percentage without resetting or clamping it to 100%.

### REF-REQ-147.2: Dual-Tier Thermal Failsafe Architecture (85°C Emergency Invariant)
* The temperature domain is partitioned into two distinct thermal operating tiers:
  1. **Acoustic Regulation Tier ($T < 85^\circ\text{C}$)**:
     * For $30^\circ\text{C} \le T \le 70^\circ\text{C}$, fan speed and ThinkPad EC levels are determined by the Monotone Cubic Hermite Spline lookup table.
     * For $70^\circ\text{C} < T < 85^\circ\text{C}$, the fan speed is held constant at the profile's configured maximum ceiling ($S_{\max} = \text{LUT}[40]$). This suppresses acoustic runaway while the CPU stays safely below critical throttling thresholds.
  2. **Emergency Thermal Failsafe Tier ($T \ge 85^\circ\text{C}$)**:
     * Whenever CPU package temperature reaches or exceeds **$85^\circ\text{C}$** (`FanCurveEngine::EMERGENCY_TEMP_C`), the daemon must unconditionally command **$100\%$ full speed (`FAN_LEVEL_FULL_SPEED`)**, overriding any user-defined ceiling to protect the silicon.
     * Once the temperature drops below $85^\circ\text{C}$, the daemon smoothly restores the user-configured profile fan speed without requiring user intervention.

### REF-REQ-147.3: UI Visualization & Feedback
* `FanCurveStudioView.qml` must reflect the new dual-tier architecture:
  * The header banner text must indicate the customizable max speed and the 85°C emergency protection.
  * The crosshair readout and curve stroke must accurately follow the capped fan speed up to 85°C, transitioning to full speed only in the emergency zone ($T \ge 85^\circ\text{C}$).
  * Interactive dragging of the 70°C control point must allow fluid vertical adjustment of the profile's maximum fan speed.

---

## 3. Verification Criteria & Ref-IDs
* Requirement: `REF-REQ-147`
* Architecture: `REF-ARCH-094`
* Test Specification: `REF-TEST-101` (Endpoint speed mutability, 70°C~84°C ceiling holding, 85°C emergency full-speed trigger, IPC round-trip persistence).
