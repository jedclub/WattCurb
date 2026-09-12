# [REF-ARCH-014] PMU Power Proxy Engine & Milestone Telemetry Axis

- **Ref-ID**: `REF-ARCH-014`
- **Related Architecture**: [`REF-ARCH-001`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-001-daemon-architecture.md), [`REF-ARCH-003`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-003-pgo-pmu-pipeline.md), [`REF-ARCH-013`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-013-battery-telemetry-fine-grained-profiling.md)
- **Related Requirements**: [`REF-REQ-024`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-021-pmu-energy-proxy-telemetry.md)
- **Related Research**: [`REF-RES-005`](file:///home/jedclub/Develop/WattCurb/docs/research/PMU_BENCHMARKS.md)
- **Author**: WattCurb Core Architecture Team
- **Status**: Implemented / Active Reference
- **Date**: 2026-09-13

---

## 1. Subsystem Overview

The **PMU Power Proxy Engine** bridges the gap between raw hardware event counters and empirical energy attribution. By capturing CPU core activity, cache misses, and pipeline mispredictions entirely on-die via `perf_event_open`, it produces three zero-overhead proxy metrics:

```mermaid
flowchart TD
    subgraph PMU["CPU On-Die PMU (perf_event_open)"]
        P1["Instructions Retired"]
        P2["CPU Clock Cycles"]
        P3["Last-Level Cache Misses"]
        P4["Branch Mispredictions"]
    end

    subgraph Probe["HardwareProbe (hw/hardware_probe.cpp)"]
        S1["Direct single-read syscalls into HardwareSample"]
    end

    subgraph Engine["AttributionEngine (policy/attribution_engine.cpp)"]
        D1["Calculate Event Deltas across Window Δt"]
        D2["Energy Proxy Index: EPI = (ΔInst * IPC) + 200*ΔLLC + 30*ΔBranch"]
        D3["Instantaneous Power: P_est = 500mW + P_dyn + P_dram + P_branch"]
        D4["Energy Waste Ratio: EWR = (Stall Energy / EPI) * 100%"]
    end

    subgraph Output["Output Reporters & Telemetry"]
        O1["Detailed ASCII Report"]
        O2["Executive Briefing Line"]
        O3["Machine-Readable JSON"]
        O4["Milestone Benchmark Axis (PMU_BENCHMARKS.md)"]
    end

    P1 & P2 & P3 & P4 --> S1
    S1 --> D1
    D1 --> D2 & D3 & D4
    D2 & D3 & D4 --> O1 & O2 & O3 & O4
```

---

## 2. Zero-Wakeup & Zero-Allocation Invariants

1. **Unprivileged Calling-Process Counting**:
   - `perf_event_open` is initialized with `pid = 0, cpu = -1, exclude_kernel = 1, exclude_hv = 1`.
   - Requires zero root privileges (`CAP_PERFMON` not required).
   - Reads take place via single 8-byte `::read(fd, &buf, sizeof(buf))` calls directly into stack variables.
2. **Zero Polling & Zero-EC Interaction**:
   - The engine operates purely in CPU registers and L1 cache.
   - Eliminates periodic queries to the slow Embedded Controller (EC) SMBus, maintaining CPU Package C10 residency.
3. **Deterministic Arithmetic**:
   - All divisions guard against zero cycles and zero delta time.
   - Outputs are saturated and clamped to physical bounds ($0.0 \sim 100.0\%$).
