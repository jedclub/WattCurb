# REF-REQ-023: Battery Telemetry Dense Profiling & Full-Scope Oracle Gate Specification

## 1. Context & Motivation

In [`REF-REQ-022`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-019-deep-battery-and-power-supply-telemetry.md), WattCurb introduced full-fidelity physical battery telemetry, electrochemical wear tracking, ThinkPad charge thresholds, and wireless peripheral battery ingestion. 

To maintain the absolute Zero-Wakeup and Sub-Microsecond latency invariants required by [`AGENTS.md`](file:///home/jedclub/Develop/WattCurb/AGENTS.md), the entire battery data acquisition and calculation pipeline must be audited at dense, sub-microsecond resolution across every physical step:
1. Deconstruct battery telemetry into granular, dense profile scopes covering AC verification, uevent pread, SIMD parsing, cache updates, EC SMBus interactions, USB-PD telemetry, peripheral polling, and 5 distinct sub-phases of battery physics.
2. Maintain zero Heisenberg probe distortion: preserve function-level boundaries to prevent intra-loop profiler overhead from contaminating inner SIMD performance.
3. Validate through a full-scope end-to-end benchmark in the automated Oracle Gate regression pipeline.

---

## 2. Technical Requirements

### 2.1 Dense Profiling Scopes Breakdown (`REF-REQ-023-A`)
The battery telemetry pipeline must expose 15 distinct hardware and physical profiling targets:
- **Hardware Telemetry Ingestion (`HardwareProbe`)**:
  - `hw.battery_rail`: Root scope covering overall battery rail sampling.
  - `hw.battery.ac_check`: AC line power state verification.
  - `hw.battery.uevent_io`: Single 1KB `pread()` syscall on `BAT0/uevent`.
  - `hw.battery.uevent_simd_parse`: AVX2 SIMD delimiter scanner and O(1) jump table parser.
  - `hw.battery.cache_state_update`: In-memory synchronization of 13 dynamic/static battery fields.
  - `hw.battery.cached_replay`: Steady-state cache replay under AC line or unthrottled conditions.
  - `hw.battery.thresholds`: ThinkPad EC charge thresholds and behaviour setpoints wrapper.
  - `hw.battery.threshold_io`: Synchronous ACPI EC SMBus transactions (subsampled to 60s).
  - `hw.battery.usbc_pd`: USB-C Power Delivery provider status wrapper.
  - `hw.battery.usbc_io`: VFS file descriptor reads for USB-PD voltage, current, and provider type.
  - `hw.battery.peripherals`: Wireless peripheral battery iteration wrapper.
  - `hw.battery.peripheral_scan`: Individual Bluetooth/HID peripheral battery capacity and status inspection.
- **Electrochemical & Attribution Physics (`AttributionEngine`)**:
  - `attr.battery_physics`: Root scope covering all battery physical deductions.
  - `attr.battery.system_watts`: Net system battery discharge rate derivation.
  - `attr.battery.wear_and_health`: Battery health percentage, electrochemical degradation, and Wh lost capacity.
  - `attr.battery.runtime_projection`: Dual-domain Time-to-Empty (discharge) vs Time-to-Threshold/Full (charge).
  - `attr.battery.passthrough_detect`: ThinkPad Conservation Guard (80%) and Direct AC Hardware Pass-Through detection.
  - `attr.battery.usbc_flow`: USB-C input wattage derivation.

### 2.2 ACPI EC SMBus Transaction Elimination (`REF-REQ-023-B`)
- ThinkPad EC threshold nodes (`charge_control_start_threshold`, `charge_control_end_threshold`, `charge_behaviour`) block on SMBus/I2C hardware bus transactions (~60ms-88ms latency).
- Subsample EC charge threshold queries exclusively to daemon initialization (`sample_counter_ == 1`) and once every 30 passes (~60s), replaying cached values in the fast path to reduce per-turn threshold cost to $< 0.1 \mu s$.

---

## 3. Verification & Oracle Gate Standards (`REF-TEST-009`)

- **Automated Test Unit**: [`test_battery_telemetry_profiling_scopes()`](file:///home/jedclub/Develop/WattCurb/tests/test_units.cpp) in `tests/test_units.cpp`.
- **Benchmark Iterations**: 50,000 continuous iterations evaluating both micro-parsers and full-scope E2E pipelines.
- **Oracle Gate Latency Thresholds**:
  1. **Release Mode (`-DNDEBUG`)**:
     - Average SIMD uevent parser latency $< 0.35 \mu s/\text{op}$ (Achieved: **0.185 $\mu s$** / 314 cycles).
     - Average battery physics calculation $< 0.15 \mu s/\text{op}$ (Achieved: **0.074 $\mu s$**).
     - Full-scope E2E pipeline $< 0.60 \mu s/\text{op}$ (Achieved: **0.384 $\mu s$** / 651 cycles).
  2. **Development Profiler Mode (`-DWATTCURB_DEV_PROFILE=ON`)**:
     - All 8 essential sub-scopes (`hw.battery.uevent_simd_parse`, `attr.battery_physics`, `attr.battery.system_watts`, `attr.battery.wear_and_health`, `attr.battery.runtime_projection`, `attr.battery.passthrough_detect`) must be asserted in `ScopedProfilerRegistry`.
