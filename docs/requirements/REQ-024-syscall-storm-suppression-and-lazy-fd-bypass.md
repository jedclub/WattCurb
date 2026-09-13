# [REF-REQ-027] Syscall Storm Suppression, Lazy FD Bypassing & Subsampled Sysfs Telemetry

- **Ref-ID**: `REF-REQ-027`
- **Related Architecture**: [`REF-ARCH-017`](../architecture/ARCH-017-syscall-storm-suppression-architecture.md)
- **Related Tests**: [`REF-TEST-013`](../../tests/test_units.cpp)
- **Status**: Approved / Implemented

---

## 1. Problem Statement & Motivation

During fine-grained subsystem scoped profiling (Milestone M23 analysis), Linux kernel Virtual File System (VFS) syscalls accounted for over **90% of cumulative execution time (197.95 ms)**:
1. **FD Traversal Syscall Storm**: Scanning `/proc/[pid]/fd` via `SYS_getdents64` and `SYS_readlinkat` on all active processes consumed **72.1 ms (36.4% of total daemon runtime)**, despite over 85% of processes never holding network sockets or DRM handles.
2. **ACPI EC SMBus Polling Penalties**: Querying chassis fan RPM and battery AC adapter online state triggered blocking embedded controller bus transactions, wasting **7.0 ms (3.5% of total runtime)** across monitoring passes.
3. **Redundant Process Metadata I/O**: Reading `/proc/[pid]/io` and `/proc/[pid]/statm` for processes with zero tick delta triggered unnecessary VFS file descriptor allocations.

---

## 2. Functional & Performance Requirements

1. **Lazy FD Bypassing (REF-REQ-027-1)**:
   - Ephemeral and low-activity processes ($(\text{utime} + \text{stime}) < 20$ ticks and $< 50$ context switches) must bypass directory open and `readlinkat` scans entirely.
   - Non-network processes ($\text{prev\_sockets} == 0$) with switch delta $< 200$ must pace FD directory re-evaluations to once every 8 passes (~16s), preserving previous socket counts.
   - Established network processes ($\text{prev\_sockets} > 0$) must pace FD re-evaluations to once every 6 passes (~12s).
2. **Relative `openat` Directory Traversal (REF-REQ-027-2)**:
   - The process analyzer must utilize existing directory file descriptors (`proc_dfd`) to perform `openat(proc_dfd, "12345/fd", ...)` avoiding absolute string path formatting and root VFS dentry path resolution.
   - Symbolic link scanners must immediately reject `DT_REG` and `DT_DIR` entries without issuing `SYS_readlinkat`.
3. **Subsampled Hardware Telemetry Caching (REF-REQ-027-3)**:
   - Chassis fan RPM and PWM queries must be subsampled to once every 6 passes (~12s), serving cached metrics on intermediate passes to avoid ACPI EC bus stalls.
   - AC adapter online state (`ac_online_fd_`) must be subsampled to once every 4 passes (~8s).
4. **Interleaved Metadata Pacing (REF-REQ-027-4)**:
   - For processes with low execution deltas ($\Delta \text{ticks} < 2$), `/proc/[pid]/io` and `/proc/[pid]/statm` queries must be interleaved across alternating passes (even passes for `statm`, odd passes for `io`), cutting metadata syscalls in half.

---

## 3. Verification & Oracle Gate Standards ([`REF-TEST-013`])

- **Bypass Latency**: `inspect_pid_fds` under bypass conditions must achieve an average latency of $\le 50.0\ \text{ns/op}$ in micro-benchmarks (achieved **$12.07\ \text{ns/op}$ / 20.5 cycles**).
- **Cumulative Runtime Reduction**: Dev profiler cumulative instrumented time must drop by $\ge 15\%$ (achieved **-18.1% reduction**, $197.95\ \text{ms} \rightarrow 162.20\ \text{ms}$).
- **Fan & AC Overhead Reduction**: Chassis and battery AC checking time must decrease by $\ge 50\%$ (achieved **-52.5%** on fan and **-67.0%** on AC check).
