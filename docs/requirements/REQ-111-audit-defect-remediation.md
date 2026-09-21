# REF-REQ-111: Audit Defect Remediation

Fixes the four defects found by
[`REF-RES-029`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-029-full-codebase-audit.md).
None were memory-safety defects - sanitizers were clean across every path
exercised. All four are lifetime or authorization defects in the policy layer.

## DEF-1: Process-level mitigations are released on shutdown

`~DaemonRunner()` restored the window governor and the hardware baseline but
never released `FeatureManager::m_tracked`, and `FeatureManager` had no
destructor - unlike `WindowAwareGovernor`, which has always had one.

Affinity, `nice`, scheduling class, timer slack and cgroup quota are **process**
state. They outlive the daemon, outlive a restart, and are inherited by every
child, which is how a login shell came to hand half the machine to everything
launched from it (REF-RES-027).

- `~DaemonRunner()` now calls `feature_manager_.rollback_all_tracked()` before
  the hardware restore, so the ordering is stated rather than implied.
- `FeatureManager` gains `~FeatureManager() noexcept { rollback_all_tracked(); }`
  so an exit path that does not run the explicit call is still covered.

`REF-REQ-110`'s bootstrap repair sweep remains, but it is now a backstop for
unclean exits rather than the only thing standing between the user and a
permanently degraded session.

## DEF-2: Capacity is checked before actuating, not after

`AntiStarvationHeadroom` acted first and recorded only
`if (m_tracked.size() < MAX_TRACKED_MITIGATIONS)` (128). Past that limit a
process was masked and reniced with **no record**, so no rollback path existed
for it in any profile, while `status.throttled_count` still counted it - the
damage was invisible in telemetry.

The capacity check now precedes the actuation and suppresses it. An un-undoable
mitigation is not worth its saving.

## DEF-3: A pid is not an identity

`TrackedMitigation` stored a bare pid. Linux recycles pids, and every restore
path writes scheduling policy, `nice` and a CPU affinity mask - so a recycled pid
received a dead process's saved state.

`TrackedMitigation` now carries `start_time_ticks`, field 22 of
`/proc/<pid>/stat`. `same_process(pid, ticks)` gates **both** restore paths: the
bulk `rollback_all_tracked()` and the 15-second de-escalation timer, which is the
one most likely to reach a recycled pid. A recorded value of zero means the start
time could not be read when the mitigation was taken; such an entry is not
trusted for restore.

## DEF-4: The command socket is authorized, not merely attributed

`REF-REQ-093` added `SO_PASSCRED` so a profile change could be logged with the
requesting comm, pid and uid. That value was formatted into a log line and never
consulted. Any local process could drive the root daemon.

A uid check alone does not fix it: the script that demonstrated the problem ran
as the desktop user, the same uid the tray runs as. What distinguishes a real
client is the executable behind it.

`PROFILE` now requires the peer's `/proc/<pid>/exe` to resolve to an installed
client binary (`/usr/local/bin/wattcurb{,-tray,-dashboard}` or the `/usr/bin`
equivalents) **and** that file to be root-owned with no group or other write bit.
`uid == 0` is accepted directly, since root already owns every knob the command
touches. Read-only queries stay open.

### Consequences, stated

- A copy running out of `~/.local/bin` is refused. A user-writable binary is not
  a trustworthy basis for commanding a root daemon; the installed binary is the
  one that may command. Desktop entries and the autostart unit already point at
  `/usr/local/bin`.
- **TOCTOU is inherent.** The kernel captures credentials at send time, so by the
  time `/proc` is read the sender may have exited and its pid been reused. The
  window is one datagram wide and the consequence is bounded: a refusal, or a
  change attributed to the wrong client. It is not closed.
- The source comment describing the socket as "authenticated" was wrong and is
  corrected.

## Verification

Measured on the host after deployment, from the same unprivileged script that
caused 17 unwanted profile changes during the audit:

```
시험 전 프로파일: performance
  PROFILE 3  ->  ERROR: not an authorized WattCurb client (REF-REQ-111)
  PROFILE 0  ->  ERROR: not an authorized WattCurb client (REF-REQ-111)
  PROFILE 2  ->  ERROR: not an authorized WattCurb client (REF-REQ-111)
  BRIEFING   ->  응답 받음 (510 bytes)
시험 후 프로파일: performance
```

with three matching `ALERT:DENY` entries naming `python3[2508784] uid=1000`. The
installed tray runs from `/usr/local/bin/wattcurb-tray`, root-owned `-rwxr-xr-x`,
and is unaffected.

[`REF-TEST-062`](file:///home/jedclub/Develop/WattCurb/tests/test_units.cpp)
covers the process-identity read and the `FeatureManager` destructor. DEF-2 is an
ordering invariant inside `evaluate_and_actuate()` against a private constant;
asserting it would mean widening the class interface for the test's benefit, so
it is verified by inspection and the test says so rather than implying coverage.
