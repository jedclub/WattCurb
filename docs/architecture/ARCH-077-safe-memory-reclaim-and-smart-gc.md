## 1. Architectural Overview: The 3-Tier Non-Destructive Memory Pipeline

```
                                 [Memory Saturation Signal (PSI / MemAvailable)]
                                                        │
                   ┌────────────────────────────────────┼────────────────────────────────────┐
                   ▼                                    ▼                                    ▼
       [PHASE 1: IN-APP GC & TRIM]            [PHASE 2: RAM COMPRESSION]            [PHASE 3: DISK SWAP FALLBACK]
   ┌─────────────────────────────────┐   ┌─────────────────────────────────┐   ┌─────────────────────────────────┐
   │ LowMemoryNotifier (D-Bus)       │   │ ProcessPageoutActuator          │   │ SwapTierManager & Expander      │
   │ - Broadcast LowMemoryWarning    │   │ - process_madvise(PAGEOUT)      │   │ - Spillover to /swap/swapfile   │
   │   (Level 100 Moderate/255 Crit) │   │ - Target minimized/idle windows │   │   only when ZRAM > 85% full     │
   │ - Chromium/Electron/Firefox GC  │   │ - Force into /dev/zram0 (zstd)  │   │ - SwapExpander dynamic files    │
   │ - In-app cache & malloc_trim    │   │ - ZERO disk I/O, RAM-to-RAM     │   │ - CFS cpu.max allocation brake  │
   └─────────────────────────────────┘   └─────────────────────────────────┘   └─────────────────────────────────┘
                   │                                     │                                     │
                   ▼                                     ▼                                     ▼
        I/O: 0 Bytes (Pure CPU)               I/O: 0 Disk (RAM Bus only)             I/O: Flash/Disk Spillover
        Latency: 0ms System Stall             Latency: < 5µs Page Fault              Latency: 1ms ~ 10ms
```

This subsystem extends WattCurb's `MemoryPressureGuard` ([`REF-ARCH-072`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-072-memory-pressure-guard-and-ceiling-assertion.md)) by strictly enforcing a **Phase 1 (GC) → Phase 2 (RAM Compression) → Phase 3 (Disk Swap)** progression. Disk swap is never engaged early when memory can be freed via garbage collection or absorbed by fast in-memory compression.

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

## 3. Safe Memory Recovery Decision Matrix (GC → Compression → Swap)

| Phase & Escalation Stage | Metric Threshold | Primary Actuation | I/O & Latency Characteristic | Expected Recovery |
| :--- | :--- | :--- | :--- | :--- |
| **Phase 1-A (Moderate GC)** | **MemAvail < 20%** or **PSI > 5.0** | D-Bus `LowMemoryWarning(100)` | **0 I/O**, 0ms system pause | 200MB ~ 1GB (Image/font cache discard) |
| **Phase 1-B (Critical GC)** | **MemAvail < 14%** or **PSI > 12.0** | D-Bus `LowMemoryWarning(255)` + clean page cache trim | **0 I/O**, in-app background sweep | 500MB ~ 2GB (V8 Major GC + malloc_trim) |
| **Phase 2-A (RAM Compression)**| **MemAvail < 10%** or **PSI > 18.0** | `process_madvise(MADV_PAGEOUT)` on minimized windows | **0 Disk I/O**, RAM bus speed (<5µs)| 1GB ~ 4GB (Compressed into ZRAM zstd) |
| **Phase 2-B (Deep Compaction)**| **MemAvail < 7%** or **PSI > 25.0** | MGLRU cold generation reclaim into ZRAM | **0 Disk I/O**, memory-to-memory | 500MB ~ 2GB (Inactive background heap) |
| **Phase 3-A (Disk Spillover)** | **ZRAM > 85%** & **MemAvail < 5%** | Secondary `/swap/swapfile` engagement | Disk I/O (NVMe/SSD, 1~5ms latency) | Spills overflow to 32GB disk swap |
| **Phase 3-B (Dynamic Expand)** | **Disk Swap > 80%** | `SwapExpander::expand()` (8GB increment) | Sequential allocation | 8GB ~ 32GB incremental disk headroom|
| **Phase 3-C (Allocation Brake)**| **Total Swap Exhausted** | `cpu.max = 20000 100000` (CFS 20ms/100ms) | Zero I/O, rate throttling | Halts dirty page generation (**Zero Kill**)|

---

## 4. Safety & Liveness Guarantees

1. **Non-Halting / Zero-Kill**: Under no circumstances is `SIGKILL` or `SIGTERM` transmitted to any process.
2. **Audio Stream Immunity**: Audio-producing PIDs (PipeWire/PulseAudio clients) are strictly excluded from Tier 2 (`MADV_PAGEOUT`) and Tier 5 (CPU Quota).
3. **Focused Window Immunity**: The active focused window PID is never subjected to `process_madvise` or direct cgroup throttling.
4. **Instant Page-Fault Recovery**: Paged-out memory resides in RAM-compressed `zram` (`zstd`), ensuring fault latency is < 5 microseconds upon window restoration.
