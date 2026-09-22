# [REF-REQ-112] CPU Boost Restoration & Non-Halting Memory Pressure Guard

**Status**: Implemented · **Date**: 2026-09-22
**Related**: [`REF-ARCH-072`](../architecture/ARCH-072-memory-pressure-guard-and-ceiling-assertion.md),
[`REF-REQ-044`](REQ-044-absolute-zero-kill-and-non-halting-safety.md),
[`REF-REQ-109`](REQ-109-single-owner-for-contested-knobs.md),
[`REF-REQ-110`](REQ-110-process-state-repair-on-bootstrap.md),
[`REF-TEST-068`](#ref-test-068), [`REF-TEST-069`](#ref-test-069), [`REF-TEST-070`](#ref-test-070)

## 1. Reported Symptoms

Two field reports on the reference machine (Lenovo 20U7S01000, Ryzen 7 PRO
4750U, 15 GiB RAM, KDE Plasma 6 Wayland, kernel 7.2.5-cachyos):

1. **The CPU does not boost in Performance or Balanced mode.**
2. **Under memory exhaustion, applications are force-killed instead of being
   swapped out safely.** The operator's requirement is absolute: *no process
   may ever be force-terminated.*

---

## 2. Part A - Orphaned CPU Frequency Cap

### 2.1 Root Cause

`MitigationEngine::capture_hardware_baseline()` recorded
`/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq` as "the user's
baseline". The CPU frequency ceiling and the `cpufreq/boost` bit are **global
hardware state**: they are not owned by a process and they survive daemon exit.

PowerSaver writes a 1,700,000 kHz ceiling and clears the boost bit;
UltraEndurance writes 1,400,000 kHz. If the daemon exits without rolling those
back - SIGKILL, a crash, a power cut, or simply `install.sh` restarting the
service mid-profile - the cap stays in sysfs. The next bootstrap then captures
**WattCurb's own leftover** as the baseline, and every subsequent
`apply_power_profile(Performance)` and `apply_power_profile(Balanced)`
faithfully "restores" a 1.4 GHz ceiling. On this part `cpuinfo_max_freq` is
1,700,000 kHz (the base clock; boost to 4.1 GHz is Core Performance Boost above
the P-state table), so a 1.4 GHz ceiling puts the machine **below base clock**
and no boost state is reachable at all.

This is the same defect class as [`REF-REQ-110`](REQ-110-process-state-repair-on-bootstrap.md)
(orphaned affinity masks), with one aggravating difference: the affinity repair
runs *after* the baseline capture, so affinity damage is repaired, while the
frequency cap was laundered into the baseline *before* anything could repair it.

A second path with the same effect: `apply_power_profile()` runs only on a
profile **transition**. Nothing re-asserted the ceiling while a profile sat
still, so a cap applied behind WattCurb's back - a competing tool, a
suspend/resume that resets cpufreq - persisted indefinitely.

### 2.2 Requirements

- **REQ-112.1** `HardwareBaselineState` shall carry `hw_max_freq_khz`, read
  from `cpuinfo_max_freq`, as the authoritative hardware ceiling.
- **REQ-112.2** A captured `scaling_max_freq` below `hw_max_freq_khz` shall be
  repaired to the hardware ceiling at capture time and logged as
  `[REPAIR]`. When such a repair occurs and the boost bit is also found clear,
  the boost baseline shall be re-armed to 1 - both were set by the same
  actuation, so restoring one without the other yields a half-capped machine.
- **REQ-112.3** Performance and Balanced shall assert the hardware ceiling and
  the boost bit rather than replaying the captured value.
- **REQ-112.4** The assertion shall be idempotent (read before write, write only
  diverging CPUs) and shall run on every observation cycle while an
  unrestricted profile is in force. It shall be placed on the **production**
  actuation path, `FeatureManager::evaluate_and_actuate()`.
  `MitigationEngine::evaluate_and_actuate()` is not that path - it has no
  production caller and is reached only from the test suite - and the first
  implementation of this clause was placed there, where it never ran.
  `REF-TEST-068` asserts the invocation counter advances when the production
  entry point is driven.
- **REQ-112.5** The systemd unit shall set `OOMScoreAdjust=-900`. The daemon's
  rollback state exists only in its address space; a SIGKILL destroys it and
  creates exactly the orphaned state above.

### 2.3 Accepted Trade-off

Raising a ceiling that WattCurb did not impose is possible: a static cap left by
a third-party tool at boot is indistinguishable from WattCurb's own leftover,
and will be raised to the hardware maximum. This is deliberate. A **live**
competing manager is a different case and is handled by
[`REF-REQ-109`](REQ-109-single-owner-for-contested-knobs.md), which refuses to
fight for the knobs that manager holds.

**Not measured**: the repair path has not been exercised against a real
orphaned cap on live hardware, because the reference machine was found
uncapped. [`REF-TEST-068`](#ref-test-068) proves the invariant that makes the
symptom impossible; it does not reproduce the original failure.

---

## 3. Part B - Kernel OOM Kill Under Memory Exhaustion

### 3.1 Measured Evidence

Kernel log, 2026-09-22 13:28:02, this machine:

```
kswapd0 invoked oom-killer: gfp_mask=0xcc0(GFP_KERNEL), order=0
oom-kill:constraint=CONSTRAINT_NONE,...,global_oom,task=ChatGPT,pid=2227526
Out of memory: Killed process 2227526 (ChatGPT) anon-rss:136464kB
Free swap  = 200kB
Total swap = 24098808kB
zspages: 467988kB (DMA32) + 3866564kB (Normal)
```

The decisive line is `Free swap = 200kB` of 24 GB. This was **not** a WattCurb
cgroup limit - `constraint=CONSTRAINT_NONE, global_oom` is the global killer,
and WattCurb sets no `memory.max` or `memory.high` anywhere. Two structural
causes:

1. **zram was sized at 100% of RAM.** `/usr/lib/systemd/zram-generator.conf`
   shipped `zram-size = ram` → 15 GiB of zram on 14.9 GiB of RAM, at priority
   100. zram's compressed store lives *in RAM*: at the measured ~3.5x zstd
   ratio, a full zram consumed **4.33 GB of the RAM it existed to free**. The
   disk swapfile (8 GiB) sat at priority -1 and was only reached after zram was
   completely full - by which point RAM was already consumed by zspages.
2. **Nothing in userspace intervened before the kernel did.** systemd-oomd,
   earlyoom and nohang were all inactive. WattCurb had no memory-pressure
   awareness of any kind: it never read `/proc/meminfo` or `/proc/pressure/*`,
   and its one memory action (`memory.reclaim`) *pushes anonymous pages into
   swap*, accelerating the exhaustion it should have been defending against.

### 3.2 Requirements

- **REQ-112.6 (Swap topology)** zram shall be sized well below RAM, with a disk
  swapfile large enough to absorb overflow. Applied on the reference machine:
  zram 15 GiB → **6 GiB**, `/swap/swapfile` 8 GiB → **32 GiB** (btrfs
  `mkswapfile`, nocow, uncompressed). Total swap 23 GiB → **38.9 GiB**; worst
  case RAM consumed by zspages ~1.7 GB instead of 4.33 GB.
- **REQ-112.7 (Pressure awareness)** The daemon shall observe memory pressure
  from `/proc/meminfo` (MemTotal, MemAvailable, SwapTotal, SwapFree) and
  `/proc/pressure/memory` (`some`/`full` avg10).
- **REQ-112.8 (Event-driven, not polled)** Pressure shall be delivered by a PSI
  trigger (`some 150000 2000000`) registered in the daemon's existing `epoll`
  set with `EPOLLPRI`. The descriptor is silent on a healthy machine, so the
  Zero-Wakeup contract of [`REF-REQ-002`] is preserved. A kernel that refuses
  the primary trigger shall be offered a fallback ladder, and a kernel without
  `CONFIG_PSI` degrades to tick-driven sampling rather than losing the guard.

  The parameters are constrained by the kernel and were established by
  measurement on this host (7.2.5-cachyos), not assumed:

  | Trigger | root | unprivileged |
  | :--- | :---: | :---: |
  | `some 150000 1000000` | EINVAL | EINVAL |
  | `some 150000 2000000` | accepted | EINVAL |
  | `some 200000 2000000` | accepted | EINVAL |
  | `some 500000 2000000` | EINVAL | EINVAL |
  | `some 1000000 10000000` | accepted | EINVAL |

  Three rules follow: the window must be a multiple of 2,000,000 us, the
  threshold may not exceed one tenth of the window, and trigger creation
  requires `CAP_SYS_RESOURCE`. The first implementation used a 1 s window, was
  silently declined at runtime, and fell back to tick sampling - caught only
  because the daemon logs the fallback.
- **REQ-112.9 (Ladder)** Two escalating tiers, both reversible, **neither
  terminating nor halting**:

  | Tier | Entry condition (any) | Action |
  | :--- | :--- | :--- |
  | Advisory | SwapFree < 30% · MemAvailable < 15% · PSI full avg10 > 10 | `memory.reclaim` with `swappiness=0` (**file-backed pages only**) on Tier 4/5 processes holding ≥ 256 MiB; the power-side reclaim ladder switches to file-only for the duration |
  | Throttle | SwapFree < 12% · MemAvailable < 8% · PSI full avg10 > 25 | cgroup v2 `cpu.max = 20000 100000` (20%) + `SCHED_IDLE` on the largest non-protected holders |

- **REQ-112.10 (Hysteresis)** De-escalation requires SwapFree > 45% **and**
  MemAvailable > 25% **and** PSI full avg10 < 5, and proceeds one tier per
  evaluation. Releasing at the escalation threshold makes the guard oscillate
  against its own effect.
- **REQ-112.11 (Who may be slowed)** Safety tiers 0-2 (CriticalImmune,
  DesktopCore, DesktopShell) and the focused window
  ([`REF-REQ-085`](REQ-085-kde-active-window-resource-guarantee-and-c0-pinning.md))
  are never candidates, at any pressure level, at any resident size.
- **REQ-112.12 (No halting)** The ladder ends at CPU throttling. `SIGSTOP`,
  cgroup freezing and termination are **not** used. This is an explicit
  operator decision, taken in full knowledge of the limit stated in §3.3.
- **REQ-112.13 (Rollback)** A cgroup CPU quota is cgroup state and outlives the
  daemon. Every throttle shall be released on de-escalation and on shutdown,
  before the hardware baseline is restored.
- **REQ-112.14 (Swapless hosts)** `SwapTotal == 0` shall disable the swap
  criterion entirely, not read as "0% free".

### 3.3 Stated Limit - This Does Not Guarantee Zero Kills

A CPU quota is an allocation-*rate* brake, not a ceiling. A single thread that
allocates faster than the guard can throttle will still reach the kernel OOM
killer. The guard raises the pressure at which a kill becomes possible and buys
time; the 38.9 GiB of backing store from REQ-112.6 is what actually removes the
condition that produced the 13:28 kill. **Describing this as a guarantee would
be false.** The only in-process mechanism that could guarantee it is halting the
allocator (`SIGSTOP`), which REQ-112.12 excludes by decision.

**Not measured**: no induced-OOM test has been run against the guard on live
hardware. The tiers, thresholds, hysteresis and target selection are verified
on synthetic samples ([`REF-TEST-069`](#ref-test-069)); the actuation path under
genuine exhaustion is not.

---

## 4. Verification

### REF-TEST-068
`test_cpu_ceiling_baseline_is_hardware_max` - asserts
`scaling_max_freq_khz >= hw_max_freq_khz` after capture on the live host (the
invariant the old code could violate), that a repaired capture re-arms the boost
baseline, and that `assert_unrestricted_cpu_ceiling()` performs no write under
the actuation sandbox.

### REF-TEST-069
`test_memory_pressure_ladder` - the ladder on synthetic samples: swap-led
escalation in the exact shape of the 13:28 kill (swap gone, MemAvailable still
comfortable), PSI-only escalation, one-step hysteresis, refusal to release
inside the guard band, swapless-host safety, and target selection - tiers 0-2
and the focused window rejected however much memory they hold, small processes
and pid 1 rejected.

### REF-TEST-070
`test_memory_pressure_parsers` - `/proc/meminfo` and `/proc/pressure/memory`
parsing: line-anchored key matching (`SwapCached` must not be mistaken for
`SwapTotal`), truncated-buffer safety, absent-field handling, and PSI `avg10`
extraction including a kernel that exposes no `full` row. Partially closes the
unfuzzed-procfs-parser gap recorded in
[`REF-RES-029`](../research/RES-029-full-codebase-audit.md).
