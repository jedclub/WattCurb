# REF-REQ-092: Ultimate Performance Unleash Full-Silicon Actuation Specification

## 1. Context & Motivation
In modern mobile workstations and battery-aware Linux daemons, a naive "Performance Mode" often merely sets the CPU scaling governor to `performance` or increases clock ceilings. However, when users demand unconstrained maximum throughput for intensive parallel compilation, 3D rendering, machine learning inference, and low-latency interactive workflows, multiple subtle power-saving mechanisms continue to induce latency and limit throughput:
1. **CPU C-State Sleep Incursions**: Cores intermittently enter shallow sleep states ($C_1, C_2, C_3$), introducing microsecond wakeup penalties and cold execution pipelines.
2. **Background Process Throttling Penalties**: WattCurb's background mitigation engine may inadvertently penalize or mask CPU cores (e.g. reserving cores 14-15 or demoting nice levels) for heavy tasks like `cargo`, `clang++`, `rustc`, or video encoders.
3. **GPU Compute Profile Latency**: AMDGPU remains in standard DPM dynamic frequency scaling rather than locking the compute/3D power profile mode (`pp_power_profile_mode = 1 / 5`), preventing sustained peak memory clock and compute shader throughput.
4. **NVMe APST & Storage Stalls**: NVMe Autonomous Power State Transitions (APST) permit drives to sleep during intermittent I/O gaps (default latency up to 100,000 $\mu$s), stalling heavy file writes.
5. **Wi-Fi Power Save Queuing**: Standard 802.11 power saving causes bursty packet queuing and jitter spikes.
6. **Kernel CFS Scheduler Cache Thrashing**: Default `sched_migration_cost_ns` allows threads to rapidly bounce between CCX clusters, degrading L1/L2/L3 cache locality.

To fulfill the user's directive for an unapologetic, unconstrained **Ultimate Performance Mode**, WattCurb must actuate full-silicon capabilities while guaranteeing strict idempotency and clean baseline restoration when transitioning back to `Balanced` or power-saving profiles.

---

## 2. Functional Requirements

### REQ-092.1: System-Wide CPU PM QoS Zero-Latency Clamp
- Upon activating `Performance` mode, WattCurb must acquire and hold an open file descriptor to `/dev/cpu_dma_latency` with a target latency of $0\,\mu\text{s}$.
- This enforces strict $C_0$ residency across all online cores, completely eliminating CPU idle transition latency and pipeline wakeup stalls.
- When leaving `Performance` mode, the file descriptor must be cleanly closed, immediately releasing the zero-latency clamp and restoring hardware C-state sleep.

### REQ-092.2: Total Mitigation Unleash (Full 16-Core Affinity & CFS Elevation)
- All background process mitigations must be immediately rolled back via [`UnifiedRollbackCoordinator`](file:///home/jedclub/Develop/WattCurb/src/policy/unified_rollback_coordinator.hpp).
- While in `Performance` mode, the periodic mitigation engine must bypass all throttling actions (`AntiStarvationCap`, `AdaptiveClusterDispersion`, `HeadroomMask`), granting 100% unrestricted access to all 16 logical cores (`0xFFFF`) and native CFS priority.

### REQ-092.3: GPU Compute & 3D Maximum Power Profile Actuation
- Write `1` (`3D_FULL_SCREEN`) or `5` (`COMPUTE`) to `/sys/class/drm/card*/device/pp_power_profile_mode` in addition to `power_dpm_force_performance_level = high`.
- Guarantees sustained peak VRAM clocks, memory interface voltages, and shader execution engine responsiveness.
- The bootstrap probe must capture the **active** row of the `pp_power_profile_mode` table (the entry marked with `*`) and demotion must restore that captured mode. `0` (Default/Auto) is permitted only as a fallback when the active row could not be read.

### REQ-092.4: Storage NVMe APST Zero-Latency Actuation
- Set `/sys/module/nvme_core/parameters/default_ps_max_latency_us` to `0` and `/sys/class/nvme/nvme*/power/control` to `on`.
- Completely prevents autonomous drive sleep transitions during disk-intensive builds and database operations.
- The two knobs are independent and must be captured independently: the bootstrap probe records the module parameter **and** each controller's runtime PM string, and demotion restores both to their captured values. Writing a constant `auto` back into `power/control` would overwrite a configuration WattCurb never set.

### REQ-092.5: Network Wi-Fi Power Save Disabling
- Disable 802.11 power save on the wireless interface and write `on` to its device runtime PM node.
- The interface must be **discovered dynamically** by scanning `/sys/class/net/*/wireless`; interface names must never be hardcoded (`wlan0`, `wlp2s0`, ...).
- Actuation must be performed through **nl80211 generic netlink** (`NL80211_CMD_SET_POWER_SAVE`), never by shelling out to `iw` via `::system()`. A single `::system()` call costs ~4.2 ms (fork + exec of `/bin/sh`), which alone exceeds 84% of the < 5 ms unified rapid-rollback budget of [`REF-TEST-020`](file:///home/jedclub/Develop/WattCurb/tests/test_units.cpp) and violates the zero-fork daemon doctrine (AGENTS.md Sec 9).
- The bootstrap probe must capture the pre-actuation power-save state via `NL80211_CMD_GET_POWER_SAVE`, and demotion must restore **that captured state** rather than unconditionally re-enabling power save.
- Eliminates 802.11 beacon sleep cycles and packet latency jitter.

### REQ-092.6: Kernel CFS Scheduler Cache Locality Optimization
- Temporarily elevate `/proc/sys/kernel/sched_migration_cost_ns` from standard $500\,\mu\text{s}$ to $5\,000\,\mu\text{s}$ ($5\,\text{ms}$).
- Discourages aggressive migration across CCX boundaries, ensuring threads remain pinned to warm L1/L2/L3 CPU caches during heavy parallel compiles.

---

## 3. Non-Functional & Verification Requirements
- **Idempotency Guarantee**: Rapid toggling between `Performance` and `Balanced` must produce zero resource leaks, leaked file descriptors, or corrupted hardware states.
- **Baseline Fidelity**: Every actuator must carry a `*_modified` flag, and `restore_hardware_baseline()` must write a hardware knob only when WattCurb itself actuated it. Restoring a constant "default" into hardware the daemon never touched is a defect, not a safety measure.
- **Zero-Fork Constraint**: No actuator reachable from `rollback_all()` may create a process. The unified rapid-rollback Oracle Gate ([`REF-TEST-020`](file:///home/jedclub/Develop/WattCurb/tests/test_units.cpp)) bounds the entire sweep at $< 5\,\text{ms}$; one `fork`/`exec` consumes that budget on its own.
- **Transition-Path Ordering Invariant (REQ-092.2)**: In `evaluate_and_actuate()`, `rollback_all()` must run **before** `apply_power_profile()`. Rollback reaches `restore_hardware_baseline()`, so the reverse order silently undoes the full-silicon unleash within the same tick, leaving `Performance` actuated in name only. The `performance_unleash_engaged` flag records the last actuation to run and makes this observable without privileges.
- **Host Isolation**: The unit suite must not actuate the machine it runs on. Every actuator that mutates live kernel, sysfs or process state passes through a sandbox choke point that the Oracle Gate engages, and the suite must not consume a live daemon's `/dev/shm` telemetry. Assertions that depend on the host's instantaneous power state are defects.
- **Oracle Gate Performance Threshold**: Actuation of the full performance unleash pipeline must complete in $< 50\,\text{ms}$.
- **Verification**: Verified by [`REF-TEST-056`](file:///home/jedclub/Develop/WattCurb/tests/test_units.cpp).
