# [REF-REQ-055] Pre-Transition State Journaling & 4-Stage Power Profile Enforcement Specification

## 1. Problem Statement & Motivation

A robust power management daemon must satisfy two fundamental correctness criteria:
1. **Functional Differentiation**: Each power profile (`Performance`, `Balanced`, `PowerSaver`, `UltraEndurance`) must enforce distinct, quantifiable, and predictable physical hardware and kernel constraints.
2. **State Preservation & Faithful Restoration**: The daemon must NEVER blindly overwrite system or process properties with generic default values upon de-escalation or rollback. If a process had a custom nice level (e.g. `nice -5` or `nice +12`) or was pinned to a specific CPU affinity mask by the user/systemd (e.g. pinned to Cores 2..5), applying and later releasing a mitigation must restore the **exact pre-mitigation state** rather than trampling it with hardcoded values (`nice 0`, all cores `0xFFFF`).
Similarly, host hardware governors, CPU boost status, PCIe ASPM policies, and platform profiles must be snapshotted into a **Hardware Baseline State Journal** at startup and accurately restored when exiting aggressive power-saving modes or upon daemon shutdown.

---

## 2. Functional Requirements

### 2.1 Process-Level State Journaling (`REF-REQ-055-01`)
Before applying any mitigation action (`SCHED_IDLE`, `SCHED_BATCH`, `AffinityCap`, `RelaxTimerSlack`), WattCurb must record the target process's exact state:
1. **Original CPU Affinity**: Captured via `::sched_getaffinity(pid, sizeof(cpu_set_t), &original_affinity)`.
2. **Original Scheduler Policy**: Captured via `::sched_getscheduler(pid)`.
3. **Original Nice Level**: Captured via `::getpriority(PRIO_PROCESS, pid)`.
4. **Original Timer Slack**: Captured by reading `/proc/<pid>/timerslack_ns`.

### 2.2 Process-Level Faithful Restoration (`REF-REQ-055-02`)
Upon mitigation de-escalation (load subsided), user profile override change, or daemon termination:
1. If the process's affinity was capped, it must be restored to its recorded `original_affinity` using `sched_setaffinity` across all threads in `/proc/<pid>/task/`.
2. If the scheduler or nice was modified, it must be restored to its recorded `original_sched_policy` and `original_nice`.
3. If timer slack was relaxed, it must be restored to its recorded `original_timerslack_ns`.
4. If a process terminated while tracked, its journal entry must be reclaimed without error.

### 2.3 Hardware-Level Baseline State Journaling (`REF-REQ-055-03`)
At daemon initialization (`DaemonRunner::initialize()`), WattCurb must capture the system's baseline hardware state into a non-volatile in-memory structure `HardwareBaselineState`:
1. **ACPI Platform Profile**: Read from `/sys/firmware/acpi/platform_profile`.
2. **CPU Scaling Governor**: Read from `/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor`.
3. **CPU Boost State**: Read from `/sys/devices/system/cpu/cpufreq/boost`.
4. **PCIe ASPM Policy**: Read from `/sys/module/pcie_aspm/parameters/policy`.
5. **CPU Max Scaling Frequency**: Read from `/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq`.
6. **AMDGPU Panel Power Savings**: Read from `/sys/class/drm/card*-eDP-1/amdgpu/panel_power_savings`.

### 2.4 Deterministic 4-Stage Power Profile Enforcement (`REF-REQ-055-04`)
Each power profile must actuate clear and verifiable hardware/kernel policies:

| Feature / Domain | Performance Mode (0) | Balanced Mode (1) | PowerSaver Mode (2) | UltraEndurance Mode (3) |
| :--- | :--- | :--- | :--- | :--- |
| **ACPI Platform Profile** | `performance` | `balanced` | `low-power` | `low-power` |
| **CPU Governor** | `performance` | `schedutil` | `schedutil` | `schedutil` |
| **CPU Boost** | `1` (Enabled, up to 4.1GHz) | `1` (Dynamic) | `0` (Disabled, 1.7GHz base cap) | `0` (Disabled) |
| **CPU Scaling Max Freq** | Maximum (Unconstrained) | Maximum (Unconstrained) | 1,700,000 kHz (1.7 GHz) | 1,400,000 kHz (1.4 GHz) |
| **PCIe ASPM Policy** | `performance` | `default` | `powersave` | `powersave` |
| **Panel Power Savings** | Level 0 (Disabled) | Level 1 (Moderate) | Level 2 (High Efficiency) | Level 2 (High Efficiency) |
| **Process Throttling** | None (All Unthrottled) | Headroom Capping on Runaways | Background Workers `SCHED_IDLE` | Background Workers `SCHED_IDLE` |
| **Timer Slack Coalescing**| Baseline (50 µs) | Baseline (50 µs) | 100 ms on Background Tasks | 100 ms on Background Tasks |
| **Memory Reclamation** | None | None | Inactive Background Anonymous | Active ZRAM Compression |

### 2.5 Hardware Restoration Mandate (`REF-REQ-055-05`)
When returning to `Balanced` mode or when WattCurb daemon stops:
- The hardware must be restored precisely to the captured `HardwareBaselineState`.
- Zero lingering limits (such as capped scaling frequencies or locked ASPM states) may persist on the system.

---

## 3. Non-Functional & Safety Constraints

1. **Zero Dynamic Allocation**: State journaling must use static POD storage (`TrackedMitigation`, `HardwareBaselineState`) with fixed capacities.
2. **Absolute Zero-Kill & Zero-Freeze Invariant (`REF-REQ-044`)**: All profile actuations and restorations must remain non-halting.
3. **Execution Latency**: Snapshot capture and profile hardware actuation must execute directly in C++ within < 100 microseconds, eliminating bash subshell fork/exec overhead.

---

## 4. Verification & Oracle Gate Standards (`REF-TEST-020`)

1. **Journaling & Restoration Test**:
   - Set a dummy process with non-default nice (`+5`) and custom affinity mask (e.g. CPUs 1..3).
   - Apply mitigation (`SCHED_IDLE` + headroom affinity cap).
   - Rollback mitigation.
   - Assert that nice is restored to `+5` (NOT 0) and affinity is restored to CPUs 1..3 (NOT all cores).
2. **Hardware Baseline Roundtrip**:
   - Capture baseline.
   - Actuate `PowerSaver` parameters.
   - Restore baseline.
   - Assert that governor, boost, and ASPM match baseline values.
