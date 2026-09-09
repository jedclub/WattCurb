# [REF-ARCH-004] Resident Daemon & Direct Access Epoll Architecture

- **Ref-ID**: `REF-ARCH-004`, `REF-TEST-004`
- **Related Requirements**: [`REF-REQ-007`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-004-resident-daemon-direct-access.md)
- **Status**: Approved / Implementation Phase

---

## 1. System Architecture

```
+-----------------------------------------------------------------------------------+
|                           WattCurb Resident Daemon                                |
|                                                                                   |
|  +-----------------------------------------------------------------------------+  |
|  |             SingletonLock (\0wattcurb.lock - Abstract UNIX Socket)          |  |
|  +-----------------------------------------------------------------------------+  |
|                                                                                   |
|  +-----------------------------------------------------------------------------+  |
|  |                           Linux epoll_wait Dispatcher                       |  |
|  |                                                                             |  |
|  |   [timerfd] ------------> Periodic Analysis Tick (prctl TIMERSLACK aligned) |  |
|  |   [signalfd] -----------> Graceful Shutdown / SIGHUP config reload          |  |
|  |   [ipc_socket] ---------> Client Query (wattcurb --status / --live)         |  |
|  +-----------------------------------------------------------------------------+  |
|                                         |                                         |
|                                         v                                         |
|  +-----------------------------------------------------------------------------+  |
|  |                       HardwareProbe with Persistent FDs                     |  |
|  |    - battery_fd_    (pread 0)  -> /sys/class/power_supply/BAT*/power_now    |  |
|  |    - gpu_power_fd_  (pread 0)  -> /sys/class/drm/card*/hwmon*/power1_input  |  |
|  |    - rapl_pkg_fd_   (pread 0)  -> /sys/class/powercap/intel-rapl/energy_uj  |  |
|  |    - backlight_fd_  (pread 0)  -> /sys/class/backlight/*/brightness         |  |
|  |    * ZERO path lookup overhead, ZERO open/close syscalls per tick           |  |
|  +-----------------------------------------------------------------------------+  |
|                                         |                                         |
|                                         v                                         |
|  +-----------------------------------------------------------------------------+  |
|  |               ProcessAnalyzer & AttributionEngine (In-Memory)               |  |
|  |    - Live AnalysisReportData cached in memory                               |  |
|  |    - Serviced via Abstract IPC socket without disk writes                   |  |
|  +-----------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------+
```

---

## 2. Core Components

### 2.1 `wattcurb::core::SingletonLock`
- Uses `socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0)`.
- Binds to abstract name `\0wattcurb.lock`.
- If `bind()` fails with `EADDRINUSE`, another daemon is running; immediately exit with message.
- Provides IPC server capabilities: receives query packet and replies with latest cached JSON report over the same socket.

### 2.2 `wattcurb::hw::HardwareProbe` (Persistent FDs)
- Resolves sysfs symlinks once at startup.
- Opens file descriptors with `O_RDONLY | O_CLOEXEC` and stores them:
  - `int battery_fd_{-1};`
  - `int gpu_fd_{-1};`
  - `int rapl_pkg_fd_{-1};`
  - `int rapl_core_fd_{-1};`
  - `int backlight_fd_{-1};`
- Each tick calls `pread(fd, buffer, size, 0)` followed by `std::from_chars`.
- Destructor closes all descriptors.

### 2.3 `wattcurb::core::DaemonRunner`
- Creates epoll instance via `epoll_create1(EPOLL_CLOEXEC)`.
- Instantiates `timerfd_create(CLOCK_BOOTTIME, TFD_NONBLOCK | TFD_CLOEXEC)`.
- Masks signals (`SIGINT`, `SIGTERM`, `SIGHUP`, `SIGUSR1`) and binds them to `signalfd`.
- Epoll event loop processes timers, signals, and IPC requests without busy-polling.

---

## 3. [`REF-TEST-004`] Oracle Gate Unit Test Specifications

1. **Singleton Lock Verification**:
   - Acquire primary singleton lock: Assert success.
   - Attempt secondary lock acquisition: Assert failure with `EADDRINUSE`.
   - Release primary lock: Assert secondary acquisition succeeds.
2. **Persistent FD Reading Verification**:
   - Perform repeated `pread()` on hardware file descriptor: verify consistency with standard reads.
3. **Overhead & Memory Telemetry**:
   - Ensure daemon memory RSS $< 5\text{MB}$.
   - Ensure zero calls to `fork()` or external binaries.
