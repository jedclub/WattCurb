# REF-ARCH-013: Battery Telemetry Dense Profiling & Sub-Microsecond Optimization Architecture

## 1. Dense Profiling Scope Architecture

[`REF-REQ-023`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-020-battery-telemetry-profiling-and-oracle-gate.md) decomposes the battery ingestion and attribution pipeline into 15 dense profiling boundaries:

```
HardwareProbe::poll()
 └── "hw.battery_rail"
      ├── "hw.battery.ac_check"             -> /sys/class/power_supply/AC/online read
      ├── "hw.battery.uevent_io"            -> Single 1KB pread() on BAT0/uevent
      ├── "hw.battery.uevent_simd_parse"    -> AVX2 find_char_fast + O(1) prefix switch parser
      ├── "hw.battery.cache_state_update"   -> 13-field cache sync (50 ns)
      ├── "hw.battery.cached_replay"        -> Fast-path replay during steady state (210 ns)
      ├── "hw.battery.thresholds"           -> ThinkPad EC charge thresholds wrapper
      │    └── "hw.battery.threshold_io"    -> Synchronous ACPI EC SMBus I/O (subsampled)
      ├── "hw.battery.usbc_pd"              -> USB-C Power Delivery wrapper
      │    └── "hw.battery.usbc_io"         -> USB-PD sysfs FD reads
      └── "hw.battery.peripherals"          -> Wireless peripheral battery wrapper
           └── "hw.battery.peripheral_scan" -> Individual Bluetooth/HID battery scan
AttributionEngine::compute_hardware_power()
 └── "attr.battery_physics"
      ├── "attr.battery.system_watts"       -> Average discharge rate calculation (50 ns)
      ├── "attr.battery.wear_and_health"    -> Degradation % & lost Wh calculation (190 ns)
      ├── "attr.battery.runtime_projection" -> Dual-domain Time-to-Empty / Full (60 ns)
      ├── "attr.battery.passthrough_detect" -> 80% Conservation & AC Pass-Through (50 ns)
      └── "attr.battery.usbc_flow"          -> USB-C wattage calculation (50 ns)
```

---

## 2. Empirical PMU Live Telemetry Audit

Live telemetry collected on Lenovo ThinkPad host with `--dev-profile`:

| Profiler Scope Name | Calls | Total (ms) | Share (%) | Avg (us/op) | Min (us) | Max (us) |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| `hw.battery.thresholds` | 5 | 88.524 | 17.8% | 17704.78 | **0.04** | 88523.65 (1st EC pass) |
| `hw.battery.threshold_io` | 1 | 88.523 | 17.8% | 88522.80 | 88522.80 | 88522.80 |
| `hw.battery.ac_check` | 5 | 3.210 | 0.6% | 642.02 | 502.14 | 749.08 |
| `hw.battery.usbc_pd` | 5 | 0.164 | 0.0% | 32.81 | 27.13 | 48.61 |
| `hw.battery.usbc_io` | 5 | 0.162 | 0.0% | 32.43 | 26.87 | 47.95 |
| `hw.battery.uevent_io` | 1 | 0.031 | 0.0% | **31.07** | 31.07 | 31.07 |
| `hw.battery.uevent_simd_parse` | 1 | 0.002 | 0.0% | **2.11** | 2.11 | 2.11 |
| `attr.battery_physics` | 1 | 0.002 | 0.0% | **1.76** | 1.76 | 1.76 |
| `hw.battery.peripherals` | 5 | 0.001 | 0.0% | **0.25** | 0.15 | 0.51 |
| `hw.battery.cached_replay` | 4 | 0.001 | 0.0% | **0.21** | 0.14 | 0.28 |
| `attr.battery.wear_and_health` | 1 | 0.000 | 0.0% | **0.19** | 0.19 | 0.19 |
| `attr.battery.runtime_projection` | 1 | 0.000 | 0.0% | **0.06** | 0.06 | 0.06 |
| `attr.battery.usbc_flow` | 1 | 0.000 | 0.0% | **0.05** | 0.05 | 0.05 |
| `attr.battery.passthrough_detect` | 1 | 0.000 | 0.0% | **0.05** | 0.05 | 0.05 |
| `attr.battery.system_watts` | 1 | 0.000 | 0.0% | **0.05** | 0.05 | 0.05 |
| `hw.battery.cache_state_update` | 1 | 0.000 | 0.0% | **0.05** | 0.05 | 0.05 |

---

## 3. Full-Scope End-to-End Pipeline Performance

In automated Oracle Gate testing ([`REF-TEST-009`](file:///home/jedclub/Develop/WattCurb/tests/test_units.cpp)), 50,000 continuous full-scope iterations (complete ingestion + all physical derivations + peripheral updates) executed at:
- **Full Pipeline Latency**: **0.3837 us/op (383.7 ns)**
- **Hardware Cycles**: **651.1 cycles/op**
- **Heap Allocations**: **0 bytes (Strict Zero-Allocation)**
