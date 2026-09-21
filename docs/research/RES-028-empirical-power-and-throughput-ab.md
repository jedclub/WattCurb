# REF-RES-028: Empirical Power & Throughput A/B

**Date**: 2026-09-22 · **Host**: ThinkPad L15 Gen 1, Ryzen 7 PRO 4750U, battery, kernel 7.2.5-1-cachyos
**Build under test**: `output/` staged 09-22 01:22, md5 `17469bede6d73a00d9db954fc69d3031`
(includes REF-REQ-107 and REF-REQ-108)

This is the **first empirical measurement of WattCurb's central claim**. Every
prior figure in the repository - the README table, `PGO_PMU_REPORT.md`,
`PMU_BENCHMARKS.md` - measures compiler output quality (IPC, cache misses,
binary size), not power saved.

## 1. Method

System draw sampled from `/sys/class/power_supply/BAT0/power_now` every 2 s,
20 samples (40 s) per condition, after a 20 s settle. Desktop otherwise idle;
`wattcurb-tray` and `wattcurb-dashboard` not running in any condition.
`power-profiles-daemon` and `ananicy-cpp` left running in all conditions, as in
normal use. Profile selected by writing `/var/lib/wattcurb/power_profile_mode`
before starting the unit.

Throughput measured separately as wall time for a deterministic
8,000,000-iteration integer loop.

## 2. Results

### 2.1 Power

| Condition | Run 1 (A->B->C) | Run 2 (C->B->A) | Run 2 repeat |
| :--- | ---: | ---: | ---: |
| UltraEndurance | 13.97 W | **13.69 W** | - |
| Performance | 16.58 W | **15.89 W** | - |
| Daemon stopped (control) | 21.29 W | **32.05 W** | 25.98 W |

### 2.2 Order was reversed to test for drift

The first run measured the control immediately after a PGO build finished, so a
machine still settling would produce exactly the monotonic A > B > C seen there.
Run 2 reversed the order and measured the control **last**: it came out
**highest** (32.05 W), not lowest. Settling drift is therefore rejected as the
explanation. The two profiles reproduce closely across orders (13.97/13.69 and
16.58/15.89), while the control is both the highest and by far the most variable
(21.3 - 32.1 W).

### 2.3 Savings

Against the most conservative control reading (21.29 W):

| Profile | Draw | Saving |
| :--- | ---: | ---: |
| Performance | ~16 W | **-22%** |
| UltraEndurance | ~13.7 W | **-35%** |

Against the control mean across all three control samples (~26.4 W): -38% and
-47% respectively. A secondary effect is that the control's draw swings by more
than 10 W while the daemon's profiles hold within ~2 W.

### 2.4 Throughput, same build

| Condition | Fixed-work wall time |
| :--- | ---: |
| Daemon stopped | 1.45 s |
| Performance | 2.02 s (**1.4x slower**) |

## 3. Finding: Performance mode behaves as a saving profile

WattCurb reduces power. That claim is now supported. But **Performance mode
draws 22-38% LESS power than no power management while also running 1.4x
slower** - it is saving, not unleashing, which is the opposite of its contract
([`REF-REQ-107`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-107-performance-mode-throughput-guarantee.md)).

Removing the C0 latency clamp (REQ-107.2, measured at 1.88x on its own) did not
close the gap, so at least one further throughput-reducing actuation remains in
the `Performance` branch. A profile that both spends less power and does less
work per second is holding the machine back somewhere that has not yet been
identified.

## 4. Limitations

- 40 s per condition, two runs. Not a long-duration or full-discharge test.
- The control is not a quiescent machine: its 21-32 W spread means background
  activity was present and differed between samples. A quieter control would
  likely read lower, which would shrink the measured saving.
- Idle-ish desktop only. No measurement under a sustained workload, which is
  where a power manager's trade-off actually matters.
- Backlight was identical (39321) across all conditions in run 1, so the saving
  is not a screen-dimming artifact. Run 2 did not record it.
- Battery state of charge fell across the session; `power_now` is an
  instantaneous reading and was not corrected for that.
