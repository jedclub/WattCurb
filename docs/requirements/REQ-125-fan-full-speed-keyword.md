# [REF-REQ-125] Fan "Full Speed" Was Not Full Speed - Numeric 7 vs the Keyword

**Status**: Implemented · **Date**: 2026-09-22
**Related**: [`REF-REQ-114`](REQ-114-thinkpad-fan-thermal-assist-curve.md),
[`REF-REQ-118`](REQ-118-all-profile-fan-curve-and-ultra-cold-stop.md),
[`REF-REQ-124`](REQ-124-fan-curve-60c-full-speed.md),
[`REF-TEST-073`](../../tests/test_units.cpp)

## 1. Why

Reported after the REF-REQ-124 release: *"팬이 지금은 MAX 로 돌지 않아"* - the fan is
not running at maximum.

The daemon was commanding the curve correctly (`level: 7` in
`/proc/acpi/ibm/fan`, `status: enabled`, `fan_control=Y`), so the first question
was whether the EC was silently reverting the write. It was not: re-writing
level 7 changed nothing, and the level stayed 7. The problem was the **value**
being written.

## 2. Measurement

Every `thinkpad_acpi` level, 12-14 s to settle, on this host at `Tctl` 70.2 C:

| Command | Reported level | RPM |
| :--- | :--- | ---: |
| `level 2` | 2 | 3472 |
| `level 3` | 3 | 3472 |
| `level 4` | 4 | 3610 |
| `level 5` | 5 | 4332 |
| `level 6` | 6 | 4789 |
| **`level 7`** | 7 | **4780** |
| **`full-speed`** | disengaged | **5346** |
| `disengaged` | disengaged | 5349 |
| `auto` | 4 | 4464 |

Cross-checked twice, alternating the two writes: `level 7` -> 4780 / 4778 RPM,
`full-speed` -> 5345 / 5348 RPM. The full-speed state held for 60 s
(5345/5341/5338/5342) with no EC revert.

**Numeric 7 is not the fan's maximum.** It sits **566 RPM (12 %)** below it.

The reason: `thinkpad_acpi` maps the `full-speed` keyword (and its synonym
`disengaged`) to `TP_EC_FAN_FULLSPEED`, a state distinct from the discrete steps
`0..7`. The discrete scale tops out below the EC's full-speed state, and levels 6
and 7 are effectively identical (4789 vs 4780 RPM).

### 2.1 The comment that hid it

`fan_level_for_temp()` carried this justification for using numeric 7:

> *"Numeric 7 is the real full speed here (measured ~5.3k RPM)."*

That reading was taken while the fan was still coasting down from a
`full-speed` write, so the 5.3k belonged to the *previous* state, not to level 7.
The conclusion inverted the truth: the fix for the REF-REQ-114 defect (a cache
that never retried) replaced a correct write with a weaker one, and the weaker
write was then defended by a mis-attributed measurement.

## 3. Requirements

- **REQ-125.1** The full-speed step shall be written as the `full-speed` keyword
  (`TP_EC_FAN_FULLSPEED`), never as numeric 7.
- **REQ-125.2** The ramp shall use numeric steps `1..6` only. Numeric 7 shall
  never be commanded by the curve; the level-7 sentinel shall not collide with a
  numeric step.
- **REQ-125.3** The cached last level shall still be the daemon's own written
  value, so the write-on-change behaviour is unchanged and a steady temperature
  still costs no sysfs write.

## 4. Mechanism

| Item | Location |
| :--- | :--- |
| Sentinel | `FAN_LEVEL_FULL_SPEED = 8` (`src/policy/mitigation_engine.hpp`) |
| Curve top step | `fan_level_for_temp()` returns the sentinel at/above 60 C |
| Ramp cap | `if (lvl > 6) lvl = 6;` |
| Formatting | `apply_fan_for_temp()` emits `"full-speed"` for the sentinel, the number otherwise |

## 5. Blast Radius & Failure Modes

- **The fan is louder at and above 60 C.** That is the point: 5346 RPM instead of
  4780. Below the threshold the ramp is unchanged.
- **`/proc/acpi/ibm/fan` reports `level: disengaged` while at full speed**, not a
  number. Any tool reading that file to mean "WattCurb is not in control" will
  misread it; `status: enabled` remains the control indicator.
- **`disengaged` and `full-speed` are synonyms here** and both reach 5346 RPM. The
  keyword is written explicitly for readability; `auto` is NOT a synonym (it gave
  level 4 / 4464 RPM) and is never used.
- **The EC could in principle refuse the keyword**, in which case `set_fan_level()`
  fails and `apply_fan_for_temp()` returns -1 without updating the cache - the
  next cycle retries. Not observed: the write held for 60 s.
- **Not measured:** whether `full-speed` is the absolute hardware maximum. It is
  the highest state reachable through `thinkpad_acpi` on this host; nothing above
  it was found.

## 6. Verification & Oracle Gate Standards (REF-TEST-073)

- The curve returns the sentinel at/above 60 C and never returns numeric 7 at any
  temperature in 1..110 C.
- The sentinel is distinct from every numeric step.
- The ramp still tops out at 6 and stays monotonic across 1..120 C.
- The >= 60 C rule holds in all four profiles (REF-REQ-118.1).
- Live on the host after release: with `Tctl` at 70.2 C the daemon's write must
  produce ~5346 RPM (previously 4780).

## 7. Fan state is verified, not assumed (REF-REQ-125.4)

`g_last_fan_level` caches what this process WROTE. Nothing checked that the EC kept
it. Observed on the host: after an external `echo level auto` the daemon went on
believing it held full speed and did not correct it until the temperature crossed a
level boundary - the fan sat at the EC's quiet curve (~3.5k RPM) while the daemon
reported maximum cooling.

While the curve asks for full speed - the case where losing the state actually
matters - `apply_fan_for_temp()` now reads `/proc/acpi/ibm/fan` back and
re-asserts the write when the state no longer matches. The comparison maps
`disengaged`/`full-speed` to `FAN_LEVEL_FULL_SPEED`; a naive integer comparison
would conclude control was lost on every cycle and rewrite the fan forever. `auto`
and unknown words never match, which is exactly the state that must be corrected.

Cost: one ~100-byte procfs read per cycle, and only while the full-speed step is
in force. The write itself still happens only on a change or on a detected loss.

### 7.1 Every fan state measured on this host (Tctl 70 C)

| State | RPM |
| :--- | ---: |
| `auto` (EC control) | 3490 |
| `level 4` | 3610 |
| `pwm1=128` (hwmon) | 3831 |
| `level 5` | 4332 |
| `level 6` | 4789 |
| `level 7` | 4780 |
| `pwm1=255` (100 % duty) | 4771 |
| **`full-speed` / `disengaged`** | **5340-5357** |

`full-speed` is the maximum reachable through the OS on this host. 100 % PWM duty
is 570 RPM BELOW it, so "write 100 %" is not the answer either. A 5701 RPM reading
was observed once earlier in the session; it could not be reproduced by any state
above, and the EC's own `auto` mode is far lower, so it is recorded as
unexplained rather than assumed reachable.
