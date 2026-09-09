# [REF-REQ-001] Hardware Power Profiling and System Specification

- **Ref-ID**: `REF-REQ-001`, `REF-REQ-002`, `REF-REQ-003`
- **Related Research**: [`REF-RES-001`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-001-prior-art-and-hardware-telemetry.md)
- **Related Architecture**: [`REF-ARCH-001`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-001-daemon-architecture.md)
- **Status**: Draft / Approved

---

## 1. Scope & Objectives

WattCurb is a lightweight background daemon engineered to reduce power consumption on battery-powered Linux systems (laptops, UMPCs, embedded devices) through hardware-level energy telemetry and intelligent process throttling.

---

## 2. Detailed Requirements

### 2.1 [`REF-REQ-001`] Physical Hardware Power Profiling
1. **Domain Detection**:
   - The daemon must discover available hardware power sensors on boot without requiring proprietary out-of-tree drivers.
   - Domains include:
     - **CPU Package & Subdomains**: RAPL (`package-0`, `core`, `uncore`, `dram`).
     - **Battery Discharge**: `/sys/class/power_supply/BAT*` (`power_now` or `voltage_now` $\times$ `current_now`).
     - **GPU**: Integrated/Discrete GPU power sensors via `hwmon` or vendor sysfs (`amdgpu`, `i915`, `xe`).
     - **Display**: Screen brightness via `/sys/class/backlight/` correlated with panel baseline wattage.
2. **Energy Metrology**:
   - Energy readings from counter registers ($\mu J$) must be converted into instantaneous average power ($\text{Watts}$ and $\text{mWatts}$) over the sampling interval $\Delta t$.
   - Must handle 32-bit counter rollover gracefully.

### 2.2 [`REF-REQ-002`] Zero-Wakeup Daemon & Singleton Execution
1. **Strict Singleton Lifecycle**:
   - Only one instance of the daemon may run at any time.
   - Enforced through Linux Abstract Namespace UNIX Domain Socket (`@wattcurb.lock`) or `flock` on `/run/wattcurb.pid`.
   - Prevent stale lock issues on system crash.
2. **Zero-Wakeup Design Principle**:
   - Daemon must not wake the CPU package out of deep C-states unnecessarily.
   - Forbidden: tight polling loops or high-frequency polling (`sleep(1)`).
   - Enforce Linux Kernel Netlink Process Connector (`NETLINK_CONNECTOR` with `PROC_EVENT_FORK/EXEC/EXIT`) for event-based process tracking.
   - Periodic timer checks must use `timerfd` with `prctl(PR_SET_TIMERSLACK_NS)` to coalesce timer wakeups with other system activity.

### 2.3 [`REF-REQ-003`] Automated Evaluation & Oracle Gate Testing
1. **Granular Unit Tests**:
   - All parsing routines (RAPL sysfs, procfs stat line parsing, battery status, TOML parser) must have isolated unit tests.
2. **Evaluation Telemetry Metrics**:
   - Daemon memory resident set size (RSS) must remain $< 10\text{MB}$.
   - Hot monitoring path must perform zero dynamic heap allocations.
   - Steady-state daemon CPU usage must remain $< 0.1\%$.
3. **Oracle Gate Regression Gate**:
   - Continuous verification loop where any degradation in evaluation metrics or test failures halts deployment and triggers targeted remediation.
