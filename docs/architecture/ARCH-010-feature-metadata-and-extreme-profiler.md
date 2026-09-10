# [REF-ARCH-010] Feature Metadata Registry & Extreme Profiler Architecture

- **Ref-ID**: `REF-ARCH-010`
- **Related Requirements**: [`REF-REQ-021`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-018-extreme-profile-and-llm-feature-generation.md), [`REF-REQ-020`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-017-detailed-briefing-and-modular-optimization-features.md)
- **Related Architecture**: [`REF-ARCH-008`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-008-two-part-telemetry-and-mitigation-engine.md), [`REF-ARCH-009`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-009-modular-optimization-feature-framework.md)
- **Status**: Approved Design Document

---

## 1. Feature Metadata Architecture

Every battery optimization feature is defined as an immutable compile-time descriptor accessible without runtime memory allocation:

```cpp
struct FeatureDescriptor {
    FeatureId id;
    const char* feature_code;              // e.g. "FEAT-001"
    const char* name;                      // e.g. "SchedIdleThrottle"
    const char* target_domain;             // e.g. "CPU Compute / Scheduler"
    const char* kernel_mechanism;          // Concrete kernel API details
    const char* power_saving_rationale;    // Hardware physics & energy savings explanation
    const char* safety_constraints;        // Immunity rules & safety limits
    bool default_enabled;
};
```

This ensures:
1. **Auditable Logging**: Every mitigated process logs the exact reason and hardware physics rationale behind the action.
2. **LLM Consumability**: When exporting reports to JSON or text, the complete rationale and constraints are embedded directly, allowing an LLM to reason about active versus missing mitigations.

---

## 2. Extreme 30s Battery Profiler Pipeline

The Extreme Profiler operates as a high-density, multi-interval accumulator over 30 continuous seconds:

```
[Start T0]
   |
   +--> Interval 1 (2.0s)  --> HardwareSample[1], ProcessSnapshot[1]
   +--> Interval 2 (2.0s)  --> HardwareSample[2], ProcessSnapshot[2]
   +--> ...
   +--> Interval 15 (2.0s) --> HardwareSample[15], ProcessSnapshot[15]
   |
[End T30]
   |
   v
[AttributionEngine::compute_windowed_attribution]
   |
   +--> Total Energy (Joules)
   +--> C-State & PMU IPC/LLC Accumulation
   +--> Top Process Culprits with Multi-Interval Delta Averages
   |
   v
[ReportGenerator::render_extreme_profile]
   |
   +--> Section 1: Power Rail & Battery Discharge Telemetry
   +--> Section 2: Full Hardware Subsystem State
   +--> Section 3: In-Depth Process Causation Profiles
   +--> Section 4: Modular Features Active vs Passive Breakdown
   +--> Section 5: Unmitigated Drain Opportunities for LLM Feature Synthesis
```

---

## 3. Data Representation Standards

- **In-Memory Safety**: Metadata strings reside in read-only `.rodata` segment.
- **Zero-Allocation Access**: `FeatureManager::descriptor(id)` returns `const FeatureDescriptor&` with zero heap allocation.
- **Export Formats**:
  - `render_extreme_profile(report, out)`: Rich markdown text optimized for human engineers and LLMs.
  - `render_json(report, out)`: Full JSON export with `feature_catalog` object array.
