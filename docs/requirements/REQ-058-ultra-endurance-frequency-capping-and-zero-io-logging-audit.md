# REQ-058: Ultra-Endurance Frequency Capping & Zero-I/O Logging Verification Specification

- **Ref ID**: `REF-REQ-058`
- **Title**: Ultra-Endurance Frequency Capping (1.0GHz CPU & 40% GPU) & Zero-I/O Logging Architecture Verification
- **Status**: Approved
- **Domain**: Power Profiles, AMDGPU DPM, CPUFreq, Telemetry Storage & Logging
- **Related Requirements**: [`REF-REQ-002`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-001-hardware-power-profiling.md#ref-req-002), [`REF-REQ-008`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-005-zero-residue-release.md), [`REF-REQ-029`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-026-binary-seqlock-data-supply-and-json-elimination.md), [`REF-REQ-055`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-055-pre-transition-state-journaling-and-profile-enforcement.md)
- **Related Architecture**: [`REF-ARCH-031`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-031-dual-domain-state-journal-and-profile-actuation.md), [`REF-ARCH-034`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-034-gpu-overdrive-dpm-actuator-and-seqlock-shm-audit.md)

---

## 1. Executive Summary

This specification formalizes two critical system behaviors:
1. **UltraEndurance Extreme Power & Process Capping**:
   - Maintains CPU maximum frequency target at the **1.4 GHz** hardware P-state floor (`1400000` kHz on AMD Cezanne `acpi-cpufreq`).
   - Caps maximum process CPU utilization in UltraEndurance mode:
     - **Spatial Core Capping**: Runaway/heavy processes are restricted to at most **25% of system cores** (`allowed = std::max(2, n / 4)`, i.e. 4 cores out of 16), preventing multi-threaded workloads from exceeding 25% total compute capacity.
     - **CFS Priority Demotion**: Throttled processes are assigned `nice +19` (`SCHED_BATCH` or `SCHED_IDLE`) to ensure interactive desktop threads always preempt them with zero latency.
     - **cgroup v2 `cpu.max` Quota**: Where available, cgroup bandwidth is clamped to at most 2 cores (200,000 µs quota per 100,000 µs period).
   - Caps GPU frequency to **40% of maximum capability** (640 MHz on 1600 MHz AMD Vega / RDNA silicon) via native sysfs AMDGPU OverDrive (`pp_od_clk_voltage`) and DPM level management.
   - Guarantees 100% faithful restoration to baseline max clocks and unthrottled affinity upon returning to Balanced, PowerSaver, or Performance modes.
2. **Forensic Logging & Storage Optimization Verification**:
   - Confirms that the periodic telemetry collection pipeline generates **strictly 0 bytes of disk writes** during steady-state monitoring.
   - Ensures that telemetry exchange is conducted via **128-byte Seqlock shared memory (`/dev/shm/wattcurb_state.bin`)** in RAM, eliminating NVMe/SATA wakeups from deep L1.2 sleep states.

---

## 2. Functional Requirements

### 2.1 UltraEndurance Hardware & Process Limits
- **CPU Frequency**:
  - Target: 1,400,000 kHz (1.4 GHz hardware floor).
  - Actuation: Written to `/sys/devices/system/cpu/cpu*/cpufreq/scaling_max_freq`.
- **Maximum Process CPU Utilization Hard Capping**:
  - In UltraEndurance mode, processes with `cpu_watts > 0.20W` or `num_threads >= 2 && cpu_watts > 0.12W` or `wdi_score > 4.0` are subjected to:
    - **25% Core Affinity Cap**: Confined to Cores 0..3 on a 16-core system (`std::max(2, n / 4)`).
    - **CFS `nice +19` Deprioritization**: Reduced to ~1.5% CFS weight.
    - **cgroup v2 Quota**: Capped via `/sys/fs/cgroup/.../cpu.max` to `200000 100000` (2 cores max).
- **GPU Frequency**:
  - Target: 40% of maximum rated boost clock (640 MHz on 1600 MHz hardware).
  - Actuation:
    - Set `/sys/class/drm/card*/device/power_dpm_force_performance_level` to `manual`.
    - Write OverDrive SCLK level: `"s 1 640\n"` to `pp_od_clk_voltage`.
    - Commit OverDrive configuration: `"c\n"` to `pp_od_clk_voltage`.
- **Restoration Invariant**:
  - When transitioning away from UltraEndurance:
    - Reset OverDrive table: `"r\n"` followed by `"c\n"` to `pp_od_clk_voltage`.
    - Re-enable all DPM states: `"0 1 2\n"` to `pp_dpm_sclk`.
    - Reset DPM level: `"auto"` (Balanced/Save) or `"high"` (Performance).
    - Uncap core affinity and restore original process nice values and cgroup `cpu.max` (`"max 100000"`).

### 2.2 Telemetry Storage & Logging Optimization
- **Zero Disk Writes**: No log files (`.log`, `.txt`), journal records, or diagnostic traces may be written to physical disk during the periodic monitoring loop (`process_observation_cycle`).
- **RAM-Only State Publishing**: All telemetry is published exclusively to `/dev/shm/wattcurb_state.bin` as a 128-byte TriviallyCopyable POD with atomic Seqlock versioning.
- **On-Demand Socket Streaming**: Verbose process attribution and deep JSON diagnostics are computed and streamed over the abstract Unix Domain Socket (`@wattcurb.lock`) only when explicitly requested by an interactive client (e.g. `wattcurb-dashboard`).

---

## 3. Verification & Oracle Gate Standards (`REF-TEST-023`)

1. **Hardware Capping Verification**:
   - Actuate `PowerProfileMode::UltraEndurance` and verify that `pp_od_clk_voltage` OD SCLK level 1 is exactly 640 MHz.
   - Actuate `PowerProfileMode::Balanced` and verify that `pp_od_clk_voltage` OD SCLK level 1 restores to 1600 MHz.
2. **Zero-Disk-I/O Verification**:
   - Inspect open file descriptors of `wattcurb` daemon process (`/proc/[pid]/fd`) and verify no regular files are opened with `O_WRONLY` or `O_RDWR` pointing to physical storage paths during steady-state execution.
