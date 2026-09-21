# REF-REQ-094: Deterministic Battery-Threshold Profile Demotion

## 1. Context & Defect
The selected power profile changed on its own. Four sites each implemented their
own ladder with different thresholds:

| Site | Rule it enforced |
| :--- | :--- |
| `MitigationEngine::determine_profile()` | `<20%` Ultra, `<=50%` PowerSaver, `>55%` Balanced |
| `FeatureManager::evaluate_and_actuate()` | `<20%` Ultra, `<=50%` PowerSaver, else Balanced, plus a `<=20%` Performance lockout |
| `DaemonRunner` (two copies) | `<=20%` Performance -> Balanced |
| `DaemonRunner` PROFILE handler | rejected Performance at `<=20%` |

Worse, the user's selection was stored only in `FeatureManager::m_profile_override`
while `MitigationEngine::set_profile_override()` was **never called by the daemon**,
so the engine's own ladder re-derived a profile from battery percentage on every
cycle and overwrote the choice. On AC the code additionally forced Balanced
unconditionally.

## 2. Functional Requirements

### REQ-094.1: One authority
Exactly one function decides automatic profile changes:
`MitigationEngine::resolve_profile(current, on_battery, battery_pct, latch)`.
Every policy site calls it. No component may implement a second ladder.

### REQ-094.2: Stability is the default
Above 30% on battery, and on AC at **any** charge level, the current profile
stands. Connecting or disconnecting the charger never changes the profile.

### REQ-094.3: Three thresholds, each firing at most once
| Battery | Action | Scope |
| :--- | :--- | :--- |
| `<= 30%` | Performance -> Balanced | only when currently Performance; once per discharge cycle |
| `<= 20%` | -> PowerSaver | any profile; once per discharge cycle |
| `<= 5%`  | -> UltraEndurance | enforced continuously, overrides explicit selection |

### REQ-094.4: The latch records the crossing, not the action
Once a threshold is crossed it is spent, whether or not it demoted anything.
Re-selecting Performance at 25% must stick: the user is not demoted twice for one
crossing. A single drop past both 30% and 20% consumes both latches and demotes
once.

### REQ-094.5: Demotion is monotonic towards saving
A threshold may only move the profile towards **more** saving
(`Performance < Balanced < PowerSaver < UltraEndurance`). A user sitting in
UltraEndurance is never pulled back up to PowerSaver by the 20% rule.

### REQ-094.6: Latches rearm on recovery
`crossed_30` rearms above 35%, `crossed_20` above 25%, and both rearm on AC, so
the next discharge cycle protects the battery again. The guard band prevents a
reading hovering on the boundary from re-triggering.

### REQ-094.7: A demotion becomes the new baseline
When a threshold demotes, the stored override is updated to the demoted profile,
so the next cycle does not silently restore what the rule just moved away from.

### REQ-094.8: Only the critical floor overrides an explicit request
The `PROFILE` command rejects a request only below 5%, and only for profiles
other than UltraEndurance. Between 5% and 30% the user may select anything.

## 3. Verification
[`REF-TEST-057`](file:///home/jedclub/Develop/WattCurb/tests/test_units.cpp) covers
stability above 30% and on AC, each threshold's one-shot behaviour, the monotonic
guard, latch rearm after recovery, and a single drop past both thresholds.
