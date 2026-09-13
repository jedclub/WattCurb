# [REF-ARCH-017] Syscall Storm Suppression, `openat` Relative Directory Walk & ACPI Subsampling Architecture

- **Ref-ID**: `REF-ARCH-017`
- **Related Requirements**: [`REF-REQ-027`](../requirements/REQ-024-syscall-storm-suppression-and-lazy-fd-bypass.md)
- **Related Tests**: [`REF-TEST-013`](../../tests/test_units.cpp)
- **Status**: Approved / Implemented

---

## 1. Architectural Overview

```
+---------------------------------------------------------------------------------------------------+
|                               ProcessAnalyzer Execution Loop                                      |
+---------------------------------------------------------------------------------------------------+
                                                  |
                  +-------------------------------+-------------------------------+
                  |                                                               |
                  v                                                               v
    [Process Sampling (do_capture)]                              [Subsystem Hardware Probe]
                  |                                                               |
  1. Low-Delta Interleaved Pacing:                                 1. ACPI EC Fan Subsampling:
     * Δticks < 2 & (pass % 2 == 1) -> Reuse I/O                      * pass % 6 == 1 -> Poll EC
     * Δticks < 2 & (pass % 2 == 0) -> Reuse statm                    * Else -> Return cached_fan_rpm
                  |                                                2. AC Online Subsampling:
                  v                                                   * pass % 4 == 1 -> Query ac_online_fd
  2. Relative Directory Traversal:                                    * Else -> Return cached_ac_online
     * openat(proc_dfd, "PID/fd", ...)
     * O(1) dentry lookup without root walk
                  |
                  v
  3. Multi-Tier Lazy FD Bypass:
     * Tier 1: Ephemeral ((utime+stime) < 20 & ctxt < 50) -> Bypass 100%
     * Tier 2: Non-Network (prev_sockets == 0 & Δsw < 200 & pass % 8 != 0) -> Bypass 87.5%
     * Tier 3: Network-Established (prev_sockets > 0 & pass % 6 != 0) -> Bypass 83.3%
                  |
                  v
  4. Non-Link Early Skip:
     * entry->d_type == DT_REG || DT_DIR -> continue without readlinkat
+---------------------------------------------------------------------------------------------------+
```

---

## 2. Key Technical Implementations

### 2.1 Multi-Tier Lazy FD Bypass (`ProcessAnalyzer::inspect_pid_fds`)

To eradicate the **72.1 ms** FD inspection bottleneck:
1. **Bootstrap Ephemeral Process Guard**:
   ```cpp
   if (prev == nullptr) {
       if ((sample.utime_ticks + sample.stime_ticks < 20 && sample.voluntary_ctxt_switches < 50) ||
           (sample.num_threads == 1 && sample.io_syscalls == 0 && sample.minflt == 0)) {
           sample.open_sockets = 0;
           sample.pinned_drm_fd = -1;
           return;
       }
   }
   ```
   Completely avoids thousands of `readlinkat` calls on short-lived build jobs, helper threads, and command-line utilities.

2. **Paced Steady-State Rescanning**:
   - `prev->open_sockets == 0`: If context switch delta is under 200, rescan only once every 8 passes (~16s).
   - `prev->open_sockets > 0`: Established daemon sockets are rescanned once every 6 passes (~12s).

3. **Relative `openat` & Dirent Type Filtering**:
   - Replaced `::open("/proc/[pid]/fd")` with `::openat(proc_dfd, rel_fd_path, ...)`.
   - Bypasses `SYS_readlinkat` on all directory entries with `d_type == DT_REG` or `DT_DIR`.

---

### 2.2 Hardware Sensor Subsampling (`HardwareProbe`)

- **Fan Chassis Polling**: ACPI EC SMBus transactions take 1.5 ~ 3.0 ms per read. Polling frequency is reduced from every 2 passes to every 6 passes (`sample_counter_ % 6 == 1`), eliminating **2.50 ms** of bus wait time.
- **AC Adapter Check**: Reduced from every pass to every 4 passes (`sample_counter_ % 4 == 1`), saving **1.50 ms** per monitoring window.
