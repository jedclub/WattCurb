# [REF-REQ-118] All-Profile Fan Curve & UltraEndurance Cold Stop

**Status**: Implemented · **Date**: 2026-09-22
**Related**: [`REF-REQ-114`](REQ-114-thinkpad-fan-thermal-assist-curve.md),
[`REF-REQ-115`](REQ-115-smu-thermal-power-limit-raise.md),
[`REF-TEST-073`](#5-verification--oracle-gate-standards-ref-test-073)

## 1. Why

[`REF-REQ-114`](REQ-114-thinkpad-fan-thermal-assist-curve.md) scoped the fan curve
to `Performance` and `Balanced` only, on the reasoning that a saving profile should
not spend battery on cooling it is not using. In the field that produced two
problems:

1. **The fan was not controlled in `PowerSaver`/`UltraEndurance` at all.** If the
   part is hot - a long AC charge, a resumed workload, a machine left in the sun -
   the saving profiles had no say over the fan, so a hot machine stayed on the
   EC's conservative curve regardless of profile.
2. **The curve was never a real variable curve.** With `FAN_FULL_TEMP_C = 70 C`
   and this host's Tctl sitting at 63-69 C at ordinary desktop load, the mapping
   resolved to level 6-7 nearly always. The "variable" fan was effectively a fixed
   high setting.

The user's directive is therefore: the thermal mapping is a **safety** rule that
applies everywhere, and `UltraEndurance` gets one additional exception because
that profile exists to minimise every load - at or below 45 C it stops the fan
outright rather than letting the EC idle it at several thousand RPM for no
thermal reason.

## 2. Requirements

- **REQ-118.1 (All profiles)** The curve shall be evaluated in every power
  profile (`Performance`, `Balanced`, `PowerSaver`, `UltraEndurance`).
- **REQ-118.2 (Common mapping)** The mapping shall be:
  - `>= 70 C` → level **7** (full speed)
  - `35 C .. 70 C` → linear, **20 % .. 100 %**, mapped onto steps `1..7`
  - `<= 35 C` → level **1** (20 %; never 0 by the common rule)
  - no reading (non-positive) → no write, report `-1`
- **REQ-118.3 (UltraEndurance cold stop)** In `UltraEndurance` only, at or below
  **45 C** the fan shall be stopped: level **0**.
- **REQ-118.4 (No leak)** The cold stop shall not apply to the other three
  profiles; they keep the common floor of level 1.
- **REQ-118.5 (Numeric top step)** The top step shall be written as the numeric
  `7`, never the string `full-speed` (see REF-REQ-114 §4.1 - on this host the
  keyword resolves to `disengaged`).
- **REQ-118.6 (Write economy)** Writes shall still occur only on a level change.

## 3. Mechanism

| Item | Location |
| :--- | :--- |
| Profile-aware mapping | `MitigationEngine::fan_level_for_temp_in_profile()` |
| Common mapping | `MitigationEngine::fan_level_for_temp()` |
| Cycle driver | `FeatureManager::evaluate_and_actuate()` (`src/policy/battery_feature.cpp`) |
| Constants | `FAN_CURVE_MIN_TEMP_C=35`, `FAN_CURVE_MIN_FRACTION=0.2`, `FAN_FULL_TEMP_C=70`, `FAN_ULTRA_STOP_TEMP_C=45` |

The call was moved out of the `Performance || Balanced` block so it runs before
that branch, once per observation cycle, for all four profiles.

Measured on this host: `level 0` stops the fan (`status: disabled`, tachometer
decays to `0` over ~25 s); `level 7` is full speed. The EC still holds final
authority and may reduce a requested level under its own thermal policy.

## 4. Blast Radius & Failure Modes

- **Fan runs in saving profiles now.** A hot machine on `PowerSaver` or
  `UltraEndurance` will have its fan driven by this mapping instead of the EC's
  curve, which spends fan power in profiles chosen to save it. This is the
  intended trade: not cooking the part outranks the fan's ~3.6 W.
- **UltraEndurance can stop the fan.** At or below 45 C the fan is written to
  level 0. If the temperature rises the common curve re-engages automatically on
  a later cycle, but during the stop the only cooling is passive. On a machine
  with blocked vents or a hot ambient, the 45 C entry condition could be met and
  the fan stopped with little margin. The threshold and the re-engagement
  behaviour were not measured under sustained load.
- **Fan is a shared, safety-relevant device.** As in REF-REQ-114.
- **Baseline restore unchanged.** `restore_fan_level()` still returns the captured
  level on release/exit.

## 5. Verification & Oracle Gate Standards (REF-TEST-073)

Extended in `test_thinkpad_fan_thermal_assist_and_smu_limits()`:

1. `>= 70 C` is level 7 in **all four** profiles;
2. no reading is `-1` in all four;
3. the 35 C floor is level 1 in `Performance`/`Balanced`/`PowerSaver`;
4. `UltraEndurance` at 30 C, 35 C and 45 C is level 0, and above the threshold it
   equals the common curve;
5. the cold stop does not leak into the other profiles at 30 C;
6. the production path evaluates the curve in `Performance`, `Balanced` **and**
   `PowerSaver` (application counter increases).

The tests assert the mapping and the wiring. They do not measure the fan's
thermal effect, nor that the host stays cool with the fan stopped below 45 C.
