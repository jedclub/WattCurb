# REF-REQ-109: Single Owner for Contested Hardware Knobs

## 1. Observed Defect
`power-profiles-daemon` is active on the development host, set to `balanced`,
and its `PlatformDriver` is `platform_profile` - the same
`/sys/firmware/acpi/platform_profile` node WattCurb writes.

Observed live during the REF-RES-027 reproduction soak, 90 seconds after
starting the daemon in `Performance`:

```
[t+90s] fixed-work 2.458s   (platform=balanced cur=3532965)
```

WattCurb had written `performance` at bootstrap. Ninety seconds later the node
read `balanced`. Neither daemon is wrong on its own terms; the node's content
simply depends on which wrote last.

This is worse than either policy alone. The machine's power behaviour changes
without any user action and without either daemon logging anything unusual, and
any measurement taken across such a window is unreproducible - which is exactly
the character of the unexplained 12.4x collapse in
[`REF-RES-027`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-027-performance-mode-clock-collapse-incident.md).

## 2. Functional Requirements

### REQ-109.1: A contested knob has one owner, and WattCurb is not it by default
`MitigationEngine::competing_power_manager()` scans `/proc` for a running
`power-profiles-daemon`, `tuned`, `tlp` or `auto-cpufreq`. When one is present,
`set_platform_profile()` declines the write and returns `false`.

Deferring is chosen over winning because winning is not available: the
competitor re-asserts on its own schedule, so "winning" means racing forever.
Which daemon should own the node is a deployment decision.

### REQ-109.2: The conflict is reported, once, with the remedy
On first detection the daemon logs a `CONFLICT` alert naming the competitor and
the exact command that resolves it:

```
power-profiles-daemon owns /sys/firmware/acpi/platform_profile; WattCurb will
not write it. Run 'systemctl mask --now power-profiles-daemon' to give WattCurb
full control.
```

The unit name, not the process name: the kernel truncates `comm` to 15
characters, and the first implementation emitted
`systemctl mask --now power-profiles-`, which is not a unit and fails. A remedy
printed in an alert has to be copy-pasteable, so detection matches on the
truncated `comm` and reports the real unit.

Silence would leave the user with a profile switch that appears to do nothing.

### REQ-109.3: Detection does not run on the hot path
The scan walks `/proc` and the evaluation loop runs every 3 s, so the result is
cached after the first call. A competitor started later is therefore not noticed
until the daemon restarts; this is accepted, because the alternative is a `/proc`
walk per cycle in a daemon whose entire design premise is zero-wakeup.

### REQ-109.4: Scope
Only `platform_profile` is guarded. Other knobs WattCurb writes
(`scaling_max_freq`, `energy_performance_preference`, PCIe ASPM) can also be
touched by these daemons, and are not yet covered. Stated here so the gap is
recorded rather than implied not to exist.

## 3. Measured Context
`platform_profile=performance` was measured at **1.27x slower** on battery on
this ThinkPad (REF-RES-027 section 6.3), so declining to write it costs little
in `Performance`. In `UltraEndurance` the `low-power` value is not applied
either, which may reduce that profile's saving; this has not been measured
separately from the other UltraEndurance actuations.

## 4. Verification
[`REF-TEST-061`](file:///home/jedclub/Develop/WattCurb/tests/test_units.cpp):
when a competitor is detected on the host, `set_platform_profile()` must decline.
The assertion is conditional on the host's actual service state and reports which
branch it took.
