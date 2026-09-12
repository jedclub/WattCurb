# REF-REQ-023: Battery Telemetry Fine-Grained Profiling & Oracle Gate Verification

## 1. Context & Motivation

In [`REF-REQ-022`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-019-deep-battery-and-power-supply-telemetry.md), WattCurb introduced full-fidelity physical battery telemetry, electrochemical wear tracking, ThinkPad charge thresholds, and wireless peripheral battery ingestion. 

To maintain the absolute Zero-Wakeup and Sub-Microsecond latency invariants required by [`AGENTS.md`](file:///home/jedclub/Develop/WattCurb/AGENTS.md), the entire battery ingestion and calculation pipeline must be transparently audited at sub-microsecond and PMU cycle resolution:
1. Individual phases of battery telemetry (AC state query, uevent `pread()`, AVX2 SIMD tokenization, EC charge threshold access, USB-PD status, wireless peripherals, and attribution calculations) must have dedicated scoped profiler boundaries.
2. In-loop hardware bus stalls (specifically ACPI Embedded Controller SMBus round trips on `/sys/class/power_supply/BAT*/charge_control_*_threshold`) must be identified, quantified, and suppressed through intelligent subsampling.
3. Automated regression validation ("Oracle Gate") must enforce strict latency thresholds for battery SIMD parsing (< 0.35 us/op) and zero heap allocations.

---

## 2. Technical Requirements

### 2.1 Fine-Grained Profiling Scopes (`REF-REQ-023-A`)
The battery polling routine in [`HardwareProbe::poll`](file:///home/jedclub/Develop/WattCurb/src/hw/hardware_probe.cpp) and calculation in [`AttributionEngine::compute_hardware_power`](file:///home/jedclub/Develop/WattCurb/src/policy/attribution_engine.cpp) must emit the following distinct scoped profiler telemetry targets:
- `hw.battery.ac_check`: AC online state sysfs read and integer extraction.
- `hw.battery.uevent_io`: Single `pread()` syscall on `BAT0/uevent` (kernel VFS + battery driver residency).
- `hw.battery.uevent_simd_parse`: AVX2 SIMD newline scanning, O(1) prefix branch table, and string-to-integer conversion.
- `hw.battery.cached_replay`: Low-overhead cache copy when detailed battery read is subsampled during AC or idle.
- `hw.battery.thresholds`: ThinkPad Embedded Controller charge thresholds (`start`, `end`, `behaviour`).
- `hw.battery.usbc_pd`: USB-C Power Delivery provider status, voltage, current, and maximum capacity.
- `hw.battery.peripherals`: Wireless Bluetooth/HID battery device polling (mice, keyboards, styluses).
- `attr.battery_physics`: Electrochemical degradation Wh, dual-domain runtime prediction, AC direct pass-through, and conservation guard logic.

### 2.2 ACPI EC SMBus Transaction Elimination (`REF-REQ-023-B`)
- ThinkPad EC threshold nodes (`charge_control_start_threshold`, `charge_control_end_threshold`, `charge_behaviour`) block on SMBus/I2C hardware bus transactions (~60ms latency).
- Polling these nodes on every monitoring tick (e.g. 2s) wastes active CPU cycles and impedes deep sleep.
- Requirement: Sample EC charge thresholds exclusively upon daemon initialization (`sample_counter_ == 1`) and subsample to once every 30 monitoring passes (~60s), replaying cached values in the fast path to reduce per-turn threshold cost to < 0.3 us.

### 2.3 SIMD uevent Parser O(1) Branch Dispatch (`REF-REQ-023-C`)
- Replace sequential string scans (`line.rfind()`) with a common prefix stripper (`"POWER_SUPPLY_"`) and a jump table switch based on the leading character (`'S'`, `'V'`, `'C'`, `'P'`, `'E'`, `'M'`, `'T'`).
- Enforce throughput: parser must process full ThinkPad `BAT0/uevent` mock in < 0.20 us (target: < 350 CPU cycles).

---

## 3. Verification & Oracle Gate Standards (`REF-TEST-009`)

- **Automated Test Unit**: [`test_battery_telemetry_profiling_scopes()`](file:///home/jedclub/Develop/WattCurb/tests/test_units.cpp) in `tests/test_units.cpp`.
- **Benchmark Iterations**: 50,000 continuous iterations on representative ThinkPad `uevent` payload.
- **Oracle Gate Assertions**:
  1. Average SIMD parsing latency < 0.35 us/op.
  2. Average battery physics calculation < 0.10 us/op.
  3. Under `-DWATTCURB_DEV_PROFILE=ON`, `hw.battery.uevent_simd_parse` and `attr.battery_physics` must be recorded in `ScopedProfilerRegistry`.
  4. Under `-DNDEBUG` (Production), zero diagnostic strings or profiler symbols remain in the compiled binary.
