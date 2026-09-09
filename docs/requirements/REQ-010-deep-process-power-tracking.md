# [REF-REQ-013] Deep Process Physical Telemetry Specification

- **Ref-ID**: `REF-REQ-013`
- **Related Requirements**: [`REF-REQ-001`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-001-hardware-power-profiling.md), [`REF-REQ-004`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-001-hardware-power-profiling.md#24-ref-req-004-hardware-to-process-power-attribution), [`REF-REQ-011`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-008-process-hardware-feature-tracking.md), [`REF-REQ-012`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-009-continuous-window-evaluation.md)
- **Related Architecture**: [`REF-ARCH-002`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-002-profiler-implementation.md), [`REF-ARCH-005`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-005-zero-cost-cpuid-simd.md)
- **Status**: Approved

---

## 1. Overview & Problem Statement

Standard Linux power profilers reduce process tracking to generic CPU utilization (`utime + stime`) and disk I/O bytes. However, physical power dissipation in modern multi-core SoCs (such as AMD Zen 2 Renoir) and mobile chipsets is heavily influenced by microarchitectural and peripheral side effects:

1. **Cross-CCX Core Migration (Infinity Fabric / L3 Thrashing)**:
   - On AMD Zen 2 (2 Core Complex modules, CCX 0: Cores 0-3 / Threads 0-7; CCX 1: Cores 4-7 / Threads 8-15), migrating a thread between CCX modules invalidates the private 4MB L3 cache slice.
   - Cache lines must be re-fetched over the AMD Infinity Fabric interconnect, directly driving up SoC Uncore power and memory bus activations.
2. **Timer Slack Violations (`timerslack_ns`)**:
   - The Linux kernel implements timer coalescing to allow CPUs to remain in deep C-states (`C3/CC6`).
   - Runaway or misconfigured processes setting `timerslack_ns < 50,000ns` (e.g. 50ns) demand ultra-high timer resolution, forcing the kernel to wake the CPU out of `NO_HZ_IDLE` tickless sleep for individual microsecond interrupts.
3. **DRAM Physical Memory & Major Page Faults (`majflt`)**:
   - `minflt` (minor faults) triggers cross-core TLB shootdown Inter-Processor Interrupts (IPI).
   - `majflt` (major faults) forces synchronous page fetches from storage, keeping NVMe SSD controllers in active power state (PS0, ~3.5W).
   - Physical DRAM proportional allocation (`PSS`) determines ongoing DDR4/LPDDR4 DRAM refresh and row activation power.
4. **Network Sockets & WiFi Constantly Awake Mode (CAM)**:
   - Holding active network sockets and sending periodic keep-alives forces the 802.11 WiFi transceiver out of Power Save Mode (PSM, ~30mW) into Constantly Awake Mode (CAM, 800mW ~ 1,800mW).

---

## 2. Technical Specification

### 2.1 Extended Process Metrics (`ProcessSample`)

| Metric | Source | Purpose / Physical Attribution |
| :--- | :--- | :--- |
| `cpu_core` | `/proc/[pid]/stat` (Field 39) | Physical core ID on which the task executed. |
| `num_threads` | `/proc/[pid]/stat` (Field 20) | Thread count; multi-threaded processes scatter across cores, preventing CC6 sleep. |
| `minflt` | `/proc/[pid]/stat` (Field 10) | Minor page faults (TLB shootdowns / memory allocations). |
| `majflt` | `/proc/[pid]/stat` (Field 12) | Major page faults (disk reads, NVMe APST wakeups). |
| `timerslack_ns` | `/proc/[pid]/timerslack_ns` | Nanosecond timer slack; values $< 50,000$ ns break timer coalescing. |
| `pss_kib` | `/proc/[pid]/statm` (or `smaps_rollup`) | Proportional physical DRAM footprint. |
| `open_sockets` | `/proc/[pid]/fd/*` (`socket:[...]`) | Active network sockets holding WiFi transceiver in CAM mode. |

### 2.2 Microarchitectural Attribution Policies

1. **Cross-CCX Migration Penalty**:
   - If a process migrates between CCX domains ($\lfloor \text{core}_{\text{prev}} / 8 \rfloor \neq \lfloor \text{core}_{\text{cur}} / 8 \rfloor$), increment `cross_ccx_migration`.
   - Attribute proportional Uncore / Infinity Fabric interconnect power ($P_{\text{Uncore}}$).
2. **Timer Slack Coalescing Breaker Penalty**:
   - If `timerslack_ns < 50000`, apply a multiplier to the process's `WakeTax`:
     $$\text{WakeTax}_{\text{penalized}} = \text{WakeTax} \times \left(1.0 + \frac{50000 - \text{timerslack\_ns}}{50000}\right)$$
3. **WiFi CAM Mode Attribution**:
   - Attribute WiFi peripheral power ($P_{\text{WiFi}}$) to processes holding `open_sockets > 0` in proportion to their socket count and network wakeups.
4. **DRAM Memory Attribution**:
   - Attribute DRAM / platform power according to $\text{PSS}_{\text{KiB}}$ and major page fault frequency.

---

## 3. Zero-Allocation & Performance Invariants

1. `parse_proc_stat` must parse tokens 10, 12, 14, 15, 20, 39 in a single SIMD pass without additional file reads or heap allocations.
2. `open_sockets` must be tallied during the existing `/proc/[pid]/fd` iteration for DRM fdinfo without additional `stat` or `readlink` loops.
3. Reading `statm` and `timerslack_ns` must be restricted to active processes via Lazy Deep Inspection.
4. Oracle Gate parser threshold: $< 0.5 \mu s/\text{op}$.
