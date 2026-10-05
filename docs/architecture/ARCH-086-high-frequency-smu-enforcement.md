# [REF-ARCH-086] High-Frequency SMU Power Enforcement & Multi-Domain Thermal Unshackling Architecture

**Implements**: [`REF-REQ-139`](../requirements/REQ-139-performance-mode-anti-clawback-power-enforcer.md)  
**Date**: 2026-10-05  

---

## 1. Architectural Architecture & Subsystem Split

The SMU Anti-Clawback Engine operates at the intersection between the profile state machine (`battery_feature.cpp`) and hardware actuation primitives (`MitigationEngine`).

```
                    FeatureManager / Low-Frequency Telemetry Tick
                                         │
                   ┌─────────────────────┴─────────────────────┐
                   ▼                                           ▼
      Profile Mode Evaluation                      Thermal / Fan Subsystem
   (Balanced / Performance / Save)             (FanCurveEngine, REF-REQ-124)
                   │
                   ▼
     Adaptive Anti-Clawback Gate
   - Perf: Cadence = 1 tick (~3 s), Load >= 0.5
   - Bal : Cadence = 3 ticks (~30 s), Load >= 1.0
                   │
                   ▼
       MitigationEngine::verify_and_reassert_smu_limits()
                   │
         [read_back_stapm_limit()]
                   │
        ┌──────────┴──────────┐
        ▼                     ▼
    >= 18 W                < 18 W (EC Clawback Detected)
   (Healthy)                  │
        │                     ▼
    [No Action]   [apply_smu_performance_limits()]
                  - tctl-temp     = 85 °C
                  - apu-skin-temp = 85 °C
                  - stapm-limit   = 28,000 mW
                  - fast-limit    = 35,000 mW
                  - slow-limit    = 30,000 mW
```

---

## 2. Parameter Blueprint

| SMU Parameter | Former Baseline | REF-REQ-139 Target | Purpose |
| :--- | :--- | :--- | :--- |
| `tctl-temp` | 85 °C | **85 °C** | Die thermal junction ceiling |
| `apu-skin-temp` | omitted (default 45 °C) | **85 °C** | Prevents chassis surface predictive throttling |
| `stapm-limit` | 25,000 mW (25 W) | **28,000 mW (28 W)** | High-throughput sustained power headroom |
| `fast-limit` | 35,000 mW (35 W) | **35,000 mW (35 W)** | Transient boost surge capability |
| `slow-limit` | 30,000 mW (30 W) | **30,000 mW (30 W)** | Long-duration compile/render ceiling |
| `apu-slow-limit`| 30,000 mW (30 W) | **30,000 mW (30 W)** | Dedicated APU subsystem ceiling |

---

## 3. Pure Decision Functions (Oracle Gate)

To ensure determinism without requiring root privilege or live `/dev/mem` access during unit tests:

```cpp
[[nodiscard]] static constexpr uint32_t smu_verify_interval_cycles(PowerProfileMode mode) noexcept {
    return (mode == PowerProfileMode::Performance) ? 1 : 3;
}

[[nodiscard]] static constexpr double smu_verify_min_load(PowerProfileMode mode) noexcept {
    return (mode == PowerProfileMode::Performance) ? 0.5 : 1.0;
}
```
