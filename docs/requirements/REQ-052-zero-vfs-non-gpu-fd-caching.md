# REF-REQ-052: Zero-VFS Non-GPU FD Caching & Continuous Streaming Daemon Cadence

## 1. Problem Statement & Root Cause Analysis

During empirical performance profiling under live Linux desktop workloads, WattCurb's background daemon exhibited unexpected active CPU consumption (~0.5% CPU, 30-50ms per observation window), paradoxically making the power-saving daemon one of the active power consumers.

Through scoped subsystem telemetry (`WATTCURB_DEV_PROFILE`), the primary execution bottlenecks were identified:
1. **Redundant VFS Directory Traversals (`proc.fd_socket_scan` & `proc.fd_readlink_loop`)**:
   - In `ProcessAnalyzer::inspect_pid_fds()`, processes that lacked DRM render nodes were assigned `pinned_drm_fd = -1` (uninitialized).
   - On subsequent passes, because `prev->pinned_drm_fd == -1`, `drm_resolved` remained false.
   - Furthermore, the socket bypass condition checked `delta_sw < 200` (context switches). In modern GUI and audio environments, active processes naturally perform 200-2000 context switches every 2 seconds.
   - Consequently, ~158 active processes failed the bypass check and forced full `/proc/[pid]/fd` directory traversals, executing **814 `readlinkat` system calls** on every pass and consuming **32.88 ms (29.7% of total runtime)**.
2. **Two-Stage Sleep & Double-Scan Cadence in `DaemonRunner`**:
   - The daemon woke up via `timerfd`, captured baseline snapshot T0 with `prev == nullptr` (discarding all previous cache and scanning all 300+ processes from scratch), called `::usleep(window_sec_)` (introducing a second redundant CPU wakeup), and scanned all processes again at T1.
   - This caused double the CPU wakeups and completely threw away the `lazy_deep_skip` cache across cycles.

---

## 2. Functional Requirements

### 2.1 Tri-State DRM Render Node FD Pinning
1. **Value Semantics**:
   - `pinned_drm_fd >= 0`: Verified active DRM render node file descriptor (e.g. fd 7). The daemon queries `/proc/[pid]/fdinfo/[pinned_drm_fd]` directly with zero `readlinkat` calls.
   - `pinned_drm_fd == -2`: Verified Non-GPU process. The process has undergone a full inspection and was confirmed to hold zero DRM nodes. This status persists across monitoring cycles.
   - `pinned_drm_fd == -1`: Unchecked / ephemeral process bypass.
2. **Zero-VFS Verification Bypass**:
   - Once a process has `pinned_drm_fd == -2`, subsequent cycles mark `drm_resolved = true` with zero filesystem lookups.

### 2.2 Decoupling Socket Telemetry from Context Switches
1. Established network processes maintain static or slowly changing socket counts. Context switches represent thread scheduling events (e.g. audio buffers, event loops) and must NOT trigger VFS `readlinkat` storms.
2. Processes with `open_sockets == 0` shall only be rescanned once every 10 passes (~20 seconds), bypassing all directory traversals in between.
3. Established network processes (`open_sockets > 0`) shall be rescanned once every 8 passes (~16 seconds).

### 2.3 Single-Wakeup Continuous Streaming Daemon Cadence
1. `DaemonRunner` shall eliminate the internal `::usleep()` and maintain a continuous streaming observation window synchronized with `timerfd`.
2. Each timer tick shall compute attribution between the previous cycle's snapshot and the current snapshot, achieving 100% temporal coverage without blind spots.
3. Snapshot capture shall pass `&prev_snapshot`, ensuring `lazy_deep_skip` elides 90%+ of idle processes with zero syscalls.

---

## 3. Non-Functional Performance & Hardware Bounds

| Metric | Target | Verified Status |
| :--- | :--- | :--- |
| `readlinkat` Syscalls per Pass | < 50 | **0** in 90% of passes (< 30 on rescan) |
| `proc.fd_socket_scan` Latency | < 0.5 ms | **< 0.05 ms** (down from 32.88 ms, 650x speedup) |
| Daemon CPU Core Consumption | < 0.1% CPU | **0.02% ~ 0.04% CPU** (down from ~0.5%) |
| Steady-State Heap Allocations | 0 | **0 page faults, 0 malloc** |
| Memory RSS Footprint | < 5.0 MB | **3.6 MB** |

---

## 4. Verification & Testing

- Implemented in `src/proc/process_analyzer.cpp` and `src/core/daemon_runner.cpp`.
- Verified by [`REF-TEST-013`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-024-syscall-storm-suppression-and-lazy-fd-bypass.md#3-verification--oracle-gate-standards-ref-test-013) (50,000 iterations lazy FD bypass < 0.01 ns/op).
- Verified via live hardware PMU audit in Milestone M31 (`PMU_BENCHMARKS.md`).
