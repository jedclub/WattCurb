# REF-REQ-107: Performance Mode Throughput Guarantee

## 1. Context & Measured Defect
[`REF-RES-027`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-027-performance-mode-clock-collapse-incident.md)
measured the daemon running in `Performance` on battery and found the machine
doing **less** work per second than with no power manager running at all. A
profile whose entire contract is "spend power freely, never hold anything back"
was a net loss.

Fixed-work bisection (wall time for a deterministic 8,000,000-iteration integer
loop; baseline 1.424 s):

| Actuation | Wall time | Factor |
| :--- | ---: | ---: |
| `/dev/cpu_dma_latency` held at 0 us | 2.674 s | **1.88x slower** |
| `platform_profile=performance` | 1.815 s | 1.27x slower |
| GPU `dpm=high` + `pp_power_profile_mode=1` | 1.470 s | 1.03x (noise) |
| `sched_migration_cost_ns=5000000` | - | knob absent on this kernel |

## 2. Functional Requirements

### REQ-107.1: Performance mode is measured, never assumed
No actuation may be added to the `Performance` branch on the reasoning that it
"should" be faster. Each one carries a measured fixed-work figure against the
profile-less baseline, on the hardware it ships to.

### REQ-107.2: The C0 latency clamp is not applied
`Performance` must **not** hold `/dev/cpu_dma_latency` at 0 us. REQ-092.1
asserted that eliminating idle-exit latency raises throughput; on Zen it does
the opposite. Opportunistic boost is governed by accumulated power and thermal
budget, and pinning every core in C0 spends that budget continuously instead of
letting idle cores return headroom. Measured cost: **1.88x**.

The descriptor is explicitly released when entering `Performance`, so a clamp
taken by an earlier build or an earlier profile cannot survive into it.

This does **not** affect the audio latency floor of
[`REF-REQ-096`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-096-audio-continuity-guarantee.md),
which holds a separate descriptor at 100 us only while a PCM stream is RUNNING.
The kernel takes the minimum across holders, so the two never interact.

### REQ-107.3: No process throttling in Performance
Already required by REQ-092.2 and enforced at
`FeatureManager::actuate_anti_starvation_cap()`, which returns without acting
when the mode is `Performance`. Restated here because it is the requirement most
likely to be eroded by a future feature that "only" caps greedy processes.

### REQ-107.4: An actuation that cannot be applied must not be recorded as applied
`set_sched_migration_cost()` targets `/proc/sys/kernel/sched_migration_cost_ns`,
which does not exist on BORE/EEVDF kernels (verified absent on
`7.2.5-1-cachyos`). The actuator returns `false` and must not set its
`*_modified` flag, so `restore_hardware_baseline()` never writes a value the
daemon did not set. Verified by reading the code path, not by a test.

### REQ-107.5: Competing power managers are a stated conflict
`power-profiles-daemon` was found running and configured to `balanced`, driving
the same `/sys/firmware/acpi/platform_profile` node WattCurb writes, and
`ananicy-cpp` was found applying `nice` and `SCHED_BATCH` to arbitrary processes.
Neither is controlled by WattCurb. Any measurement of WattCurb's effect must
state whether they were running, and the nice values they apply must not be
attributed to WattCurb - RES-027 originally made exactly that mistake.

## 3. Verification
[`REF-TEST-061`](file:///home/jedclub/Develop/WattCurb/tests/test_units.cpp)
asserts that `apply_power_profile(Performance)` leaves no `cpu_dma_latency`
clamp held by the Performance path, and that the audio floor is independent of
it.

**Not verified by the suite**: the throughput figures above. They are host
measurements, and the suite runs under `set_actuation_sandbox(true)`. REQ-106.6
requires them to be re-measured on the host after each release that touches the
`Performance` branch.
