# REF-REQ-127: Balanced-Profile Fan Curve - 70 C as the 100% Anchor

- **Status**: Implemented
- **Ref ID**: `REF-REQ-127`
- **Date**: 2026-09-23
- **Related**: [`REF-REQ-114`](REQ-114-thinkpad-fan-thermal-assist-curve.md),
  [`REF-REQ-118`](REQ-118-all-profile-fan-curve-and-ultra-cold-stop.md),
  [`REF-REQ-124`](REQ-124-fan-curve-60c-full-speed.md),
  [`REF-REQ-125`](REQ-125-fan-full-speed-keyword.md),
  [`REF-REQ-123`](REQ-123-ec-smu-power-cap-diagnosis.md)
- **Tests**: [`REF-TEST-073`](../../tests/test_units.cpp) section 4b
- **Category**: Thermal Management, ThinkPad EC Fan Curve, Profile Policy

## 1. Request

Owner, 2026-09-23: *"밸런스 모드에서는 70 도를 100 % 기준으로 잡고 70 도 부터 MAX 팬
작동으로 변경 해줘"* - in Balanced mode, take 70 C as the 100% reference and run the
fan at maximum from 70 C.

## 2. What changed

The full-speed anchor became profile-specific. `fan_level_for_temp()` now takes the
anchor as a parameter and `fan_level_for_temp_in_profile()` selects it:

| Profile | 100% anchor (`fan_full_temp_for_profile`) | Source |
| :--- | ---: | :--- |
| **Balanced** | **70 C** (`FAN_BALANCED_FULL_TEMP_C`) | this requirement |
| Performance | 60 C (`FAN_FULL_TEMP_C`) | REF-REQ-124, unchanged |
| PowerSaver | 60 C | REF-REQ-124, unchanged |
| UltraEndurance | 60 C (plus the <= 45 C cold stop) | REF-REQ-118.3, unchanged |

The ramp's lower anchor is unchanged in every profile: 35 C = 20%
(`FAN_CURVE_MIN_TEMP_C`, `FAN_CURVE_MIN_FRACTION`). Only the upper anchor moved, so
Balanced's ramp is now gentler across its whole span.

### 2.1 Resulting levels

| Temperature | Performance / PowerSaver / UltraEndurance | **Balanced** |
| ---: | :--- | :--- |
| <= 35 C | 1 (20%) | 1 (20%) |
| 45 C | 3 | 3 |
| 52.5 C | 5 | 4 |
| 60 C | **`full-speed`** | 5 |
| 65 C | `full-speed` | 6 |
| 69.9 C | `full-speed` | 6 |
| **>= 70 C** | `full-speed` | **`full-speed`** |

The top step remains the `full-speed` keyword, not numeric 7 (REF-REQ-125), and
numeric 7 is still never commanded in any profile.

## 3. The trade-off, stated plainly

This is not a free change and the requirement is worth recording as a deliberate
choice:

- **Gained**: Balanced is quieter and spends less on the fan between 60 C and
  70 C. The fan is a real load - the 6-second observation in
  `docs/research/PGO_PMU_REPORT.md` measured it at **3.59 W (18.0% of system
  draw)** at 5365 RPM - so holding level 5-6 instead of full speed in that band
  is a measurable saving.
- **Given up**: 70 C is the firmware's Tctl ceiling (REF-REQ-123 section 2.4).
  Balanced now reaches full fan exactly when the part begins to throttle rather
  than 10 C before it, so under sustained all-core load Balanced will sit at the
  ceiling sooner and hold a **lower clock than Performance** at the same
  temperature. That is the intended shape of the profile: Balanced trades
  sustained throughput for noise and fan power, Performance does the opposite.
- **Unchanged**: the thermal-safety floor. Full speed is still commanded at or
  above the anchor in every profile, the UltraEndurance cold stop is untouched,
  and the REF-REQ-125.4 verify-and-re-assert behaviour (which corrects an EC that
  has taken the fan back) applies to Balanced exactly as before.

## 4. Implementation

- `src/policy/mitigation_engine.hpp`: `FAN_BALANCED_FULL_TEMP_C = 70.0`,
  `fan_full_temp_for_profile(mode)`, and a two-argument
  `fan_level_for_temp(temp, full_temp_c)`. The one-argument overload is kept as
  the common curve so existing callers and tests read unchanged.
- `src/policy/mitigation_engine.cpp`: the curve logic moved into the two-argument
  form; a degenerate anchor (`<= FAN_CURVE_MIN_TEMP_C`) falls back to the common
  curve instead of dividing by zero.
- No new interface, no new write target, no change to *when* the fan is written -
  only to the level chosen in one profile.

## 5. Verification

| Check | Result |
| :--- | :--- |
| `scripts/harness.py build` | OK |
| `scripts/harness.py test` | 81/81 pass |
| REF-TEST-073 section 4b | Balanced: anchor == 70, >= 70 C full speed, 69.9 C == 6, 65 C == 6, 60 C == 5, 52.5 C == 4, 35 C == 1, monotonic 1..70, numeric 7 never commanded; the other three profiles still assert the 60 C anchor |
| Live, post-install | see section 6 |

The unit tests prove the mapping. They do **not** prove that the EC accepts the
level or that the resulting RPM differs - that needs the hardware and is measured
on the host.

## 6. Deployment and live measurement

Deployed as part of the REF-REQ-127 PGO release; see the release commit for the
binary sizes, hash/mtime verification and the live fan state read from
`/proc/acpi/ibm/fan` while the daemon was in Balanced.
