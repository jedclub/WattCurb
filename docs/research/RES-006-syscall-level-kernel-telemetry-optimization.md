# [REF-RES-006] Deep Kernel & Syscall-Level Telemetry Optimization Research

- **Ref-ID**: `REF-RES-006`
- **Title**: Deep Kernel Syscall Optimization: Eliminating Redundant VFS Inode Traversals across procfs and sysfs
- **Created**: 2026-09-10
- **Authors**: Antigravity Architecture & Performance Team
- **Related Requirements**: [`REF-REQ-007`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-004-resident-daemon-direct-access.md), [`REF-REQ-010`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-007-extreme-hardware-telemetry.md), [`REF-REQ-014`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-011-zero-overhead-scoped-profiler.md), [`REF-REQ-015`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-012-syscall-level-hardware-telemetry.md)
- **Related Benchmarks**: [`REF-RES-005`](file:///home/jedclub/Develop/WattCurb/docs/research/PMU_BENCHMARKS.md) (Milestones M6 - M9)

---

## 1. Executive Summary & Problem Formulation

In Milestone M9, WattCurb achieved a host-wide average CPU overhead of **0.028%** over a sustained 30-second continuous evaluation window. However, an empirical fine-grained audit using the Subsystem Scoped Profiler (`--dev-profile`) revealed that out of the total 202.7 ms instrumented execution time across 3 passes:
1. **`proc.fd_socket_scan` & `proc.fd_readlink_loop`**: Consumes **44.2 ms (21.8% of total daemon runtime)**.
   - For ~166 active processes, the daemon opens `/proc/[pid]/fd` and executes `readlinkat` on every single file descriptor to find `socket:[...]` links and `/dev/dri/...` DRM render nodes.
   - For graphical processes holding 100~200 FDs, this results in **over 6,000 `readlinkat` system calls per evaluation pass**.
2. **`hw.fan_chassis`**: Consumes **22.1 ms (10.9%)**.
   - Synchronous LPC serial communication with the ThinkPad ACPI Embedded Controller (EC).
3. **`proc.stat_read`**: Consumes **14.4 ms (7.1%)**.
   - 1,245 individual `openat` + `read` + `close` syscalls across 417 processes, including ~150 kernel threads (`kthread`) whose PIDs and parent PIDs (`ppid == 2`) never change.
4. **`hw.battery_rail`**: Consumes **10.4 ms (5.1%)**.
   - Reading 5 distinct battery sysfs nodes (`power_now`, `status`, `voltage_now`, `current_now`, `energy_now`) even when the system is operating on AC power.
5. **Auxiliary Process Nodes (`status`, `statm`, `io`, `timerslack_ns`)**: Consumes **5.2 ms**.
   - Redundant reading of immutable fields (`timerslack_ns`) and repeated `EACCES` permission failures on unprivileged `/proc/[pid]/io`.

This research document analyzes low-overhead, kernel-conforming techniques to dismantle each of these top 10 cost drivers down to the bare minimum syscall budget without violating Linux POSIX compatibility or requiring root-only dependencies like eBPF.

---

## 2. Deep Decomposition of Top 10 Subsystem Bottlenecks & Optimization Strategies

### 2.1 Optimization 1: Persistent DRM FD Pinning & Socket Subsampling (`proc.fd_socket_scan`)
- **Current Bottleneck**:
  - Scanning `/proc/[pid]/fd` requires `opendir` (`openat` + `getdents64`), followed by $N$ calls to `readlinkat(dfd, entry->d_name)`.
  - For 166 processes averaging 40 FDs, this issues $\approx 6,640$ `readlinkat` syscalls per pass.
- **Root Cause & Invariant Analysis**:
  1. **DRM Render Node Stability**: Graphical applications (`kitty`, `kwin_wayland`, `chrome`, `plasmashell`) open their GPU render node (`/dev/dri/renderD128`) at process startup and hold it continuously until exit. The FD number rarely changes during the process lifetime.
  2. **Socket Invariant**: Network applications (`chrome`, `plasmashell`, `agy`) maintain long-lived network sockets. Background non-network utilities and CLI tools never open network sockets.
- **Optimization Design**:
  1. **Persistent DRM FD Pinning**:
     - In the process sample cache, record `pinned_drm_fd` (e.g. `12`).
     - On subsequent monitoring passes, **bypass the entire directory scan of `/proc/[pid]/fd`**. Attempt direct `openat` / `pread` on `/proc/[pid]/fdinfo/[pinned_drm_fd]`.
     - If the direct read succeeds and contains `drm-driver`, update GPU attribution immediately with **zero `readlinkat` calls**.
     - Only trigger a full directory scan if `pinned_drm_fd == -1` or if the pinned read fails (FD closed).
  2. **Socket Count Caching & Subsampling**:
     - For processes with zero voluntary context switch delta ($\Delta \text{ctxt} == 0$), socket state cannot have changed; retain previous `open_sockets` with zero syscalls.
     - Subsample full socket FD walking to once every $K$ passes for processes with stable FD counts.
- **Projected Impact**:
  - Eliminates > 90% of `readlinkat` syscalls (from ~6,600 down to < 200).
  - Reduces `proc.fd_socket_scan` from 44.2 ms to **< 3.0 ms**.

---

### 2.2 Optimization 2: Kernel Thread (`kthread`) Static PID Bitmask Filtering (`proc.stat_read`)
- **Current Bottleneck**:
  - `/proc` directory scan enumerates all numeric entries. 
  - On a typical Linux host, ~150 out of 400 PIDs belong to kernel threads (`kworker/*`, `rcu_*`, `migration/*`, `irq/*`).
  - Currently, the daemon opens and reads `/proc/[pid]/stat` for every kernel thread on every pass just to verify `ppid == 2`.
- **Root Cause & Invariant Analysis**:
  - In Linux, kernel thread PIDs are permanently assigned at boot or thread creation and never transition into user-space processes.
  - A PID with `ppid == 2` will permanently remain a kernel thread.
- **Optimization Design**:
  - Maintain a lightweight fixed-size bitset or flat array of known kernel thread PIDs (`std::bitset<65536> kthread_mask_`).
  - Once a PID is identified as `ppid == 2`, set its bit in `kthread_mask_`.
  - On subsequent passes, check `kthread_mask_.test(pid)`. If true, immediately synthesize `ProcessSample{pid, "[kthread]", is_kernel_thread=true}` and skip `openat` + `read` + `close` entirely.
  - Periodically evict dead PIDs from the mask when `/proc/[pid]` no longer exists.
- **Projected Impact**:
  - Eliminates ~150 `openat` + `read` + `close` cycles (450 syscalls per pass).
  - Reduces `proc.stat_read` time by **~40%** (saving ~6.0 ms per pass).

---

### 2.3 Optimization 3: Smart AC-Online Power Rail Subsampling (`hw.battery_rail`)
- **Current Bottleneck**:
  - `hw.battery_rail` reads 5 battery sysfs nodes (`power_now`, `status`, `voltage_now`, `current_now`, `energy_now`) on every pass.
  - On AC power, ACPI power supply driver calls still incur sysfs VFS overhead (avg 3.45 ms per sample).
- **Optimization Design**:
  - Probe `ac_online_fd_` first.
  - If `is_ac_online == true`:
    - System is powered by external mains (DC rail drain = 0 W battery draw).
    - Subsample battery state-of-charge reads (`capacity`, `voltage`, `cycle_count`) to once every 30 passes (~60 seconds).
    - Only read active discharge sensors if `status == "Discharging"`.
- **Projected Impact**:
  - Reduces `hw.battery_rail` steady-state time from 10.4 ms to **< 0.5 ms** while on AC power.

---

### 2.4 Optimization 4: Immutable Metadata & Permission Failure Caching (`proc.status`, `timerslack`, `io`)
- **Current Bottleneck**:
  - Reading `/proc/[pid]/timerslack_ns` for 166 processes: takes 0.82 ms per pass.
  - Attempting to read `/proc/[pid]/io` on unprivileged PIDs: returns `EACCES` repeatedly, wasting syscall round-trips.
- **Optimization Design**:
  - **Timerslack Immutability**: Timerslack defaults to 50,000 ns and rarely changes. Once read for a live PID, cache it across passes; do not re-read unless PID is recycled.
  - **Permission Cache (`io_denied_mask_`)**: If `openat(pid, "io")` fails with `EACCES`, mark the PID in an `io_denied` bitmask. Do not attempt reading `/proc/[pid]/io` on subsequent passes for that PID.
- **Projected Impact**:
  - Saves 166 `openat` calls for timerslack and ~120 failed `openat` calls for unreadable IO files.
  - Reduces auxiliary procfs inspection time by **~2.5 ms**.

---

### 2.5 Optimization 5: Direct `SYS_getdents64` Syscall & Zero-Heap Stack Buffer (`proc.fd_opendir`, `proc.fd_readlink_loop`)
- **Current Bottleneck**:
  - glibc `opendir()` and `readdir()` internally allocate a 32 KB `DIR` buffer via dynamic `malloc()`, incurring heap fragmentation and libc wrapper overhead for every active PID.
- **Optimization Design**:
  - Bypassed glibc directory streams completely.
  - Implemented direct Linux kernel system call `SYS_getdents64` into a 64-byte aligned stack buffer (`alignas(64) char dentry_buf[2048]`).
  - Read directory entries in bulk via direct kernel pointer arithmetic (`bpos += entry->d_reclen`).
  - Evaluated `entry->d_type` directly; skipped non-symlinks with zero `readlinkat` calls.
- **Empirical Impact**:
  - Eliminated 100% of glibc heap allocations during `/proc/[pid]/fd` traversal.
  - Slashed `proc.fd_opendir` from 0.514 ms to **0.283 ms (-45.1%)**.

---

### 2.6 Optimization 6: Mechanical Fan Time-Constant Adaptive Subsampling (`hw.fan_chassis`)
- **Current Bottleneck**:
  - Reading ThinkPad ACPI Embedded Controller (EC) sensors (`fan1_input`, `pwm1`) incurs hardware-level LPC bus stalls (7~18 ms per query).
  - Mechanical cooling fans have large thermal inertia ($\tau > 5\text{s}$); polling at 2-second granularity wastes CPU cycles on identical RPM readings.
- **Optimization Design**:
  - Implemented alternating-pass caching: EC fan queries execute every other turn (`sample_counter_ % 2 == 1`), returning cached RPM/PWM values on intermediate turns in < 5 ns.
  - Delayed non-critical keyboard backlight ACPI queries (`sample_counter_ % 30 == 15`), eliminating first-turn 16 ms latency spikes.
- **Empirical Impact**:
  - Slashed `hw.fan_chassis` from 22.154 ms to **3.510 ms (-84.2% reduction!)**.

---

### 2.7 Optimization 7: NVMe APST Zero-Wakeup Thermal Guard (`hw.storage_nvme`)
- **Current Bottleneck**:
  - Querying NVMe SMART temperatures (`temp1_input`) causes the Linux kernel to issue an NVMe Admin Command over PCIe, forcing the SSD controller to transition from autonomous power state (APST PS3/PS4) into active D0 state (10.4 ms bus stall).
- **Optimization Design**:
  - Blocked SMART temperature queries while the drive is in power-saving APST mode.
  - Subsampled active SMART queries to once every 30 passes (60 seconds) in steady state, querying only when sustained block I/O is observed.
- **Empirical Impact**:
  - Slashed `hw.storage_nvme` from 10.434 ms down to **0.012 ms (-99.9% elimination!)**.

---

### 2.8 Optimization 8: WiFi ASPM & Radio Thermal Caching (`hw.wireless_wifi`)
- **Current Bottleneck**:
  - Reading `/sys/module/pcie_aspm/parameters/policy` and WiFi hwmon temperatures on every 2-second cycle consumed 5.289 ms.
- **Optimization Design**:
  - ASPM policy is static at runtime; cached upon initial read.
  - Radio temperature subsampled to once every 5 passes (10 seconds).
- **Empirical Impact**:
  - Slashed `hw.wireless_wifi` from 5.289 ms down to **0.370 ms (-93.0%)**.

---

## 3. Empirical Verification & Before/After Comparison

Measured across identical 3-pass development profiling windows (`--duration 4 -i 2 --dev-profile`):

| Subsystem / Profile Scope | Baseline Runtime (M9 Initial) | Optimized Runtime (M10 Final) | Net Delta | Improvement Ratio |
| :--- | :---: | :---: | :---: | :---: |
| **`hw.fan_chassis`** | 22.154 ms | **3.510 ms** | -18.644 ms | **-84.2% (EC Bus Stall Eliminated)** |
| **`proc.fd_socket_scan`** | 22.551 ms | **10.126 ms** | -12.425 ms | **-55.1% (Direct getdents64 + Cache)** |
| **`proc.fd_readlink_loop`** | 21.673 ms | **9.339 ms** | -12.334 ms | **-56.9% (Pinned DRM + Subsampling)** |
| **`proc.stat_read`** | 14.392 ms | **7.102 ms** | -7.290 ms | **-50.7% (150 kthreads skipped)** |
| **`hw.storage_metrics / nvme`** | 10.777 ms | **0.044 ms** | -10.733 ms | **-99.6% (PCIe D0 Wakeup Eliminated)** |
| **`hw.wireless_wifi`** | 5.289 ms | **0.370 ms** | -4.919 ms | **-93.0% (ASPM & Thermal Cached)** |
| **`proc.status_read_parse`** | 2.661 ms | **2.241 ms** | -0.420 ms | **-15.8% (Lazy Deep Skip)** |
| **`proc.statm_read_parse`** | 0.901 ms | **0.758 ms** | -0.143 ms | **-16.7% (Fast Token Match)** |
| **`proc.io_read_parse`** | 0.847 ms | **0.715 ms** | -0.132 ms | **-15.5% (EACCES Suppressed)** |
| **`proc.fd_drm_fdinfo`** | 2.134 ms | **0.562 ms** | -1.572 ms | **-73.7% (Direct Pinning)** |
| **`proc.fd_opendir`** | 0.514 ms | **0.283 ms** | -0.231 ms | **-45.1% (Zero-malloc SYS_getdents64)** |
| **Total Cumulative Scope Time** | **202.720 ms** | **93.494 ms** | **-109.226 ms** | **-53.9% (Execution Time Halved!)** |
| **Total CPU Clock Cycles** | **344,812,019** | **158,639,444** | **-186,172,575** | **-54.0% (1억 8,617만 사이클 절약)** |

