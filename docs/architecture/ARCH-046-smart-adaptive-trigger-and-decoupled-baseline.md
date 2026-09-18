# [REF-ARCH-046] Smart Adaptive Trigger & Decoupled Baseline Architecture

## 1. Architectural Overview

This architecture implements **Option A (Smart Adaptive Trigger)** and **Decoupled Baseline Synchronization** specified in [`REF-REQ-069`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-069-smart-adaptive-trigger-and-temporal-sync.md).

It resolves the 6x CPU calculation artifact discovered in [`REF-RES-018`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-018-empirical-log-analysis-and-mitigation-patterns.md) while cutting runaway process detection latency from 60 seconds down to sub-10 seconds.

```
+-----------------------------------------------------------------------------+
|                          DaemonRunner Event Loop                            |
+-----------------------------------------------------------------------------+
                                       |
                         timerfd expiration (10.0s)
                                       v
                     [ process_observation_cycle() ]
                                       |
                 +---------------------+---------------------+
                 | (Interactive=false)                       | (Interactive=true)
                 v                                           v
       [ bg_tick_count_++ ]                   [ process_deep_observation_cycle() ]
                 |                                           |
        (bg_tick % 6 == 0)?                                  |
        /                 \                                  |
      YES                 NO                                 |
      /                     \                                |
     v                       v                               v
[Deep Sweep]    [ process_light_probe_cycle() ]      (2s Cadence)
(60s Sweep)             |
                 Hardware Power Calc
                 (hw_light_prev_ vs hw_cur)
                        |
                 Spike Detector Check:
                 - pkg_w >= 12.0W OR sys_w >= 20.0W?
                 - delta_w >= 8.0W?
                 - cooldown >= 15s?
                        |
                 +------+------+
                 |             |
               Spike        No Spike
                 |             |
                 v             v
         [ Early Deep Sweep ] [ Sleep 10s ]
         (bg_tick_count = 0)
```

---

## 2. Decoupled Baseline Data Layout in `DaemonRunner`

```cpp
class DaemonRunner {
private:
    // Decoupled Baseline State (REF-REQ-069-1, REF-ARCH-046)
    HardwareSample hw_light_prev_{}; // Updated every 10s light probe cycle
    HardwareSample hw_deep_prev_{};  // Updated strictly on deep observation cycles

    // Process Snapshot Ring Pool (Pair-synchronized with hw_deep_prev_)
    DoubleBufferedPool<ProcessSnapshot> proc_pool_{};

    // Spike Detection & Anti-Storm Cooldown State
    double   last_light_system_watts_{0.0};
    uint64_t last_early_sweep_time_ms_{0};
    static constexpr uint64_t EARLY_SWEEP_COOLDOWN_MS = 15'000; // 15 seconds
};
```

---

## 3. Detailed Execution Pipeline

### 3.1 Light Probe Cycle (`process_light_probe_cycle`)
1. Read current hardware sample: `auto hw_cur = hw_probe_.capture_sample();`.
2. Compute light hardware power:
   `cached_report_.hardware = engine_.compute_hardware_power(hw_light_prev_, hw_cur, 10.0);`
3. Export Seqlock state & append to `HistoryRingBufferShm` (< 50µs, Zero proc traversal).
4. Evaluate Spike Condition:
   ```cpp
   double pkg_w = cached_report_.hardware.cpu_package_watts;
   double sys_w = cached_report_.hardware.total_system_watts;
   bool on_battery = cached_report_.hardware.is_battery_discharging;
   double delta_w = sys_w - last_light_system_watts_;
   last_light_system_watts_ = sys_w;

   bool spike = false;
   if (on_battery) {
       spike = (pkg_w >= 10.0) || (sys_w >= 18.0) || (delta_w >= 8.0);
   } else {
       spike = (pkg_w >= 16.0) || (sys_w >= 30.0) || (delta_w >= 12.0);
   }
   ```
5. Advance `hw_light_prev_`:
   `hw_light_prev_ = std::move(hw_cur);`
6. Return `spike && (now_ms - last_early_sweep_time_ms_ >= EARLY_SWEEP_COOLDOWN_MS)`.

### 3.2 Deep Sweep Cycle (`process_deep_observation_cycle`)
1. Take current hardware sample `hw_cur` and current process snapshot `cur_snapshot`.
2. `compute_attribution` receives:
   - `hw_deep_prev_` and `hw_cur`.
   - `prev_snapshot.span()` and `cur_snapshot.span()`.
3. Because `hw_deep_prev_` was last updated when `prev_snapshot` was taken, `delta_sec = hw_cur.timestamp - hw_deep_prev_.timestamp` **exactly equals the process accumulation interval** (whether 2s, 10s early, or 60s steady-state).
4. Evaluate features, update SHM, update History.
5. Synchronized advancement:
   ```cpp
   hw_deep_prev_ = hw_cur;
   hw_light_prev_ = std::move(hw_cur); // Re-align light baseline as well
   proc_pool_.swap(); // 0ns pointer swap
   ```

---

## 4. Oracle Gate & Verification Specifications (`REF-TEST-034`)

- Unit tests must verify:
  1. `test_decoupled_baseline_timebase_accuracy`: Synthetic ticks over 60s with 10s light steps produce exact 1x CPU usage, not 6x.
  2. `test_smart_adaptive_spike_trigger`: Injecting 22W total load flags `spike = true` and triggers early deep sweep within 10s.
  3. `test_early_sweep_cooldown`: Successive spikes within 15s respect cooldown window and do not trigger redundant proc scans.
