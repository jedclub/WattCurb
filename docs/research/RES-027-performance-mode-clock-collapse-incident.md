# REF-RES-027: Performance-Mode Clock Collapse Incident (399 MHz)

**Date**: 2026-09-22
**Severity**: Critical - the daemon made the machine ~9x slower than running no
power manager at all, while displaying "Performance".
**Status**: Cause **NOT yet identified**. Daemon left stopped.

---

## 1. Summary

With `wattcurb.service` running in `Performance` mode on battery, every CPU was
pinned at **399 MHz** and the CPU package drew **2.11 W under a saturating
16-thread load** - less than the 7.71 W the same machine drew *idle* minutes
earlier. Stopping the daemon restored **3,793 MHz** within 4 seconds.

The failure is the exact inverse of the profile's stated intent. The user
observed it independently ("또 갑자기 최대 hz 가 399 로 돌아가고 있어").

## 2. Platform

| | |
| :--- | :--- |
| Machine | ThinkPad L15 Gen 1 (20U7S01000) |
| CPU | AMD Ryzen 7 PRO 4750U, Zen2, 8C/16T, base 1.7 GHz, max boost 4.1 GHz |
| Kernel | 7.2.5-1-cachyos |
| cpufreq driver | `acpi-cpufreq` (`amd_pstate` not loaded, not on cmdline) |
| ACPI P-states | 1700 / 1600 / 1400 MHz only |
| Power source | **Battery**, 45-50%, discharging, throughout |
| Deployed build | `output/` staged 09-22 00:39, installed 00:40, md5 `1865ebb0c97aacf80661a109fb2fa2a2` |

## 3. Measurement Log

All clock figures are the maximum `cpu MHz` across the 16 threads in
`/proc/cpuinfo` unless stated; `scaling_cur_freq` agreed with it to within 1%
wherever both were sampled.

| # | Condition | Clock | Notes |
| :-- | :--- | ---: | :--- |
| 1 | Daemon **inactive**, `platform_profile=balanced`, 16-thread spin load | **2,495 MHz** | k10temp 49 C - not thermal |
| 2 | Daemon **inactive**, idle | 3,605 MHz | single-core boost working |
| 3 | Daemon **active**, Performance, 16-thread spin load | **399 MHz** | `scaling_cur_freq` cpu0 = 399,211 / 399,225 / 399,231 over three samples; loadavg 14.35 |
| 4 | Daemon **active**, Performance, same load | package **2.11 W** | vs 7.71 W measured idle before the restart |
| 5 | Daemon **stopped** | **3,793 MHz** | recovered within 4 s |

### Knob state while collapsed (measurement 3)

```
scaling_governor   = performance
platform_profile   = performance        (baseline was balanced)
boost              = 1
cpb                = 1
scaling_max_freq   = 1700000  (all 16)  <- driver maximum, no cap applied
scaling_min_freq   = 1400000  (all 16)  <- driver minimum, no cap applied
cpu online         = 0-15
smt/control        = on
cgroup.freeze      = 0
k10temp            = 49-51 C
```

Nothing in the cpufreq layer explains 399 MHz: the requested P-state range was
the driver's full 1.4-1.7 GHz, boost was enabled, no core was offline, and the
package was cool. The clock was being forced **below the lowest ACPI P-state**,
which means the constraint is at the SMU / EC / MSR level, not in `cpufreq`.

## 4. Ruled Out

- **Thermal throttling** - 49-51 C throughout, fan at 4,321 RPM.
- **Frequency caps written by WattCurb** - `scaling_max_freq` and
  `scaling_min_freq` were at the driver's own limits during the collapse.
- **Core offlining / SMT** - all 16 logical CPUs online, `smt/control = on`.
- **cgroup freeze** - `cgroup.freeze = 0`; the load processes were in state `S`
  and `ps` showed them at 83-92% CPU.

## 5. Hypothesis Tested and **Rejected**

`platform_profile=performance` (ThinkPad DYTC) was the leading suspect, since
the daemon writes it in `apply_power_profile()` and DYTC drives STAPM/PPT on
ThinkPads. Isolated test with the daemon stopped, on battery:

```
[baseline]      platform=balanced     clock=2495 MHz
[performance]   platform=performance  clock=2495 MHz
[performance+3s]                      clock=2495 MHz
[reverted]      platform=balanced     clock=2495 MHz
```

No effect observed. **Caveat**: the metric did not move by a single MHz across
the whole test, so the test's sensitivity is unproven - a stuck reading and a
null result are indistinguishable here. This hypothesis is rejected only
provisionally and the measurement method must be validated before relying on it.

## 6. Not Yet Established

**Which write causes the collapse is unknown.** `apply_power_profile(Performance)`
performs at least twelve actuations
(`src/policy/mitigation_engine.cpp:1126` onward), any of which could be
responsible, including the six added under
[`REF-REQ-092`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-092-ultimate-performance-unleash-actuation.md)
in commit `4c0751a`. It is also not established whether this is a regression
from that commit or pre-existing behaviour that was simply never observed,
because the daemon had not been running - `wattcurb.service` was `inactive`
before this deployment, so Performance mode had not actuated on this host in
that window.

Note also that in Performance mode the engine applied `nice +15` to ordinary
background processes (19 processes at nice 15, 2 at nice 10 were observed),
which contradicts REQ-092.2's "the periodic mitigation engine must bypass all
throttling actions" in Performance. Whether this is related to the clock
collapse is unknown; it is a separate defect either way.

## 7. Provenance

The `set_platform_profile("performance")` call and the surrounding
`apply_power_profile()` switch were introduced in commit `178e151`
(2026-09-17, *"feat(policy): implement dual-domain state journaling and profile
actuation"*). The six additional Performance actuations landed in `4c0751a`
(2026-09-22). Both were written by Claude in this repository's development
sessions; the git author field reflects the repository owner's identity, not
authorship of the logic.

The defect reached the machine because the actuation path has **no hardware
verification anywhere**: `REF-TEST-056` runs under
`MitigationEngine::set_actuation_sandbox(true)`, so it asserts that the engine
*decided* to actuate and that its rollback bookkeeping balances. It never reads
back a clock, a power figure, or any other physical consequence. A profile that
makes the machine nine times slower passes that gate cleanly. This was stated as
a limitation in the `4c0751a` commit body, and it is now a demonstrated failure
rather than a theoretical one.

## 8. Required Next Steps

1. **Validate the measurement method** before any further bisection: establish a
   clock reading that demonstrably tracks real work (e.g. fixed-work timing -
   wall time for a known instruction count - rather than a sysfs average).
2. **Bisect the actuation list** by applying each write from
   `apply_power_profile(Performance)` individually with the daemon stopped, on
   battery, measuring after each.
3. **Add an Oracle Gate that reads back a physical consequence**: no profile may
   leave measured throughput below the profile-less baseline. This is the gate
   whose absence let the defect ship.
4. **Re-examine the nice +15 application in Performance** against REQ-092.2.
5. Only then restart the daemon on this host.
