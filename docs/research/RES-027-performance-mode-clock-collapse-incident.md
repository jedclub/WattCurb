# REF-RES-027: Performance-Mode Clock Collapse Incident (399 MHz)

**Date**: 2026-09-22
**Severity**: Critical - the daemon made the machine ~9x slower than running no
power manager at all, while displaying "Performance".
**Status**: Collapse **confirmed real** by fixed-work timing. Single dominant
cause still **NOT identified** - the individually-tested knobs account for only a
fraction of it. Daemon left stopped and `disabled`.

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

## 5. Confirmation: The Collapse Is Real, Not a Reporting Artifact

`scaling_cur_freq` and `/proc/cpuinfo` both derive from aperf/mperf averaged
since the previous read, and the daemon polls those nodes every 3 s, so 399 MHz
could in principle have been an artifact of the daemon's own sampling. It is
not. A fixed-work probe - wall-clock time for a deterministic 8,000,000-iteration
integer loop, independent of every frequency interface - gives:

| Condition | Run 1 | Run 2 | Run 3 |
| :--- | ---: | ---: | ---: |
| Daemon **stopped** | 1.802 s | 1.754 s | 1.546 s |
| Daemon **active**, Performance | **17.737 s** | **16.267 s** | **19.508 s** |
| Daemon stopped again | 1.632 s | - | - |

**The machine performs roughly 10x less work per second with the daemon running
in Performance mode than with no power manager running at all.**

## 6. Knob-Level Bisection

Each actuation applied alone, daemon stopped, on battery, same probe:

| Knob | Wall time | Factor |
| :--- | ---: | ---: |
| baseline, nothing applied | 1.424 s | 1.00x |
| `platform_profile=performance` | 1.815 s | 1.27x |
| **`/dev/cpu_dma_latency` held at 0** | **2.674 s** | **1.88x** |
| GPU `power_dpm_force_performance_level=high` + `pp_power_profile_mode=1` | 1.470 s | 1.03x |
| `sched_migration_cost_ns=5000000` | - | **node does not exist** |
| *(all of them, via the daemon)* | *17.7 s* | *12.4x* |

### 6.1 `/dev/cpu_dma_latency = 0` is the largest single contributor, and its premise is wrong

REQ-092.1 holds `/dev/cpu_dma_latency` at 0 us to "eliminate CPU idle transition
latency and pipeline wakeup stalls". On this silicon it makes the machine
**1.9x slower**. Zen's opportunistic boost is governed by accumulated power and
thermal budget; pinning every core in C0 spends that budget continuously, leaving
the boost algorithm less headroom, not more. The requirement asserts a benefit
that was never measured - REF-TEST-056 only checks that the descriptor is held.

### 6.2 REQ-092.6 is a silent no-op on this kernel

`/proc/sys/kernel/sched_migration_cost_ns` does not exist on `7.2.5-1-cachyos`
(BORE/EEVDF scheduler). `set_sched_migration_cost()` writes to a path that cannot
be opened and discards the failure, so the daemon records a baseline and a
`*_modified` flag for a knob it never changed.

### 6.3 `platform_profile=performance` is a real but minor contributor

An earlier isolation attempt reported no effect, but the metric used
(`/proc/cpuinfo` MHz) did not move by a single MHz during that test and was
unreliable. Re-measured with the fixed-work probe it costs 1.27x. Writing DYTC
`performance` while on **battery** does make this ThinkPad slower, but it is not
the main cause.

### 6.4 The bisection does not add up

The measured knobs compose to roughly 2.5x. The daemon produces 12.4x. The
residual is unexplained and is the open question. Untested candidates:

- ~~the mitigation ladder acting on the measured process itself - `nice +15` on
  19 processes~~ **RETRACTED, see section 9**: that was `ananicy-cpp`, not
  WattCurb;
- CPU affinity masking (`HeadroomMask` / cluster dispersion) confining work to a
  subset of cores;
- `SCHED_IDLE` demotion;
- an actuation outside the tested set - PCIe ASPM, panel power savings, NVMe
  APST, Wi-Fi power save, SMT control.

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

1. ~~Validate the measurement method~~ - done, section 5.
2. ~~Bisect the actuation list~~ - partially done, section 6. **Continue** with
   the untested actuations in 6.4 and with the mitigation ladder itself.
3. **Drop or gate `/dev/cpu_dma_latency = 0`.** It costs 1.9x on this silicon.
   If a latency clamp is wanted at all it must be justified by a measurement, not
   by the assumption that shallower idle means faster.
4. **Make failed actuations observable.** REQ-092.6 wrote to a non-existent path
   and reported success. Any actuator whose write fails must not set its
   `*_modified` flag, and the failure must reach the event log.
5. **Gate `platform_profile=performance` on AC.** It is a measured regression on
   battery.
6. **Add an Oracle Gate that reads back a physical consequence**: no profile may
   leave measured throughput below the profile-less baseline. This is the gate
   whose absence let the defect ship, and no amount of sandboxed assertion
   replaces it.
7. **Re-examine the nice +15 application in Performance** against REQ-092.2.
8. Only then re-enable and restart the daemon on this host.

---

## 9. Corrections to This Document

Two claims made earlier in this investigation were wrong and are retracted here
rather than quietly edited away.

### 9.1 The `nice +15` was not WattCurb

Section 6.4 originally attributed `nice +15` on 19 processes to WattCurb's
mitigation ladder running in Performance mode, and called it a REQ-092.2
violation. A control measurement with **wattcurb.service stopped** showed spin
processes still receiving `nice 10`, `nice 15`, `nice -4` and `SCHED_BATCH`:

```
pid=2278844 nice=10 cls=B
pid=2278850 nice=15 cls=B
pid=2278852 nice=-4 cls=B
```

The source is `ananicy-cpp.service`, which is active on this host and applies
per-process nice, scheduling class and ionice by its own rules. WattCurb's
`actuate_anti_starvation_cap()` does return without acting in Performance mode,
as REQ-104 requires. The attribution was made without a control.

### 9.2 The environment has three power managers, which invalidated earlier measurements

Also running throughout, and not accounted for in sections 3-6:

| Service | What it touches |
| :--- | :--- |
| `power-profiles-daemon` | set to `balanced`, drives `/sys/firmware/acpi/platform_profile` - the same node WattCurb writes |
| `ananicy-cpp` | per-process `nice`, scheduling class, ionice |
| `upower` | battery state only; no actuation |

`power-profiles-daemon` and WattCurb write the same knob with different
intentions. Re-measured with both competitors stopped:

| Condition | Fixed-work (3 runs) |
| :--- | :--- |
| as-is, wattcurb stopped | 1.386 / 1.375 / 1.351 s |
| competitors stopped, wattcurb stopped | 1.397 / 1.575 / 1.645 s |
| **competitors stopped, wattcurb Performance** | **2.042 / 1.920 / 2.085 s** |
| competitors restored, wattcurb stopped | 1.395 / 1.478 / 1.461 s |

WattCurb's own steady-state cost in Performance is therefore about **1.4x**, not
12x. **The 12.4x collapse was not reproduced by a fresh 14-second daemon run.**
It is real - it was measured three times at 16-19 s against a 1.5 s baseline -
but it requires something a short run does not produce: most likely accumulated
mitigation state from the ~20 minutes the daemon had been running, or contention
with `power-profiles-daemon` over `platform_profile`. **The 12x case remains
unexplained and unreproduced.**

### 9.3 `sched_migration_cost_ns`

Section 6.2 stated the engine "records a baseline and sets its `*_modified`
flag" for this absent knob. The flag part is wrong: `set_sched_migration_cost()`
sets the flag only inside the `fd >= 0` branch, so a failed open leaves it clear
and `restore_hardware_baseline()` will not write it back. The knob is still a
silent no-op on this kernel, and the failure is still discarded without a log
entry.
