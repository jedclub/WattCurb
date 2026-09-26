# REF-RES-033: Safe Non-Destructive Memory Reclamation, Runtime GC Triggering, and Proactive Paging Survey

## 1. Executive Summary & Problem Formulation

### 1.1 The Legacy Paradigm: Destructive Termination ("Kill-to-Free")
Historically, Linux memory exhaustion has been addressed primarily through **destructive termination** mechanisms:
- **Kernel OOM Killer**: Invoked during direct reclaim stalls; scores processes (`badness`) and emits unconditional `SIGKILL`s.
- **Userspace OOM Daemons (`systemd-oomd`, early Android LMK)**: Monitor PSI (Pressure Stall Information) or watermark metrics, but react by terminating entire cgroups or root slices, frequently cascading across parent terminal sessions, development environments, and user processes.

This legacy approach conflates **physical memory saturation** with **unrecoverable state**. In reality:
1. **Cold Anonymous Memory**: In modern desktop and server workloads, 40% to 65% of resident anonymous memory has not been read or written in the preceding 15 minutes (inactive tabs, minimized background windows, initialization buffers).
2. **Runtime Internal Heap Bloat**: Managed runtimes (V8/Chromium, Electron, JVM, Python, Go) and heap allocators (glibc ptmalloc, jemalloc) retain allocated memory pools without releasing free arenas back to the OS via `sbrk`/`madvise(MADV_DONTNEED)` unless explicitly signaled.

### 1.2 The Modern Non-Destructive Paradigm ("Reclaim-to-Prevent-Killing")
Recent breakthroughs from **Meta (TMO/Senpai, ASPLOS '22)**, **Google (MGLRU, Android Proactive Reclaim, Linux 5.10–6.1+)**, and the **freedesktop.org ecosystem (`LowMemoryMonitor`)** prove that systems can reclaim **20% to 35% of total system memory non-destructively** without terminating a single user application.

---

## 2. Comprehensive Survey of Advanced Mechanisms

### 2.1 Meta TMO (Transparent Memory Offloading) & Senpai (ASPLOS '22 Best Paper)
- **Core Architecture**: Meta's production architecture replaces static memory provisioning with continuous, closed-loop proactive offloading driven by userspace agent **Senpai**.
- **The PSI Feedback Loop**: Senpai monitors Pressure Stall Information (`/proc/pressure/memory`). Rather than waiting for a severe spike, Senpai injects **mild memory pressure** (e.g. tracking when memory stall exceeds 0.1%–0.5% over a 10s window).
- **Control Actuation**: It progressively shrinks target cgroup memory via `memory.high` or writes to `memory.reclaim`. If application-level metrics indicate latency degradation (SLA violation), Senpai instantly relaxes the pressure.
- **Production Metric**: Deployed across millions of datacenter servers, delivering **20%–32% memory footprint reduction** with near-zero impact on service latency.

### 2.2 Google Android LMKD & `process_madvise(MADV_PAGEOUT)` (Linux 5.10+)
- **System Call Mechanics**: Linux 5.10 introduced `process_madvise(2)`:
  ```c
  ssize_t process_madvise(int pidfd, const struct iovec *iovec, size_t vlen, int advice, unsigned int flags);
  ```
  Allowing privileged supervisors (such as WattCurb or Android `lmkd`) to submit memory advice on behalf of a target PID using `pidfd_open(2)` without attaching via `ptrace` or pausing the process.
- **`MADV_PAGEOUT` vs `MADV_COLD`**:
  - `MADV_COLD`: Marks page ranges as inactive, moving them to the inactive list for eventual LRU reclamation.
  - `MADV_PAGEOUT`: Instructs the kernel to **immediately page out / swap out** the specified anonymous pages to compressed swap (`zram`) or backing store.
- **Non-Destructive Guarantee**: Unlike SIGKILL, paged-out memory is transparently faulted back in (via minor/major page faults) the instant the user re-focuses or accesses the application.

### 2.3 Desktop Cooperative Memory Pressure: `org.freedesktop.LowMemoryMonitor` & `GMemoryMonitor`
- **Ecosystem Standard**: The freedesktop.org project standardizes cooperative memory reduction across Linux desktop applications through D-Bus:
  - Bus: `org.freedesktop.LowMemoryMonitor` (System Bus)
  - Object Path: `/org/freedesktop/LowMemoryMonitor`
  - Signal: `LowMemoryWarning(uint8 level)`
- **Standardized Pressure Levels**:
  - `level = 0`: Normal / No pressure.
  - `level = 100`: **Moderate Pressure** (System memory available < 15%~20%). Applications are expected to flush disk caches, drop decoded image buffers, and trim unused font glyph caches.
  - `level = 255`: **Critical Pressure** (System memory available < 5%~8%). Applications are expected to trigger full Garbage Collection (V8 Major GC, JVM `System.gc()`), unload background browser tabs, and invoke `malloc_trim(0)`.
- **Application Adoption**:
  - **Chromium / WebKitGTK**: Listen for memory pressure signals to discard background renderer memory and compact V8 heaps.
  - **GTK4 / GNOME Apps**: `GMemoryMonitor` automatically bridges this signal to application callbacks.
  - **Flatpak Apps**: The XDG Desktop Portal forwards `LowMemoryWarning` into application sandboxes.

### 2.4 Kernel-Level Proactive Reclaim: cgroup v2 `memory.reclaim` (Linux 5.19+)
- Introduced in Linux 5.19, `memory.reclaim` allows root/system daemons to write an exact byte target to a cgroup:
  ```bash
  echo 256M > /sys/fs/cgroup/user.slice/user-1000.slice/memory.reclaim
  ```
- The kernel attempts to proactively reclaim up to the specified amount by cycling through clean file pages and inactive anonymous pages without tripping hard limits (`memory.max`) or killing processes.

### 2.5 Multi-Gen LRU (MGLRU) Proactive Reclaim (Linux 6.1+)
- **Generational Classification**: Replaces the classic 2-list (Active/Inactive) LRU with multi-generational access-frequency bins.
- **Sysfs Proactive Interface**: `/sys/kernel/mm/lru_gen/` enables user-space triggering of cold page eviction:
  ```bash
  echo "memcg_id node_id min_gen_nr [swappiness]" > /sys/kernel/mm/lru_gen/
  ```
- Provides up to **80% reduction in kswapd CPU overhead** and enables deterministic eviction of genuinely cold generations.

### 2.6 Compressed In-Memory Swap (`zram` with `zstd`)
- Disk swap incurs 5ms~20ms seek/write latencies on rotational drives and wear on NVMe flash.
- `zram` compresses memory pages directly in RAM using lightweight algorithms (`zstd` or `lz4`):
  - **Compression Ratio**: 2.5x to 3.5x for typical application heap and text pages.
  - **Fault Latency**: Under 5 microseconds (sub-page decompression in CPU cache), eliminating UI stutter or interactive freezes when waking swapped memory.

---

## 3. Comparative Matrix of Memory Mitigation Mechanisms

| Mechanism | Invocation Path | Destructive? | Latency Impact | Kernel Version | Application Compatibility |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **OOM Killer / SIGKILL** | Kernel direct / userspace daemon | **YES (Data Loss)** | Catastrophic (Process dead) | All | All (Kills everything) |
| **`LowMemoryMonitor` D-Bus** | D-Bus Broadcast (`level=100/255`) | **NO (100% Safe)** | Zero (Cooperative in-app) | All | Chromium, WebKit, GTK4, Flatpak |
| **`process_madvise(MADV_PAGEOUT)`**| Syscall via `pidfd` | **NO (100% Safe)** | Sub-millisecond (Kernel swap) | Linux 5.10+ | Any Linux process (Transparent) |
| **cgroup v2 `memory.reclaim`** | Sysfs write to target cgroup | **NO (100% Safe)** | Low (Kernel background scan)| Linux 5.19+ | All cgroup v2 workloads |
| **`zram` (zstd) Compression** | Kernel Swap Device | **NO (100% Safe)** | Microseconds (RAM decompression) | Linux 3.14+ | Universal |
| **CPU Quota Brake (`cpu.max`)** | cgroup v2 write | **NO (Allocation Brake)**| Graceful throughput slowdown | Linux 4.5+ | All cgroup v2 workloads |

---

## 4. Architectural Synthesis: The 5-Tier Safe Memory Recovery Ladder

Based on empirical research across Meta, Android, and freedesktop standards, WattCurb should establish a **5-Tier Non-Destructive Safe Memory Escalation Ladder**:

```
[Normal State]
      │
      ▼ (Pressure: MemAvail < 20% or PSI Memory avg10 > 5%)
┌─────────────────────────────────────────────────────────────┐
│ Tier 1: Cooperative D-Bus LowMemoryMonitor (Moderate = 100) │
│ - Broadcast org.freedesktop.LowMemoryMonitor.LowMemoryWarning│
│ - Triggers in-app cache pruning & mild GC in browsers/apps  │
└─────────────────────────────────────────────────────────────┘
      │
      ▼ (Pressure: MemAvail < 12% or PSI Memory avg10 > 15%)
┌─────────────────────────────────────────────────────────────┐
│ Tier 2: Targeted process_madvise(MADV_PAGEOUT) on Minimized │
│ - Target non-focused / minimized windows (REF-REQ-128)      │
│ - Pushes cold anonymous pages into high-speed zram (zstd)   │
└─────────────────────────────────────────────────────────────┘
      │
      ▼ (Pressure: MemAvail < 8% or PSI Memory avg10 > 25%)
┌─────────────────────────────────────────────────────────────┐
│ Tier 3: Cgroup-Level memory.reclaim & Critical D-Bus (255)  │
│ - Broadcast LowMemoryWarning(level = 255) (Major V8/JVM GC) │
│ - Echo proactive reclaim target (e.g. 256M) to user.slice   │
└─────────────────────────────────────────────────────────────┘
      │
      ▼ (Pressure: MemAvail < 5% or Swap Free < 10%)
┌─────────────────────────────────────────────────────────────┐
│ Tier 4: Dynamic Fast In-Memory zram Expansion               │
│ - Expand zram capacity on-the-fly or attach secondary pool  │
│ - Zero disk I/O, zero NVMe wear, 3x effective compression   │
└─────────────────────────────────────────────────────────────┘
      │
      ▼ (Pressure: Extreme saturation, Swap fully exhausted)
┌─────────────────────────────────────────────────────────────┐
│ Tier 5: Non-Destructive CPU Allocation Rate Quota (cpu.max) │
│ - Cap CPU quota of runaway allocators (10-20ms per 100ms)    │
│ - Prevents dirty page generation without SIGKILLing process │
└─────────────────────────────────────────────────────────────┘
```

This 5-Tier pipeline guarantees that memory is aggressively freed, compressed, and trimmed while **strictly preserving the Zero-Kill and Non-Halting Invariant** (`REF-REQ-044`).
