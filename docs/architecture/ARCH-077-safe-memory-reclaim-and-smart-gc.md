# REF-ARCH-077: Non-Destructive Memory Recovery & Smart GC Architecture

## 1. Architectural Overview

```
                            ┌───────────────────────────────────────────────┐
                            │   WattCurb Memory Pressure Supervisor         │
                            │   (Event-Driven PSI & /proc/meminfo Monitor)  │
                            └───────────────────────┬───────────────────────┘
                                                    │
                ┌───────────────────────────────────┼───────────────────────────────────┐
                ▼                                   ▼                                   ▼
   [Tier 1: Cooperative GC]            [Tier 2: Proactive Pageout]        [Tier 3: Cgroup Reclaim]
 ┌─────────────────────────────┐     ┌─────────────────────────────┐    ┌─────────────────────────────┐
 │ LowMemoryNotifier (D-Bus)   │     │ ProcessPageoutActuator      │    │ CgroupMemoryReclaimer       │
 │ - Broadcast D-Bus Signal    │     │ - pidfd_open(target_pid)    │    │ - Write to target slice     │
 │   LowMemoryWarning(100/255) │     │ - process_madvise()         │    │   memory.reclaim            │
 │ - In-app cache flush & GC   │     │   with MADV_PAGEOUT         │    │ - Reclaim unmapped pages    │
 └─────────────────────────────┘     └─────────────────────────────┘    └─────────────────────────────┘
                │                                   │                                   │
                ▼                                   ▼                                   ▼
 ┌─────────────────────────────┐     ┌─────────────────────────────┐    ┌─────────────────────────────┐
 │ Chromium, Firefox, WebKit,  │     │ Kernel Swap Subsystem       │    │ High-Speed In-Memory        │
 │ Electron, GTK4 Applications │     │ (Transparent Minor Faults)  │    │ /dev/zram0 (zstd Comp.)     │
 └─────────────────────────────┘     └─────────────────────────────┘    └─────────────────────────────┘
```

This subsystem extends WattCurb's `MemoryPressureGuard` ([`REF-ARCH-072`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-072-memory-pressure-guard-and-ceiling-assertion.md)) by introducing **three non-destructive memory recovery tiers** prior to any CPU throttling or swap file creation.

---

## 2. Component Design & System Interfaces

### 2.1 `LowMemoryNotifier`: Cooperative D-Bus Signal Engine
- **Target Interface**:
  - Bus: `DBUS_BUS_SYSTEM`
  - Name: `org.freedesktop.LowMemoryMonitor`
  - Path: `/org/freedesktop/LowMemoryMonitor`
  - Interface: `org.freedesktop.LowMemoryMonitor`
- **Signal**: `LowMemoryWarning(byte level)`
- **Behavior**:
  - During bootstrap, tests whether `org.freedesktop.LowMemoryMonitor` is already claimed on the bus via `dbus_bus_name_has_owner`. If owned by an external daemon (e.g. system service), WattCurb acts as a signal emitter via direct method call or broadcast hook.
  - When memory pressure escalates to **Advisory**, emits `level = 100`.
  - When memory pressure escalates to **Severe**, emits `level = 255`.
  - When memory pressure recovers below the release band, resets to `level = 0`.

### 2.2 `ProcessPageoutActuator`: Direct `process_madvise` Syscall Engine
- **Target System Call**:
  ```cpp
  #include <sys/mman.h>
  #include <sys/syscall.h>
  #include <unistd.h>

  // process_madvise is syscall 440 on x86_64
  #ifndef SYS_process_madvise
  #define SYS_process_madvise 440
  #endif
  ```
- **VMA Discovery**:
  - For candidate background processes (determined by `WindowAwareGovernor`, [`REF-ARCH-075`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-075-window-minimized-cstate-governor.md)), reads `/proc/<pid>/maps` using zero-allocation chunked buffers.
  - Filters for writable, anonymous segments (`rw-p` with no filesystem backing).
  - Populates up to 16 `struct iovec` entries per batch.
- **Actuation**:
  - Obtains `pidfd = pidfd_open(pid, 0)`.
  - Executes `syscall(SYS_process_madvise, pidfd, iov, count, MADV_PAGEOUT, 0)`.
  - Closes `pidfd`.

### 2.3 `CgroupMemoryReclaimer`: Cgroup v2 Kernel Reclaim Engine
- **Path Resolution**: Resolves `/sys/fs/cgroup/user.slice/user-1000.slice/memory.reclaim`.
- **Target Sizing**:
  - Writes incremental byte targets: `echo 256M > memory.reclaim` (clamped between 64 MiB and 512 MiB).
  - Reclaims clean page caches and non-accessed file-backed memory without incurring process stalls.

### 2.4 `SwapTierManager`: ZRAM vs Disk Swap Prioritization
- Reads `/proc/swaps` to verify device topology:
  - If `/dev/zram0` is present, verifies its swap priority is higher than any secondary disk swapfile (e.g. `100` vs `-1`).
  - Tracks ZRAM compression ratio (`DATA / TOTAL`) via sysfs:
    `/sys/block/zram0/orig_data_size` and `/sys/block/zram0/compr_data_size`.
  - When ZRAM space exceeds 85%, coordinates with `SwapExpander` ([`REF-ARCH-073`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-073-dynamic-swap-expansion.md)) to dynamically stage disk-backed fallback files.

---

## 3. Safe Memory Recovery Decision Matrix

| Metric Threshold | Escalation Tier | Primary Actuation | Expected Recovery |
| :--- | :--- | :--- | :--- |
| **MemAvail < 20%** or **PSI Memory > 5.0** | **Tier 1 (Advisory)** | D-Bus `LowMemoryWarning(100)` | 200MB ~ 1GB (In-app cache discard) |
| **MemAvail < 12%** or **PSI Memory > 15.0** | **Tier 2 (Proactive Pageout)** | `process_madvise(MADV_PAGEOUT)` on minimized windows | 500MB ~ 3GB (Compressed into zram) |
| **MemAvail < 8%** or **PSI Memory > 25.0** | **Tier 3 (Kernel Reclaim)** | D-Bus `LowMemoryWarning(255)` + `memory.reclaim` (256M) | 1GB ~ 4GB (V8 Major GC + Page cache) |
| **MemAvail < 5%** & **zram > 85%** | **Tier 4 (Dynamic Swap Expand)** | `SwapExpander::expand()` (8GB file increment) | 8GB ~ 32GB backing store |
| **Swap Space Fully Exhausted** | **Tier 5 (Allocation Brake)** | `cpu.max` CFS quota (20ms/100ms) on runaway PID | 100% halt of new dirty pages (No Kill) |

---

## 4. Safety & Liveness Guarantees

1. **Non-Halting / Zero-Kill**: Under no circumstances is `SIGKILL` or `SIGTERM` transmitted to any process.
2. **Audio Stream Immunity**: Audio-producing PIDs (PipeWire/PulseAudio clients) are strictly excluded from Tier 2 (`MADV_PAGEOUT`) and Tier 5 (CPU Quota).
3. **Focused Window Immunity**: The active focused window PID is never subjected to `process_madvise` or direct cgroup throttling.
4. **Instant Page-Fault Recovery**: Paged-out memory resides in RAM-compressed `zram` (`zstd`), ensuring fault latency is < 5 microseconds upon window restoration.
