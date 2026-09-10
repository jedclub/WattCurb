# REQ-012: Syscall-Level Direct Hardware Telemetry Specification

- **Ref ID**: `REF-REQ-015`
- **Title**: Syscall-Level Direct Hardware Telemetry (perf_event_open PMU, Raw Binary PCIe Config pread, AMD Zen MSR Probe)
- **Status**: Approved
- **Created**: 2026-09-10
- **Authors**: Antigravity Architecture & Performance Team
- **Related Documents**:
  - [`REF-REQ-007`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-004-resident-daemon-direct-access.md): Resident Background Daemon & Direct Kernel/Hardware Access Specification
  - [`REF-REQ-010`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-007-extreme-hardware-telemetry.md): Full-Domain Physical Hardware Power & Telemetry Probe Specification
  - [`REF-ARCH-004`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-004-resident-daemon-event-loop.md): Resident Daemon & Direct Access Epoll Architecture
  - [`REF-RES-005`](file:///home/jedclub/Develop/WattCurb/docs/research/PMU_BENCHMARKS.md): Continuous PMU Milestone Benchmark & Optimization History

---

## 1. Executive Summary & Rationale

Prior hardware profiling mechanisms relied on ASCII-formatted `sysfs` files (e.g. `current_link_speed`, `current_link_width`, `scaling_cur_freq`, `energy_uj`). While functional, parsing text strings incurs recurring overhead:
1. Path traversal and inode lookup for multiple distinct files.
2. String allocation or stack buffer copying.
3. String parsing algorithms (`std::from_chars` or ASCII delimiter scanning).

To realize the true mission of **WattCurb**—extracting maximal hardware metrics at the theoretical lower limit of CPU and power cost—this specification introduces **Syscall-Level Direct Hardware Telemetry** (`REF-REQ-015`):
1. **Raw PMU Event FDs via `perf_event_open` (Syscall 298)**:
   - Directly configure PMU hardware counters for Instructions (`PERF_COUNT_HW_INSTRUCTIONS`), Cycles (`PERF_COUNT_HW_CPU_CYCLES`), and LLC Cache Misses (`PERF_COUNT_HW_CACHE_MISSES`).
   - Steady-state sample requires **exactly 1 `read()` syscall of 8 bytes per counter**, executing in under 200 nanoseconds with **0 ASCII parsing**.
2. **PCIe Binary Config Space Parsing via `pread()`**:
   - Instead of reading 3 text files per device (`current_link_speed`, `current_link_width`, `power/control`), open the 4096-byte raw binary `/sys/bus/pci/devices/*/config` space once.
   - Sample via a single `pread()` at offset 0 (standard header + capabilities pointer).
   - Traverse the linked capability list directly in the binary buffer to locate PCI Express Capability (ID `0x10`).
   - Extract Link Speed (`Gen1` to `Gen5`) and Link Width (`x1` to `x16`) with 1-cycle bitmask operations (`link_status & 0x0F`, `(link_status >> 4) & 0x3F`).
3. **AMD Zen Direct MSR Telemetry (`/dev/cpu/0/msr`)**:
   - Access SVI2 Core Telemetry / VID registers directly via binary 64-bit `pread` when root privileges (`CAP_SYS_RAWIO`) are present.
   - Non-privileged graceful fallback: retain hwmon sysfs millivolt voltage sensors without failing or aborting.

---

## 2. Functional Requirements

### 2.1 Hardware PMU Counters via `perf_event_open`
- **Identifier**: `REQ-015-PMU`
- **Interface**: `syscall(__NR_perf_event_open, struct perf_event_attr *attr, pid_t pid, int cpu, int group_fd, unsigned long flags)`
- **Configuration**:
  - `pid = 0` (calling process) or `-1` (all processes if privileged).
  - `cpu = -1` (any CPU).
  - `exclude_kernel = 1`, `exclude_hv = 1` to ensure non-privileged execution without `CAP_PERFMON` or `perf_event_paranoid <= 1`.
  - Group leader architecture: `group_fd = -1` for Instructions, and subsequent counters (Cycles, LLC Misses) linked as siblings for synchronized counter reading.
- **Sampling Mechanism**:
  - `read(fd, &val, sizeof(val))` returns the 64-bit counter directly into CPU registers.
  - IPC calculation: `pmu_ipc = (cycles > 0) ? (double)instructions / (double)cycles : 0.0;`

### 2.2 Direct PCIe Binary Config Space Decoding
- **Identifier**: `REQ-015-PCIE-BIN`
- **Interface**: `pread(config_fd, buffer, 64, 0)` on `/sys/bus/pci/devices/<bdf>/config`.
- **Capability Traversal**:
  - Verify Status Register (offset `0x06`) has Capabilities bit set (`0x10`).
  - Read Capability Pointer (offset `0x34`).
  - Loop through linked capability headers:
    - Offset `0x00`: Capability ID (search for `0x10` = PCI Express).
    - Offset `0x01`: Next Capability Pointer.
  - At PCIe Capability:
    - Offset `0x12` (Link Status Register, 16 bits):
      - Bit `[3:0]`: Current Link Speed (`1` = 2.5 GT/s Gen1, `2` = 5.0 GT/s Gen2, `3` = 8.0 GT/s Gen3, `4` = 16.0 GT/s Gen4, `5` = 32.0 GT/s Gen5).
      - Bit `[9:4]`: Negotiated Link Width (`1` = x1, `2` = x2, `4` = x4, `8` = x8, `16` = x16).
- **Zero-Allocation**:
  - Decoded entirely within a 64-byte stack buffer (`uint8_t hdr[64]`).

### 2.3 AMD Zen SVI2 Voltage / MSR Telemetry
- **Identifier**: `REQ-015-MSR`
- **Interface**: `/dev/cpu/0/msr` via binary `pread64`.
- **Registers**:
  - MSR `0xC0010293` (AMD Core Energy Status, 32/64-bit counter).
  - MSR `0xC0010064` (AMD P-State Status / Core VID).
- **Graceful Fallback**:
  - If `/dev/cpu/0/msr` fails with `EACCES` or `ENOENT`, probe `hwmon` voltage sensors (`in0_input`, `in1_input`) directly.

---

## 3. Data Structure Extensions (`HardwareSample`)

The following fields must be integrated into `HardwareSample` in `src/core/types.hpp`:

```cpp
// Syscall-level PMU hardware metrics (REF-REQ-015)
uint64_t pmu_instructions{0};
uint64_t pmu_cycles{0};
double   pmu_ipc{0.0};
uint64_t pmu_llc_misses{0};

// Silicon Core Voltage (mV) from MSR or hwmon
std::optional<uint32_t> cpu_core_vid_mv;

// PCIe Physical Bus Link state from raw config space
uint8_t pcie_link_speed_gen{0};     // e.g. 3 for Gen3, 4 for Gen4
uint8_t pcie_link_width_lanes{0};   // e.g. 4 for x4, 16 for x16
```

---

## 4. Oracle Gate & Verification Standards

1. **`REF-TEST-005` Direct PMU Counter Verification**:
   - Execute a controlled compute loop (e.g. 100,000 SIMD operations).
   - Assert `pmu_instructions > 0` and `pmu_cycles > 0`.
   - Assert `pmu_ipc > 0.0`.
2. **`REF-TEST-006` PCIe Binary Config Decoding**:
   - Open PCIe config space of host primary device (e.g. GPU or NVMe).
   - Parse binary header and decode PCIe Link Speed and Width.
   - Assert `pcie_link_speed_gen >= 1` and `pcie_link_width_lanes >= 1`.
3. **Execution Overhead Benchmark**:
   - Reading PMU counters must require `< 1 μs` in total per evaluation cycle.
   - Memory allocation must remain strictly **0 bytes** in steady-state monitoring.
