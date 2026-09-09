# [REF-REQ-007] Resident Background Daemon & Direct Kernel/Hardware Access Specification

- **Ref-ID**: `REF-REQ-007`
- **Related Research**: [`REF-RES-001`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-001-prior-art-and-hardware-telemetry.md), [`REF-RES-002`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-002-hardware-power-measurement-mechanisms.md), [`REF-RES-003`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-003-hardware-to-process-attribution.md)
- **Related Architecture**: [`REF-ARCH-004`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-004-resident-daemon-event-loop.md)
- **Status**: Approved / Implementation Phase

---

## 1. Objectives

Establish absolute cost-minimization directives for WattCurb when running continuously as a resident Linux daemon. The daemon must:
1. Eliminate all external process spawning (`fork`/`exec`/`popen`/`system` strictly prohibited).
2. Avoid external library dependencies, relying entirely on direct Linux kernel syscalls and libc.
3. Eliminate repetitive VFS pathname resolution via persistent open file descriptors (`pread`).
4. Guarantee crash-safe singleton execution via Linux Abstract Namespace UNIX domain sockets.
5. Provide on-demand live reporting to client CLI invocations without disk I/O churn.

---

## 2. Detailed Functional Requirements

### 2.1 Elimination of External Processes and Dependencies
- **Strict Prohibition**: The daemon must never invoke child processes (e.g. `lsof`, `cat`, `ps`, `awk`, `which`). All inspections must be performed via direct kernel syscalls (`open`, `pread`, `readlink`, `epoll_wait`).
- **Zero Third-Party Library Footprint**: Standalone binary using only standard Linux POSIX APIs (`sys/epoll.h`, `sys/timerfd.h`, `sys/signalfd.h`, `sys/prctl.h`, `sys/socket.h`, `sys/un.h`).

### 2.2 Direct Hardware Access via Persistent File Descriptors
- For all frequently sampled sysfs hardware counters (`power_now`, `power1_input`, `energy_uj`, `brightness`):
  - Open file descriptors once during discovery (`O_RDONLY | O_CLOEXEC`).
  - Sample via `pread(fd, buffer, size, 0)`.
  - Avoid repeated `open()` and `close()` syscalls, saving VFS path resolution and kernel inode/file struct allocation overhead.

### 2.3 Non-Filesystem Abstract Singleton Lock
- Single-instance enforcement must use a Linux Abstract Namespace UNIX domain socket (`\0wattcurb.lock`).
- Advantages:
  - No stale PID files on power cuts or abnormal termination (`SIGKILL`).
  - No filesystem writes or storage drive wakeups.
  - Automatically cleaned up by the kernel when the file descriptor closes.

### 2.4 Resident Event Loop & Zero-Wakeup Directives
- Implement event-driven `epoll` architecture:
  - `timerfd` configured with `CLOCK_BOOTTIME` and `PR_SET_TIMERSLACK_NS` ($500\text{ms} \sim 1000\text{ms}$) to coalesce wakeups with other system activity.
  - `signalfd` handling `SIGINT`, `SIGTERM`, `SIGHUP`, `SIGUSR1`.
  - Abstract socket for IPC queries (`wattcurb --status`).
- CPU utilization in idle resident state must remain $< 0.05\%$.
- Memory RSS must remain $< 5\text{MB}$.
