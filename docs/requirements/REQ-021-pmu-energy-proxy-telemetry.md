# [REF-REQ-024] PMU-Based Micro-Energy & Power Proxy Telemetry

- **Ref-ID**: `REF-REQ-024`
- **Related Requirements**: [`REF-REQ-001`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-001-hardware-power-profiling.md), [`REF-REQ-015`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-015-syscall-level-kernel-telemetry-optimization.md), [`REF-REQ-023`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-020-battery-telemetry-profiling-and-oracle-gate.md)
- **Related Architecture**: [`REF-ARCH-001`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-001-daemon-architecture.md), [`REF-ARCH-014`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-014-pmu-power-proxy-engine.md)
- **Related Research**: [`REF-RES-003`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-003-hardware-to-process-attribution.md), [`REF-RES-005`](file:///home/jedclub/Develop/WattCurb/docs/research/PMU_BENCHMARKS.md)
- **Author**: WattCurb Core Architecture Team
- **Status**: Approved / In Implementation
- **Date**: 2026-09-13

---

## 1. Executive Summary & Rationale

Traditional hardware power meters (ACPI Embedded Controller via SMBus, battery fuel gauge, discrete GPU hwmon) suffer from two fatal flaws when measuring low-power desktop workloads:
1. **The Observer Effect (관측자 효과)**: Interrogating external hardware buses (LPC/eSPI, SMBus, I2C) wakes up microcontrollers (EC, BMS) and degrades the CPU's deep C-state residency (forcing Package C10 into C2/C3), burning more battery to observe than the workload itself.
2. **High Temporal Latency & Bus Blocking**: SMBus queries block execution for 20ms ~ 80ms, making per-loop or per-subsystem micro-power attribution impossible.

**REF-REQ-024** establishes a zero-bus, on-die Performance Monitoring Unit (PMU) telemetry subsystem that estimates instantaneous power ($P_{\text{est}}$) and execution energy consumption ($E_{\text{proxy}}$) directly from CPU hardware event counters (`perf_event_open`).

---

## 2. Mathematical & Physical Power Proxy Models

### 2.1 Physics Grounding
CMOS dynamic switching power is governed by:
$$P_{\text{dynamic}} = \alpha \cdot C_{\text{eff}} \cdot V^2 \cdot f$$

- **Instruction Execution ($\alpha \cdot f$)**: Direct switching activity of core execution units scales with retired instructions and instruction-level parallelism (IPC).
- **DRAM Off-Chip Bus ($C_{\text{eff}} \gg C_{\text{on-die}}$)**: Last-Level Cache (LLC) misses incur DDR PHY bus charging, row activation, and DRAM refresh, costing $\sim 1,000 \sim 5,000 \text{ pJ}$ per access (approx. 200x~1000x of an on-die L1 hit).
- **Speculative Pipeline Flushes**: Branch mispredictions cause speculative micro-ops to be discarded after consuming charge, representing pure wasted energy.

### 2.2 Formal Metrics Specification

1. **Energy Proxy Index (EPI)**:
   A dimensionless composite proxy representing the relative electrical energy consumed across an observation interval:
   $$EPI = (\Delta \text{Instructions} \times \text{IPC}) + (200.0 \times \Delta \text{LLC\_Misses}) + (30.0 \times \Delta \text{Branch\_Misses})$$
   - $w_{\text{core}} = 1.0$: Baseline instruction retirement energy weight.
   - $w_{\text{dram}} = 200.0$: Physical penalty weight for off-die DRAM bus activations.
   - $w_{\text{waste}} = 30.0$: Pipeline flush penalty for discarded speculative execution.

2. **Estimated Instantaneous Power ($P_{\text{est}}$, mW)**:
   Physical power estimated across observation time $\Delta t$ (seconds):
   $$P_{\text{est}} = P_{\text{idle}} + \frac{(\Delta \text{Instructions} \times \text{IPC} \times 0.015) + (\Delta \text{LLC\_Misses} \times 3.0) + (\Delta \text{Branch\_Misses} \times 0.20)}{\Delta t \times 10^6}$$
   - $P_{\text{idle}} = 500.0\text{ mW}$: Baseline quiescent silicon package power.
   - Converts physical event counts to milliwatts using calibrated energy constants ($15\text{ pJ/inst}$, $3,000\text{ pJ/LLC-miss}$, $200\text{ pJ/branch-miss}$).

3. **Energy Waste Ratio (EWR, %)**:
   The proportion of consumed energy dissipated on non-productive stalls and pipeline flushes:
   $$\text{EWR} = \frac{(200.0 \times \Delta \text{LLC\_Misses}) + (30.0 \times \Delta \text{Branch\_Misses})}{\max(EPI, 1.0)} \times 100.0\%$$
   - Bounded in $[0.0\%, 100.0\%]$.
   - Target for WattCurb monitoring hot loops: $\text{EWR} < 10.0\%$.

---

## 3. Functional Requirements

- **REQ-024-1**: The daemon must sample `PERF_COUNT_HW_INSTRUCTIONS`, `PERF_COUNT_HW_CPU_CYCLES`, `PERF_COUNT_HW_CACHE_MISSES`, and `PERF_COUNT_HW_BRANCH_MISSES` using unprivileged `perf_event_open` single-read file descriptors.
- **REQ-024-2**: The attribution engine must compute delta counters between successive snapshots and calculate `EPI`, `P_est`, and `EWR` with zero heap allocations.
- **REQ-024-3**: Output report generators (detailed ASCII, snapshot table, executive briefing, and JSON) must expose `pmu_branch_misses`, `pmu_energy_proxy_index`, `pmu_estimated_power_mw`, and `pmu_energy_waste_ratio`.
- **REQ-024-4**: The PMU milestone telemetry history (`PMU_BENCHMARKS.md`) must include `Branch Misses`, `EPI`, and `EWR` as comparative evaluation axes across all milestones.
