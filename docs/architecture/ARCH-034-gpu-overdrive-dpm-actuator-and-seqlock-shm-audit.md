# ARCH-034: AMDGPU OverDrive DPM Actuator & Zero-I/O Seqlock Telemetry Architecture

- **Ref ID**: `REF-ARCH-034`
- **Title**: AMDGPU OverDrive DPM Actuator & Zero-I/O Seqlock Telemetry Architecture
- **Status**: Approved
- **Scope**: Direct Kernel Sysfs Actuation, AMDGPU OverDrive, RAM-Only Seqlock Data Supply
- **Related Requirements**: [`REF-REQ-058`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-058-ultra-endurance-frequency-capping-and-zero-io-logging-audit.md), [`REF-REQ-029`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-026-binary-seqlock-data-supply-and-json-elimination.md)
- **Related Tests**: [`REF-TEST-023`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-058-ultra-endurance-frequency-capping-and-zero-io-logging-audit.md#3-verification--oracle-gate-standards-ref-test-023)

---

## 1. System Architecture Diagram

```
+-------------------------------------------------------------------------------------------------+
|                                 WattCurb Core Daemon Loop                                       |
|                                                                                                 |
|   +-----------------------+     Zero-VFS pread()      +-------------------------------------+   |
|   |  hw_probe (RAPL/BAT)  | <------------------------ | Persistent sysfs / procfs FDs       |   |
|   +-----------------------+                           +-------------------------------------+   |
|               |                                                                                 |
|               v                                                                                 |
|   +-----------------------+     In-Memory Calc        +-------------------------------------+   |
|   |  AttributionEngine    | ------------------------> | cached_report_ (RAM buffer)         |   |
|   +-----------------------+                           +-------------------------------------+   |
|               |                                                          |                      |
|               | Steady-State Tick (0ms Disk I/O)                         | On-Demand Query      |
|               v                                                          v                      |
|   +---------------------------------------+           +-------------------------------------+   |
|   |  /dev/shm/wattcurb_state.bin (128B)  |           |  UDS Socket (@wattcurb.lock)        |   |
|   |  Atomic Seqlock Sequence Counter      |           |  Streamed JSON to Matrix Dashboard  |   |
|   +---------------------------------------+           +-------------------------------------+   |
|               |                                                                                 |
|               v                                                                                 |
|   +-----------------------------------------------------------------------------------------+   |
|   |                         MitigationEngine Direct Sysfs Actuator                          |   |
|   |                                                                                         |   |
|   |  UltraEndurance Mode:                                                                   |
|   |  * CPU Scaling Max : 1.4 GHz (hardware P-state floor + SMU 4W TDP)                       |
|   |  * Process Cap     : 25% core affinity cap (4 cores max on 16T) + nice 19 + cpu.max quota |
|   |  * GPU OverDrive   : echo manual > power_dpm_force_performance_level                    |
|   |                      echo "s 1 640" > pp_od_clk_voltage && echo "c" > pp_od_clk_voltage |   |
|   |                                                                                         |   |
|   |  Balanced / Perf Restoration:                                                           |   |
|   |  * GPU Reset       : echo "r" > pp_od_clk_voltage && echo "c" > pp_od_clk_voltage       |   |
|   |  * DPM Mode        : echo auto (or high) > power_dpm_force_performance_level            |   |
|   +-----------------------------------------------------------------------------------------+   |
+-------------------------------------------------------------------------------------------------+
```

---

## 2. AMDGPU OverDrive & DPM Actuation Primitives

### 2.1 Direct Sysfs Manipulation
The `MitigationEngine` interacts with the kernel's DRM subsystem directly via `O_WRONLY | O_CLOEXEC` system calls without subprocess forking:
- **Device Node Discovery**: Traverses `/sys/class/drm/card1/device` and `/sys/class/drm/card0/device`.
- **Clock Capping (`set_gpu_max_clock`)**:
  1. Writes `"manual\n"` to `power_dpm_force_performance_level`.
  2. Writes `"s 1 <mhz>\n"` to `pp_od_clk_voltage` to set state 1 (max SCLK) to 640 MHz (40% of 1600 MHz).
  3. Writes `"c\n"` to commit the change into the GPU firmware power management coprocessor.
- **Clock Restoration (`restore_gpu_max_clock`)**:
  1. Writes `"r\n"` to `pp_od_clk_voltage` to clear OverDrive customizations.
  2. Writes `"c\n"` to commit table restoration.
  3. Writes `"0 1 2\n"` to `pp_dpm_sclk` to re-enable all hardware clock steps.
  4. Writes `"auto\n"` or `"high\n"` to `power_dpm_force_performance_level`.

---

## 3. Zero-I/O Storage Architecture

1. **Elimination of Continuous Disk Logging**:
   - Continuous logging to NVMe or SSD storage prevents drive controllers from entering low-power states (PCIe ASPM L1.1/L1.2, NVMe APST State 4/5, consuming ~5mW vs ~1.5W active).
   - `wattcurb` emits **zero bytes** of disk logs during steady-state monitoring.
2. **RAM Seqlock IPC (`/dev/shm/wattcurb_state.bin`)**:
   - Sized exactly at 128 bytes (two cache lines).
   - Ingested by the tray client via `mmap(MAP_SHARED)` with lock-free atomic sequence verification (`seqlock.read_atomic()`).
   - Zero context switches, zero memory allocations, and zero disk writes.
