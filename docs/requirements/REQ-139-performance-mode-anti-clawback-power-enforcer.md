# [REF-REQ-139] Performance Mode Anti-Clawback Power Enforcer & Skin Temperature Ceiling Lift

**Status**: Implemented · **Date**: 2026-10-05  
**Related**: [`REF-ARCH-086`](../architecture/ARCH-086-high-frequency-smu-enforcement.md),
[`REF-REQ-115`](REQ-115-smu-thermal-power-limit-raise.md),
[`REF-REQ-123`](REQ-123-ec-smu-power-cap-diagnosis.md),
[`REF-REQ-126`](REQ-126-ec-smu-power-limit-clawback-and-reassertion.md),
[`REF-TEST-093`](#ref-test-093)

---

## 1. Operational Problem & Hardware Motivation

On the reference host (ThinkPad L15 Gen 1, AMD Ryzen 7 PRO 4750U, Renoir APU), real-world profiling under continuous compile, development, and compute loads revealed severe recurring performance degradation:

1. **Embedded Controller (EC) Power Clawback**:
   * Even when WattCurb applies elevated sustained power limits (STAPM 25 W), the ThinkPad Embedded Controller (EC) periodically drops the CPU STAPM sustained limit from 25 W down to **12 W (or 6 W)** whenever chassis thermals reach 60–70 °C.
   * Under a 12 W sustained power cap, all 8 cores (16 threads) collapse to the P-state floor (550–1400 MHz), inducing catastrophic latency spikes and UI stutter.
2. **Missing APU Skin Temperature Lift (STAPM Trigger)**:
   * The AMD APU firmware relies on **Skin Temperature Aware Power Management (STAPM)**. While [`REF-REQ-115`](REQ-115-smu-thermal-power-limit-raise.md) targeted `THM LIMIT CORE` (`--tctl-temp`), it omitted `--apu-skin-temp` (`STT LIMIT APU`).
   * When chassis surface thermals cross the factory 45 °C skin threshold, the hardware predictive thermal model triggers proactive STAPM throttling regardless of core temperature.
3. **Excessive Verification Latency**:
   * Prior art ([`REF-REQ-126`](REQ-126-ec-smu-power-limit-clawback-and-reassertion.md)) enforced a 3-cycle (~30 s) verification interval (`SMU_VERIFY_INTERVAL_CYCLES = 3`).
   * A 30-second duration of 600–1400 MHz throttling is immediately tangible to the developer during interactive workloads. In `Performance` profile, this latency must be eliminated.

---

## 2. Functional Requirements

```
┌────────────────────────────────────────────────────────────────────────┐
│               Stage 1: Multi-Domain Thermal Unshackling                │
│  • Lift STT LIMIT APU (--apu-skin-temp = 85 °C)                        │
│  • Assert THM LIMIT CORE (--tctl-temp = 85 °C)                         │
│  • Enforce STAPM 28 W, Fast PPT 35 W, Slow PPT 30 W                    │
└──────────────────────────────────┬─────────────────────────────────────┘
                                   │
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│            Stage 2: Adaptive Cadence Anti-Clawback Loop                │
│  • Performance Mode: Check cadence = 1 cycle (~3.0 s)                  │
│  • Balanced Mode: Check cadence = 3 cycles (~30 s)                     │
│  • Minimum load trigger: load1 >= 0.5                                  │
└──────────────────────────────────┬─────────────────────────────────────┘
                                   │ Clawback Detected (STAPM < 18 W)
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│               Stage 3: Sub-Second Power Re-assertion                   │
│  • Immediately re-apply SMU performance table via ryzenadj             │
│  • Stifle EC 12 W throttle within < 3 seconds                          │
│  • Maintain all-core clock >= 2.6 - 3.2 GHz under sustained load       │
└────────────────────────────────────────────────────────────────────────┘
```

### REF-REQ-139.1 (Dual Thermal Domain Unshackling)
* `MitigationEngine::apply_smu_performance_limits()` shall command **both** the die thermal limit and the skin thermal limit:
  * Core limit: `--tctl-temp=85` (`SMU_TCTL_PERF_C = 85 °C`)
  * APU Skin limit: `--apu-skin-temp=85` (`SMU_APU_SKIN_PERF_C = 85 °C`)
* Lifting the skin temperature limit prevents premature predictive STAPM power cuts before the cooling solution reaches capacity.

### REF-REQ-139.2 (Enhanced Sustained Power Ceilings for Performance)
* To guarantee sustained all-core frequency under heavy parallel workloads:
  * Sustained limit (`STAPM`): **28,000 mW (28 W)** (up from 25,000 mW)
  * Fast limit (`PPT FAST`): **35,000 mW (35 W)**
  * Slow limit (`PPT SLOW`): **30,000 mW (30 W)**
  * APU Slow limit: **30,000 mW (30 W)**

### REF-REQ-139.3 (Profile-Adaptive Enforcement Cadence)
* Verification of SMU limits shall operate with profile-differentiated timing:
  * **Performance Mode**: `SMU_VERIFY_INTERVAL_PERF_CYCLES = 1` (evaluates every tick, $\sim 3.0\,\text{s}$). If the EC reclaims the power limit, the raise is re-applied within 3 seconds, eliminating perceptible throttling lag.
  * **Balanced Mode**: `SMU_VERIFY_INTERVAL_BALANCED_CYCLES = 3` ($\sim 30\,\text{s}$). Preserves low overhead on balanced everyday computing.
  * **PowerSaver / UltraEndurance**: SMU verification is bypassed completely; the firmware's conservative limits remain in force as intended.

### REF-REQ-139.4 (Relaxed Load Trigger for Performance)
* In `Performance` mode, the minimum load threshold to trigger SMU verification shall be lowered:
  $$\text{Load1}_{\text{min}} = 0.5 \quad (\text{formerly } 1.0)$$
  This ensures anti-clawback protection activates immediately when starting interactive tasks or single-threaded compile spikes.

### REF-REQ-139.5 (Sandbox Containment & Idempotent Safety)
* When `MitigationEngine::actuation_sandboxed()` is active, zero writes to `/dev/mem` or `ryzenadj` invocations are permitted.
* If `read_back_stapm_limit()` returns $\ge 18,000\,\text{mW}$, the limit is considered healthy and zero writes are issued, preventing unnecessary bus traffic.

---

## 3. Verification & Oracle Gate Standards (`REF-TEST-093`)

* **`test_performance_smu_anti_clawback_policy`**:
  1. Verify constant ordering: $\text{STAPM} (28\,\text{W}) \le \text{Slow} (30\,\text{W}) \le \text{Fast} (35\,\text{W})$.
  2. Verify skin temperature parameter format (`--apu-skin-temp=85`).
  3. Verify adaptive cadence selection: 1 cycle for Performance, 3 cycles for Balanced.
  4. Verify clawback threshold gating: trips when $\text{observed} < 18,000\,\text{mW}$; healthy when $\ge 18,000\,\text{mW}$.
