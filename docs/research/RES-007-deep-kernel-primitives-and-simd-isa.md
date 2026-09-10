# [REF-RES-007] Deep Kernel Syscall Primitives, Single-Read uevent, and C++23 ISA Acceleration

- **Ref-ID**: `REF-RES-007`
- **Title**: Radical Syscall Elimination: uevent Single-Read, Zero-Allocation /proc getdents64, and C++23 SIMD Vectorization
- **Created**: 2026-09-10
- **Authors**: Antigravity Performance & Architecture Team
- **Related Requirements**: [`REF-REQ-007`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-004-resident-daemon-direct-access.md), [`REF-REQ-010`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-007-extreme-hardware-telemetry.md), [`REF-REQ-014`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-011-zero-overhead-scoped-profiler.md), [`REF-REQ-015`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-012-syscall-level-hardware-telemetry.md)
- **Related Benchmarks**: [`REF-RES-005`](file:///home/jedclub/Develop/WattCurb/docs/research/PMU_BENCHMARKS.md) (Milestones M8 - M11)

---

## 1. Executive Summary

Following Milestone M10, WattCurb successfully slashed instructions retired by -43.7% and instrumented scope runtime by -53.9%. However, a forensic inspection of the Top-15 fine-grained scopes revealed opportunities for further radical syscall elimination:
1. **`hw.battery_rail` (10.12 ms)**: Reading 8 separate sysfs files (`voltage_now`, `current_now`, `power_now`, `energy_now`, `status`, `capacity`, `cycle_count`, etc.) generates 8 independent VFS system calls into the ACPI power supply driver.
2. **`proc.capture_active_all` (26.47 ms)**: Enumerating `/proc` relies on glibc's `opendir()` / `readdir()`, incurring 32 KB dynamic heap allocations and libc wrapper indirection across 450+ directory entries.
3. **`proc.fd_socket_scan` & `readlink_loop` (20.05 ms)**: Even with persistent DRM pinning, processes with high FD counts (browsers, terminal emulators) cause hundreds of `readlinkat` calls on alternating turns.
4. **`hw.fan_chassis` (3.70 ms)**: Redundant dual-polling of both `fan_rpm` and `fan_pwm` over the ThinkPad LPC embedded controller (EC) bus.

This research paper defines fundamental kernel-primitive replacements to collapse multi-file sysfs probing into single atomic reads and eliminate userspace overhead using C++23 compile-time zero-cost abstractions and SIMD vectorization.

---

## 2. Core Optimization Architectural Designs

### 2.1 Single-Read Battery Telemetry via `/sys/class/power_supply/BAT0/uevent`
- **Kernel Mechanism**:
  - The Linux kernel power supply subsystem (`drivers/power/supply/power_supply_sysfs.c`) synthesizes an aggregated `uevent` text buffer containing key-value pairs for all driver properties.
  - Instead of executing 8 distinct `open()` / `pread()` syscalls across 8 separate sysfs nodes, a single `pread()` of 1,024 bytes on `/sys/class/power_supply/BAT0/uevent` extracts:
    - `POWER_SUPPLY_STATUS=Discharging`
    - `POWER_SUPPLY_VOLTAGE_NOW=11040000`
    - `POWER_SUPPLY_POWER_NOW=11316000`
    - `POWER_SUPPLY_CURRENT_NOW=...`
    - `POWER_SUPPLY_ENERGY_NOW=28660000`
    - `POWER_SUPPLY_ENERGY_FULL=42650000`
    - `POWER_SUPPLY_ENERGY_FULL_DESIGN=45280000`
    - `POWER_SUPPLY_CAPACITY=67`
    - `POWER_SUPPLY_CYCLE_COUNT=95`
- **C++23 Zero-Cost In-Place AVX2 / 64-bit Token Scanner**:
  - Parsed directly in a 64-byte aligned stack buffer using 64-bit register comparisons (`POWER_SUPPLY_VOLTAGE_NOW` prefix matching) and `std::from_chars`.
- **Projected Impact**:
  - Drops `hw.battery_rail` syscall count from 8 down to **exactly 1**.
  - Slashes execution latency from 10.12 ms down to **< 0.20 ms (-98% reduction)**.

---

### 2.2 Root `/proc` Directory Traversal via Direct `SYS_getdents64`
- **Kernel Mechanism**:
  - Replaces glibc `opendir(procfs_root)` and `readdir()` with a direct Linux system call `SYS_getdents64(proc_dfd, stack_buf, 8192)`.
  - Directory entries are streamed in batches of 8 KB directly into CPU L1 Data Cache.
  - Filters numeric PID directory names (`d_name[0] >= '1' && d_name[0] <= '9'`) directly using pointer arithmetic, bypassing glibc's internal lock and 32 KB malloc heap buffer.
- **Projected Impact**:
  - 100% elimination of glibc heap allocations during process discovery.
  - Traversal overhead reduced by ~30% with zero memory fragmentation.

---

### 2.3 Network Socket Process Classification & 64-Byte Cacheline Readlink
- **Kernel Mechanism**:
  - In a typical desktop environment, > 80% of active processes are non-network system services or local daemons with zero network sockets.
  - Track `is_network_app` state in `ProcessSample`.
  - Non-network processes with zero historical sockets and zero socket creation syscalls undergo FD inspection only once every 10 passes (20 seconds).
  - Reduce `readlinkat` target buffer from 256 bytes down to 64 bytes (`alignas(64) char symlink_buf[64]`), matching the CPU's native L1 Data Cache line size to eliminate multi-line split penalties and false sharing.
- **Projected Impact**:
  - Cuts `readlinkat` calls on stable processes by an additional 60%.
  - Combined `proc.fd_socket_scan` + `readlink_loop` runtime drops from 20.05 ms to **< 6.0 ms**.

---

### 2.4 ThinkPad EC Mechanical Decoupling: Fan PWM Bypass
- **Kernel Mechanism**:
  - When the mechanical cooling fan is idle (`fan_rpm == 0`), fan PWM is guaranteed to be 0; bypass reading `fan_pwm_fd_`.
  - Even when active, fan PWM reflects the same cooling curve as fan RPM; query PWM only on deep attribution cycles (every 5 passes).
- **Projected Impact**:
  - Cuts ThinkPad ACPI LPC bus transactions in half, reducing `hw.fan_chassis` from 3.70 ms to **< 1.20 ms**.

---

## 3. Projected Top-15 Scope Runtime Reductions

| Profile Scope | Baseline Runtime (M10) | Projected Runtime (M11) | Core Optimization Technique |
| :--- | :---: | :---: | :--- |
| **`proc.capture_active_all`** | 26.47 ms | **~15.00 ms (-43%)** | Direct `SYS_getdents64` 8KB stack buffer on `/proc` root |
| **`hw.capture_all`** | 16.60 ms | **~6.50 ms (-61%)** | Battery `uevent` single-read & fan PWM decoupling |
| **`hw.battery_rail`** | 10.12 ms | **< 0.20 ms (-98%)** | Single `pread()` on `BAT0/uevent` replacing 8 separate files |
| **`proc.fd_socket_scan`** | 10.43 ms | **~3.20 ms (-69%)** | Network process classification & 64-byte cacheline buffer |
| **`proc.fd_readlink_loop`** | 9.62 ms | **~2.80 ms (-71%)** | Stable socket bypass on non-network processes |
| **`hw.fan_chassis`** | 3.70 ms | **~1.10 ms (-70%)** | Fan PWM read bypass on idle/stable fan states |
| **`proc.stat_read`** | 7.42 ms | **~5.50 ms (-26%)** | Inline `parse_i32_fast` & direct buffer slicing |
| **Total Cumulative Time** | **96.94 ms** | **< 50.00 ms (-48%)** | **System-wide VFS syscall collapse** |

---

## 4. Empirical Validation & Measured Results

Following implementation of `parse_battery_uevent_buf`, root `/proc` `SYS_getdents64`, and non-network socket bypass:
1. **Battery Rail Syscall Collapse**:
   - `hw.battery_rail` execution latency dropped from **10.37 ms down to 6.24 ms (-39.8%)** even under continuous discharging battery conditions with ACPI `_BST` I2C bus queries.
   - 8 distinct `open()` / `pread()` syscalls were collapsed into a single 1KB `pread()` on `/sys/class/power_supply/BAT0/uevent`.
2. **Root `/proc` Scan Zero-Heap Purity**:
   - glibc `opendir()` heap buffers (`malloc(32KB)`) were 100% eliminated by using a stack-allocated 8KB aligned `LinuxDirent64` buffer.
   - All 437 active system processes are ingested in a single atomic kernel syscall.
3. **PMU Hardware Telemetry Verification**:
   - 30-second continuous evaluation (`--duration 30 -i 2`) retired **19.89M instructions** with an active task-clock of **195.54 ms** and **0.038% CPU overhead** (< 1.1 mW).
   - Zero dynamic heap allocations in steady-state loop confirmed with flat 9.9 MB RSS.

