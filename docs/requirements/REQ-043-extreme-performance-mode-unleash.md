# REQ-043: Extreme Performance Mode Full Hardware & Software Unleash

## 1. Context & Motivation
- **Ref-ID**: `REF-REQ-043`
- **Component**: `src/core/daemon_runner.cpp`, `src/policy/battery_feature.cpp`, `src/policy/mitigation_engine.cpp`, `src/ui/dashboard_backend.cpp`, `src/tray/tray_client.cpp`
- **User Requirement**: "성능 모드일때는 모든것을 개방 해서 극한의 성능을 뽑아 줘야해" (In Performance mode, unleash and open up everything to extract extreme maximum performance).
- **Previous Limitations**:
  - In `dashboard_backend.cpp`, selecting Performance invoked a nonexistent CLI flag `wattcurb --profile performance`.
  - In `mitigation_engine.cpp`, Performance mode merely skipped mitigations but did not actuate the full hardware boost sequence.
  - The hardware AMD Renoir SMU TDP remained clamped at 12W, CPU governor remained on `schedutil` instead of `performance`, and GPU clocks were throttled.

## 2. Technical Specifications & Unleash Directives
1. **Full Hardware Power Unleash Sequence**:
   - When Performance mode (mode 0) is engaged:
     - **CPU Core Performance Boost (CPB)**: Enable boost to reach peak 4.1 GHz turbo frequencies (`/sys/devices/system/cpu/cpufreq/boost` -> `1`).
     - **CPU Governor**: Lock all CPU cores (`cpu0` ~ `cpu15`) to `performance` mode, removing dynamic frequency downscaling.
     - **ACPI Platform Profile**: Command EC/firmware to `performance` mode, unlocking full thermal and power headroom (sustained 44A / peak 70A VRM phase).
     - **AMD SMU Hardware TDP Unlock**: Trigger `power-profile-manager performance` to elevate STAPM/Fast/Slow limits up to 25W/30W.
     - **GPU Silicon Acceleration**: Set `power_dpm_force_performance_level` to maximum boost clock (1600 MHz).
     - **Display Panel Power**: Disable panel power savings (ABM level 0) to eliminate color/refresh distortion.
     - **PCIe & NVMe Latency**: Eliminate PCIe ASPM link throttling and set NVMe APST latency to 0.
2. **Total Software Mitigation Rollback (Zero Throttling Guarantee)**:
   - Thaw 100% of frozen cgroups (`cgroup.freeze` = 0).
   - Restore all processes to `SCHED_OTHER` with standard nice priority (0).
   - Reset timer slack to normal (50,000 ns).
   - Clear all CPU affinity masks so every process can access all 16 hardware threads.
   - Mitigation count drops to `0개 (전면 개방 / Full Unconstrained)`.
3. **Multi-Channel Orchestration**:
   - Daemon IPC handler (`DaemonRunner::handle_ipc_datagram`) orchestrates the hardware actuation on `PROFILE 0`.
   - Dashboard (`DashboardBackend::setProfile`) directly executes `/home/jedclub/.local/bin/power-profile-manager` with immediate fallback.
   - Tray Client (`TrayClient::apply_hardware_profile`) synchronizes state.
