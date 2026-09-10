# [REF-REQ-019] Two-Part Telemetry Interface & Adaptive Closed-Loop Mitigation Specification

## 1. Overview & Operational Scope
- **Ref-ID**: `REF-REQ-019`
- **Module**: `core::DaemonRunner`, `report::ReportGenerator`, `policy::MitigationEngine`, `policy::ProcessClassifierDB`
- **Related Requirements**: [`REF-REQ-018`](../requirements/REQ-015-memory-safety-guards.md), [`REF-REQ-012`](../requirements/REQ-009-continuous-window-evaluation.md)
- **Status**: Approved

WattCurb must decouple telemetry consumption into two distinct operational paradigms while establishing an autonomous closed-loop mitigation engine that operates on a standard 60-second schedule with a 5-second measurement window.

---

## 2. Functional Requirements

### 2.1. Two-Part Telemetry Decoupling
1. **Part 1: Human-Readable Executive Text Briefing (`--briefing` / `-b` / IPC Datagram `BRIEFING`)**:
   - Must generate a concise, human-readable executive briefing:
     - Current battery status (percentage, discharge rate in Watts, remaining runtime in hours).
     - Hardware domain power breakdown summary.
     - Top 3 physical culprit processes with their primary hardware mechanism.
     - Active mitigation status (number of throttled/frozen processes, estimated power savings in mW).
     - Actionable recommendations.
2. **Part 2: Programmatically Queryable Structured Memory Telemetry (`AnalysisReportData` & IPC `TELEMETRY_STRUCT`)**:
   - Must expose zero-allocation, typed C++23 structures with contiguous memory fields:
     - Complete hardware measurements (`HardwarePowerBreakdown`).
     - Up to 2048 per-process attribution records (`FixedVector<ProcessAttributedPower, 2048>`).
     - 16 hardware domain records with up to 8 top culprits each.
     - Process category, safety tier, and mitigation status fields for external analytics, tools, or UI frontends.

### 2.2. Daemon Execution Schedule: 60-Second Cadence / 5-Second Window
1. **Observation Window**:
   - On each cycle, collect continuous telemetry for **5 seconds** across 5 sampling intervals (1-second granularity).
2. **Zero-Wakeup Idle Period**:
   - Sleep for the remaining **55 seconds** via kernel `timerfd` and `epoll_wait` (Zero-Wakeup state).
3. **Transient Noise Rejection**:
   - The 5-second window eliminates momentary bursts (e.g. terminal scroll or window drag) and isolates sustained power drainers.

### 2.3. Adaptive Closed-Loop Mitigation Engine
1. **Pipeline Cycle**:
   - **Analyze**: 5-second continuous hardware-to-process attribution.
   - **Decide**: Evaluate battery state and process classification safety tiers.
   - **Actuate**: Apply conservative or progressive mitigation (`SCHED_IDLE`, `timerslack_ns`, `memory.reclaim`, `cgroup.freeze`).
   - **Measure**: Measure hardware power delta in subsequent cycle.
   - **Verify**: Confirm power reduction; if ineffective or if user focus changes, adjust or restore.
2. **Policy Modes**:
   - **AC Power**: Mitigation disabled; monitoring only.
   - **Conservative (Battery > 40%)**: Demote background indexers (`baloo`, `tracker`, `updatedb`) to `SCHED_IDLE` and relax timer slack to 500ms.
   - **Moderate (Battery 15% - 40%)**: Trigger `memory.reclaim` on idle apps; freeze idle sync agents (`dropbox`, `nextcloud`).
   - **Progressive (Battery < 15%)**: Freeze unfocused heavy applications (`slack`, `electron`, idle browser tabs); throttle non-essential services.
3. **Instantaneous Reactivation Guard**:
   - User focus or window restore must unfreeze processes in `< 1ms`.
