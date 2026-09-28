# REF-REQ-134: Safe Memory Hygiene & Progressive Swap Recovery Specification

## 1. Context & Motivation

Through empirical production analysis on developer Linux workstations running modern AI agent workloads and long-duration background toolchains (e.g. Claude Code, Git maintenance, compilers, Vulkan/SVG test benches), three critical system resource leakage vectors were identified:

1. **Transient Tmpfs Inflation**: RAM-backed filesystems (`/tmp`, `/dev/shm`) accumulate multi-gigabyte orphaned directories (e.g. `/tmp/claude-*`, compiler scratchpads, PPM frames). Because tmpfs files reside in memory/swap, they remain permanently allocated even after the generating process exits.
2. **Persistent Swap Holding (Lazy Swap-in Trap)**: Due to Linux's lazy swap-in design, pages pushed to swap/ZRAM during transient pressure are never reclaimed back to RAM, even when the system returns to an idle state with ample available physical memory. Uncoordinated manual flushing (`swapoff -a`) creates an extreme risk of invoking the kernel OOM Killer by flooding RAM.
3. **Unchecked Background Batch Contention**: Long-running background maintenance processes (`git repack`, `git pack-objects`) can run for hours, consuming gigabytes of RSS and inducing severe swap thrashing and battery drain while the user is unaware.

To solve these failure modes without introducing system instability, WattCurb specifies a **Tri-Stage Architecture**:
**Detection (탐지) → Strategy Selection (전략 선택) → Safe Reclamation (안전 회수)**.

---

## 2. Functional Requirements

```
┌─────────────────────────┐     ┌─────────────────────────┐     ┌─────────────────────────┐
│   Stage 1: Detection    │ ──> │ Stage 2: Strategy Select│ ──> │ Stage 3: Safe Reclaim   │
│ • Tmpfs usage & orphans │     │ • AC vs Battery policy  │     │ • 5-layer Tmpfs gates   │
│ • Swap/ZRAM saturation  │     │ • Margin verification   │     │ • 2x + 3GB swap margin  │
│ • Batch worker runaway  │     │ • Strategy classification│    │ • Granular deswapping   │
└─────────────────────────┘     └─────────────────────────┘     └─────────────────────────┘
```

### 2.1 Stage 1: Detection (탐지)
- **REF-REQ-134.1 (Hierarchical Ultra-Low-Overhead Gating)**:
  - To preserve the Zero-Wakeup invariant ([`REF-REQ-001`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-001-zero-wakeup-architecture.md)), detection MUST NOT poll or continuously crawl filesystems.
  - **Level 0 (Zero-Wakeup Base)**: Detection evaluates exclusively during existing low-frequency daemon cycles (60s tick) or on kernel PSI memory pressure events (`EPOLLPRI` on `/proc/pressure/memory`).
  - **Level 1 (Single-Syscall Fast Filter)**: Executes a single `statvfs("/tmp", &st)` (< 2 µs latency, zero heap allocation). If `/tmp` utilization is below **70% capacity** AND absolute footprint is below **2 GiB**, Stage 1 terminates immediately. Zero directory crawling is performed.
  - **Level 2 (Conditional Deep Scan)**: Directory inspection executes ONLY when Level 1 triggers AND the strict Cooldown Timer has elapsed.
- **REF-REQ-134.2 (Strict Cooldown & Exponential Backoff Invariants)**:
  - **Tmpfs Deep Scan Cooldown**: Enforces a minimum interval of **900 seconds (15 minutes)** between filesystem directory scans.
  - **Swap Reclaim Evaluation Cooldown**: Enforces a minimum interval of **1800 seconds (30 minutes)** between swap deswapping evaluations.
  - **Post-Action Quench Window**: Following any successful reclamation action, a mandatory **3600-second (1 hour)** lockout is engaged to prevent oscillations.
  - **Exponential Backoff on Inaction**: If a deep scan discovers no actionable orphans, or if safety gates abort reclamation, the cooldown doubles ($15\,\text{min} \to 30\,\text{min} \to 60\,\text{min}$, capped at $120\,\text{min}$) to guarantee detection never loops indefinitely.
- **REF-REQ-134.3 (Bounded Scanning & Zero-Allocation Constraints)**:
  - Directory traversal depth is strictly capped at **depth 2** (`/tmp/<tool>/<session>`). Unbounded recursion is forbidden.
  - Maximum inspected entries per cycle is capped at **64 entries**. If a directory contains more entries, evaluation is truncated to prevent CPU spikes.
  - Memory buffers must be stack-allocated and cache-line aligned (`alignas(64)`), strictly adhering to zero heap allocations in the monitoring path.
  - Employs $O(1)$ tombstone pre-filtering: checks process survival via `kill(pid, 0)` or stat on `/proc/<pid>` before performing any global file descriptor table sweeps.
- **REF-REQ-134.4 (Swap Saturation & Device Classification)**:
  - Monitors `/proc/swaps` and `/sys/block/zram0/` to distinguish high-speed in-memory ZRAM from secondary disk swap (`/swap/swapfile`).
  - Flags swap reclamation candidates when total swap utilization exceeds **2 GiB** while physical `MemAvailable` is substantial.
- **REF-REQ-134.5 (Runaway Background Batch Identification)**:
  - Tracks non-interactive background processes (sessions lacking a controlling TTY and without active GUI window focus per [`REF-REQ-085`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-085-window-focus-aware-process-governor.md)).
  - Identifies maintenance batch workloads when RSS exceeds **500 MiB** or continuous CPU time exceeds **1800 seconds** (e.g. `git pack-objects`, `ccache`, offline indexing).

### 2.2 Stage 2: Strategy Selection (전략 선택)
- **REF-REQ-134.5 (Hardware & Power Context Evaluation)**:
  - Probes system power state (`AC` vs `Battery` via `/sys/class/power_supply/`).
  - Evaluates user idle state: requires at least **300 seconds** (5 minutes) of user input inactivity (display idle or lock).
- **REF-REQ-134.6 (Strategy Decision Matrix)**:
  - **Strategy A (Tmpfs Orphan Eviction)**:
    - *Condition*: Dead orphaned tmpfs files > 1 GiB.
    - *Priority*: Always executed **before** any swap reclamation, because unlinking tmpfs immediately frees swap slots at zero RAM cost.
  - **Strategy B (Background Batch Soft-Clamp)**:
    - *Condition*: Runaway batch process detected under memory or swap pressure.
    - *Actuation*: Apply `SCHED_IDLE` scheduler class and `I/O Class 3 (IDLE)`. Under memory pressure (PSI > 10.0), apply CFS quota (`cpu.max = 20000 100000`). Never terminate (`Zero-Kill`).
  - **Strategy C (Safe Idle Deswap)**:
    - *Condition*: System is Idle (5+ min), AC connected, CPU load < 10%, AND strict memory margin satisfies:
      $$\text{MemAvailable} \ge (\text{SwapUsed} \times 2.0 + 3\,\text{GiB})$$
    - *Action*: Proceed to Stage 3 granular deswapping.
  - **Strategy D (Hold & Protect)**:
    - *Condition*: Memory margin test fails ($\text{MemAvailable} < \text{SwapUsed} \times 2.0 + 3\,\text{GiB}$) OR on Battery.
    - *Action*: Inhibit all swap flushing. Keep compressed pages safely in ZRAM to prevent OOM.

### 2.3 Stage 3: Safe Reclamation (안전하게 회수)
- **REF-REQ-134.7 (5-Layer Tmpfs Immunity Gate)**:
  - Before unlinking any file or directory in `/tmp`:
    1. *System Whitelist*: Permanently exempt `/tmp/.X11-unix`, `/tmp/.ICE-unix`, `/tmp/wayland-*`, `/tmp/pulse-*`, `/tmp/pipewire-*`, `/tmp/systemd-private-*`.
    2. *File Type Immunity*: Reject sockets (`S_IFSOCK`), FIFOs (`S_IFIFO`), device nodes, and `.lock` files.
    3. *Kernel FD Scan*: Scan `/proc/*/fd/` across all living PIDs; if any process holds an open file descriptor or `mmap` reference, exempt immediately.
    4. *Process Tombstone*: Confirm owner PID does not exist in `/proc`.
    5. *Age Hysteresis*: Confirm `mtime` > 3600 seconds.
- **REF-REQ-134.8 (Granular Phased Deswapping)**:
  - Do NOT issue `swapoff -a` (which dumps all devices simultaneously).
  - Phase 1: Reclaim secondary slow disk swap (`swapoff /swap/swapfile`), migrating pages into ZRAM and physical RAM.
  - Phase 2: Retain `/dev/zram0` active for responsive application caching.
  - Re-evaluate `MemAvailable` after Phase 1; if headroom remains > 50%, compact ZRAM via sysfs trigger (`echo 1 > /sys/block/zram0/compact`).
  - Re-enable the disk swap device (`swapon /swap/swapfile -p -1`) to restore emergency backing capacity.

---

## 3. Non-Functional & Safety Constraints

1. **Zero-Kill Guarantee**: Under no circumstances may `SIGKILL` or `SIGTERM` be transmitted to any user or background process.
2. **Zero-OOM Guarantee**: Swap reclamation is mathematically locked out unless physical RAM has at least 200% headroom plus 3 GiB headroom over the swap footprint.
3. **Zero Active File Corruption**: Active files with open file descriptors in `/proc` are 100% immune from deletion.
4. **Energy Efficiency (Zero-Wakeup & AC Only)**: Deswapping involves CPU-intensive decompression and is strictly forbidden when operating on battery.

---

## 4. Verification & Oracle Gate Standards (`REF-TEST-088`)

- **`test_tmpfs_detection_and_orphan_scanning`**: Verify synthetic tmpfs directories with various ages and simulate open FD vs closed FD.
- **`test_swap_margin_gate_validation`**: Verify mathematical gate logic under low RAM, high swap, boundary conditions, and verify rejection when headroom is insufficient.
- **`test_batch_governor_sched_idle_demotion`**: Verify scheduler class and I/O nice demotion on simulated runaway worker without process termination.
