# [REF-RES-001] Prior Art, Academic Papers, and Hardware Energy Telemetry Survey

- **Ref-ID**: `REF-RES-001`
- **Related Requirements**: [`REF-REQ-001`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-001-hardware-power-profiling.md), [`REF-REQ-002`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-001-hardware-power-profiling.md#ref-req-002)
- **Author**: WattCurb Core Team
- **Status**: Completed / Active Reference

---

## 1. Executive Summary

This document surveys academic literature, existing industry tools, and Linux kernel subsystems regarding physical hardware power profiling, energy attribution to software processes, and low-overhead daemon execution. The findings directly inform the architecture of **WattCurb** ([`REF-ARCH-001`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-001-daemon-architecture.md)).

---

## 2. Academic Literature Review

### 2.1 Intel/AMD RAPL Accuracy & Limitations
- **Key Paper**: *Khan, K. N., et al. (2018). "RAPL in Action: Experiences in Using RAPL for Power Measurements." ACM Transactions on Modeling and Performance Evaluation of Computing Systems.*
  - **Findings**:
    - Intel RAPL (Running Average Power Limit) provides hardware-metered microjoule ($\mu J$) accumulators updated at approximately 1 ms intervals with < 3% error compared to external physical power analyzers.
    - Domains tracked: `package-0` (entire socket), `core` (all execution units), `uncore` (ring bus, LLC cache, memory controller), and `dram` (memory channels).
    - Modern AMD Zen processors (Zen 2 and later) implement compatible RAPL interfaces exposed through the Linux `rapl` / `powercap` driver.
  - **Implication for WattCurb**: RAPL is the gold standard for software-based physical energy measurement without dedicated external hardware instrumentation.

### 2.2 Fine-Grained Process Energy Attribution
- **Key Project/Paper**: *Kepler (Kubernetes Efficient Power Level Exporter - CNCF / Red Hat Research, 2022-2024)*
  - **Methodology**: Uses eBPF tracepoints (`sched_switch`, `cpu_cycles`, `instructions`, `cache_misses`) to track per-PID execution quanta.
  - **Attribution Model**:
    $$\text{Power}_{PID} = \text{Power}_{RAPL\_Package} \times \left( \frac{\Delta \text{WorkUnits}_{PID}}{\sum \Delta \text{WorkUnits}_{Total}} \right) + \text{StaticBaseline}_{PID}$$
  - **Implication for WattCurb**: We can adapt this ratio-based formula. When eBPF is unavailable due to unprivileged environments or kernel constraints, CPU time differential ($\Delta utime + \Delta stime$) across active PIDs serves as an extremely low-cost fallback.

### 2.3 CPU C-States & The "Race-to-Sleep" Tradeoff
- **Key Literature**: *Le Sueur, E., & Heiser, G. (2011). "Slow down or sleep, that is the question." USENIX ATC.*
  - **Findings**: Modern multi-core processors consume dramatically less power in deep sleep states (C6/C8/C10/PC10) than in throttled lower P-states.
  - **Crucial Rule**: Polling every $100\text{ms}$ forces the CPU package out of deep C-states, consuming orders of magnitude more energy than the computation itself.
  - **Implication for WattCurb**: WattCurb must strictly avoid uncoordinated periodic wakeups. All timers must leverage `PR_SET_TIMERSLACK_NS` or Netlink event triggers.

---

## 3. Existing Open-Source Tool Ecosystem

| Tool | Approach | Strengths | Limitations / Gaps Addressed by WattCurb |
| :--- | :--- | :--- | :--- |
| **Powertop** | Passive diagnostic utility (ncurses) | Calculates system wakeups/sec; excellent hardware tunables | Not an active daemon; does not dynamically throttle or isolate runaway processes |
| **TLP** | Static rule-based PM scripts | Broad hardware parameter coverage (SATA LPM, PCIe ASPM, WiFi power save) | Hardware-centric only; completely blind to rogue processes hogging power |
| **auto-cpufreq** | Dynamic CPU governor switcher | Adjusts CPU EPP and turbo state based on AC/BAT and load | Coarse-grained; lowers entire CPU clock rather than throttling specific culprit apps |
| **Scaphandre** | Rust RAPL exporter (Prometheus) | Accurate RAPL readings | High memory/CPU footprint for desktop/laptop use; purely an exporter with zero action capability |
| **Kepler** | eBPF cloud energy exporter | Accurate attribution per container/cgroup | Designed for Kubernetes clusters; heavy infrastructure; lacks local desktop power mitigation |

**WattCurb's Synthesis**: Combines the physical hardware telemetry of RAPL/hwmon with the dynamic process control of cgroups v2/SCHED_IDLE in a single, hyper-optimized C++23 native daemon.

---

## 4. Linux Kernel Hardware Telemetry Interfaces

WattCurb monitors physical hardware domains via standardized kernel interfaces:

### 4.1 CPU & DRAM Energy (`powercap` / `RAPL`)
- Base path: `/sys/class/powercap/intel-rapl/`
  - `/sys/class/powercap/intel-rapl:0/energy_uj`: Accumulated energy in microjoules ($\mu J$).
  - `/sys/class/powercap/intel-rapl:0/name`: e.g., `package-0`.
  - `/sys/class/powercap/intel-rapl:0:0/name`: e.g., `core`.
  - `/sys/class/powercap/intel-rapl:0:1/name`: e.g., `uncore`.
  - `/sys/class/powercap/intel-rapl:0:2/name`: e.g., `dram`.
- Sampling delta formula:
  $$\text{Watts} = \frac{\Delta \text{energy\_uj} \times 10^{-6}}{\Delta t_{\text{seconds}}}$$

### 4.2 Battery Power Subsystem
- Base path: `/sys/class/power_supply/BAT*/`
  - `power_now`: Current power discharge rate in microwatts ($\mu W$).
  - If `power_now` is not exposed: `voltage_now` ($\mu V$) $\times$ `current_now` ($\mu A$) $\times 10^{-12} = \text{Watts}$.
  - `energy_now` / `energy_full`: Remaining capacity in $\mu Wh$.
  - `status`: `Discharging`, `Charging`, `Full`.

### 4.3 GPU Power Sensors
- **AMDGPU**: `/sys/class/drm/card*/device/hwmon/hwmon*/power1_average` ($\mu W$).
- **Intel Graphics**: `/sys/class/drm/card0/device/hwmon/hwmon*/power1_input` ($\mu W$) or RAPL discrete graphics domain (`intel-rapl:1`).
- **NVIDIA GPU**: NVML C API (`nvmlDeviceGetPowerUsage`) or `/proc/driver/nvidia/gpus/*/power` if proprietary drivers are active.

### 4.4 Display & Backlight
- Path: `/sys/class/backlight/*/brightness` and `max_brightness`.
- Attribution: Linear approximation against panel rated maximum draw ($2\text{W} \sim 8\text{W}$ typical for IPS/OLED laptop panels).

### 4.5 Storage / NVMe
- Path: `/sys/class/nvme/nvme*/power/pm_qos_latency_tolerance_us` and APST states in `/sys/class/block/nvme*n1/queue/nomerges`.

---

## 5. Architectural Recommendations for WattCurb

1. **Dual-Tier Sampling Strategy**:
   - **Tier 1 (Event-Driven)**: Netlink Process Connector (`PROC_EVENT_FORK`, `EXEC`, `EXIT`) tracks alive processes with zero CPU polling overhead.
   - **Tier 2 (Coalesced Metric Sampling)**: Hardware energy counters (RAPL, Battery, GPU) are sampled only when AC/BAT transitions occur or at long, timer-slack aligned intervals ($5\text{s} \sim 15\text{s}$).
2. **Attribution Engine**:
   - Attribute sampled package/system wattage across active PIDs proportionally to their CPU quantum consumption during the sample window.
3. **Progressive Remediation Pipeline**:
   - Flag processes exceeding threshold energy share.
   - Stage 1: Demote to `SCHED_IDLE` / `SCHED_BATCH`.
   - Stage 2: Confine to E-cores (`sched_setaffinity`).
   - Stage 3: Throttle via Cgroups v2 `cpu.max`.
   - Stage 4: Freeze background tab / daemon (`cgroup.freeze`).
