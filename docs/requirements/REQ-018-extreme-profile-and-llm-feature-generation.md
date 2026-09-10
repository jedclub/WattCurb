# [REF-REQ-021] Extreme 30s Battery Hardware Profiler & LLM Feature Generation Loop

- **Ref-ID**: `REF-REQ-021`
- **Related Requirements**: [`REF-REQ-001`](file:///home/jedclub/Develop/WattCurb/docs/requirements/power_profiler.md), [`REF-REQ-019`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-016-two-part-telemetry-and-adaptive-mitigation.md), [`REF-REQ-020`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-017-detailed-briefing-and-modular-optimization-features.md)
- **Related Architecture**: [`REF-ARCH-009`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-009-modular-optimization-feature-framework.md), [`REF-ARCH-010`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-010-feature-metadata-and-extreme-profiler.md)
- **Related Research**: [`REF-RES-008`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-008-deep-process-classification-and-mitigation-db.md), [`REF-RES-009`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-009-linux-desktop-sleep-and-resource-reclaim.md)
- **Status**: Approved Specification

---

## 1. Motivation & Operational Workflow

Static heuristics and pre-defined rules provide reliable, conservative baseline battery preservation. However, novel software workloads, runaway daemon edge cases, and hardware-specific sleep-state breakers constantly emerge.

To continuously expand WattCurb's battery optimization feature set, the system incorporates an **empirical LLM-in-the-loop feature generation workflow**:

```
+-----------------------------------------------------------------------------------------+
|                               WattCurb Operational Loop                                 |
|                                                                                         |
|  1. Routine Daemon Mode (1 min / 5s Window):                                            |
|     - Executes baseline static analysis & established modular optimization features.    |
|     - Zero-string in-memory struct pipeline.                                            |
|                                                                                         |
|  2. Extreme 30-Second Hardware Profiler (`wattcurb --extreme-profile`):                |
|     - Captures a high-resolution 30-second continuous multi-sample physical profile.    |
|     - Isolates steady-state energy ($J$), C-states, PMU IPC/LLC, and unmitigated drain. |
|                                                                                         |
|  3. LLM (AI Agent) Analysis & Feature Synthesis:                                        |
|     - The AI Agent ingests the Extreme Profile markdown/JSON output.                    |
|     - Identifies unmitigated hardware mechanisms (e.g. GPU canvas poll, socket CAM).    |
|     - Synthesizes a new modular BatteryOptimizationFeature with strict safety bounds.   |
|                                                                                         |
|  4. Integration & Verification:                                                         |
|     - New feature integrated into FeatureManager with complete metadata.                |
|     - Verified via unit tests and 30-second PMU hardware audit.                         |
+-----------------------------------------------------------------------------------------+
```

---

## 2. Functional Requirements

### 2.1 Rich Feature Metadata Requirement
Every `BatteryOptimizationFeature` must encapsulate comprehensive metadata:
1. `feature_code`: Unique identifier (e.g., `FEAT-001`).
2. `name`: Human-readable identifier.
3. `target_domain`: Specific physical subsystem (CPU, GPU, DRAM, NVMe, WiFi, PCIe).
4. `kernel_mechanism`: Concrete kernel interface, syscall, and parameters used.
5. `power_saving_rationale`: Scientific and empirical rationale for energy preservation (e.g., allowing package C-states, reducing bus clock frequency, or coalescing timers).
6. `safety_constraints`: Hard guarantees (e.g., Tier 0/1 immunity, rollback triggers).

### 2.2 Extreme 30s Battery Profile CLI (`--extreme-profile`, `-X`)
The command line tool must provide a dedicated flag `--extreme-profile` (or `-X`) that:
1. Gathers 30 continuous seconds of hardware and process telemetry across multiple intervals (e.g. 15 intervals of 2.0s).
2. Computes windowed multi-interval delta attribution, filtering transient spikes.
3. Outputs an exhaustive **Extreme Battery Hardware Causation Profile** containing:
   - Full power rail measurements, battery design vs actual capacity, and remaining energy ($Wh$).
   - Core-by-core C-state residencies and PMU hardware IPC/LLC performance counters.
   - GPU VRAM, engine activity, and PCIe substate link status.
   - Storage read/write throughput and NVMe controller states.
   - Top power culprits with thread count, context switches, faults, sockets, and CCX migration.
   - Active modular features with detailed execution metadata.
   - Dedicated section: **"Unmitigated Power Drain Opportunities for LLM Feature Synthesis"**.

### 2.3 Feature Metadata Export in JSON and Human Briefings
- Both Part 1 (Detailed Briefing) and Part 2 (JSON) must include full feature metadata to enable automated offline analysis, auditing, and LLM consumption.
