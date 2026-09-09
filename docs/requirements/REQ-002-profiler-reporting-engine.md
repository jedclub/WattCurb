# [REF-REQ-005] Detailed Hardware-to-Software Power Profiler & Report Generator

- **Ref-ID**: `REF-REQ-005`
- **Related Research**: [`REF-RES-001`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-001-prior-art-and-hardware-telemetry.md), [`REF-RES-002`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-002-hardware-power-measurement-mechanisms.md), [`REF-RES-003`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-003-hardware-to-process-attribution.md)
- **Related Architecture**: [`REF-ARCH-002`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-002-profiler-implementation.md)
- **Status**: Approved / Implementation Phase

---

## 1. Objective

Implement a lightweight, ultra-low-overhead C++23 power profiling and report generation utility for WattCurb. The profiler measures energy consumption across physical hardware components and traces that energy directly to running software processes, generating an exhaustive, easy-to-read analytical report.

---

## 2. Detailed Functional Requirements

### 2.1 Hardware Domain Telemetry
1. **Battery Discharge Measurement**:
   - Read `/sys/class/power_supply/BAT*/power_now` (or compute $V \times I$ via `voltage_now` and `current_now`).
   - Determine whether the system is on AC power or Battery (`status: Discharging/Charging`).
2. **CPU & Memory Telemetry (RAPL & Dual-Mode Fallback)**:
   - Primary: Read `/sys/class/powercap/intel-rapl/intel-rapl:0/energy_uj` and `core/energy_uj` if accessible.
   - Fallback (Unprivileged mode): Deduce CPU package power from $P_{\text{battery}} - P_{\text{gpu}} - P_{\text{display}} - P_{\text{base}}$ or thermal/activity scaling model.
3. **GPU Telemetry**:
   - Read AMDGPU / Intel DRM hwmon (`power1_input` / `power1_average`) for package power (PPT).
   - If NVIDIA GPU is present, poll NVML if available.
4. **Display & Backlight Telemetry**:
   - Read `/sys/class/backlight/*/brightness` and `max_brightness`.
   - Estimate display panel power draw using baseline affine model.

### 2.2 Software (Process) Attribution
1. **CPU Attribution**:
   - Sample `/proc/[pid]/stat` or cgroups v2 for `utime` and `stime` over the sampling interval $\Delta t$.
   - Calculate $\Delta \text{CPU\_Ticks}$ and allocate dynamic CPU power proportionally.
2. **GPU Attribution (DRM fdinfo)**:
   - Scan `/proc/[pid]/fdinfo/*` for DRM file descriptors (`/dev/dri/renderD*`).
   - Parse `drm-engine-gfx`, `drm-engine-compute`, and `drm-memory-vram`.
   - Allocate GPU hardware wattage to processes based on active GPU engine nanoseconds.
3. **Wakeup & Context Switch Attribution**:
   - Read `/proc/[pid]/status` for `voluntary_ctxt_switches` and `nonvoluntary_ctxt_switches`.
   - Quantify the C-state disruption penalty ("Wakeup Tax").
4. **Storage Activity Attribution**:
   - Read `/proc/[pid]/io` for `read_bytes` and `write_bytes`.

### 2.3 Report Generation
1. **Terminal / CLI Report**:
   - High-visibility ASCII/ANSI formatted dashboard.
   - Physical Hardware Breakdown section (Watts, Percentage of total draw).
   - Top-N Software Consumers section (PID, Process Name, User, CPU Watts, GPU Watts, Wakeups/sec, WDI Score).
   - Runaway Process Warnings (highlighting background processes draining significant power).
2. **Machine-Readable JSON Output**:
   - Output structured JSON via `--json` flag for telemetry ingestion, automated testing, and Oracle Gate verification.

---

## 3. Non-Functional Requirements (Zero-Wakeup & Extreme Efficiency)
1. **Sampling Latency**: Profiler snapshot collection overhead must complete in $< 15\text{ms}$.
2. **Zero Dynamic Allocation in Inner Path**: Parsing numeric values must use `std::from_chars` and pre-allocated stack/static buffers without dynamic heap churn.
3. **Memory Footprint**: Memory RSS must remain $< 8\text{MB}$.
