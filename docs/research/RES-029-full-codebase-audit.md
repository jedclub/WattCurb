# REF-RES-029: Full Codebase Audit - Memory, UB, Resource and Policy

**Date**: 2026-09-22 · **Scope**: all of `src/` (24,486 lines) · **Build audited**:
commit `7d720c6`

Method was empirical first, reading second: sanitizers and live instrumentation
on the running daemon, then static analysis, then targeted review of the policy
layer. Findings are separated into what was **measured clean** and what is
**defective**, because "no leaks found" is only meaningful alongside what was
actually exercised.

---

## 1. Measured Clean

### 1.1 Sanitizers (ASan + UBSan + LeakSanitizer)

Built at `-O1 -g` with `-fsanitize=address,undefined -fno-sanitize-recover=all`,
`detect_leaks=1`, `detect_stack_use_after_return=1`, `strict_string_checks=1`,
`check_initialization_order=1`.

| Path exercised | Result |
| :--- | :--- |
| `--briefing -w 8 -i 1` (real hardware profiling) | clean |
| `--detail -w 6 -i 1` | clean |
| `-X -w 6 -i 1` (extreme profile) | clean |
| `--features`, `--status`, `--history` | clean |
| `wattcurb-tray --benchmark` | clean |
| **`--daemon --period 3.0` as root, 50 s, actuating** | clean |

Zero reports of any kind: no heap/stack overflow, no use-after-free, no
uninitialized read, no signed overflow, no misaligned access, no leaked
allocation.

The unit suite could not be run under sanitizers: its Oracle Gate latency
assertions (e.g. `< 6.00 us/op`) fail under instrumentation, aborting at
`test_units.cpp:1442`. The gates have no bypass. This is a **gap in the audit**,
not a pass.

### 1.2 Resource stability, live daemon

| Metric | 60 s steady state | 16 profile-switch cycles |
| :--- | :--- | :--- |
| Open file descriptors | 6, flat | - |
| Threads | 1, flat | - |
| RSS | 4,708 KB, flat | 5,772 -> 5,780 KB (+8 KB, flat after cycle 2) |
| VmSize | - | 11,968 -> 11,972 KB (+4 KB) |

The 16 cycles drove all four profiles four times each, so every actuator and
every rollback path ran repeatedly. **No file-descriptor leak and no heap growth
across actuation.**

### 1.3 Ownership and lifetime

- **No `shared_ptr`, `weak_ptr` or `unique_ptr` anywhere in `src/`.** Reference
  cycles are structurally impossible. The single `new` is
  `poll_timer_ = new QTimer(this)`, which is Qt parent-owned.
- All namespace-scope statics (`s_hardware_baseline`, `s_actuation_sandbox`,
  `s_audio_state`, `s_effective_profile`, `s_hover_probe`, `s_tip_cache`) are
  constant-initialized aggregates, so there is no static initialization order
  dependency between translation units.
- `FixedVector`'s placement-new / manual-destroy protocol is correct: the
  destructor calls `clear()`, `resize()` destroys on shrink, and both assignment
  operators `clear()` before re-constructing. Guarded by
  `if constexpr (!std::is_trivially_destructible_v<T>)`.

### 1.4 Static analysis

`clang-tidy` with `bugprone-*`, `cert-*`, `clang-analyzer-*` across the main
translation units produced **no true positives**. Every finding was inspected:

| Finding | Verdict |
| :--- | :--- |
| `bugprone-infinite-loop` at `attribution_engine.cpp:1007` | false positive - `idx_acc++` is inside the subscript |
| `bugprone-not-null-terminated-result` at `mitigation_engine.cpp:466` | false positive - `memchr` with explicit length is correct |
| `bugprone-branch-clone` x3 in `battery_feature.cpp` | by design - several conditions setting the same flag |
| `bugprone-incorrect-roundings` x7 in `icon_renderer.cpp` | `(int)(x + 0.5)` on non-negative pixel and colour values; cosmetic |

---

## 2. Defects

### DEF-1 (critical): Process-level mitigations are never released on shutdown

`DaemonRunner::~DaemonRunner()` performs:

```cpp
stop();
window_governor_.rollback_all();                       // window governor state
policy::MitigationEngine::restore_hardware_baseline(); // sysfs and /dev knobs
cleanup_descriptors();
```

It never releases `FeatureManager::m_tracked`, and **`FeatureManager` has no
destructor at all** - unlike `WindowAwareGovernor`, which does
(`~WindowAwareGovernor() noexcept { rollback_all(); }`).

Every `nice`, scheduling class, CPU affinity mask, timer slack and cgroup CPU
quota the feature layer applied therefore survives daemon exit. Affinity and nice
are **process** state: they outlive the daemon, outlive a restart, and are
inherited by every child.

This is the root cause of the damage investigated in
[`REF-RES-027`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-027-performance-mode-clock-collapse-incident.md).
[`REF-REQ-110`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-110-process-state-repair-on-bootstrap.md)
added a repair sweep at the *next* startup, which treats the symptom. The daemon
still leaks the state on the way out.

### DEF-2 (high): Actuation proceeds when the tracking table is full

`battery_feature.cpp`, AntiStarvationHeadroom:

```cpp
if (actuate_anti_starvation_cap(proc.pid, eff_profile, proc.comm.c_str())) {   // acts first
    ...
    if (m_tracked.size() < MAX_TRACKED_MITIGATIONS) {                          // records only if room
        m_tracked.push_back(tm);
    }
    ++status.throttled_count;
}
```

`MAX_TRACKED_MITIGATIONS` is **128**. Past that limit the process is still
masked, reniced and batched, but no record is kept - so `rollback_all_tracked()`
and the de-escalation loop can never restore it. There is no path by which that
process recovers, at any point, in any profile. The counter
`status.throttled_count` is still incremented, so the damage is invisible in
telemetry.

### DEF-3 (high): No PID-reuse guard on restore

`TrackedMitigation` stores `pid`, `comm`, `timestamp_sec` and the saved
`original_nice` / `original_sched_policy` / `original_timerslack_ns` /
`original_affinity` - but **no process start time** (`/proc/<pid>/stat` field 22),
and nothing anywhere checks that the PID still refers to the same process.

If a tracked process exits and Linux recycles its PID, the restore path applies
the **previous** process's saved nice, scheduling policy and affinity mask to an
unrelated new process. `restore_process_affinity()` will happily install a stale
cluster mask on a process WattCurb never throttled.

The de-escalation loop restores and removes entries on a 15-second grace timer
with no liveness check, so this is reachable in ordinary operation on a busy
desktop holding up to 128 tracked PIDs.

### DEF-4 (high, security): The command socket is attributed, not authenticated

`REF-REQ-093` added `SO_PASSCRED` and `recvmsg()` so the daemon can log which
process asked for a profile change. `cred.uid` is used in exactly one place:

```cpp
std::snprintf(requester, sizeof(requester), "%s[%d] uid=%d", comm, cred.pid, cred.uid);
```

It is formatted into a log string and **never consulted in an authorization
decision**. Any local user can drive the root daemon. Demonstrated from an
unprivileged shell during this audit - sixteen consecutive `PROFILE n` datagrams,
all answered `OK`, cycling the machine through all four profiles including
UltraEndurance (1.4 GHz ceiling, 48 Hz panel, GPU clock cap).

A local unprivileged process can therefore degrade the machine persistently, and
- combined with DEF-1 - leave process-level throttling behind after the daemon
stops. The source comment describing this as an "authenticated Unix command
socket" is incorrect.

---

## 3. Audit Gaps

State these rather than imply coverage:

- The unit suite was never run under sanitizers (section 1.1).
- `wattcurb-dashboard` was not sanitized; Qt was excluded from the ASan build.
- No fuzzing of the procfs/sysfs parsers, which consume untrusted-width kernel
  text.
- Concurrency was not analyzed because the daemon is single-threaded (measured:
  1 thread), but the `/dev/shm` Seqlock is read by separate processes and that
  protocol was not modelled.
- The audit covers `commit 7d720c6`. DEF-1 through DEF-4 are unfixed as of this
  document.
