# REF-ARCH-081: Tri-Stage Memory Hygiene & Safe Progressive Swap Recovery Engine

## 1. Architectural Overview

```
                                      [WattCurb Periodic Loop / Pressure Trigger]
                                                          │
                                                          ▼
    ┌───────────────────────────────────────────────────────────────────────────────────────────────────────────┐
    │                                            STAGE 1: DETECTION (탐지)                                       │
    │  ┌──────────────────────────────┐       ┌──────────────────────────────┐       ┌───────────────────────┐  │
    │  │     TmpfsHygieneProbe        │       │     SwapSaturationProbe      │       │ BackgroundBatchProbe  │  │
    │  │ • statvfs("/tmp", "/dev/shm")│       │ • /proc/swaps device topology│       │ • RSS > 500MB, no TTY │  │
    │  │ • Dead orphan directory scan │       │ • ZRAM vs Disk swap footprint│       │ • CPU time > 1800s    │  │
    │  └──────────────────────────────┘       └──────────────────────────────┘       └───────────────────────┘  │
    └─────────────────────────────────────────────────────┬─────────────────────────────────────────────────────┘
                                                          │
                                                          ▼
    ┌───────────────────────────────────────────────────────────────────────────────────────────────────────────┐
    │                                      STAGE 2: STRATEGY SELECTION (전략 선택)                                │
    │                       ┌─────────────────────────────────────────────────────────────┐                     │
    │                       │                  HygieneStrategySelector                    │                     │
    │                       │  • Power state: AC vs Battery                               │                     │
    │                       │  • User state: Active vs Idle (>= 300s)                     │                     │
    │                       │  • Margin: MemAvailable >= (SwapUsed * 2.0 + 3 GiB)?        │                     │
    │                       └──────────────────────────────┬──────────────────────────────┘                     │
    │                                                      │                                                    │
    │             ┌─────────────────────────┬──────────────┴───────────────┬────────────────────────┐           │
    │             ▼                         ▼                              ▼                        ▼           │
    │     [STRATEGY ALPHA]          [STRATEGY BETA]                [STRATEGY GAMMA]          [STRATEGY DELTA]   │
    │     Tmpfs Orphan Evict        Batch Soft-Clamp               Safe Idle Deswap          Hold & Protect     │
    └─────────────┬─────────────────────────┬──────────────────────────────┬────────────────────────┬───────────┘
                  │                         │                              │                        │
                  ▼                         ▼                              ▼                        ▼
    ┌───────────────────────────────────────────────────────────────────────────────────────────────────────────┐
    │                                    STAGE 3: SAFE RECLAMATION (안전하게 회수)                                │
    │  ┌──────────────────────────────┐ ┌──────────────────────────────┐ ┌────────────────────────────────────┐ │
    │  │      SafeTmpfsActuator       │ │    BatchWorkloadGovernor     │ │       GranularDeswapActuator       │ │
    │  │ • Gate 1: System Whitelist   │ │ • Zero-Kill guarantee        │ │ • Strict 2x+3GB margin assertion   │ │
    │  │ • Gate 2: S_IFSOCK/FIFO immunity│ • Set SCHED_IDLE            │ │ • Phased: /swap/swapfile first     │ │
    │  │ • Gate 3: Kernel FD scan     │ │ • Set IOPRIO_CLASS_IDLE      │ │ • ZRAM compaction trigger          │ │
    │  │ • Gate 4: Process tombstone  │ │ • Apply CFS quota (cpu.max)  │ │ • Immediate swapon restoration     │ │
    │  │ • Gate 5: Age > 3600 seconds │ │   under memory pressure      │ │                                    │ │
    │  └──────────────────────────────┘ └──────────────────────────────┘ └────────────────────────────────────┘ │
    └───────────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

This engine operationalizes [`REF-REQ-134`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-134-safe-memory-hygiene-and-progressive-swap-recovery.md), extending WattCurb's non-destructive memory pipeline ([`REF-REQ-130`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-130-safe-non-destructive-memory-reclaim.md), [`REF-ARCH-077`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-077-safe-memory-reclaim-and-smart-gc.md)).

---

## 2. Component Design & System Interfaces

### 2.1 Stage 1: Detection Subsystem & Cooldown Governor

#### 1. Hierarchical Gated Detection (Zero-Wakeup & Zero-Poll)
To prevent CPU wakeups and eliminate polling costs:
- **Level 0 (Zero-Wakeup Base)**: Passive. Triggered strictly on existing daemon low-frequency 60s periodic ticks or kernel PSI memory stall events (`EPOLLPRI` on `/proc/pressure/memory`).
- **Level 1 (Single-Syscall Fast Filter)**:
  - Invokes `statvfs("/tmp", &st)` (< 2 µs latency, O(1), reads VFS superblock in kernel memory).
  - Computes $\text{UsedRatio} = 1.0 - \frac{\text{f\_bavail}}{\text{f\_blocks}}$.
  - If $\text{UsedRatio} < 0.70$ and $\text{UsedBytes} < 2\,\text{GiB}$, **halts immediately**. Total CPU overhead: < 0.0001% core capacity. Zero directory traversal.
- **Level 2 (Conditional Deep Scan)**:
  - Evaluated ONLY if Level 1 crosses thresholds AND `m_tmpfs_cooldown_timer.expired()`.

#### 2. Strict Cooldown & Exponential Backoff State Machine
To guarantee that detection never loops indefinitely or creates background storms:
- **`m_tmpfs_scan_cooldown`**: Initialized to **900s (15 minutes)**.
- **`m_swap_eval_cooldown`**: Initialized to **1800s (30 minutes)**.
- **`m_post_reclaim_quench`**: When any reclamation executes, locks out further actions for **3600s (1 hour)**.
- **Exponential Backoff**:
  ```cpp
  if (orphans_found == 0 || margin_check_failed) {
      m_current_cooldown = std::min(m_current_cooldown * 2, 7200u); // 15m -> 30m -> 60m -> 120m
  } else {
      m_current_cooldown = 900u; // Reset to baseline on actionable recovery
  }
  ```

#### 3. Bounded Traversal & Zero-Allocation Probing
- **Recursion Ceiling**: Directory traversal is strictly clamped to `max_depth = 2` (`/tmp/<tool>/<session>`).
- **Entry Ceiling**: Maximum 64 `dirent` items inspected per scan cycle.
- **Zero Heap Overhead**: Fixed stack buffers (`alignas(64) char scratch[4096]`) and static fixed vectors.
- **$O(1)$ Process Tombstone Pre-filter**: Verifies process death via `kill(pid, 0) == -1 && errno == ESRCH` before touching the global `/proc/*/fd/` file descriptor table.

#### 4. Probes
- **`TmpfsHygieneProbe`**: Executes Level 1 fast filter and Level 2 bounded orphan scan.
- **`SwapSaturationProbe`**: Parses `/proc/swaps` with zero dynamic allocation (`pread()`), tracking Tier 1 (ZRAM) and Tier 2 (`/swap/swapfile`).
- **`BackgroundBatchProbe`**: Cross-references active processes against `/proc/<pid>/stat` (`tty_nr == 0`, `rss > 500 MiB`, cumulative CPU time > 1800s) excluding focused GUI PIDs ([`REF-REQ-085`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-085-window-focus-aware-process-governor.md)).

---

### 2.2 Stage 2: Strategy Selection Engine (`HygieneStrategySelector`)

The selector evaluates system state using a deterministic, pure decision matrix without side effects:

```cpp
enum class HygieneStrategy : uint8_t {
    HoldAndProtect = 0,    // Retain current state; do not touch swap or memory
    TmpfsOrphanEvict = 1,  // Evict dead tmpfs directories passing 5-layer gate
    BatchSoftClamp = 2,    // Demote runaway maintenance batch to SCHED_IDLE
    SafeIdleDeswap = 3     // Perform phased disk swap evacuation into RAM/ZRAM
};
```

#### Decision Matrix
| Power Supply | User State | Tmpfs Orphan Footprint | Swap Used | MemAvailable Headroom | Selected Strategy | Rationale |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Any** | Any | $> 1.0\,\text{GiB}$ (Dead) | Any | Any | **TmpfsOrphanEvict** | Unlinking tmpfs frees swap slots immediately with 0 RAM cost |
| **Any** | Any | $< 1.0\,\text{GiB}$ | Any | PSI > 10 & Batch Worker Runaway | **BatchSoftClamp** | Prevent swap thrashing and CPU starvation |
| **AC** | **Idle ($\ge 5\,\text{min}$)**| Negligible | $> 2.0\,\text{GiB}$ | $\ge (\text{Swap} \times 2.0 + 3\,\text{GiB})$ | **SafeIdleDeswap** | System returns to pristine cold-start state safely |
| **Battery**| Any | Negligible | $> 2.0\,\text{GiB}$ | Any | **HoldAndProtect** | Deswapping burns CPU cycles; strictly forbidden on battery |
| **AC** | Any | Negligible | $> 2.0\,\text{GiB}$ | $< (\text{Swap} \times 2.0 + 3\,\text{GiB})$ | **HoldAndProtect** | Inadequate memory margin; prevent kernel OOM killer |

---

### 2.3 Stage 3: Safe Reclamation Actuation

#### 1. `SafeTmpfsActuator` (5-Layer Immunity Barrier)
To guarantee zero application crashes and zero data loss:
- **Layer 1 (System Whitelist)**:
  Fixed string matching against critical runtime paths:
  `"/tmp/.X11-unix"`, `"/tmp/.ICE-unix"`, `"/tmp/wayland-"`, `"/tmp/pulse-"`, `"/tmp/pipewire-"`, `"/tmp/systemd-private-"`, `"/tmp/runtime-"`.
- **Layer 2 (File Type Immunity)**:
  `lstat()` verification: reject `S_IFSOCK`, `S_IFIFO`, `S_IFBLK`, `S_IFCHR`, or files ending in `".lock"`.
- **Layer 3 (Kernel FD Check)**:
  Scans active `/proc/*/fd/` links to verify target inode is NOT currently open by any living PID.
- **Layer 4 (Tombstone Verification)**:
  If the directory name embeds a PID or session ID (e.g. `/tmp/claude-<uid>`, `/tmp/CMakeCCompilerId-<pid>-*`), confirms the originating process has terminated.
- **Layer 5 (Age Gate)**:
  `st.st_mtime` must be $\ge 3600\,\text{seconds}$ in the past.

#### 2. `GranularDeswapActuator` (Safe Phased Swap Flushing)
- **Margin Pre-Assertion**:
  Before any system call, re-evaluates:
  ```cpp
  if (mem_avail_kb < (swap_used_kb * 2 + 3 * 1024 * 1024)) {
      return false; // Absolute abort: safety gate engaged
  }
  ```
- **Phased Execution**:
  1. Issues `swapoff("/swap/swapfile")`. Secondary disk swap pages migrate directly into `/dev/zram0` (fast RAM compression) and free RAM.
  2. Issues sysfs compaction: `write("/sys/block/zram0/compact", "1")` to eliminate memory fragmentation within ZRAM.
  3. Re-enables secondary swap: `swapon("/swap/swapfile", -1)` to guarantee emergency overflow headroom remains armed.

#### 3. `BatchWorkloadGovernor`
- Sets scheduler class:
  ```cpp
  struct sched_param sp{.sched_priority = 0};
  sched_setscheduler(pid, SCHED_IDLE, &sp);
  ```
- Sets I/O priority via `ioprio_set(IOPRIO_WHO_PROCESS, pid, IOPRIO_PRIO_VALUE(IOPRIO_CLASS_IDLE, 0))`.
- If memory pressure escalates, attaches process to cgroup v2 with `cpu.max = 20000 100000`.

---

## 3. Data Structures & Zero-Cost Memory Footprint

Hot telemetry structures align to cache lines (`alignas(64)`) to adhere to [`REF-REQ-002`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-002-zero-allocation-telemetry.md):

```cpp
struct alignas(64) MemoryHygieneSample {
    uint64_t tmpfs_total_kb{0};
    uint64_t tmpfs_used_kb{0};
    uint64_t shm_used_kb{0};
    uint64_t zram_used_kb{0};
    uint64_t disk_swap_used_kb{0};
    uint64_t mem_available_kb{0};
    uint32_t stale_orphan_dirs_count{0};
    uint64_t stale_orphan_bytes{0};
    int32_t runaway_batch_pid{0};
    bool on_ac_power{true};
    bool user_is_idle{false};
};
```

---

## 4. Verification & Testing Strategy (`REF-TEST-088`)

- Unit tests must prove:
  1. `test_strategy_selection_matrix_hold_on_low_margin`: Verifies that `SafeIdleDeswap` is rejected whenever `MemAvailable < SwapUsed * 2 + 3GB`.
  2. `test_strategy_selection_hold_on_battery`: Verifies that deswapping is completely inhibited on battery power.
  3. `test_tmpfs_orphan_scanner_whitelist`: Verifies that `.X11-unix`, active sockets, and locked files are 100% exempt from reclamation.
  4. `test_batch_governor_non_halting`: Verifies that runaway batch workloads are demoted to `SCHED_IDLE` without `SIGKILL`.
