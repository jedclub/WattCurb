# [REF-REQ-123] CPU Pinned at 600 MHz in Performance Mode - EC/SMU Cap Diagnosis

**Status**: Implemented · **Date**: 2026-09-22
**Related**: [`REF-REQ-112`](REQ-112-boost-restoration-and-non-halting-memory-guard.md),
[`REF-REQ-115`](REQ-115-smu-thermal-power-limit-raise.md),
[`REF-REQ-114`](REQ-114-thinkpad-fan-thermal-assist-curve.md),
[`REF-REQ-118`](REQ-118-all-profile-fan-curve-and-ultra-cold-stop.md),
[`REF-TEST-079`](../../tests/test_units.cpp)

## 1. Why

Reported: *"현재 성능 모드인데 600 MHz에 시스템이 고정되어있어"* - Performance mode
selected, and the whole machine sat at ~600 MHz.

Measured on the host (ThinkPad, Ryzen 7 PRO 4750U, `acpi-cpufreq`):

| Knob | Value | Verdict |
| :--- | :--- | :--- |
| `scaling_governor` | `performance` | correct |
| `cpufreq/boost` | `1` | correct |
| `scaling_max_freq` | `1700000` (= `cpuinfo_max_freq`) | at ceiling |
| `/sys/firmware/acpi/platform_profile` | `performance` | correct |
| `scaling_min_freq` | `1400000` | floor set |
| **actual frequency under 8-thread load** | **550-600 MHz** | below the floor |
| `scaling_driver` | `acpi-cpufreq` (no CPPC, no `amd_pstate`) | - |

Every knob the OS owns was already correct, and the clock was *below*
`scaling_min_freq`. The limit was therefore not in cpufreq at all.

## 2. Root cause

`ryzenadj -i` (run as root; the tool needs `/dev/mem`) showed the real limits:

| SMU limit | Value | WattCurb's Performance target |
| :--- | ---: | ---: |
| **STAPM LIMIT** (sustained) | **6.000 W** | 25.000 W |
| PPT LIMIT FAST | 30.000 W | 35.000 W |
| PPT LIMIT SLOW | 12.000 W | 30.000 W |
| PPT LIMIT APU | 25.000 W | - |
| **THM LIMIT CORE** (Tctl) | **70.000 C** | 85 C |

The EC's STAPM is **not a constant**: the first reading above was 6.000 W, and at
the daemon's next bootstrap the capture read 10.000 W (verified by switching to a
saving profile, which restores the captured baseline verbatim, and reading back
10.000 W). The EC moves it with its own power state. What matters for this defect
is not the exact figure but that it sits far below the 25 W Performance target and
that nothing in the OS was reaching it.

### 2.1 The decisive experiment

Same 8-thread load, only STAPM changed:

| STAPM | Highest core observed | Tctl | zone0 |
| ---: | ---: | ---: | ---: |
| 6 W (EC default) | 550-600 MHz | ~70 C | 56 C |
| 25 W (`ryzenadj --stapm-limit=25000`) | **2.60-2.77 GHz** | ~70 C | 56-57 C |

A 4.6x frequency change at an unchanged temperature proves the sustained power
limit, not temperature and not the governor, was binding.

### 2.2 Why WattCurb never lifted it

`find_ryzenadj()` searches only `/usr/local/bin` and `/usr/bin`. The tool was at
`/home/jedclub/.local/bin/ryzenadj`, so `apply_smu_performance_limits()` returned
early on every cycle. **REF-REQ-115 has never executed on this host.** The daemon
was, by design, not allowed to exec a user-writable path (the trust argument of
REF-REQ-111) - the path was simply never made available in a root-owned location.

### 2.3 The watchdog fired and could not help

REF-REQ-112.10's detector worked and logged four times:

```
[ALERT:WARN] CPU frequency starved under load: max core 621-682 MHz vs ceiling 1700 MHz,
             load1=11.67-17.67, profile=Performance; re-asserted ceiling and platform_profile
```

The remedy re-asserted `scaling_max_freq` and `platform_profile` - both of which
were already correct. The log line read like a successful repair while nothing
changed. That is the second defect: a repair that cannot reach the binding limit
must not describe itself as a repair.

### 2.4 The Tctl ceiling is EC-owned

`ryzenadj --tctl-temp=85` reports success, but the value reverts immediately:

| Time after write | `THM LIMIT CORE` |
| :--- | ---: |
| +0 s | 70.000 |
| +1 s | 70.000 |
| +2 s | 70.000 |
| +4 s | 70.000 |
| +8 s | 70.000 |

The EC re-imposes 70 C. Under load `THM VALUE CORE` sits at 70.0-71.0 C, i.e. the
die is thermally saturated at that ceiling. `dytc_lapmode` (ThinkPad Dynamic
Thermal Control) reads `1` and is read-only (`-r--r--r--`), so the EC's lap-mode
thermal table - which is where both the 6 W STAPM and the 70 C Tctl come from -
cannot be switched off from the OS.

## 3. The 4.1 GHz boost question

Asked: does Performance mode reach the 4.1 GHz boost clock?

**Measured: no.** Highest effective frequency observed, computed from
`perf stat -e cycles` over a pinned single-thread burst (`cycles / task-clock`):

| Load | Effective frequency |
| :--- | ---: |
| 1 thread, 0.6 s bursts | 2.28-2.56 GHz |
| 1 thread, 1.5 s bursts after 60 s cooldown | 2.16-2.22 GHz |
| 8 threads, sustained | 2.60-2.77 GHz |

Two limits explain it, both EC-owned:

1. **Tctl 70 C** - the die is already at the ceiling, so the SMU's boost
   algorithm has no thermal headroom to spend on the top boost states.
2. **STAPM** - 6 W by default, and even at 25 W the package settles around
   15 W of measured draw, which is what 8 cores at ~2.7 GHz costs.

`acpi-cpufreq` exposes no CPPC (`/sys/devices/system/cpu/cpu0/acpi_cppc` absent,
`amd_pstate` not loaded), so the OS cannot request boost states directly; boost is
entirely the SMU's autonomous decision within those two limits.

**Not measured:** a true single-core 4.1 GHz burst on an *idle* host. Every
measurement in this report was taken while an unrelated project held the machine
at load 8-18, which keeps the package hot and removes the headroom a 4.1 GHz
single-core boost needs. The claim "4.1 GHz works when the machine is idle" is
neither confirmed nor refuted here. What is established is that with a hot package
and the EC's 70 C ceiling it is not reachable, and that the previous 600 MHz state
was the STAPM limit.

## 4. Requirements (regression prevention)

- **REQ-123.1 (Make the lever reachable)** `install.sh` shall install `ryzenadj`
  into `/usr/local/bin` (root-owned, 0755) when a copy exists in the user's
  `~/.local/bin`, and shall print an explicit warning when no copy exists.
  Copying - never referencing in place - keeps the root daemon from executing a
  user-writable file (REF-REQ-111).
- **REQ-123.2 (Fail loudly at bootstrap)** When the tool is absent,
  `capture_hardware_baseline()` shall log one `ALERT:WARN` naming the consequence
  (SMU limits can neither be raised nor restored) and the measured symptom
  (6 W STAPM / 70 C Tctl pinning the CPU near 600 MHz).
- **REQ-123.3 (The repair must reach the binding limit)** The frequency-starvation
  remedy shall re-apply the SMU performance limits for Performance and Balanced,
  in addition to re-asserting the ceiling and the platform profile.
- **REQ-123.4 (Honest repair logging)** The starvation alert shall state that the
  ceiling and profile were already at target, what was done about the SMU (applied
  / refused / tool absent / saving profile), and the captured baseline STAPM.
- **REQ-123.5 (Recovery is provable)** When starvation clears, the daemon shall
  log one `INFO` with the achieved max core clock, so a reader can tell whether a
  repair worked instead of only ever seeing the fault.
- **REQ-123.6 (Guard the guardrails)** The test suite shall assert that the
  actuation sandbox suppresses the SMU write, that a restore with nothing modified
  is a clean no-op, and that the limit set is internally consistent
  (REF-TEST-079).

## 5. Blast Radius & Failure Modes

- **Raising STAPM to 25 W overrides a firmware thermal decision.** That decision
  (lap mode) is made by the EC from a table, not from a surface sensor, and it
  cannot be inspected or cleared from the OS. Measured cost on this host: 56-57 C
  die at 2.7 GHz all-core with the fan at full speed above 70 C (REF-REQ-118).
  Tctl protection is not disabled - the raise *targets* 85 C, and on this host the
  EC keeps it at 70 C regardless, so the effective thermal ceiling is unchanged.
- **On a host where the Tctl write does stick, the part may run to 85 C.** That is
  the documented REF-REQ-115 trade; the fan curve is the mitigation.
- **Installing a third-party binary into `/usr/local/bin`** makes it available to
  every user and to the root daemon. It is the same binary the user already
  downloaded; the change is its location and ownership.
- **A machine without `ryzenadj`** keeps its EC limits and now says so at
  bootstrap instead of silently under-performing.
- **The 600 MHz symptom can still be produced by the EC** if it re-imposes STAPM
  6 W after the daemon's raise (observed: it does not - 25 W held across a 20 s
  load test). The watchdog re-applies every time it trips, so a re-imposition is
  answered within 5 cycles.

## 6. Verification & Oracle Gate Standards (REF-TEST-079)

| Item | Evidence |
| :--- | :--- |
| Cause identified | STAPM 6 W -> 600 MHz, STAPM 25 W -> 2.7 GHz at equal temperature |
| Watchdog works | 4 `ALERT:WARN` starvation lines in `/var/log/wattcurb/audit.log` |
| Tool reachable after install | `/usr/local/bin/ryzenadj` root-owned, `ryzenadj_available()` true |
| Sandbox suppresses SMU writes | `test_smu_raise_is_guarded` (REF-TEST-079) |
| Suite | 80/80 pass (`scripts/harness.py test`) |

The end-to-end check - daemon captures the EC baseline, Performance raises STAPM,
and the clock recovers under load - is run on the host after each release because
it needs the real SMU. It is not part of the unit suite: a unit test that raised a
real power limit would change the machine it runs on and survive the test run.
