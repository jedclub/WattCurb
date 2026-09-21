# REF-REQ-110: Process-State Repair on Bootstrap

## 1. Observed Defect
`sched_setaffinity` writes **process** state, not hardware state.
`restore_hardware_baseline()` restores sysfs and `/dev` knobs on shutdown and
touches none of it, so a mask applied by the daemon:

- survives the daemon being stopped,
- survives the daemon being restarted,
- is **inherited by every child** of a masked process,
- and persists until the process exits - for a login shell or the desktop shell,
  that means until logout.

Found on the development host with **no daemon running at all**:

```
systemd(1)        0-15
foot(1724)        0-15      <- terminal, unmasked
fish(1726)        0-7       <- masked here; parent is not
claude(2242744)   0-7       <- inherited
zsh(2310662)      0-7       <- inherited

plasmashell(1262) 0-7
ksecretd(1045)    8-14:2    <- CPUs 8,10,12,14
ksystemstats(1463) 8-15
at-spi2-registr, dconf-service, kactivitymanage, polkit-kde-auth,
org_kde_powerde, xembedsniproxy, xsettingsd, xdg-desktop-por (x3), ...  0-7
```

`foot` at `0-15` with its direct child `fish` at `0-7` proves the mask was
applied **after** the process was created, by an external agent. The strided
`8-14:2` set on `ksecretd` is the C2-cluster dispersion pattern named in the
REQ-104 code comment. WattCurb is the only component on this host that calls
`sched_setaffinity` on other processes.

Consequences:
- The user's desktop shell ran on half the machine indefinitely.
- Every command launched from the affected terminal inherited the mask, so
  **every throughput measurement taken in that shell was made on half the
  machine** - including those in
  [`REF-RES-027`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-027-performance-mode-clock-collapse-incident.md)
  and [`REF-RES-028`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-028-empirical-power-and-throughput-ab.md).

## 2. Why Existing Healing Missed It
`heal_over_throttled_processes()` (REQ-103) opens with a cost gate:

```cpp
if (::sched_getscheduler(pid) != SCHED_IDLE) continue;
```

A masked process sits in a normal scheduling class, so the gate skips it. The
sweep restores affinity, but only for processes it has already selected for
being in the idle class. Affinity damage on its own was healed by nothing.

## 3. Functional Requirements

### REQ-110.1: Repair before actuating
`MitigationEngine::repair_orphaned_affinity_masks()` runs once at daemon
bootstrap, before `apply_power_profile()`. It restores full-machine affinity to
any process whose mask covers fewer than the online CPU count.

### REQ-110.2: Repair by mask pattern, not by process class
The sweep restores a process only when its mask is **exactly one of the masks
this engine can apply**: the C1 cluster, the C2 cluster, the interactive shield
set, or the headroom set of any of the four profiles
(`mask_matches_engine_pattern()`).

The first implementation scoped the repair by process class instead
(`is_liveness_critical`, `is_graphical_session_process`, `is_stall_shielded`,
`is_immune_process`). Run on the host that motivated this requirement it
repaired 66 processes but **missed `ksecretd`** - still pinned to the strided set
8,10,12,14 afterwards - and missed the agent's own shell, because neither
classifies into those buckets. The classifier's opinion of a process has nothing
to do with whether WattCurb masked it.

Matching the mask keeps the property the class scope was there to provide: a
deliberate `taskset -c 3` by the user is not a mask this engine produces, so it
is still left alone.

### REQ-110.3: Bootstrap only
The sweep walks `/proc` and reads every process's affinity. It must not run on
the evaluation cycle.

### REQ-110.4: The repair is reported
A non-zero repair count logs a `REPAIR` alert with the number of processes
restored, so the damage from a previous run is visible rather than silently
papered over.

## 4. Not Covered
- `nice`, scheduling class, ionice and timer slack are also process state and
  also outlive the daemon. This requirement repairs affinity only, because that
  is the one measured to have caused harm. The others are recorded here as a
  known gap.
- Another agent that happens to apply an identical cluster mask is
  indistinguishable from WattCurb, and its mask would be repaired too.
- Repair cannot help a process that has already exited, nor un-inherit a mask
  from children already spawned.

## 5. Verification
[`REF-TEST-061`](file:///home/jedclub/Develop/WattCurb/tests/test_units.cpp)
asserts that the C1 and C2 cluster masks are recognised as engine-applied and
that a single-CPU pin is not.

The sweep's effect on live processes is not asserted by the suite: it is a
property of host process state, and reproducing it would require the test to
mask a real process, which REQ-092's Host Isolation clause forbids. Measured on
the host instead - the first run reported
`Restored full CPU affinity to 66 shielded process(es) left masked by a previous
run`, and `plasmashell` went from `0-7` to `0-15`.
