# REF-REQ-090: Process-Level CPU C-State Affinity & Residency Badge Telemetry

- **Document ID**: `REF-REQ-090`
- **Related Requirements**: [`REF-REQ-004`](REQ-004-process-attribution-engine.md), [`REF-REQ-011`](REQ-011-hardware-domain-direct-attribution.md), [`REF-REQ-037`](REQ-037-btop-style-power-profiler-dashboard.md), [`REF-REQ-089`](REQ-089-matrix-dashboard-expanded-power-shares-and-typography.md)
- **Related Architecture**: [`REF-ARCH-067`](../architecture/ARCH-067-process-cstate-classification-and-badge-visual-architecture.md)
- **Status**: Approved
- **Date**: 2026-09-21

---

## 1. Objective & Scope

In the WattCurb btop-Style Power & Hardware Matrix Dashboard (`DashboardWindow.qml`), the Top Process Attribution Matrix lists process energy consumption, CPU/GPU/DRAM breakdowns, and PSS memory footprints. However, users currently cannot immediately discern each process's direct relationship with CPU sleep states ($C_0, C_1, C_2, C_3$). 

Because processor power consumption escalates drastically when cores are forced out of deep package sleep ($C_3+$) into active ($C_0$) or shallow halt ($C_1$) states, WattCurb must provide explicit, color-coded, badge-level telemetry indicating the exact CPU C-State affinity and impact of every listed process:
1. **Per-Process C-State Affinity Classification (`REF-REQ-090-F01`)**: Dynamically classify each process into a dominant CPU C-State state ($C_0, C_1, C_2, C_3$) based on hardware tick consumption, context switches/wakeups per second, timer slack, and wake tax penalties.
2. **Dedicated C-STATE Matrix Column & Cyber Badge (`REF-REQ-090-F02`)**: Introduce a dedicated `C-STATE` column in the Process Attribution Matrix table with a clear, color-coded tactile badge for instant visual recognition.
3. **Hover Card Detailed C-State Diagnostics (`REF-REQ-090-F03`)**: Expand the floating Cyber Inspection Card (`hoverCard`) to explain the exact hardware mechanism driving that C-State classification (e.g. Active Core Execution vs. Wakeup Storm vs. Deep Sleep Retention).

---

## 2. Functional Requirements

### 2.1 C-State Classification Criteria (`REF-REQ-090-F01`)
Each process is deterministically mapped to a single dominant CPU C-State tier during attribution:
- **`C0` (Active Execution / 활성 실행)**:
  - Condition: Process consumes significant CPU execution energy ($\ge 0.25\,W$) OR exhibits active CPU quantum ticks ($\Delta \text{ticks} > 15$).
  - Meaning: Actively keeping CPU execution cores awake in high-power $C_0$ state.
- **`C1` (Light Idle / Wakeup Breaker / 얕은 유휴)**:
  - Condition: High wakeups per second ($\text{wakeups/s} \ge 30$) OR high wake tax penalty ($\text{wakeTaxWatts} \ge 0.15\,W$) OR aggressive timer slack ($\text{timerslack} < 50,000\,\text{ns}$) while having low compute load ($< 0.25\,W$).
  - Meaning: Shallow idle halt state. Process sleeps briefly but wakes up constantly, preventing core from transitioning into deeper sleep states.
- **`C2` (Moderate Idle / I/O Wait / 중간 대기)**:
  - Condition: Moderate wakeups ($5 \le \text{wakeups/s} < 30$) OR moderate disk I/O / socket activity while compute watts $< 0.25\,W$.
  - Meaning: Intermediate clock-stop idle state. Reasonable sleep retention but periodic wakeups.
- **`C3` (Deep Sleep Retention / 심층 절전)**:
  - Condition: Low wakeups ($< 5\,\text{wakeups/s}$) AND low CPU watts ($< 0.10\,W$).
  - Meaning: Truly quiescent background state. Allows the core and package to achieve and maintain ultra-low-power $C_3+$ / $C_6+$ / $C_8+$ deep power-gated states.

### 2.2 Table Column & Visual Badge Specification (`REF-REQ-090-F02`)
- In `src/ui/qml/DashboardWindow.qml`:
  - Add a dedicated column header: `C-STATE` (width 54px).
  - Place between `TIER` and `PRIMARY HARDWARE MECHANISM`.
  - Badge Rendering:
    - **`C0`**: Deep Red/Orange background (`#361c0a`), vibrant orange border (`#f59e0b`), orange monospace text (`#f59e0b`).
    - **`C1`**: Deep Navy/Cyan background (`#132738`), sky cyan border (`#00d2ff`), cyan monospace text (`#00d2ff`).
    - **`C2`**: Deep Indigo/Slate background (`#1f1d38`), purple border (`#a855f7`), lavender monospace text (`#c084fc`).
    - **`C3`**: Deep Emerald background (`#0d2b1d`), green border (`#10b981`), mint green monospace text (`#10b981`).

### 2.3 Interactive Cyber Inspection Card Enhancement (`REF-REQ-090-F03`)
- The floating hover card (`hoverCard`) must include a dedicated C-State metric row displaying the badge, full English title, and contextual explanation of the power impact (e.g. `C0: Core Active`, `C1: Shallow Wakeup Storm`, `C2: Moderate Sleep`, `C3: Deep Sleep Retention`).

---

## 3. Non-Functional & Oracle Gate Invariants (`REF-TEST-054`)

1. **Sub-Microsecond Classification Latency**: C-state mapping must execute in $< 50\,\text{ns}$ per process without any dynamic string allocations on the hot attribution path.
2. **Zero-Division & Fallback Safety**: If telemetry lacks process-specific wakeups, fallback safely to $C_3$ (for low power) or $C_0$ (for high power) without emitting NaN/null.
3. **Deterministic State Invariance**: The same process sample metrics must always produce the exact same C-state classification across successive poll cycles.
