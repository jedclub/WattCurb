# REF-REQ-130: Safe Non-Destructive Memory Recovery & Runtime GC Notification Specification

## 1. Context & Motivation

Current Linux desktop and server distributions suffer from severe usability defects when memory approaches saturation:
- Traditional Out-Of-Memory (OOM) mechanisms rely on destructive termination (`SIGKILL`), terminating parent terminal shells, developer IDEs, or active web sessions.
- In modern computing environments, 40% to 65% of memory allocated by applications is **inactive or fragmented heap** (e.g. uncollected JavaScript/V8 objects in Electron/Chromium, idle background tabs, un-trimmed glibc memory pools).
- Terminative OOM responses violate WattCurb's core mission: **Absolute Zero-Kill & Non-Halting Invariant** ([`REF-REQ-044`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-044-absolute-zero-kill-and-non-halting-safety.md)) and **User-Application Exemption** ([`REF-REQ-117`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-117-user-app-exemption-from-mitigation.md)).

WattCurb must shift from reactive killing to **proactive, safe, non-destructive memory recovery** based on empirical research ([`REF-RES-033`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-033-safe-memory-reclamation-and-smart-gc.md)).

---

## 2. Functional Requirements: The Strict Three-Phase Hierarchy (GC → Compression → Swap)

To guarantee minimum I/O latency, zero disk wear, and maximum user responsiveness, memory reclamation MUST strictly execute in the following sequential order:

```
[Phase 1: In-App GC & Trimming]  ──(If insufficient)──>  [Phase 2: In-Memory Compression]  ──(If ZRAM saturated)──>  [Phase 3: Disk Swap Spillover]
• Zero I/O, Zero Paging                                  • Zero Disk I/O, RAM-only (ZRAM zstd)                 • Fallback disk swapfile
• Drop caches, V8/JS GC, malloc_trim                     • process_madvise(MADV_PAGEOUT) into ZRAM             • SwapExpander dynamic growth
• D-Bus LowMemoryWarning (100/255)                        • Cold anonymous heap compaction                      • CFS CPU quota allocation brake
```

### 2.1 Phase 1 (First Priority): Cooperative In-App GC & Runtime Heap Trimming
- **I/O Cost**: **0 Bytes (Absolute Zero Disk/Swap Activity)**
- **Latency Impact**: Zero system stall (cooperative asynchronous application cleanups).
- **Specification (REF-REQ-130.1)**:
  - The daemon monitors PSI memory pressure and available memory capacity.
  - When memory pressure first emerges (Available Memory < 20% or PSI memory `some.avg10` > 5.0), WattCurb triggers **Phase 1** before performing any paging or compression:
    1. **D-Bus `LowMemoryWarning(100)` (Moderate)**: Emitted via `org.freedesktop.LowMemoryMonitor` system bus. Instructs Chromium, Firefox, WebKit, Electron, and GTK4 apps to flush decoded image caches, glyph tables, and idle buffers.
    2. **D-Bus `LowMemoryWarning(255)` (Critical)**: Emitted if pressure persists (Available Memory < 14% or PSI memory `some.avg10` > 12.0). Instructs applications to invoke full Garbage Collection (V8 Major GC, JVM `System.gc()`), unload inactive tab renderers, and invoke `malloc_trim(0)`.
    3. **Page Cache Trimming**: Writes clean-cache reclaim requests to cgroup v2 `memory.reclaim` (reclaiming clean filesystem cache pages without touching anonymous heap).

### 2.2 Phase 2 (Second Priority): High-Speed In-Memory Compression (ZRAM zstd)
- **I/O Cost**: **Zero Disk I/O (RAM-to-RAM CPU Compression only)**
- **Latency Impact**: < 5 microseconds (sub-page decompression at memory bus speed).
- **Specification (REF-REQ-130.2)**:
  - If Phase 1 GC does not release sufficient memory and Available Memory remains < 12%:
    1. **Targeted Cold Anonymous Paging**: The daemon identifies minimized or background windows ([`REF-REQ-128`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-128-window-minimized-progressive-cstate-governor.md)) with large anonymous heaps.
    2. Issues `process_madvise(pidfd, iov, vlen, MADV_PAGEOUT, 0)` targeting cold heap segments.
    3. **Strict ZRAM Destination Invariant**: The daemon ensures `/dev/zram0` (compressed in RAM using `zstd` with priority 100) absorbs 100% of these paged-out anonymous pages.
    4. **Zero Disk I/O Guarantee**: No data is written to NVMe/SATA SSDs during Phase 2; compressed pages remain in physical RAM at a 2.5x to 3.5x compression ratio.

### 2.3 Phase 3 (Third Priority / Last Resort): Disk Swap Spillover & Safe Allocation Braking
- **I/O Cost**: Disk I/O to secondary swapfile (NVMe/SSD).
- **Latency Impact**: 1ms ~ 10ms.
- **Specification (REF-REQ-130.3)**:
  - Phase 3 is engaged **ONLY when In-Memory ZRAM is nearly exhausted** (ZRAM capacity utilization > 85% and Available Memory < 6%):
    1. **Secondary Disk Swap Engagement**: Allows memory to spill over to the disk-backed swapfile (`/swap/swapfile`, priority -1).
    2. **Dynamic Backing Store Expansion**: Coordinates with `SwapExpander` ([`REF-REQ-113`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-113-performance-mode-dynamic-swap-expansion.md)) to dynamically stage incremental swap files if disk swap is nearing capacity.
    3. **Safe Allocation Rate Quota (Allocation Brake)**: If swap capacity is completely exhausted, the daemon applies a CFS bandwidth quota (`cpu.max = 20000 100000`, 20ms per 100ms) to the single largest runaway allocator process, capping its page fault and dirty page generation rate.
    4. **Zero-Kill Guarantee**: Under no circumstances is `SIGKILL` or `SIGTERM` issued. The process remains alive and responsive.

---

## 3. Non-Functional & Safety Constraints

1. **Zero-Kill Guarantee**: No user process may be terminated (`SIGKILL`, `SIGTERM`) under any memory pressure scenario handled by WattCurb.
2. **Audio & Input Protection**: Processes with active PulseAudio/PipeWire audio streams or input focus are 100% exempt from `MADV_PAGEOUT` and cgroup reclamation.
3. **Bounded CPU Overhead**: Scanning `/proc/<pid>/maps` and calling `process_madvise` must operate within bounded chunks (maximum 16 VMAs per batch) to avoid CPU spikes.

---

## 4. Verification & Oracle Gate Standards (`REF-TEST-084`)

- **`test_low_memory_monitor_dbus_signaling`**: Verify D-Bus signal encoding, level escalation (100 and 255), and fallback behavior.
- **`test_process_madvise_pageout_safety`**: Verify `process_madvise` with `MADV_PAGEOUT` on child test process; verify child process survives and heap integrity is preserved.
- **`test_cgroup_memory_reclaim_actuation`**: Verify simulated `memory.reclaim` generation and focused PID exemption.
