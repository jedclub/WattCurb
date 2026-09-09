# [REF-ARCH-001] High-Level Daemon Architecture & Subsystem Specification

- **Ref-ID**: `REF-ARCH-001`, `REF-TEST-001`
- **Related Requirements**: [`REF-REQ-001`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-001-hardware-power-profiling.md), [`REF-REQ-002`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-001-hardware-power-profiling.md#ref-req-002), [`REF-REQ-003`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-001-hardware-power-profiling.md#ref-req-003)
- **Related Research**: [`REF-RES-001`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-001-prior-art-and-hardware-telemetry.md)
- **Status**: Draft

---

## 1. Architecture Overview

WattCurb is structured as a non-allocating, event-driven daemon designed around Linux epoll and modern C++23 (`std::expected`, `std::span`, concepts, `constexpr`).

```
+--------------------------------------------------------------------------+
|                                WattCurb Daemon                           |
|                                                                          |
|  +--------------------------------------------------------------------+  |
|  |                      Single-Instance Lock Manager                  |  |
|  |             (Linux Abstract Unix Domain Socket: @wattcurb.lock)     |  |
|  +--------------------------------------------------------------------+  |
|                                                                          |
|  +--------------------------------------------------------------------+  |
|  |                          Event Dispatcher                          |  |
|  |     (Linux epoll_wait: Netlink Socket + timerfd + signalfd)        |  |
|  +--------------------------------------------------------------------+  |
|          |                                   |                           |
|          v                                   v                           |
|  +------------------------+      +------------------------------------+  |
|  | Netlink Process Monitor|      |     Hardware Energy Profiler       |  |
|  |  - PROC_EVENT_FORK     |      |  - RAPL (CPU Package, Cores, DRAM) |  |
|  |  - PROC_EVENT_EXEC     |      |  - Battery (/sys/class/power_supply|  |
|  |  - PROC_EVENT_EXIT     |      |  - GPU hwmon / Backlight sysfs     |  |
|  +------------------------+      +------------------------------------+  |
|          |                                   |                           |
|          +-----------------+-----------------+                           |
|                            v                                             |
|  +--------------------------------------------------------------------+  |
|  |               Energy Attribution & Rule Engine                     |  |
|  |       - Calculates per-process work quantum / wattage share        |  |
|  |       - Identifies power-hogging & unneeded background tasks       |  |
|  +--------------------------------------------------------------------+  |
|                            |                                             |
|                            v                                             |
|  +--------------------------------------------------------------------+  |
|  |               Progressive Remediation Subsystem                    |  |
|  |       - Stage 1: SCHED_IDLE / SCHED_BATCH (Demote Priority)        |  |
|  |       - Stage 2: CPU Affinity (Isolate to E-cores)                 |  |
|  |       - Stage 3: Cgroups v2 cpu.max Throttling                     |  |
|  |       - Stage 4: Cgroups v2 cgroup.freeze / SIGSTOP               |  |
|  +--------------------------------------------------------------------+  |
+--------------------------------------------------------------------------+
```

---

## 2. Core Subsystems

### 2.1 Core & Singleton Subsystem
- **Class**: `wattcurb::core::SingletonLock`
- **Mechanism**: Linux Abstract Namespace Socket binding (`\0wattcurb.lock`). Automatically released by the kernel if the daemon terminates or crashes, eliminating stale lockfile issues.
- **Signal Handling**: Linux `signalfd` integrated into the central epoll loop for graceful teardown (`SIGTERM`, `SIGINT`, `SIGHUP`).

### 2.2 Hardware Profiler Subsystem
- **Class**: `wattcurb::hw::HardwareProfiler`
- **Drivers**:
  - `RaplProbe`: Reads `/sys/class/powercap/intel-rapl/intel-rapl:*/energy_uj`.
  - `BatteryProbe`: Reads `/sys/class/power_supply/BAT*/power_now` or calculates $V \times I$.
  - `GpuProbe`: Reads sysfs `power1_average` or `power1_input` from drm/hwmon.
  - `BacklightProbe`: Reads `/sys/class/backlight/*/brightness`.
- **Zero-Allocation**: Uses stack buffers and `std::from_chars` to parse integer registers without string allocations.

### 2.3 Process Tracking Subsystem
- **Class**: `wattcurb::proc::NetlinkProcConnector`
- **Mechanism**: Opens `AF_NETLINK` socket with `NETLINK_CONNECTOR`, sends `CN_IDX_PROC` subscription. Receives instant notifications on process lifecycle without querying `/proc` on a periodic timer.

### 2.4 Progressive Remediation Subsystem
- **Class**: `wattcurb::policy::ActionManager`
- Implements the 4-tier mitigation strategy:
  1. `sched_setscheduler(pid, SCHED_IDLE, ...)`
  2. `sched_setaffinity(pid, e_core_mask)`
  3. cgroup v2 write to `cpu.max`
  4. cgroup v2 write to `cgroup.freeze`

---

## 3. [`REF-TEST-001`] Automated Evaluation & Oracle Gate Architecture

To fulfill [`REF-REQ-003`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-001-hardware-power-profiling.md#ref-req-003):
1. **Unit Test Suite**:
   - Built under `tests/` using standard test runners.
   - Mocked sysfs and procfs structures for offline CI/CD execution without root or specific hardware.
2. **Oracle Gate Regression Check**:
   - Measures:
     - Peak Heap Allocations in monitor loop (Assertion: 0 bytes).
     - Execution latency per event (Assertion: $< 50\mu s$).
     - Valgrind/Sanitizer (ASan, UBSan) clean status.
   - Any commit failing these criteria triggers immediate rollback/fix loop.
