# [REF-REQ-124] Fan Curve Revision - Full Speed at 60 C, Ramp 60 -> 35 C

**Status**: Implemented · **Date**: 2026-09-22
**Related**: [`REF-REQ-114`](REQ-114-thinkpad-fan-thermal-assist-curve.md),
[`REF-REQ-118`](REQ-118-all-profile-fan-curve-and-ultra-cold-stop.md),
[`REF-REQ-123`](REQ-123-ec-smu-power-cap-diagnosis.md),
[`REF-TEST-073`](../../tests/test_units.cpp)

## 1. Why

Requested: *"기존 70 부터 MAX FAN 작동 되던 것을 60도로 낮추고 가변 팬구간을 60을
100% 기준으로 35 까지 20% 로 수정하자"*.

REF-REQ-114/118 put full speed at 70 C. REF-REQ-123 then established that **70 C is
the firmware's Tctl ceiling in every platform profile** and cannot be raised from
the OS. So the old curve did not start cooling at "a warm temperature" - it
reached full speed exactly when the part was already at its hard ceiling and the
SMU had begun throttling. There was no margin in which more fan could buy more
clock. Starting the ramp 10 C earlier creates that margin.

## 2. Requirements

- **REQ-124.1** At or above **60 C** the fan shall be at level **7** (full speed).
- **REQ-124.2** Between **35 C and 60 C** the fan shall ramp linearly from **20 %**
  to **100 %**, mapped onto the `thinkpad_acpi` steps `1..7`.
- **REQ-124.3** At or below **35 C** the fan shall be level **1** (20 %, never 0).
- **REQ-124.4** Level 7 is reserved for the threshold and above. Because 7 discrete
  steps span the 0.2..1.0 fraction, nearest-rounding alone reaches 7 at ~57.8 C,
  which would silently make the rule "full speed from ~58 C". The ramp therefore
  caps at level 6 below the threshold.
- **REQ-124.5** The curve continues to apply in **all four** profiles
  (REF-REQ-118.1), and the UltraEndurance cold stop at <= 45 C (REF-REQ-118.3) is
  unchanged.

## 3. Mechanism

| Item | Location |
| :--- | :--- |
| Threshold | `FAN_FULL_TEMP_C` 70.0 -> **60.0** (`src/policy/mitigation_engine.hpp`) |
| Ramp endpoints | `FAN_CURVE_MIN_TEMP_C` = 35.0, `FAN_CURVE_MIN_FRACTION` = 0.2 (unchanged) |
| Top-step reservation | `fan_level_for_temp()` clamps the ramp to 6 below the threshold |
| Profile wrapper | `fan_level_for_temp_in_profile()` (unchanged) |

Resulting curve (level / approximate duty):

| Temp | Level | Duty |
| ---: | ---: | ---: |
| <= 35 C | 1 | 20 % |
| 40 C | 3 | ~36 % |
| 45 C | 4 | ~52 % |
| 50 C | 5 | ~68 % |
| 55 C | 6 | ~84 % |
| 59.9 C | 6 | ~99 % |
| **>= 60 C** | **7** | **100 %** |

## 4. Blast Radius & Failure Modes

- **More fan noise and more fan power from 60 C instead of 70 C.** This is the
  intended trade: the fan is the only lever that can hold the clock, because the
  thermal ceiling itself is firmware-fixed. On this host the fan reaches ~4.7-5.3k
  RPM at level 7.
- **The curve is more aggressive at every temperature, not just at the top.** The
  ramp is compressed into 25 C instead of 35 C, so e.g. 45 C now yields level 4
  where the old curve gave level 3. Fan power draw rises across the whole warm
  range; not measured against the battery budget.
- **UltraEndurance now stops the fan below 45 C and then jumps into a steeper
  ramp.** The cold stop is unchanged, but the first step above it is now a larger
  fraction of full speed. Not measured for acoustic cycling.
- **Level 6 covers a wide band (≈52.5-60 C) and then steps to 7.** A step of one
  discrete level at the threshold; the EC's own curve does the same kind of
  stepping.
- **The threshold change does not raise the thermal ceiling.** It cannot: see
  REF-REQ-123. What it can do is hold the part at the ceiling with more airflow,
  which is what keeps the boost clocks from falling further.

## 5. Verification & Oracle Gate Standards (REF-TEST-073)

`test_thinkpad_fan_thermal_assist_and_smu_limits` asserts, in the sandbox (no
sysfs writes):

1. `fan_level_for_temp(35.0) == 1` and `(20.0) == 1` - the 20 % floor;
2. `fan_level_for_temp(60.0) == 7` and `(59.9) == 6` - the threshold is exact and
   the top step is not reached early;
3. `(70.0) == 7` and `(95.0) == 7` - above the firmware ceiling stays at full;
4. `(47.5) == 4` - the mid-ramp point of the new 60->35 span;
5. monotonic non-decreasing and always within `1..7` across 1..120 C;
6. the >= 60 C rule holds in all four profiles, and the UltraEndurance cold stop
   still applies only there.

Live on the host after release (`/proc/acpi/ibm/fan`, 60 s observation):

| `Tctl` (k10temp `temp1_input`) | Level written | Curve expects | RPM |
| ---: | ---: | ---: | ---: |
| 70.2 C (steady) | 7 | 7 | 4770 |

## 6. The curve input is Tctl, and that bounds the variable range

Measured while verifying: the daemon's `cpu_temp_c` comes from the `k10temp`
hwmon `temp1_input`, which on this Renoir part is labelled **Tctl** (not a
chassis sensor - `/sys/class/thermal/thermal_zone0` is `iwlwifi_1`, and the
`thinkpad` hwmon reads ~53 C).

Tctl is the SMU's control temperature and REF-REQ-123 established that the
firmware holds it at its 70 C ceiling under any real load. Consequence, stated
plainly rather than discovered later:

- **At or above 60 C the fan is at level 7, and on this host Tctl sits at ~70 C
  whenever the machine is doing work.** The practical behaviour of this change is
  therefore "full fan whenever the CPU is warm", which is what was asked for, but
  the 35-60 C ramp only engages when the CPU is genuinely idle and cool.
- The ramp was verified by unit test and by the level-vs-temperature match above,
  not by observing the fan sweep across 35-60 C on a loaded machine. That sweep
  needs an idle host.
- If the intent is for the ramp to engage earlier, the input sensor is the lever
  (e.g. the EC's `thinkpad` sensor, which reads ~53 C in the same conditions).
  That is a separate decision and is NOT part of this change.
