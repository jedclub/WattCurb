# REF-REQ-086: Power Profile Telemetry Filtering & Multi-Mode Comparative Breakdown

- **Document ID**: `REF-REQ-086`
- **Related Requirements**: [`REF-REQ-001`](REQ-001-hardware-power-profiling.md), [`REF-REQ-019`](REQ-016-two-part-telemetry-and-adaptive-mitigation.md), [`REF-REQ-059`](REQ-035-thinkpower-faithful-ultra-low-overhead-tray.md), [`REF-REQ-078`](REQ-019-deep-battery-and-power-supply-telemetry.md)
- **Related Architecture**: [`REF-ARCH-063`](../architecture/ARCH-063-power-profile-telemetry-filtering-and-comparison-architecture.md)
- **Related Research**: [`REF-RES-018`](../research/RES-018-empirical-log-analysis-and-mitigation-patterns.md), [`REF-RES-024`](../research/RES-024-deep-power-log-audit-and-drain-analysis.md)
- **Status**: Approved
- **Date**: 2026-09-21

---

## 1. Objective & Motivation

The WattCurb 7-day telemetry ring buffer stores a packed 32-byte `HistoryPoint` every 10 seconds containing the active `power_profile_mode` (`0=Performance`, `1=Balanced`, `2=PowerSaver`, `3=UltraEndurance`).
While raw data has been gathered across all modes, prior analytical tools aggregated all discharging points as a single monolithic block. This obscured the empirical energy differences, C-State sleep depths, and platform losses between different power profile states.

`REF-REQ-086` mandates:
1. **Interactive Multi-Mode Filter Bar in GUI**: A tactile filtering toolbar in `BatteryReportWindow.qml` allowing instant switching between `All`, `Performance`, `Balanced`, `PowerSaver`, and `UltraEndurance`.
2. **CLI Filtering Flags**: Command-line arguments (`--mode <perf|balanced|save|ultra|all>` and `--compare`) for headless scripts, remote analysis, and automated benchmarks.
3. **Cross-Profile Comparative Breakdown Table**: A unified comparative matrix quantifying sample counts, duration, average power (W), peak power (W), and deep C-state (C3+) residency across all four profiles.

---

## 2. Functional Specifications

### 2.1 Mode Filtering Specification (`REF-REQ-086-F01`)
- The analyzer must accept a `filter_mode` parameter:
  - `-1`: All modes (aggregate baseline).
  - `0`: Performance profile frames only.
  - `1`: Balanced profile frames only.
  - `2`: PowerSaver profile frames only.
  - `3`: UltraEndurance profile frames only.
- When filtered, all summary metrics (total Wh, mAh, duration, average watts, peak watts, C3+ residency, hardware domain attribution, and culprit rankings) must be dynamically recomputed exclusively from frames matching the designated mode.

### 2.2 CLI Mode & Comparative Output (`REF-REQ-086-F02`)
- `wattcurb-dashboard --report-cli --mode <name|id>`:
  - Filter options: `perf`, `balanced`, `save`, `ultra`, `all` (case-insensitive) or integer `0, 1, 2, 3, -1`.
- `wattcurb-dashboard --report-cli --compare`:
  - Emits a dedicated Markdown table comparing all available modes chronologically recorded in the ring buffer.

### 2.3 Cross-Profile Comparative Matrix (`REF-REQ-086-F03`)
- The analyzer must compute a `ProfileComparisonEntry` for each of the 4 profile modes regardless of the active single-mode filter, summarizing:
  - Sample Count ($N$)
  - Total Active Duration (Hours / Minutes)
  - Total Energy Discharged ($Wh$)
  - Average Discharge Power ($W$)
  - Peak Discharge Power ($W$)
  - Average C3+ Deep Sleep Residency ($\%$)
  - Average CPU Temperature ($^\circ C$)
- The matrix must be rendered both in the GUI report and in Markdown output.

---

## 3. Non-Functional Invariants & Oracle Gate Criteria

1. **Zero-Allocation Telemetry Filtering**:
   - Mode filtering in `BatteryHistoryAnalyzer::analyze` must operate via stack or pre-reserved index vectors with zero intermediate string or heap allocations.
2. **Sub-Millisecond Filter Switching**:
   - Re-filtering and regenerating the report for 60,000 history entries must complete in under **2.0 ms** to ensure zero UI stutter.
3. **Graceful Fallback**:
   - If no telemetry samples exist for a specific filtered mode (e.g. UltraEndurance was never engaged), the UI must display a clear informative empty state rather than dividing by zero or crashing.
