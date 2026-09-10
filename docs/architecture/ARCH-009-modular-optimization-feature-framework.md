# [REF-ARCH-009] Modular Battery Optimization Feature Framework & In-Memory Pipeline

- **Ref-ID**: `REF-ARCH-009`
- **Related Requirements**: [`REF-REQ-020`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-017-detailed-briefing-and-modular-optimization-features.md), [`REF-REQ-019`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-016-two-part-telemetry-and-adaptive-mitigation.md)
- **Related Architecture**: [`REF-ARCH-004`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-004-resident-daemon-event-loop.md), [`REF-ARCH-008`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-008-two-part-telemetry-and-mitigation-engine.md)
- **Status**: Approved Design Document

---

## 1. System Architecture Overview

```
+-----------------------------------------------------------------------------------------+
|                                     WattCurb Daemon                                     |
|                                                                                         |
|  [60s Timer / 55s Sleep] --> [5s Observation Window]                                    |
|                                     |                                                   |
|                        +------------v------------+                                      |
|                        |  Zero-String Structural |                                      |
|                        |   Analysis Report Data  |                                      |
|                        | (Pure In-Memory Struct) |                                      |
|                        +------------+------------+                                      |
|                                     |                                                   |
|                                     v                                                   |
|               +-------------------------------------------+                             |
|               |          FeatureManager Pipeline          |                             |
|               |                                           |                             |
|               |  [FEAT-001: SchedIdleThrottle]            |                             |
|               |  [FEAT-002: TimerSlackCoalescing]         |                             |
|               |  [FEAT-003: ProactiveMemoryReclaim]       |                             |
|               |  [FEAT-004: CgroupFreezer]                |                             |
|               |  [FEAT-005: ZenCcxAffinityPinning]        |                             |
|               |  [FEAT-006: DisplayBacklightFloor]        |                             |
|               |  [FEAT-007: PcieAspmEnforcer]             |                             |
|               +---------------------+---------------------+                             |
|                                     |                                                   |
|                                     v                                                   |
|                      +-----------------------------+                                    |
|                      |  Active Feature Telemetry   |                                    |
|                      | (In-Memory Struct Metrics)  |                                    |
|                      +--------------+--------------+                                    |
+-------------------------------------|---------------------------------------------------+
                                      |
                                      v (On-Demand IPC / Signal Query Only)
                         +--------------------------+
                         |  Two-Part Presentation   |
                         |  [Part 1: Detailed Text] |
                         |  [Part 2: Raw JSON/Data] |
                         +--------------------------+
```

---

## 2. In-Memory Struct Analysis Pipeline (Zero-String Guarantee)

In routine daemon execution:
1. The 5-second sampling burst captures `HardwareSample` and `ProcessSnapshot` into the double-buffered ping-pong pool (`proc_pool_`).
2. `AttributionEngine::compute_attribution` constructs `AnalysisReportData` purely using trivially copyable POD fields and fixed stack buffers (`FixedString`, `FixedVector`).
3. `FeatureManager::evaluate_and_actuate` processes `AnalysisReportData` through each registered optimization feature.
4. **No string stream allocations or file write serialization occur** in the background loop unless requested by an IPC client. This guarantees:
   - 0 heap allocations.
   - Sub-1ms feature evaluation.
   - Minimal dTLB and L1D cache eviction.

---

## 3. Battery Optimization Feature Unit Specification

### 3.1 Feature Interface Definition
Each feature adheres to a uniform C++23 interface:

```cpp
enum class FeatureId : uint8_t {
    SchedIdleThrottle = 1,
    TimerSlackCoalescing = 2,
    ProactiveMemoryReclaim = 3,
    CgroupFreezer = 4,
    ZenCcxAffinityPinning = 5,
    DisplayBacklightFloor = 6,
    PcieAspmEnforcer = 7,
    Count = 8
};

struct FeatureDescriptor {
    FeatureId id;
    const char* name;
    const char* description;
    bool default_enabled;
};

struct FeatureMetrics {
    size_t actions_taken{0};
    uint64_t resource_reclaimed_bytes{0};
    double estimated_power_saved_watts{0.0};
    core::FixedVector<int32_t, 16> targeted_pids{};
};
```

### 3.2 Feature Manager Architecture
The `FeatureManager`:
- Maintains an array of all available features.
- Supports runtime enable/disable toggles per feature.
- Dispatches evaluation in a single linear pass over the top power culprits.
- Aggregates metrics into `ActiveMitigationStatus` without dynamic memory allocation.

---

## 4. High-Fidelity Extended Executive Briefing Design

When invoked via `--briefing` / `-b`:
- Single-shot sampling defaults to **10.0 seconds** (configurable via `-w` / `--duration`).
- Generates a comprehensive engineering dashboard including:
  1. Detailed power rails, battery design vs remaining capacity, health %, cycle count.
  2. Subsystem breakdown: CPU Package, C-state percentages ($C0, C1, C2, C3$), GPU engine distribution, Display, NVMe, Fan RPM, Platform losses.
  3. Process culprits with Safety Tier, WDI score, Threads, Faults, Sockets, and detailed hardware causation mechanism.
  4. Feature-by-feature mitigation report: Which feature acted on which process PID, and the estimated power delta.
  5. Tailored actionable recommendations.
