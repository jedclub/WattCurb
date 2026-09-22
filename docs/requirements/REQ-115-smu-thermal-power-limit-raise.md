# [REF-REQ-115] SMU Thermal & Power Limit Raise (RyzenAdj)

**Status**: Implemented · **Date**: 2026-09-22
**Related**: [`REF-REQ-092`](REQ-092-ultimate-performance-unleash-actuation.md),
[`REF-REQ-114`](REQ-114-thinkpad-fan-thermal-assist-curve.md),
[`REF-RES-030`](../research/RES-030-thinkpad-ec-fan-and-ryzenadj-smu-limits.md),
[`REF-TEST-073`](#5-verification--oracle-gate-standards-ref-test-073)

## 1. Why

The APU's power/thermal limits (STAPM, PPT fast/slow, `Tctl`) are owned by the
platform firmware, which on a thin business laptop ships them deliberately
conservative so the machine stays cool and quiet on battery. In the profiles the
user chooses for throughput (`Performance`, `Balanced`) that conservatism is
counter-productive: the part throttles on a temperature ceiling below the
silicon's own limit, so fixed work takes longer and swallows more energy than it
needs to.

RyzenAdj writes those SMU limits directly from userspace. It is a small,
optional, external tool - **not a build or runtime dependency** - discovered on
the system path. When it is present and its SMU table is readable, `Performance`
and `Balanced` raise `Tctl` to **85 °C** and lift the package power ceilings; the
thermal-assist fan curve of
[`REF-REQ-114`](REQ-114-thinkpad-fan-thermal-assist-curve.md) supplies the extra
cooling. When it is absent, every call is a no-op and the machine behaves exactly
as before.

> **Deployment caveat (measured on the reference host, 2026-09-22).** RyzenAdj is
> installed at `~/.local/bin/ryzenadj`, which is **not** on the discovery path
> below (`/usr/local/bin`, `/usr/bin`). `ryzenadj_available()` therefore returns
> false there and the SMU raise does not actuate. `ryzenadj -i` also requires
> root; the daemon has it, a test user does not. This was **not** changed by this
> requirement - state it rather than assume it.

## 2. Requirements

- **REQ-115.1 (Scope)** The raise shall be applied in `Performance` and
  `Balanced` only. `PowerSaver` and `UltraEndurance` shall keep - and restore -
  the firmware baseline; a saving profile must not raise a thermal or power
  ceiling.
- **REQ-115.2 (Values)** The raised limits shall be:
  `Tctl = 85 °C`, `STAPM = 25 W`, `PPT fast = 35 W`, `PPT slow = 30 W`,
  and `APU slow = 30 W`.
- **REQ-115.3 (Optional tool)** The tool shall be discovered by searching
  `/usr/local/bin/ryzenadj` then `/usr/bin/ryzenadj` for an executable. If neither
  exists, `ryzenadj_available()` is false and `apply_smu_performance_limits()`
  returns without invoking anything.
- **REQ-115.4 (Baseline capture, units)** The limits in force before the first
  raise shall be captured once from `ryzenadj -i`: `STAPM LIMIT`, `PPT LIMIT
  FAST`, `PPT LIMIT SLOW`, `APU SLOW LIMIT` (when present) in **watts** converted
  to milliwatts, and `THM LIMIT CORE` in **Celsius**, which shall **not** be
  scaled by 1000. A zero captured value means "not captured", never "write zero".
- **REQ-115.5 (Restore)** `restore_smu_limits()` shall write the captured
  baseline back, including only the fields that were actually captured. It is
  called from `release_performance_unleash()` on every transition out of the
  raised profiles and from `restore_hardware_baseline()` on exit. If the tool
  disappeared after the raise, the modified flag shall stay set so a later cycle
  can still restore.
- **REQ-115.6 (Refuse to raise what cannot be restored)** `apply_smu_performance_limits()`
  shall refuse to raise the limits unless the bootstrap capture read a real
  `STAPM` and `Tctl` value. A machine whose SMU table was unreadable shall be
  left with its firmware limits rather than with a raised ceiling and nothing to
  restore - the orphaned-actuation class `REF-REQ-112` had to repair for cpufreq.
- **REQ-115.7 (Sandbox)** Under the actuation sandbox (`REF-REQ-092`/`REF-ARCH-069`)
  no command may run; the raiser returns false and sets no modified flag.
- **REQ-115.8 (No brittle shell)** The command line shall be built from integer
  constants and an absolute tool path. The tool path is not configurable and not
  derived from user input.

## 3. Mechanism & Kernel Interface

| Item | Value |
| :--- | :--- |
| Tool | `ryzenadj` (optional, external) |
| Discovery | `/usr/local/bin/ryzenadj`, `/usr/bin/ryzenadj` |
| Read baseline | `ryzenadj -i` (STAPM / PPT FAST / PPT SLOW / APU SLOW / THM LIMIT CORE) |
| Apply | `ryzenadj --tctl-temp=85 --stapm-limit=25000 --fast-limit=35000 --slow-limit=30000 --apu-slow-limit=30000` |
| Units | milli-watts for `--*-limit`, degrees Celsius for `--tctl-temp` |

Implementation: `MitigationEngine::find_ryzenadj()`, `ryzenadj_available()`,
`apply_smu_performance_limits()`, `restore_smu_limits()` in
`src/policy/mitigation_engine.cpp`; baseline capture in
`capture_hardware_baseline()`. The raise is wired into `apply_power_profile()`
for `Performance` and `Balanced`, and released by `release_performance_unleash()`.

> `apply_smu_performance_limits()` runs from `apply_power_profile()`, which the
> daemon calls on profile **transitions** - unlike the fan curve of REF-REQ-114,
> which must track temperature and is therefore driven every cycle from
> `FeatureManager::evaluate_and_actuate()`.

## 4. Blast Radius & Failure Modes

- **Thermal and power ceiling raised.** This genuinely allows the APU to draw more
  power and run hotter than the firmware chose. The fan assist of REF-REQ-114 is
  the compensating mechanism, but if the fan cannot be controlled (no
  `fan_control`) the raised `Tctl` is still written. On a machine with a blocked
  or failing fan this could hold higher skin temperatures for longer. The limits
  are bounded (85 °C is below the silicon's own limit) and scoped to the two
  throughput profiles.
- **External, root-level tool.** RyzenAdj reaches the SMU through `/dev/mem` or
  the `ryzen_smu` module and requires root; the daemon already runs as root. The
  path is fixed and not attacker-influenced, matching
  [`REF-REQ-093`](REQ-093-daemon-privilege-boundary-hardening.md).
- **Restore depends on the tool and the captured values.** The tool must still be
  present and the baseline must have been read; REQ-115.6 makes the raise
  conditional on the latter so the two cannot diverge.
- **Not measured here.** The power/throughput effect of the raised SMU limits on
  the reference host was not measured, because the tool is not on a discovery
  path (§1 caveat). Do not read the constants as validated behaviour.

## 5. Verification & Oracle Gate Standards (REF-TEST-073)

`tests/test_units.cpp::test_thinkpad_fan_thermal_assist_and_smu_limits()` proves,
without touching the SMU:

1. the constants are coherent (`Tctl == 85`, `STAPM <= slow <= fast`);
2. `apply_smu_performance_limits()` under the sandbox returns false and leaves
   `smu_limits_modified` unset.

The test asserts the **shape and sandbox discipline** of the raise, not its
thermal effect. Whether the host actually gains throughput at the raised limits
is unmeasured (§4).
