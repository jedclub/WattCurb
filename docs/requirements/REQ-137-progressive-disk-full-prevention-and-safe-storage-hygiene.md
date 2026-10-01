# REF-REQ-137: Progressive Disk Full Prevention & Safe Storage Hygiene Engine

## 1. Operational Need & Motivation

On continuous, long-running Linux developer workstations and edge deployments, disk storage exhaustion (**100% ENOSPC**) causes severe catastrophic failures:
- SQLite databases, browser session caches, and journal files corrupt.
- Critical services crash due to failed `write()` syscalls.
- Desktop environments (KDE/Wayland, systemd) fail to allocate session sockets or tmpfs buffers.

While systemd and logrotate provide coarse maintenance, accumulated build artifacts, un-vacuumed systemd journals, package manager archive caches, and orphaned `/tmp` files frequently drive the host partition past **90% utilization**. WattCurb must provide an ultra-low-overhead, non-destructive **Storage Hygiene Engine** that progressively vacates disposable OS caches and logs whenever partition utilization exceeds 90%, preventing disk exhaustion without introducing CPU wakeups or I/O storms.

---

## 2. Functional Requirements

### REF-REQ-137.1: Zero-Wakeup Storage Utilization Sampling
* The daemon must inspect storage utilization on the primary root partition (`/`) using POSIX `statvfs`.
* **Sampling Cadence**:
  * Sampling must occur strictly at relaxed intervals (e.g., every 30 seconds) anchored to existing coordinated timer events. No busy polling or frequent stat loops are permitted.
* **Escalation Threshold**:
  * Storage pressure is engaged if and only if:
    $$\text{Disk Used Percentage} \ge 90.0\%$$

### REF-REQ-137.2: Anti-Churn Cooldown & Single-Batch Budget
* To prevent storage I/O bus saturation and disk thrashing:
  * **Cooldown Timer**: A mandatory cooldown of **300 seconds (5 minutes)** must lock out subsequent sweeps once a cleanup actuation completes (`is_cooldown_expired`).
  * **Single-Batch Reclaim Budget**: A single cleanup cycle must not exceed **512 MiB** of reclaimed files in one pass.
  * **Early Exit**: If disk utilization drops below **88.0%**, the sweep must terminate immediately.

### REF-REQ-137.3: Tiered Progressive Non-Destructive Reclaim Ladder
Reclaim actions must execute in strictly ordered, risk-minimized tiers:

1. **Tier 1 — High-Volume Disposable System Logs & Dumps**:
   * systemd journal vacuuming: trigger `journalctl --vacuum-size=200M` or prune rotated archived journals (`*.journal~`).
   * Clean old systemd core dumps (`/var/lib/systemd/coredump/core.*`) older than 3 days.
   * Clean WattCurb build/test ephemeral logs in `tmp/` older than 2 days.
2. **Tier 2 — Package Manager Archive Download Caches**:
   * Prune downloaded package cache archives (`/var/cache/apt/archives/*.deb`, `/var/cache/pacman/pkg/*.pkg.tar.*`, `/var/cache/dnf/*`).
   * Package metadata, database indexes, and partial locks must remain strictly untouched.
3. **Tier 3 — Orphaned Ephemeral Files in `/tmp` & `/var/tmp`**:
   * Sweep regular files older than **7 days** ($> 604,800\,\text{s}$).
   * **Strict Safety Invariants**:
     * NEVER delete UNIX domain sockets (`S_ISSOCK`), named pipes (`S_ISFIFO`), or lock/pid files (`.lock`, `.pid`).
     * NEVER recurse into or touch user home directories (`$HOME` / `/home/*`).
     * NEVER touch active XDG runtime directories (`/run/user/*`).

---

## 3. Dynamic SIMD Selection & Zero-Cost Abstraction

* Computational hot paths (including spline LUT generation and delimiter scanning) must utilize **dynamic CPU feature dispatch**:
  * Interrogate CPUID during process startup.
  * Bind function pointers once at bootstrap to specialize for AVX2+FMA, SSE2, or optimized scalar paths with zero branch overhead during runtime loops.

---

## 4. Verification Criteria & Ref-IDs

* Requirement: `REF-REQ-137`
* Architecture: `REF-ARCH-084`
* Test Specification: `REF-TEST-091` (Disk pressure trigger, 300s cooldown invariance, safe non-destructive file filter, dynamic SIMD dispatch equivalence).
