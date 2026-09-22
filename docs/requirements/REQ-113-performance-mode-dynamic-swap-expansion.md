# [REF-REQ-113] Performance Mode Dynamic Swap Expansion

**Status**: Implemented · **Date**: 2026-09-22
**Related**: [`REF-ARCH-073`](../architecture/ARCH-073-dynamic-swap-expansion.md),
[`REF-REQ-112`](REQ-112-boost-restoration-and-non-halting-memory-guard.md),
[`REF-REQ-110`](REQ-110-process-state-repair-on-bootstrap.md),
[`REF-TEST-071`](#ref-test-071)

## 1. Why

[`REF-REQ-112`](REQ-112-boost-restoration-and-non-halting-memory-guard.md) gave
the daemon a memory-pressure ladder that ends in a cgroup CPU quota, and applied
it in every profile. That made the guard the **only** remaining restriction in
Performance mode - a profile whose entire contract, asserted by `REF-REQ-099`
and `REF-REQ-104` (both of which exist only as code comments; neither has a
requirement document, see §5), is that nothing is held back.

Worse, the throttle's most likely target in Performance is the workload the
profile exists for. A parallel build's `cc1plus`/`clang++` processes are safety
tier 4-5, an LTO link comfortably exceeds the 256 MiB PSS floor, and they are
not the focused window - so the exact job the user chose Performance to
accelerate is the first thing capped at 20% CPU.

The operator's direction: **in Performance, endure by growing swap rather than
by slowing the workload.** Paying in I/O and disk space is the right currency
for that profile; paying in CPU is not.

## 2. Requirements

- **REQ-113.1 (Growth instead of braking)** While the effective profile is
  `Performance`, the daemon shall respond to swap depletion by adding swap
  capacity. The CPU throttle of REQ-112.9 shall not be applied in that profile
  while capacity can still be added.
- **REQ-113.2 (Trigger)** Expansion shall start when free swap falls below
  **25%** of the tier **or** below **6 GiB** absolute, whichever comes first.
  The absolute bound matters on a small tier, where 25% can be a few hundred
  megabytes; the relative bound matters on a large one.
- **REQ-113.3 (Bounds)** Expansion shall be bounded on three axes:
  - increment **8 GiB** per step, at most one in flight;
  - at most **4 dynamic files** (ceiling **+32 GiB**);
  - the filesystem holding them shall never be left with less than **20 GiB**
    free. An unreadable free-space figure counts as *no* permission, not as
    permission.
- **REQ-113.4 (Non-blocking)** Creating an 8 GiB swapfile takes seconds, which
  the single-threaded `epoll` loop cannot spend. Creation shall run in a forked
  child, with `swapon()` performed by the parent when the child is reaped
  successfully on a later tick. No shell shall be involved on this path.
- **REQ-113.5 (Priority)** Dynamic files shall be swapped on **without**
  `SWAP_FLAG_PREFER`, so the kernel assigns the next negative priority and they
  sit below every tier the distribution configured. Dynamic capacity is
  last-resort overflow, not a tier to prefer.
- **REQ-113.6 (Release safety)** Capacity shall be returned only when what is
  still paged out would fit in the remaining tiers with 30% headroom.
  `swapoff()` faults every page of the file back into memory; doing that while
  memory is tight would cause the exhaustion this feature prevents. The
  `swapoff()` call shall itself run in a forked child, since it can block for a
  long time.
- **REQ-113.7 (Bootstrap adoption)** A dynamic swapfile is disk state and
  outlives the daemon - the same category as the affinity masks of
  [`REF-REQ-110`](REQ-110-process-state-repair-on-bootstrap.md) and the
  frequency cap of REQ-112. At startup the daemon shall adopt files that are
  still swapped on, so they can be released later, and **delete** files that
  are not, so an unclean exit does not leave tens of gigabytes stranded.
- **REQ-113.8 (Last resort)** When no further capacity can be added - file
  ceiling reached or disk floor crossed - the Performance path shall fall back
  to the REQ-112.9 CPU throttle and log the fallback once. The remaining
  alternative is a kernel `SIGKILL`, which REQ-112 exists to avoid. **This is
  an assumption, not an instruction**: the operator's direction covered growing
  swap, not what to do when growth is impossible.
- **REQ-113.9 (Other profiles unchanged)** PowerSaver, Balanced and
  UltraEndurance keep the REQ-112 ladder. Growing swap there would spend disk
  and write energy in profiles whose purpose is to spend less of both;
  releasing capacity, however, is permitted in every profile so a tier created
  under Performance does not persist after a profile change.
- **REQ-113.10 (Sandbox containment)** Under the actuation sandbox no file is
  created and no `swapon()`/`swapoff()` is issued. The Oracle Gate must never
  allocate gigabytes on a developer's disk.

## 3. Trade-offs, Stated

- **Disk for CPU.** Up to 32 GiB of the filesystem can be consumed while
  Performance is under pressure. The 20 GiB floor bounds the damage, but a
  nearly-full disk gets no expansion at all and falls straight to REQ-113.8.
- **Latency.** Growth is not instant: the child takes seconds to allocate and
  format 8 GiB, and the trigger fires at 25% free rather than at exhaustion
  precisely to buy that time. A runaway allocation faster than the expansion
  can complete still reaches the kernel OOM killer.
- **Write amplification.** More swap means more pages actually written out.
  That is the intended cost of the profile: I/O instead of CPU.
- **btrfs.** On btrfs a swapfile must be NOCOW, uncompressed and fully
  allocated. `btrfs filesystem mkswapfile` is the only thing that gets all
  three right, so it is used when the target filesystem is btrfs and the binary
  is present; otherwise the child allocates with `fallocate()` and execs
  `mkswap`.

## 4. Verification

### REF-TEST-071
`test_performance_swap_expansion_policy`:

- **Growth thresholds** - 77% free does not expand; 20% free does; 41% free on
  a small tier does, via the 6 GiB absolute bound; a swapless host never does.
- **Disk budget** - exactly `increment + floor` is allowed, one byte less is
  not, the 4-file ceiling holds against 500 GiB of free disk, and an unreadable
  free-space figure is refused.
- **Release safety** - releasing is allowed with 4 GiB paged out of 39 GiB,
  refused with 35 GiB paged out, and refused outright when the file is as large
  as the whole tier.
- **Brake policy** - `throttle_permitted()` is false in Performance while an
  expansion is in flight or the budget still allows one, true once nothing more
  can be added, and true in every other profile.
- **Sandbox containment** - a sandboxed `ensure_headroom()` under synthetic
  pressure creates nothing and leaves no expansion in flight.

**Not measured**: no induced-exhaustion run has exercised the live path on this
host. Creation, `swapon()`, adoption of an orphan and the release path have not
been observed under real pressure - only the decision functions are covered.

---

## 5. Documentation Gap Noted While Writing This

`REF-REQ-097` through `REF-REQ-105` are cited throughout `src/policy/` but have
**no requirement documents and no INDEX entries**. REQ-099 ("cluster pinning is
a capacity cut, not an optimisation, in Performance") and REQ-104 ("Performance
mode applies no process throttling at all") are the two this document depends
on, and both exist only as comments in `mitigation_engine.cpp` and
`battery_feature.cpp`. That violates the bi-directional synchronisation rule in
AGENTS.md §4. Recorded here rather than silently linked to files that do not
exist.
