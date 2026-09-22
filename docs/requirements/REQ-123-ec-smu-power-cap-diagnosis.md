# [REF-REQ-123] CPU Pinned at 600 MHz in Performance Mode - EC/SMU Cap Diagnosis

**Status**: Implemented · **Date**: 2026-09-22 (revised 2026-09-23: the 4.1 GHz conclusion in section 3 was corrected - boost works, the first measurements were taken under continuous external load)
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

### 2.4 The Tctl ceiling is EC-owned and is 70 C in every thermal mode

Asked directly: *"70 이상으로 올라 갈수 있게 최대 85 까지 올라가게끔 수정 요청 했는데
작동이 안된건가 뭔가 충돌이 발생했나?"* - is the 85 C raise broken, or is
something fighting it?

It is neither broken nor a conflict with software. Four measurements settle it:

**(a) The write reaches the SMU.** Writing a value *below* the ceiling sticks and
has the expected physical effect:

| Write | `THM LIMIT CORE` after | Under 8-thread load |
| :--- | ---: | :--- |
| `--tctl-temp=60` | 60.000 (stable at +0/+2/+5 s) | `THM VALUE CORE` 59.945 C, 9.87 W, **1.48 GHz** |
| `--tctl-temp=70` | 70.000 | throttles at 70 C |
| `--tctl-temp=83` | **70.000** | - |
| `--tctl-temp=85` | **70.000** | - |

So the option name, the field mapping and the mailbox path are all correct - the
CPU really does throttle at 60 C when told to. Only the *upward* direction is
refused.

**(b) The ceiling is 70 C in every platform profile.** Writing each profile and
reading the limit back:

| `platform_profile` | `THM LIMIT CORE` | STAPM LIMIT |
| :--- | ---: | ---: |
| `low-power` | 70.000 | 10.000 W |
| `balanced` | 70.000 | 12.000 W |
| `performance` | 70.000 | 6.000 W |

70 C is the firmware's cap, not an artefact of the selected thermal mode. (The
STAPM column also explains the 6 W: the EC's *performance* table is the one that
sets 6 W of sustained power - short boosts are allowed, sustained power is not.)

**(c) WattCurb is not the reverter.** With `wattcurb.service` **stopped**, writing
83 C still reverts to 70 C.

**(d) power-profiles-daemon is not the reverter either.** With
`power-profiles-daemon` **stopped**, writing 83 C still reverts to 70 C. The ACPI
thermal zone on this host is `iwlwifi_1` with no valid trip points, so the kernel
is not doing it.

The reverter is the EC. The OS may lower Tctl; it may not raise it past the
firmware ceiling. **The 85 C target of REF-REQ-115 is therefore unreachable on
this machine by any software means** - it would need a firmware/BIOS change (or
different hardware). What *is* reachable, and is what actually fixed the 600 MHz
state, is the STAPM raise: 6-10 W -> 25 W took the same load from 600 MHz to
2.2-2.8 GHz.

## 3. The 4.1 GHz boost question

Asked: does Performance mode reach the 4.1 GHz boost clock?

**Yes - boost works. The earlier "no" in this document was an artefact of the
measurement conditions and is corrected here.**

| Evidence | Value |
| :--- | :--- |
| Owner's observation (2026-09-23) | reaches **4.1 GHz** |
| Agent measurement, quiet moment | **3758 MHz** on the highest core |
| `cpuinfo_max_freq` (acpi-cpufreq table maximum) | 1700 MHz |

The clock exceeds the driver's table maximum by a wide margin, which is only
possible through the SMU's autonomous boost. `cpufreq/boost = 1`, the governor is
`performance` (request pinned to the top P-state), and `scaling_max_freq` is at the
driver ceiling - so every OS-side boost enabler is correct.

**Why the first measurements said otherwise:** they were taken with
`perf stat`/`/proc/cpuinfo` while an unrelated project held the machine at load
8-18. In that state the package is hot, `Tctl` sits at the firmware's 70 C
ceiling, and the SMU spends its boost budget on the cores that are running - the
highest core observed was 2.16-2.62 GHz. Those numbers describe the
**thermally-limited loaded state**, not the boost ceiling. Measuring "does 4.1 GHz
work" requires an idle package, which was not available during that session.

Two limits bound how often the high boost states are reachable at all, both
firmware-owned:

1. **Tctl 70 C** - the ceiling the SMU regulates to, in every platform profile
   (section 2.4). It cannot be raised from the OS.
2. **STAPM** - 6-10 W by default; WattCurb's Performance profile raises it to
   25 W, which is what took the loaded state from 600 MHz to 2.2-2.8 GHz.

`acpi-cpufreq` exposes no CPPC (`/sys/devices/system/cpu/cpu0/acpi_cppc` absent,
`amd_pstate` not loaded), so the OS cannot request boost states directly; boost is
entirely the SMU's autonomous decision within those two limits. The practical
consequence for WattCurb is that the fan curve and the STAPM raise are the only
levers, and both are now at their maximum: full fan from 60 C (REF-REQ-124/125)
and STAPM 22-25 W.

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
- **REQ-123.7 (A clamped write is reported)** After applying the SMU limits the
  daemon shall read the effective Tctl limit back once and, when the firmware
  clamped it below the requested target, log an `INFO` naming both values. A
  successful mailbox write must never be reported as a raised limit
  (REF-REQ-115.4).

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
| Boost reachable | owner confirms 4.1 GHz; agent measured 3758 MHz on the highest core at a quiet moment |

The end-to-end check - daemon captures the EC baseline, Performance raises STAPM,
and the clock recovers under load - is run on the host after each release because
it needs the real SMU. It is not part of the unit suite: a unit test that raised a
real power limit would change the machine it runs on and survive the test run.
