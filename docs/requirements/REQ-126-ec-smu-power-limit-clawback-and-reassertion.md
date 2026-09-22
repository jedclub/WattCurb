# REF-REQ-126: EC SMU Power-Limit Clawback & Periodic Re-Assertion

- **Status**: Implemented
- **Ref ID**: `REF-REQ-126`
- **Date**: 2026-09-23
- **Related**: [`REF-REQ-115`](REQ-115-smu-thermal-power-limit-raise.md),
  [`REF-REQ-123`](REQ-123-ec-smu-power-cap-diagnosis.md),
  [`REF-REQ-125`](REQ-125-fan-full-speed-keyword.md) (its 125.4 is the same
  verify-and-re-assert pattern, applied to the fan),
  [`REF-RES-030`](../research/RES-030-thinkpad-ec-fan-and-ryzenadj-smu-limits.md),
  [`REF-RES-031`](../research/RES-031-bios-access-and-settings.md)
- **Tests**: [`REF-TEST-080`](../../tests/test_units.cpp), plus the recalibrated
  [`REF-TEST-072`](../../tests/test_units.cpp)
- **Category**: EC Firmware Interaction, SMU Power Limits, Monitoring Watchdog

## 1. Report and observed state

Owner's observation: *"온도가 낮은데 성능 모드에서 클럭이 잘 안 올라가는 이유는
뭘까?"* - the clock does not rise in Performance mode even though the machine is
cool.

Measured on the reference host (2026-09-23 01:36, Performance in force, AC online):

| Signal | Value | Reading |
| :--- | :--- | :--- |
| `k10temp` Tctl | **44.8 C** | 25 C below the firmware's 70 C ceiling |
| `thinkpad` CPU | 44.0 C | |
| Fan | `disengaged` / 5350 RPM | already maximum |
| `scaling_governor` | `performance` | request pinned to the top P-state |
| `scaling_max_freq` / `cpuinfo_max_freq` | 1700000 / 1700000 kHz | no OS cap |
| `cpufreq/boost` | 1 | boost enabled |
| `platform_profile` | `performance` | EC thermal table at its most permissive |
| `/proc/loadavg` | 5.29-6.73 | plenty of demand |
| Every core (`cpuinfo MHz`, all 16) | **1394-1399 MHz** | the driver's *lowest* P-state |
| `STAPM LIMIT` (`ryzenadj -i`) | **6.000 W** | the EC's own table value |

Every knob the daemon owns read **correct**. The part was pinned at the P-state
floor, thermally idle, power-capped.

## 2. Root cause chain

1. **The EC owns STAPM and takes the raise back.** `apply_smu_performance_limits()`
   writes the SMU mailbox at profile application; the EC later moves STAPM back to
   its own table value (6 W here, a 6-10 W band has been observed). This is the
   same class of behaviour REF-REQ-123 documented for the EC moving STAPM on its
   own - except there it moved *upward*, which merely helped.
2. **Nothing verified the write afterwards.** The only read-back was the Tctl
   clamp, and that runs once in the daemon's lifetime (`s_tctl_clamp_reported`).
   The STAPM value was never re-read.
3. **The watchdog could not see the resulting state.** `is_frequency_starved()`
   required `max_clock < 0.6 * hw_max` (1020 MHz) **and** `load1 >= 0.5 * ncpu`
   (8.0 on this 16-thread host). The floor-pinned state is 1400 MHz (0.824 of the
   1700 MHz table maximum) at load 3.5-7 - both gates miss it. The log confirms
   the watchdog fired exactly once, when a heavier load pushed the clock to
   443 MHz at load 14.58.
4. **Therefore the machine stayed crippled until a load spike tripped the
   watchdog.** Between those spikes, Performance mode delivered 780-1400 MHz.

## 3. Evidence: the power limit, not the temperature

Controlled test on the live host - the daemon's own values written once by hand
(`--tctl-temp=85 --stapm-limit=25000 --fast-limit=35000 --slow-limit=30000
--apu-slow-limit=30000`), Tctl never above 46 C throughout:

| Time | STAPM | Highest core | `load1` |
| :--- | ---: | ---: | ---: |
| before | 6 W | **780 MHz** | 6.73 |
| +0 s | 25 W | **3942 MHz** | 6.73 |
| +15 s | 25 W | 3289 MHz | 5.95 |
| +30 s | 25 W | 3369 MHz | 5.17 |
| +45 s | 25 W | 3219 MHz | 4.16 |
| +60 s | 25 W | 3314 MHz | 3.52 |
| +90 s | 25 W | 3622 MHz | 3.53 |

The die never approached 70 C, so temperature was not the binding constraint at
any point. The EC's 6 W STAPM was. Raising it moved the clock by ~4-5x
immediately, and the value held for the 90 s observation window.

## 4. Implemented fix

### 4.1 Periodic verification with re-assertion (`REF-REQ-126`)

`MitigationEngine::verify_and_reassert_smu_limits(mode)` reads the enforced STAPM
back and re-applies the raise when the EC has reclaimed it. It is called from the
monitoring cycle (`FeatureManager::evaluate_and_actuate`, next to the fan curve)
on a slow cadence:

| Constant | Value | Reason |
| :--- | :--- | :--- |
| `SMU_VERIFY_INTERVAL_CYCLES` | 3 | one `ryzenadj -i` exec per ~30 s at the 10 s cadence |
| `SMU_VERIFY_MIN_LOAD1` | 1.0 | an idle machine does not need the power budget; no exec at all then |
| `SMU_STAPM_CLAWED_BACK_MW` | 18000 | separates the EC's 6-10 W band from our own accepted write |

The floor is not `SMU_STAPM_PERF_MW`: our accepted 25 W write reads back at 22 W
(SMU granularity), so a tight comparison would re-write every cycle forever. The
call is a no-op when ryzenadj is absent, when the baseline was not captured, when
the actuation sandbox is on, and in the saving profiles (there the EC's cap is
intentional). The clawback is reported once per episode; a healthy read re-arms
the report so a later loss is not silent.

`SmuVerifyResult` distinguishes six outcomes (`Unavailable`, `NotUnrestricted`,
`ReadFailed`, `Healthy`, `Reasserted`, `Refused`) so "nothing to do" is never
confused with "tried and failed".

### 4.2 Watchdog recalibration

| Constant | Before | After | Reason |
| :--- | ---: | ---: | :--- |
| `FREQ_STARVED_CLOCK_FRACTION` | 0.6 | **0.85** | the floor (1400/1700 = 0.824) must trip; 0.85 x 1700 = 1445 MHz sits just above it |
| `FREQ_STARVED_MIN_LOAD_RATIO` | 0.5 | **0.25** | load 4.0 on 16 threads is real demand; the observed defect happens at 3.5-7 |
| `FREQ_STARVED_TRIP_CYCLES` | 5 | 5 (unchanged) | ~50 s of sustained starvation |

A false positive costs one idempotent re-assertion and one log line, not a
throttled machine - the trip condition only re-applies the limits the profile
already asked for.

### 4.3 Read-back parser

`parse_smu_limit_row(text, row_name)` is pure and unit-tested. It returns the raw
number printed by `ryzenadj -i`, which is **watts** for the power rows, and each
caller scales (`read_back_stapm_limit()` converts to milliwatts, because
ryzenadj's argument space is mW while its report is W - comparing 25 against
18000 would have re-written on every cycle). Two defects were caught while
building it:

- `strstr` crossed line boundaries and matched a row name on a **later** line,
  reading the current line's number - a "THM LIMIT CORE" lookup returned the
  STAPM value. Fixed by bounding the search to the line; REF-TEST-080 pins it.
- The value is the field after the **second** bar, not the first. Reading from the
  first bar works only because no row name contains a digit.

## 5. Blast radius

- **What is written**: the SMU mailbox via `ryzenadj`, as root, with the values
  REF-REQ-115 already established (`--tctl-temp=85`, `--stapm-limit=25000`,
  `--fast-limit=35000`, `--slow-limit=30000`, `--apu-slow-limit=30000`). No new
  register, no new value, no new privilege.
- **What changed is frequency**: the write can now happen every ~30 s while the
  machine is loaded and Performance/Balanced is in force, instead of twice in a
  session. The cadence bounds it; nothing else does.
- **The Lenovo 48-save limit does not apply.** That limit is
  `firmware-attributes`/`think-lmi` BIOS-setting storage (REF-RES-031), a
  different interface. WattCurb holds no reference to `firmware-attributes`.
- **Thermal protection is unchanged and still active.** Tctl is set, not
  disabled, so the firmware's 70 C ceiling still regulates; the re-assert cannot
  and does not raise it.
- **Saving profiles are untouched**: the verification returns `NotUnrestricted`
  and writes nothing. `restore_smu_limits()` on profile exit and shutdown is
  unchanged.
- **It cannot prevent the EC from taking the limit back.** It shortens the window
  in which the machine runs at the P-state floor.

## 6. What is NOT verified

- **No live production observation of the re-assert yet.** The clawback was
  measured by hand; the new code path is unit-tested for its decision logic only.
  Whether it fires in service is a post-deployment observation.
- **The clawback interval is uncharacterised.** One observation held 25 W for
  >90 s; another had reverted to 6 W roughly 30 minutes after the daemon applied
  it. No sampling was done in between, so "how fast" is unknown. 30 s was chosen
  to bound the crippled window, not from a measured reversion rate.
- **No thermal soak after the fix.** Raising the power budget on a hot day, with
  the fan already at maximum from 60 C, has not been measured under sustained
  all-core load for minutes.
- **The tests do not cover the hardware.** REF-TEST-080 proves the parser and the
  decision; it does not, and cannot, prove that a STAPM write reaches the SMU or
  that the clock then rises. That is measured on the host (section 3).

## 7. Verification

| Check | Result |
| :--- | :--- |
| `scripts/harness.py build` | OK |
| `scripts/harness.py test` | 80/80 pass |
| REF-TEST-080 | parser row selection, W/mW unit handling, clawback decision, failed-read handling |
| REF-TEST-072 | recalibrated gates: the P-state-floor pin trips, boosted clocks and idle stay clean |
| PGO release | deployed and verified (see the release commit and `docs/research/PGO_PMU_REPORT.md`) |
