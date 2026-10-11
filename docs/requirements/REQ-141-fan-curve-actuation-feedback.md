# REF-REQ-141: Real-Time Hardware Actuation Confirmation & Verified Feedback Protocol for Custom Fan Curves

## 1. Operational Need & Motivation

In previous iterations, applying a custom cooling curve via `FanCurveStudioView` provided limited feedback:
- The UI relied on a generic, optimistic toast without verifying actual hardware response.
- Users could not determine whether the root daemon had successfully received the curve, whether the configuration was written to disk, or what specific ThinkPad EC fan level and RPM had been actuated on the physical hardware.

To establish operational trust and eliminate ambiguity, WattCurb requires an **end-to-end verified feedback protocol**:
1. The daemon must inspect the actual post-actuation fan level, current RPM, and die temperature, returning structured telemetry in the IPC acknowledgement.
2. The precision dashboard must present real-time, unambiguous visual feedback, showing exact ThinkPad EC fan step and RPM confirmation.
3. The UI must provide tactile and visual validation (cyber glow pulse and live hardware status indicators).

---

## 2. Functional Requirements

### REF-REQ-141.1: Structured Wire Actuation Acknowledgement (`SET_FAN_CURVES`)
* Upon receiving and parsing `SET_FAN_CURVES`, the root daemon shall immediately execute `MitigationEngine::apply_fan_for_temp()` and read the live hardware fan status from `/proc/acpi/ibm/fan`.
* The daemon's IPC reply shall return structured telemetry:
  ```text
  OK LEVEL=<level> RPM=<rpm> TEMP=<temp_c> PROFILE=<profile_id>
  ```
  Where:
  * `<level>` is the commanded ThinkPad EC fan step ($0 \dots 6$ or $8$ for `full-speed`).
  * `<rpm>` is the real-time measured fan RPM.
  * `<temp_c>` is the CPU package temperature at actuation.
  * `<profile_id>` is the active power profile index ($0 \dots 3$).

### REF-REQ-141.2: Dashboard Real-Time Actuation Confirmation
* `DashboardBackend` shall expose the following properties:
  * `fanApplyFeedback` (`QString`): Human-readable verified feedback string (e.g., `✅ [데몬 동기화 성공] ThinkPad EC Level 5 즉시 구동 (현재 4,333 RPM · 54.2°C)`).
  * `isFanApplySuccess` (`bool`): Boolean flag denoting successful daemon verification.
  * `daemonFanLevel` (`int`): Last verified ThinkPad EC fan level from the daemon.
  * `daemonFanStatus` (`QString`): Short status badge text (e.g., `EC L5 (4333 RPM)`).
* The dashboard shall emit `fanApplyFeedbackChanged()` and update UI telemetry upon every apply operation.

### REF-REQ-141.3: Visual & Tactile Confirmation in Fan Studio
* `FanCurveStudioView.qml` shall display:
  1. **Top Header Live Actuation Badge**: An illuminated status pill showing `🟢 EC 동기화: L<level>` upon successful daemon confirmation.
  2. **Prominent Actuation Confirmation Banner**: A high-visibility cyber confirmation card displaying exact RPM, level, and timestamp.
  3. **Canvas Pulse Glow Animation**: An instantaneous cyan/green pulse glow across the spline graph upon successful hardware actuation.

---

## 3. Verification & Oracle Gate Standards (`REF-TEST-095`)

* **Unit Test**: `test_fan_curve_actuation_feedback_wire_protocol()`
  * Verifies parsing of structured acknowledgement string (`OK LEVEL=%d RPM=%d TEMP=%f PROFILE=%d`).
  * Asserts correct extraction of EC fan step, RPM, temperature, and profile id.
  * Validates graceful fallback for malformed or error replies.
