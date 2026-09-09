# [REF-RES-003] Hardware-to-Process Power Attribution: Correlating Physical Energy Drain with Software Workloads

- **Ref-ID**: `REF-RES-003`
- **Related Requirements**: [`REF-REQ-001`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-001-hardware-power-profiling.md), [`REF-REQ-002`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-001-hardware-power-profiling.md#ref-req-002)
- **Related Research**: [`REF-RES-001`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-001-prior-art-and-hardware-telemetry.md), [`REF-RES-002`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-002-hardware-power-measurement-mechanisms.md)
- **Author**: WattCurb Core Engineering Team
- **Status**: Completed / Active Reference
- **Date**: 2026-09-10

---

## 1. Executive Summary

Physical hardware telemetry (RAPL, Battery gas gauge, GPU hwmon) measures power aggregated at the hardware domain boundary (the entire CPU socket, the entire discrete GPU, or the entire DC battery rail). It does not inherently identify *which running program or thread* caused the electrons to flow.

This document establishes the scientific and mathematical models for **Hardware-to-Process Power Attribution** in WattCurb. It details how the daemon correlates physical energy draw with individual software processes (PIDs) across CPU, DRAM, GPU, Storage, and CPU Wakeup interruptions with minimum observation overhead.

---

## 2. The Core Attribution Challenge & Energy Decomposition

Total physical system power $P_{\text{sys}}(t)$ measured at the battery terminal decomposes into:

$$P_{\text{sys}}(t) = P_{\text{CPU}}(t) + P_{\text{GPU}}(t) + P_{\text{DRAM}}(t) + P_{\text{Display}}(t) + P_{\text{Storage}}(t) + P_{\text{Peripheral}}(t) + P_{\text{Loss}}$$

Within each physical subsystem (e.g., CPU Package), power further divides into:
1. **Static / Leakage Power ($P_{\text{static}}$)**: The baseline energy required to keep transistors powered, clock distributions alive, and memory cells refreshed even when idle.
2. **Dynamic / Switching Power ($P_{\text{dynamic}}$)**: Energy dissipated when capacitive CMOS gates charge and discharge during active instruction execution:
   $$P_{\text{dynamic}} = \alpha \cdot C \cdot V^2 \cdot f$$
   where $\alpha$ is switching activity, $C$ is capacitance, $V$ is supply voltage, and $f$ is operating frequency.

---

## 3. Subsystem Attribution Methodologies

### 3.1 CPU & DRAM Power Attribution

#### A. The Dynamic Execution Ratio Model (Baseline Fallback)
For standard userland deployment without privileged eBPF tracepoints, WattCurb samples process execution time over an observation interval $\Delta t = t_2 - t_1$:
- Read total CPU Package energy delta via RAPL: $\Delta E_{\text{pkg}} = E_{\text{pkg}}(t_2) - E_{\text{pkg}}(t_1)$
- Read per-process CPU time from `/proc/[pid]/stat` or cgroup v2 `cpu.stat`:
  $$\Delta \tau_i = (\text{utime}_i(t_2) + \text{stime}_i(t_2)) - (\text{utime}_i(t_1) + \text{stime}_i(t_1))$$
- Attributed Dynamic CPU Power to process $i$:
  $$P_{\text{CPU\_dyn}, i} = \left( \frac{\Delta E_{\text{pkg}}}{\Delta t} - P_{\text{pkg\_static}} \right) \times \left( \frac{\Delta \tau_i}{\sum_{k \in \text{Active}} \Delta \tau_k} \right)$$

#### B. Advanced Multi-Metric Attribution (When Perf/PMU is Enabled)
As validated in academic literature (*Kepler / Wattmeter HotCarbon '24*), simple CPU clock ticks underestimate processes that incur high LLC (Last Level Cache) misses or uncore memory bus thrashing.
With PMC counters enabled:
$$W_i = w_{\text{cycles}} \cdot \Delta \text{Cycles}_i + w_{\text{inst}} \cdot \Delta \text{Instructions}_i + w_{\text{llc}} \cdot \Delta \text{LLC\_Misses}_i$$
$$P_{\text{CPU}, i} = P_{\text{pkg\_dyn}} \times \left( \frac{W_i}{\sum_k W_k} \right)$$
- DRAM power ($P_{\text{DRAM}}$ from RAPL) is attributed directly proportional to $\Delta \text{LLC\_Misses}_i$, as cache misses force high-power external memory transactions.

#### C. The "Wakeup Tax" Attribution (C-State Disruption)
- **The Paradox**: A background process that executes for only $1\text{ms}$ every $20\text{ms}$ consumes only $5\%$ of one CPU core, but produces $50\text{ wakeups/sec}$.
- This forces the CPU package out of deep $C_6 / C_8 / C_{10}$ states, forcing the entire package into $C_0$ high-leakage state.
- **WattCurb Wakeup Attribution**:
  - Measured via `/proc/[pid]/status` (`voluntary_ctxt_switches` + `nonvoluntary_ctxt_switches`) or `sched:sched_wakeup`.
  - Penalty Energy attributed to process $i$:
    $$E_{\text{wakeup\_tax}, i} = \Delta \text{Wakeups}_i \times \left( P_{\text{C0\_idle}} - P_{\text{C6\_deep}} \right) \times \tau_{\text{residency\_penalty}}$$
  This allows WattCurb to catch insidious "low-CPU, high-wakeup" battery drainers (e.g., poorly written background Electron apps or messaging pollers).

---

### 3.2 GPU Power Attribution (The Linux DRM `fdinfo` Paradigm)

#### A. Technical Breakthrough in Modern Kernels
Starting with Linux 5.19 and perfected in Linux 6.x, the Direct Rendering Manager (DRM) standardizes per-process GPU accounting via `/proc/[pid]/fdinfo/<fd>`.
When a process opens a DRM device (`/dev/dri/card*` or `/dev/dri/renderD*`), the kernel tracks GPU hardware engine occupancy per client.

#### B. Telemetry Interface
Reading `/proc/[pid]/fdinfo/<fd>` provides:
```text
drm-driver:       amdgpu (or i915 / xe / nouveau)
drm-client-id:    28
drm-engine-gfx:   160588799074 ns
drm-engine-compute: 22170696 ns
drm-engine-dec:   0 ns
drm-total-vram:   39352 KiB
drm-memory-vram:  31032 KiB
```

#### C. Mathematical Attribution Formula
1. Read total physical GPU package power $P_{\text{GPU}}(t)$ from `/sys/class/drm/card*/device/hwmon/*/power1_input`.
2. For each active DRM client, calculate the engine time increment:
   $$\Delta T_{\text{engine}, i} = \Delta \text{drm-engine-gfx}_i + \Delta \text{drm-engine-compute}_i + \Delta \text{drm-engine-dec}_i$$
3. Total active GPU time: $\Delta T_{\text{total\_gpu}} = \sum_{k} \Delta T_{\text{engine}, k}$.
4. Attributed GPU Power:
   $$P_{\text{GPU}, i} = P_{\text{GPU\_hwmon}} \times \left( \frac{\Delta T_{\text{engine}, i}}{\Delta T_{\text{total\_gpu}}} \right)$$

*For NVIDIA GPUs*: WattCurb uses NVML C API `nvmlDeviceGetProcessUtilization`, which directly supplies per-PID compute and memory controller percentages.

---

### 3.3 Storage / NVMe Power Attribution

#### A. APST Disruption Model
- Idle NVMe drives reside in low-power Non-Operational States (PS3/PS4), consuming $5\text{mW} \sim 25\text{mW}$.
- Active reads/writes elevate the controller to PS0/PS1, consuming $3000\text{mW} \sim 7000\text{mW}$.
- Continuous micro-writes (e.g. system loggers or trackers writing 4KB every 500ms) prevent the drive from ever entering PS3/PS4.

#### B. Process Accounting
- Read `/proc/[pid]/io`:
  - `read_bytes`, `write_bytes`
  - `syscr`, `syscw` (I/O system call frequency)
- Storage attribution assigns both the direct transfer energy and the **state transition delay penalty**:
  $$P_{\text{Storage}, i} = P_{\text{active}} \times \left( \frac{\Delta \text{Bytes}_i}{\sum \Delta \text{Bytes}} \right) + P_{\text{stay\_awake\_penalty}} \times \mathbb{I}(\Delta \text{Syscalls}_i > \theta)$$

---

### 3.4 Display & Window Focus Attribution

- A background browser tab playing an unviewed video or rendering an invisible canvas generates GPU/CPU work and display compositing load.
- WattCurb correlates process identification with the active graphical session:
  - Query active window PID via Wayland compositor protocol or X11 `_NET_ACTIVE_WINDOW`.
  - **Focus Classification**:
    - **Foreground Focused Process**: High power consumption is legitimate (user is actively interacting).
    - **Background / Occluded Process**: High power consumption is classified as **Unnecessary Drain / Candidate for Throttling**.

---

## 4. The WattCurb Drain Index (WDI)

To determine which process to throttle, freeze, or alert, WattCurb combines all attributed vectors into a unified, normalized **WattCurb Drain Index (WDI)**:

$$\text{WDI}_i = w_c \cdot P_{\text{CPU}, i} + w_g \cdot P_{\text{GPU}, i} + w_d \cdot P_{\text{Storage}, i} + w_w \cdot E_{\text{wakeup\_tax}, i}$$

### Classification Thresholds:
1. **Normal / Passive ($\text{WDI} < \text{Threshold}_{\text{low}}$)**: No intervention.
2. **Foreground Workload ($\text{WDI} \ge \text{Threshold}_{\text{high}}$, Focus = Active)**: User-demanded task; allow full performance.
3. **Runaway Background Task ($\text{WDI} \ge \text{Threshold}_{\text{high}}$, Focus = Background)**:
   - Triggers the Progressive Remediation Pipeline ([`REF-ARCH-001`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-001-daemon-architecture.md)):
     1. Stage 1: `SCHED_IDLE`
     2. Stage 2: E-Core Pinning
     3. Stage 3: Cgroups v2 `cpu.max`
     4. Stage 4: `cgroup.freeze`
