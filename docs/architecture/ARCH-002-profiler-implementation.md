# [REF-ARCH-002] Hardware Profiler & Report Generator Architecture

- **Ref-ID**: `REF-ARCH-002`, `REF-TEST-002`
- **Related Requirements**: [`REF-REQ-005`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-002-profiler-reporting-engine.md)
- **Related Research**: [`REF-RES-002`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-002-hardware-power-measurement-mechanisms.md), [`REF-RES-003`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-003-hardware-to-process-attribution.md)
- **Status**: Approved / Implementation Phase

---

## 1. Subsystem Decomposition

The WattCurb Profiling & Reporting Engine consists of 5 modular C++23 components:

```
[Main Entry / CLI]
       |
       v
+-------------------------------------------------------------------------+
|                  wattcurb::engine::ProfilerSession                      |
|                                                                         |
|  1. Capture Snapshot T1                                                 |
|     - HardwareProbe::sample()                                           |
|     - ProcessAnalyzer::sample()                                         |
|                                                                         |
|  2. Sleep interval delta (coalesced timer / user specified delta)       |
|                                                                         |
|  3. Capture Snapshot T2                                                 |
|     - HardwareProbe::sample()                                           |
|     - ProcessAnalyzer::sample()                                         |
|                                                                         |
|  4. AttributionEngine::compute(T1, T2)                                  |
|     - Computes Hardware Power Breakdown (W)                             |
|     - Computes Process Attributed Watts (CPU, GPU, Wakeup Tax)          |
|     - Computes WattCurb Drain Index (WDI)                               |
|                                                                         |
|  5. ReportGenerator::render(AnalysisResult)                             |
|     - Formats ANSI Terminal Dashboard or JSON Output                    |
+-------------------------------------------------------------------------+
```

---

## 2. Component Design & Interfaces

### 2.1 `wattcurb::hw::HardwareProbe`
- **Responsibilities**:
  - Auto-discovers physical power paths at initialization (`/sys/class/power_supply/BAT*`, `/sys/class/powercap/intel-rapl`, `/sys/class/drm/card*/device/hwmon`, `/sys/class/backlight/*`).
  - Reads values into a compact, cache-aligned `HardwareSample` struct:
    ```cpp
    struct HardwareSample {
        std::chrono::steady_clock::time_point timestamp;
        std::optional<uint64_t> rapl_package_uj;
        std::optional<uint64_t> rapl_core_uj;
        std::optional<uint64_t> rapl_dram_uj;
        std::optional<uint64_t> battery_power_uw;
        std::optional<uint64_t> battery_voltage_uv;
        std::optional<int64_t>  battery_current_ua;
        bool is_discharging = false;
        std::optional<uint64_t> gpu_power_uw;
        std::optional<uint32_t> backlight_brightness;
        std::optional<uint32_t> backlight_max_brightness;
    };
    ```

### 2.2 `wattcurb::proc::ProcessAnalyzer`
- **Responsibilities**:
  - Scans `/proc` for active processes, capturing:
    - PID, process name (`comm`), user ID.
    - CPU utime and stime (clock ticks).
    - Voluntary and non-voluntary context switches from `/proc/[pid]/status`.
    - I/O bytes from `/proc/[pid]/io`.
    - GPU engine runtimes (`drm-engine-gfx`, `drm-engine-compute`) from `/proc/[pid]/fdinfo/*`.
  - Employs stack buffers and `std::from_chars` to avoid heap allocations when parsing numeric fields.

### 2.3 `wattcurb::policy::AttributionEngine`
- **Responsibilities**:
  - Computes exact duration $\Delta t = t_2 - t_1$.
  - Calculates physical component wattage:
    - $P_{\text{battery}} = \text{battery\_power\_uw} \times 10^{-6}$ W.
    - $P_{\text{gpu}} = \text{gpu\_power\_uw} \times 10^{-6}$ W.
    - $P_{\text{display}} = P_{\text{panel\_base}} + P_{\text{bl\_max}} \times (b / b_{\text{max}})$.
    - $P_{\text{cpu}} = P_{\text{rapl\_pkg}}$ (if available) or $(P_{\text{battery}} - P_{\text{gpu}} - P_{\text{display}} - P_{\text{base}})$.
  - Distributes $P_{\text{cpu}}$ across processes via $\Delta \text{CPU\_Ticks}$.
  - Distributes $P_{\text{gpu}}$ across DRM processes via $\Delta \text{Engine\_Time}$.
  - Computes Wakeup Tax and WDI (WattCurb Drain Index).
  - Sorts and ranks top power consumers.

### 2.4 `wattcurb::report::ReportGenerator`
- **Responsibilities**:
  - Outputs a visually rich ANSI terminal table displaying:
    - Hardware Domain Breakdown (Watts and % of System Total).
    - Detailed Software Process Attribution Table.
    - Anomaly / Runaway Alerts.
  - Outputs structured JSON for automated ingestion.

---

## 3. [`REF-TEST-002`] Oracle Gate Test Specifications

1. **Hardware Parser Integrity**:
   - Verify parsing of mock sysfs values (large microjoule integers, 32-bit rollover, missing fields).
2. **DRM fdinfo Parser Integrity**:
   - Verify parsing of DRM fdinfo buffers containing `drm-engine-gfx: 12345678 ns` and `drm-memory-vram: 31032 KiB`.
3. **Attribution Conservation Law**:
   - Assert that the sum of attributed dynamic CPU watts across all processes does not exceed the physical measured dynamic CPU package watts ($\sum P_{\text{proc\_dyn}} \le P_{\text{cpu\_dyn}} + \epsilon$).
4. **Performance & Memory Telemetry**:
   - Snapshot collection latency $< 20\text{ms}$.
   - Memory RSS $< 10\text{MB}$.
