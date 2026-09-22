# [REF-ARCH-073] Dynamic Swap Expansion

**Implements**: [`REF-REQ-113`](../requirements/REQ-113-performance-mode-dynamic-swap-expansion.md)
**Date**: 2026-09-22

---

## 1. Where the profile split happens

```
MemoryPressureGuard::evaluate_and_actuate(report, focused_pid, profile)
   │
   ├─ sample() ── /proc/meminfo (cached fd) + /proc/pressure/memory
   ├─ classify() ── Normal | Advisory | Throttle   (pure, REF-TEST-069)
   ├─ expander.poll_pending() ── reap a creation child, swapon() on success
   │
   ├── profile == Performance ───────────────────────────────────────────┐
   │     expander.ensure_headroom(swap_total, swap_free)                 │
   │       └─ fork → btrfs mkswapfile / fallocate+mkswap  (seconds)      │
   │     tier Normal   → release CPU caps, expander.maybe_release()      │
   │     tier ≥ Advisory → file-only reclaim                             │
   │     tier Throttle AND throttle_permitted(...) → CPU brake           │
   │       (permitted only when nothing more can be added)               │
   │     otherwise → release_all(): hold NO cpu.max while we can grow    │
   │                                                                     │
   └── every other profile → the REF-REQ-112 ladder, unchanged ──────────┘
```

`throttle_permitted(profile, in_flight, exhausted)` is a pure one-liner so the
policy itself - not just its effects - is falsifiable:

```
Performance : !in_flight && exhausted
otherwise   : true
```

## 2. Why creation is forked

`btrfs filesystem mkswapfile --size 8g` allocates and formats eight gigabytes.
That is seconds of work, and the daemon is a single thread blocking in
`epoll_wait`: spending it inline would stop telemetry, stop the PSI trigger from
being serviced and stop the control socket from answering, exactly while the
machine is under memory pressure.

So the parent forks, records the child pid and the target path, and returns
immediately. Each subsequent tick calls `poll_pending()`, which is a
`waitpid(WNOHANG)`; when the child exits zero the parent issues `swapon()`
itself - a fast syscall - and only then counts the file as capacity.

The same reasoning applies in reverse to release. `swapoff()` faults every page
of the file back into RAM and can block for minutes. It also runs in a child,
because the syscall acts on the kernel rather than on the caller's address
space, so a child's `swapoff()` has exactly the global effect the parent wanted.

There is no shell anywhere on either path. The only variable is a path this
class composed from its own constants, and `execv()` takes an argument vector,
so there is no quoting surface at all.

## 3. Bounds

| Axis | Value | What it protects |
| :--- | ---: | :--- |
| Increment | 8 GiB | Bounds how long one expansion takes |
| In flight | 1 | Two concurrent 8 GiB allocations would thrash the disk |
| File ceiling | 4 (+32 GiB) | Bounds total disk commitment |
| Filesystem floor | 20 GiB free | The machine still needs a working disk |
| Unreadable `statvfs` | refuse | Unknown free space is not permission |

The trigger fires at 25% free (or 6 GiB absolute) rather than at exhaustion
specifically because growth is not instantaneous - the gap between the trigger
and empty is the time budget the child needs.

## 4. Priority

`swapon(path, 0)` - deliberately **without** `SWAP_FLAG_PREFER`. The kernel then
assigns the next free negative priority, placing every dynamic file below the
tiers the distribution configured:

```
/dev/zram0        prio 100   ← fast compressed tier, first
/swap/swapfile    prio  -2   ← disk overflow
wattcurb-dyn-0    prio  -3   ← emergency capacity, last
wattcurb-dyn-1    prio  -4
```

Dynamic capacity exists to stop the machine hitting zero, not to be used in
preference to anything.

## 5. Lifetime across restarts

A dynamic swapfile is disk state. It survives the daemon exactly as the
affinity masks of REF-REQ-110 and the frequency cap of REF-REQ-112 do, and it
is more expensive to leave behind than either: an orphan is tens of gigabytes
of disk consumed for nothing.

`initialize()` therefore walks the four possible names before any decision is
taken on current capacity:

| Found | State | Action |
| :--- | :--- | :--- |
| file exists, in `/proc/swaps` | still serving pages | **adopt** - track it so it can be released later |
| file exists, not in `/proc/swaps` | orphan from an unclean exit | **delete** - reclaim the disk |

Shutdown deliberately does **not** `swapoff()` everything: at that moment the
pages in those files have to go somewhere, and forcing them back during
shutdown is how a clean exit turns into an OOM. Active files are left in place
and adopted by the next run, which releases them when REQ-113.6 says it is
safe.

## 6. Interaction with REF-REQ-112

The two features are one ladder seen from two profiles:

| | Performance | Balanced / PowerSaver / UltraEndurance |
| :--- | :--- | :--- |
| Advisory | file-only reclaim | file-only reclaim |
| Pressure response | **grow swap** | **cgroup `cpu.max` 20% + `SCHED_IDLE`** |
| Brake | only when growth is impossible | at the Throttle tier |
| Release of dynamic swap | yes | yes (a tier created under Performance must not outlive it) |

`swap_feeding_suspended()` still holds in both: while the guard is above
Normal, the *power* reclaim ladder switches to file-only pages so a power
optimisation cannot spend the swap this is defending.
