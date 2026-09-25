# [REF-ARCH-072] Memory Pressure Guard & CPU Ceiling Assertion

**Implements**: [`REF-REQ-112`](../requirements/REQ-112-boost-restoration-and-non-halting-memory-guard.md)
**Date**: 2026-09-22

---

## 1. CPU Ceiling Assertion

### 1.1 Why a captured value is not a baseline

```
 previous daemon (UltraEndurance)          sysfs                next daemon
 ───────────────────────────────           ─────                ───────────
 set_cpu_scaling_max_freq(1400000) ──────► scaling_max = 1.4GHz
 set_cpu_boost(false)              ──────► boost = 0
        │
        ✗ SIGKILL / crash / install.sh restart
          (no rollback runs)
                                           scaling_max = 1.4GHz ──► capture_hardware_baseline()
                                           boost = 0                 "the user's baseline
                                                                      is 1.4 GHz, no boost"
                                                                             │
                                                    Performance ◄────────────┘
                                                    restores 1.4 GHz, forever
```

`cpuinfo_max_freq` is the fixed point in this picture: it is driver-reported,
read-only, and cannot have been written by a previous run. The capture path now
reads it first and treats any observed ceiling below it as damage rather than
configuration.

### 1.2 Placement

`capture_hardware_baseline()` runs at `daemon_runner.cpp:308`, *before*
`repair_orphaned_affinity_masks()`. That ordering is why the affinity defect of
REF-REQ-110 was repairable while the frequency cap was not - by the time a
repair pass could run, the cap had already been recorded as the baseline. The
repair is therefore performed **inside** the capture, not as a separate pass.

### 1.3 `assert_unrestricted_cpu_ceiling()`

```
for each online cpu:
    read scaling_max_freq
    if value >= cpuinfo_max_freq:  continue      ← the common case: no write
    write cpuinfo_max_freq
read cpufreq/boost
if '0': write '1'
returns true iff anything was written
```

Read-before-write is what makes this safe to call every observation cycle. On a
healthy machine the whole call is *N* `read()`s and zero writes, so it adds no
sysfs write traffic and no uncoordinated wakeups. It is called from two places:

- `apply_power_profile()` for Performance and Balanced, replacing the replay of
  the captured value;
- `FeatureManager::evaluate_and_actuate()` on every cycle while an unrestricted
  profile is in force, which is what closes the "profile sat still while
  something else capped the CPU" path.

The second call site matters more than it looks. `MitigationEngine` has its own
`evaluate_and_actuate()`, and it is the obvious-looking home for a per-cycle
policy assertion - but **it has no production caller**. The daemon
(`daemon_runner.cpp:543`) and the CLI (`main.cpp:412`, `main.cpp:476`) all enter
through `FeatureManager::evaluate_and_actuate()`; the engine's own method is
reached only from the test suite. The first implementation of REQ-112.4 sat in
the engine method and therefore never executed on a running machine.
`MitigationEngine::ceiling_assertion_count()` exists so `REF-TEST-068` fails if
that is ever true again.

---

## 2. Memory Pressure Guard

### 2.1 Position in the daemon

```
            ┌─────────────────── epoll_wait (one thread, blocking) ──────────────────┐
            │                                                                        │
   timerfd ─┤ 10 s / 2 s cadence ──► process_observation_cycle()                      │
            │                            └─► memory_guard_.evaluate_and_actuate()    │  ← de-escalation
  signalfd ─┤ SIGTERM/SIGHUP                                                          │
            │                                                                        │
  PSI  fd  ─┤ EPOLLPRI, "some 150000 2000000" ──► memory_guard_.evaluate_and_actuate()│  ← escalation
            │                                                                        │
 ctrl sock ─┤ IPC                                                                     │
            └────────────────────────────────────────────────────────────────────────┘
```

The asymmetry is deliberate. The kernel raises `EPOLLPRI` when memory *starts*
stalling and never signals when pressure *falls*, so escalation is event-driven
(latency bounded by the PSI window, 2 s) while de-escalation rides the existing
tick. Nothing polls: on a machine that is not under pressure the PSI descriptor
is silent for the life of the daemon.

This is the only new descriptor in the event loop, and it is optional - a kernel
without `CONFIG_PSI`, or a process without `CAP_SYS_RESOURCE`, leaves
`psi_fd_ == -1` and the guard runs on the tick alone.

The trigger parameters are kernel-constrained and were measured rather than
assumed (see REF-REQ-112 §3.2): the window must be a multiple of 2 s, the
threshold at most one tenth of the window, and creation needs
`CAP_SYS_RESOURCE`. A refused trigger leaves the descriptor unusable, so each
candidate in the fallback ladder gets its own `open()`.

### 2.2 Ladder and state

```
                  escalation (immediate)
   Normal ──────────────────────────────────► Advisory ──────────────► Throttle
      ▲                                           │                        │
      │        release band, one step per evaluation                       │
      └───────────────────────────────────────────┴────────────────────────┘
          SwapFree > 45%  AND  MemAvailable > 25%  AND  PSI full avg10 < 5
```

| | Advisory | Throttle |
| :--- | :--- | :--- |
| Entry (any) | SwapFree < 30% · MemAvail < 15% · PSI full > 10 | SwapFree < 12% · MemAvail < 8% · PSI full > 25 |
| Action | `memory.reclaim` + `swappiness=0` on Tier 4/5 ≥ 256 MiB | `cpu.max 20000 100000` + `SCHED_IDLE` on largest non-protected |
| Side effect | power-side reclaim becomes file-only | - |

`swappiness=0` on `memory.reclaim` is the pivot of the Advisory tier: without
it the kernel is free to satisfy the reclaim by writing anonymous pages to swap,
which spends the exact resource the tier is defending. Kernels before 6.4 reject
the argument, and the actuator falls back to a plain reclaim rather than losing
the tier.

The same flag is why `MemoryPressureGuard::swap_feeding_suspended()` is read by
`battery_feature.cpp` and by the three power-side reclaim sites in
`mitigation_engine.cpp`: while the guard is above Normal, the *power*
optimisation ladder must also stop pushing pages into swap. A power feature that
worsens a memory emergency is not a power feature.

### 2.3 Target selection

`is_throttle_candidate()` is a pure predicate, deliberately separated from the
actuation loop so the Oracle Gate can falsify it without a machine under real
pressure:

```
pid > 1                                   ← init is never touched
pid != focused window                     ← REF-REQ-085
pss_kib >= 256 MiB                        ← below this there is nothing to gain
tier ∉ {CriticalImmune, DesktopCore, DesktopShell}
```

PSS rather than RSS: shared pages are counted once, so a browser's 40 renderer
processes do not each appear to hold the whole shared heap.

### 2.4 Lifetime

A cgroup `cpu.max` is cgroup state - it belongs to the process's cgroup, not to
WattCurb, and it survives daemon exit exactly as the affinity masks of
REF-REQ-110 do. `memory_guard_.shutdown()` therefore runs in `~DaemonRunner`
alongside `feature_manager_.rollback_all_tracked()`, before the hardware
baseline restore. `OOMScoreAdjust=-900` in the unit exists for the same reason:
if the daemon is itself the OOM victim, none of this rollback runs.

### 2.5 Cost

| Path | Syscalls | When |
| :--- | :--- | :--- |
| `sample()` | 1 `pread` (cached `/proc/meminfo` fd) + `open`/`read`/`close` on `/proc/pressure/memory` | per tick and per PSI event |
| `classify()` | 0 | pure |
| Advisory actuation | ≤ 8 × (`resolve_cgroup_path` + 1 write) | only above Normal |
| Throttle actuation | ≤ 16 × (quota write + `sched_setscheduler`) | only at Throttle |

The steady-state cost on a healthy machine is the `sample()` row alone, folded
into a tick the daemon was already taking. No new wakeup source, no allocation:
`MemoryPressureSample` is a 64-byte POD and the throttle table is a
`FixedVector<ThrottledProcess, 16>` inline in the guard.

---

## 3. Swap Topology (host configuration, not code)

```
 before                                    after
 ──────                                    ─────
 zram      15 GiB  prio 100  ← = RAM       zram       6 GiB  prio 100
 swapfile   8 GiB  prio  -1                swapfile  32 GiB  prio  -2*
 ─────────────────────────                 ─────────────────────────
 total     23 GiB                          total   38.9 GiB
 zspages at full: 4.33 GB of RAM           zspages at full: ~1.7 GB of RAM
```

\* The effective kernel priority of the swapfile is **-1**, not the `pri=-2`
written in `/etc/fstab`: util-linux `swapon` sets `SWAP_FLAG_PREFER` only when
the priority is non-negative, so the option is dropped and the kernel default
applies. Nothing depends on it - zram's `100` stays the fast tier either way,
and hibernation device selection uses `resume=`/`resume_offset=`, not priority
(see [`REF-RES-032`](../research/RES-032-hibernation-resume-offset-invariant.md)).

zram is a *compressed RAM* tier: sizing it at 100% of RAM (the distro default,
`zram-size = ram`) means a full swap tier consumes roughly a third of the memory
it was supposed to free, and the disk tier behind it is unreachable until that
has happened. Applied via `/etc/systemd/zram-generator.conf` and
`btrfs filesystem mkswapfile --size 32g`; both survive reboot (`/etc/fstab`
already carried the swapfile entry). Recreating that file changes its
first-extent physical offset and therefore the hibernation `resume_offset` -
the invariant and recovery procedure are in REF-RES-032.

This is host configuration and lives outside the repository. It is recorded here
because §3.3 of REF-REQ-112 depends on it: the code raises the pressure at which
a kill becomes possible, and the backing store is what removes the condition
that produced the measured kill.
