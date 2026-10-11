# [REF-REQ-143] Hot-Path VFS Single-Read & EC Telemetry Bus Stall Optimization

## 1. Overview & Rationale

Empirical 30-minute continuous scoped profiling ([`REF-REQ-142`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-142-ultra-low-overhead-scoped-profiling-and-session.md), [`REF-ARCH-089`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-089-scoped-profiler-tsc-registry-and-30min-session.md)) revealed that:
1. CPU parsing logic (`SIMD stat_parse` at 0.67µs, `kthread_filter` at 0.07µs) accounts for < 1% of daemon active time.
2. Kernel VFS system calls and hardware EC bus stalls account for > 60% of total active CPU overhead:
   - `proc.stat_read`: 333,692 invocations, 5,507 ms (15.6% share) caused by redundant second `read(fd)` loop checks against procfs single-record files.
   - `proc.fd_socket_scan`: 26,260 directory opens and 2,862 `readlink` loops (8.6% combined share) caused by periodic scan storms on dormant processes.
   - `hw.fan_chassis`: 1,494 ms (4.2% share) with severe LPC/I2C bus stalls peaking at **16.34 ms per probe** due to sequential multi-field sysfs reads (`fan_rpm`, `fan_pwm`, `chassis_temp`).

---

## 2. Quantitative Optimization Requirements

1. **Procfs Single-Syscall Read Invariant**:
   - Small procfs status files (`/proc/[pid]/stat`, `/proc/[pid]/statm`, `/proc/[pid]/status`) must be read using exactly **one** `read(fd)` system call.
   - Elimination of the redundant second `read(EOF)` system call across 330k+ invocations, reducing `proc.stat_read` latency by $\ge 40\%$.
2. **Dormant Process FD Scan Bypass**:
   - When a process exhibits zero CPU tick delta (`utime == prev->utime && stime == prev->stime`) and zero I/O syscall delta (`io_syscalls == prev->io_syscalls`), its file descriptor table cannot have added network sockets.
   - FD directory open and readlink loops must be bypassed completely for dormant processes, eliminating $\ge 80\%$ of `inspect_pid_fds` invocations.
3. **Decoupled ThinkPad EC Bus Telemetry Cadence**:
   - `fan_rpm` is the only active mechanical telemetry required by the daemon.
   - Secondary EC metrics (`fan_pwm` and `chassis_temp`) must not be polled sequentially on the fast 1-second cadence.
   - Sub-sampling decoupling eliminates the 16ms hardware LPC bus freeze.
