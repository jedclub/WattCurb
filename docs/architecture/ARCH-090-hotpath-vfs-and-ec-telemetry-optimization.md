# [REF-ARCH-090] Hot-Path VFS Single-Read & EC Telemetry Bus Topology

## 1. Single-Syscall Procfs Ingestion Topology

```
[Previous Implementation: 2 read() syscalls per file]
  openat() ──▶ read(fd, 2048) [returns 312B] ──▶ read(fd, 1736) [returns 0 (EOF)] ──▶ close(fd)
  (Result: 4 syscalls per procfs record * 333,692 = ~1,334,000 syscalls)

[Optimized Implementation: 1 read() syscall per file]
  openat() ──▶ read(fd, 2048) [returns 312B, null-terminate] ──▶ close(fd)
  (Result: 3 syscalls per procfs record * 333,692 = ~1,001,000 syscalls -> 333,000 syscalls eliminated!)
```

---

## 2. Dormant Process Inode Cache Topology

```
                  ┌───────────────────────────────┐
                  │    ProcessSample Inspection    │
                  └───────────────┬───────────────┘
                                  │
                  utime_ticks == prev->utime_ticks?
                  stime_ticks == prev->stime_ticks?
                  io_syscalls == prev->io_syscalls?
                                  │
                    ┌─────────────┴─────────────┐
                    ▼ YES                       ▼ NO (Active Process)
        ┌───────────────────────┐   ┌───────────────────────────┐
        │  Reuse prev->sockets  │   │ Open /proc/[pid]/fd       │
        │  Reuse prev->drm_fd   │   │ Call SYS_getdents64       │
        │  0-VFS Immediate Exit │   │ Execute readlink loop     │
        └───────────────────────┘   └───────────────────────────┘
```

---

## 3. Decoupled EC Chassis Thermal Sub-Sampling

```
Every 6 ticks (~6s):
  Poll fan_rpm_fd_ ONLY (Single LPC access, ~1.5ms)

Every 30 ticks (~30s):
  Poll fan_pwm_fd_ & chassis_temp_fd_ (Background thermal health)

Peak stall reduced from 16.34 ms -> < 2.5 ms!
```
