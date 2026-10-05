# [REF-REQ-138] Autonomous Base Swap Right-Sizing & Zero-Downtime Migration Engine

**Status**: Implemented · **Date**: 2026-10-05  
**Related**: [`REF-ARCH-085`](../architecture/ARCH-085-base-swap-lifecycle-and-zero-downtime-migration.md),
[`REF-REQ-113`](REQ-113-performance-mode-dynamic-swap-expansion.md),
[`REF-REQ-112`](REQ-112-boost-restoration-and-non-halting-memory-guard.md),
[`REF-REQ-134`](REQ-134-safe-memory-hygiene-and-progressive-swap-recovery.md),
[`REF-REQ-137`](REQ-137-progressive-disk-full-prevention-and-safe-storage-hygiene.md),
[`REF-TEST-092`](#ref-test-092)

---

## 1. Context & Problem Statement

On contemporary Linux installations (specifically CachyOS, Arch, Fedora, and Ubuntu), standard installer presets (such as Calamares) allocate a fixed base disk swapfile sized to **2x physical RAM** (typically **32 GiB** for a 16 GiB system) to support hibernation.

When coupled with modern compressed in-memory tiers (`/dev/zram0`, 6 GiB zstd), this fixed 32 GiB allocation creates acute storage pressure:
1. **Severe Disk Starvation**: On edge and developer workstations where the primary NVMe storage reaches 90% utilization (e.g. 24 GiB remaining), a dormant 32 GiB swapfile permanently sequesters disk space that the filesystem urgently requires to prevent ENOSPC crashes ([`REF-REQ-137`](REQ-137-progressive-disk-full-prevention-and-safe-storage-hygiene.md)).
2. **Dormant Allocation**: Real-world telemetry demonstrates that during steady-state desktop execution, peak swap consumption rarely exceeds 4–5 GiB (absorbed mostly by the fast ZRAM tier). Holding 32 GiB indefinitely is pure waste.
3. **The Unsafe Manual Trap**: A naive manual resize (`swapoff -a && rm /swap/swapfile`) either triggers the kernel OOM Killer by dumping gigabytes into physical RAM or leaves the host completely swapless while formatting a new file.

WattCurb resolves this by introducing an autonomous, non-destructive **Base Swap Right-Sizing & Zero-Downtime Migration Engine**.

---

## 2. Functional Requirements

```
┌────────────────────────────────────────────────────────────────────────┐
│                        Stage 1: Observation Window                     │
│  • Track swap utilization across a continuous 2-hour (7200 s) window  │
│  • Peak swap used <= 5.6 GiB (70% safety headroom of target 8 GiB)    │
│  • Current base swapfile size >= 16 GiB (e.g. 32 GiB candidate)       │
└──────────────────────────────────┬─────────────────────────────────────┘
                                   │ PASS (Safety Gate Verified)
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│                        Stage 2: Pre-flight Verification                │
│  • statvfs(/swap) free space >= 16 GiB (permits allocating 8 GiB new) │
│  • Current active swap used <= 5.6 GiB (Zero-OOM mathematical lock)   │
│  • MitigationEngine::actuation_sandboxed() == false                    │
└──────────────────────────────────┬─────────────────────────────────────┘
                                   │ PASS (Zero-Downtime Pipeline)
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│                 Stage 3: Zero-Downtime Migration Sequence              │
│  1. Fork Child A: btrfs mkswapfile / fallocate 8 GiB at /swapfile.new │
│  2. Parent Tick: swapon(/swap/swapfile.new, 0) [Dual Active Swaps]    │
│  3. Fork Child B: swapoff(/swap/swapfile) [Kernel Paging Eviction]    │
│  4. Parent Tick: unlink(/swap/swapfile) & rename(.new -> /swapfile)   │
│  5. Reclaim ~24 GiB NVMe storage instantly; preserve /etc/fstab entry │
└──────────────────────────────────┬─────────────────────────────────────┘
                                   │ DONE
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│                 Stage 4: Post-Migration Elasticity Guarantee           │
│  • Base 8 GiB remains permanent; disk space recovered                  │
│  • If heavy parallel builds spike swap, REF-REQ-113 dynamically adds  │
│    +8 GiB slices (wattcurb-dyn-*.swap) and releases them after cooldown│
└────────────────────────────────────────────────────────────────────────┘
```

### REF-REQ-138.1 (Observation Window & Anti-Flapping Invariant)
* The engine must observe swap consumption over a minimum sliding window of **7,200 seconds (2 hours)**.
* Base swap downsizing shall be triggered IF AND ONLY IF:
  1. The peak swap utilization during the 2-hour window did not exceed **5.6 GiB** ($\approx 70\%$ of the target 8 GiB base).
  2. The current active base swapfile size is $\ge 16\,\text{GiB}$ (e.g. 32 GiB).
  3. The target base size is set to **8 GiB** ($8 \times 1024^3$ bytes).

### REF-REQ-138.2 (Strict Pre-flight Safety Gates)
Before initiating any filesystem or swap operations, the engine must assert:
1. **Zero-OOM Mathematical Invariant**:
   $$\text{SwapUsed}_{\text{current}} \le 0.70 \times \text{TargetBaseSize} \quad (\le 5.6\,\text{GiB})$$
2. **Disk Free Space Floor**:
   $$\text{DiskFreeBytes}_{/\text{swap}} \ge 16\,\text{GiB}$$
   Allocating the transient 8 GiB `.new` swapfile must never drop filesystem free space below 8 GiB.
3. **Sandbox Invariant**: If `MitigationEngine::actuation_sandboxed()` is active, zero disk writes, forks, or swap syscalls are executed.

### REF-REQ-138.3 (Non-Blocking Zero-Wakeup Daemon Architecture)
* In accordance with [`REF-REQ-001`](REQ-001-zero-wakeup-architecture.md) and [`REF-REQ-113.4`](REQ-113-performance-mode-dynamic-swap-expansion.md), the daemon's main `epoll` event loop MUST NEVER execute blocking I/O:
  * File creation (`btrfs filesystem mkswapfile` or `fallocate + mkswap`) MUST execute in a forked child process.
  * Deactivation (`swapoff`) MUST execute in a separate forked child process.
  * The parent daemon reaps completion non-blockingly via `waitpid(pid, &status, WNOHANG)` during its regular tick.

### REF-REQ-138.4 (Zero-Downtime Swap Continuity)
* The host must never transition through a "swapless" state:
  1. The new 8 GiB file `/swap/swapfile.new` is created and activated with `swapon()` **BEFORE** the legacy 32 GiB file is taken offline.
  2. Both swapfiles coexist simultaneously in `/proc/swaps`.
  3. The legacy file is deactivated via `swapoff()`. The Linux kernel automatically migrates all cached pages from the legacy file into the newly activated 8 GiB file and ZRAM.
  4. Once `swapoff()` completes successfully, the legacy file is unlinked, and `/swap/swapfile.new` is atomically renamed to `/swap/swapfile`.

### REF-REQ-138.5 (Fstab & Persistent Configuration Immutability)
* The target file retains the exact canonical path (`/swap/swapfile`).
* Existing `/etc/fstab` entries and systemd swap generator configurations remain 100% valid across system reboots without requiring privileged edits to `/etc/fstab`.

### REF-REQ-138.6 (Post-Migration Performance Guarantee via REF-REQ-113 Elasticity)
* Downsizing the base swapfile to 8 GiB must not constrain high-throughput workloads (e.g. parallel LLVM/Clang builds):
  * When memory pressure spikes, [`REF-REQ-113`](REQ-113-performance-mode-dynamic-swap-expansion.md) (`SwapExpander`) seamlessly attaches dynamic overflow files (`/swap/wattcurb-dyn-*.swap`) in 8 GiB increments up to +32 GiB.
  * Once the workload completes and pressure clears, dynamic slices are pruned, returning the system to its lean 8 GiB base.

---

## 3. Verification & Oracle Gate Standards (`REF-TEST-092`)

* **`test_base_swap_right_sizing_policy`**:
  1. Window & threshold verification: rejection if peak used > 5.6 GiB or base size < 16 GiB.
  2. Disk headroom pre-flight verification: rejection if free space < 16 GiB.
  3. Safe migration decision verification: assertion of mathematical bounds and state transitions.
  4. Sandboxed protection: assertion that no forks or syscalls are issued in test mode.
