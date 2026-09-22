# [REF-REQ-114] ThinkPad Thermal-Assist Fan Curve

**Status**: Implemented · **Date**: 2026-09-22
**Related**: [`REF-REQ-092`](REQ-092-ultimate-performance-unleash-actuation.md),
[`REF-REQ-115`](REQ-115-smu-thermal-power-limit-raise.md),
[`REF-RES-022`](../research/RES-022-vram-wifi-fan-bus-hardware-power-isolation.md),
[`REF-TEST-073`](#5-verification--oracle-gate-standards-ref-test-073)

## 1. Why

The embedded controller (EC) owns the ThinkPad fan and, with
`thinkpad_acpi fan_control=1`, the kernel exposes a writable override at
`/proc/acpi/ibm/fan`. On the reference host (T14/P14s class, Renoir) the EC's
automatic curve is deliberately conservative: its tachometer tops out around
**4.3k RPM** while the fan itself is rated and observed at **~5.4k RPM**. The
aerodynamic shaft power of that last step is not free (it scales with the cube
of RPM, see [`REF-RES-022`](../research/RES-022-vram-wifi-fan-bus-hardware-power-isolation.md)
§2.3), but it is cheap compared to the alternative.

[`REF-REQ-115`](REQ-115-smu-thermal-power-limit-raise.md) raises the SMU core
thermal limit (`Tctl`) in Performance and Balanced. If the part then runs against
that raised ceiling with the EC still commanding only ~4.3k RPM, the machine
throttles at a lower clock than the hardware could sustain with the cooling
headroom it already has. The fan is therefore a **thermal assist for exactly the
two profiles that raise the SMU limit**, and nothing else.

## 2. Requirements

- **REQ-114.1 (Scope)** The curve shall be evaluated **only** while the effective
  profile is `Performance` or `Balanced` - the profiles in which
  [`REF-REQ-115`](REQ-115-smu-thermal-power-limit-raise.md) raises the SMU
  thermal limit. `PowerSaver` and `UltraEndurance` shall leave the fan to the EC's
  own curve; forcing a minimum fan speed there would spend battery for cooling the
  profile is deliberately not using.
- **REQ-114.2 (Curve)** The requested level shall be linear between
  **35 °C** (fraction **0.2**) and **70 °C** (full speed), mapped onto the
  `thinkpad_acpi` discrete steps `0..7`, clamped to `1..7`. At or above 70 °C the
  fan shall be pinned to `full-speed`. Below the floor the level is `1`, not `0`.
- **REQ-114.3 (No reading, no action)** A missing or non-positive CPU temperature
  reading shall produce no write and report `-1`; the EC keeps control.
- **REQ-114.4 (Write economy)** The curve shall be evaluated every observation
  cycle so it tracks temperature, but shall write to `/proc/acpi/ibm/fan` **only
  when the computed level changes**. A steady temperature must cost no sysfs
  write.
- **REQ-114.5 (Baseline capture & restore)** The level in force before the first
  override shall be captured once (including `auto`, `disengaged`, `full-speed`,
  or a numeric step) and written back by `restore_fan_level()` on every transition
  out of the raised-SMU profiles and from `restore_hardware_baseline()` on exit.
  Restore shall also clear the last-written-level cache so re-entering a
  raised-SMU profile re-applies the curve at the same temperature.
- **REQ-114.6 (Optional capability)** When `/proc/acpi/ibm/fan` is absent or
  `fan_control` is not enabled, the write is refused and the actuator is a no-op.
  WattCurb shall not fail, and shall not attempt to enable `fan_control` itself.
- **REQ-114.7 (Sandbox)** Under the actuation sandbox (`REF-REQ-092`/`REF-ARCH-069`)
  no fan write may reach the EC. This is what prevents the Oracle Gate from
  spinning the developer's fan.
- **REQ-114.8 (Production wiring)** The curve shall be driven from the
  **production** actuation entry point `FeatureManager::evaluate_and_actuate()`
  - the same path the daemon and the CLI use - not from
  `MitigationEngine::evaluate_and_actuate()`, which has no production caller.

## 3. Mechanism & Kernel Interface

| Item | Value |
| :--- | :--- |
| Node | `/proc/acpi/ibm/fan` (`thinkpad_acpi`) |
| Write form | `level <0-7\|auto\|disengaged\|full-speed>\n` |
| Read-back | `level:` line of the same node |
| Curve floor | 35 °C → level 1 (fraction 0.2) |
| Curve ceiling | 70 °C → `full-speed` (level 7) |
| Prerequisite | `thinkpad_acpi` module option `fan_control=1` (operator-set) |

Curve implementation: `MitigationEngine::fan_level_for_temp()`,
`apply_fan_for_temp()`, `set_fan_level()`, `restore_fan_level()` in
`src/policy/mitigation_engine.cpp`. Cycle driver:
`FeatureManager::evaluate_and_actuate()` in `src/policy/battery_feature.cpp`, in
the `Performance || Balanced` branch alongside the
[`REF-REQ-112`](REQ-112-boost-restoration-and-non-halting-memory-guard.md)
frequency watchdog.

## 4. Blast Radius & Failure Modes

- **Audible noise and fan power.** This is the point of the change: in
  Performance/Balanced the fan runs faster than the EC would choose. At the top
  step the thermal budget saved can exceed the ~2.45 W the fan spends
  ([`REF-RES-022`](../research/RES-022-vram-wifi-fan-bus-hardware-power-isolation.md)
  §2.3), but on a lightly loaded machine the curve still raises the floor to
  level 1 below 35 °C. The scoping of REQ-114.1 keeps that cost out of the saving
  profiles.
- **Fan is a shared, safety-relevant device.** An over-aggressive override could
  mask a thermal problem, and an incorrect `full-speed` pin would be audible and
  costly. The curve is monotonic and bounded to `1..7` (`full-speed` only at/above
  70 °C), and was not measured on this host under sustained load in the change
  that introduced it.
- **Restore can fail.** If `/proc/acpi/ibm/fan` becomes unwritable after an
  override, the captured baseline cannot be restored and `fan_level_modified`
  stays set so a later cycle still retries. On daemon exit the EC's own
  `fan_control` watchdog (if the operator set one) is the backstop.

## 5. Verification & Oracle Gate Standards (REF-TEST-073)

`tests/test_units.cpp::test_thinkpad_fan_thermal_assist_and_smu_limits()` proves,
without touching the machine:

1. the pure curve is monotonic, bounded to `1..7`, returns `-1` with no reading,
   and gives level `1` at 35 °C, `7` at 70 °C, `4` at 52.5 °C;
2. `set_fan_level()` under the sandbox returns false and leaves
   `fan_level_modified` unset; `restore_fan_level()` is a harmless no-op;
3. the curve is reached from `FeatureManager::evaluate_and_actuate()` in
   `Performance` (application counter increases) and **not** in `PowerSaver`
   (counter unchanged).

The counter exists so a test-only wiring is falsifiable, mirroring the lesson of
`REF-TEST-068`. The tests assert **that the actuation is reached and bounded**;
they do not measure that the host survives a sustained load with the fan
overridden - that remains an unmeasured risk noted in §4.
