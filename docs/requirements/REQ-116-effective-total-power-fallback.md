# [REF-REQ-116] Effective Total Power Fallback for Contribution Shares

**Status**: Implemented · **Date**: 2026-09-22
**Related**: [`REF-REQ-010`](REQ-007-extreme-hardware-telemetry.md),
[`REF-REQ-048`](REQ-048-compact-progressive-cyber-hud-and-extreme-performance-audit.md),
[`REF-REQ-115`](REQ-115-smu-thermal-power-limit-raise.md),
[`REF-TEST-074`](#5-verification--oracle-gate-standards-ref-test-074)

## 1. Why

The tray Cyber HUD renders CPU and GPU `[██████░░]` bars as each domain's
**share of total system drain** ([`REF-REQ-048`](REQ-048-compact-progressive-cyber-hud-and-extreme-performance-audit.md)
§2). That total is the battery gas gauge's DC rail
(`HardwarePowerBreakdown::total_system_watts`, written into the Seqlock as
`WattCurbSharedState::system_drain_mw`).

The gas gauge measures nothing while on AC: with the battery charged,
`POWER_SUPPLY_POWER_NOW=0` and `STATUS=Not charging`, so `total_system_watts` is
`0.0`. The tooltip's denominator then fell back to a `1 mW` floor, and

```
cpu_pct = clamp(cpu_drain_mw * 100 / 1, 0, 100)
gpu_pct = clamp(gpu_drain_mw * 100 / 1, 0, 100)
```

pinned **both** bars at 100% while the title simultaneously read `0.00 W` - an
observed symptom (AC, `POWER_NOW=0`). The percentages carried no information and
the total was wrong.

The project already defines the correct total elsewhere:
`report::get_effective_total_watts()` uses the DC rail only while discharging and
otherwise falls back to the sum of the physical domains. The tray's shared-state
writer simply did not use it, so the two surfaces disagreed.

## 2. Requirements

- **REQ-116.1 (Single definition)** The effective total shall be defined once, as
  `HardwarePowerBreakdown::effective_total_watts()`: the DC rail while
  discharging and positive, otherwise the sum of the physical domains
  (`cpu_package + gpu + display + fan + storage + uncore_and_platform`), and the
  rail again if that sum is also zero.
- **REQ-116.2 (Shared state publishes it)** `WattCurbSharedState::update_from_report()`
  shall store the effective total in `system_drain_mw`, not the raw rail. This is
  the single value the tray title, the tray contribution bars and the dashboard
  all read.
- **REQ-116.3 (No fabricated value)** When no domain is measured and no rail
  exists, the effective total shall be `0.0`. A zero total is only acceptable
  when nothing was actually measured; it must never be `0` merely because the
  machine is on AC.
- **REQ-116.4 (Hover probe must not override on AC)** `probe_sensors_for_hover()`
  refreshes `system_drain_mw` from the battery `uevent` only while the battery is
  **discharging**. On AC the `POWER_NOW` there is the charge power (or zero), not
  a system drain, so overwriting the daemon's effective total with it is wrong and
  makes the shares flicker between hover and non-hover.
- **REQ-116.5 (Reports unchanged)** `report::get_effective_total_watts()` shall
  delegate to the same definition so report and tray cannot drift apart.

## 3. Mechanism

| Item | Value |
| :--- | :--- |
| Definition | `HardwarePowerBreakdown::effective_total_watts()` (`src/core/types.hpp`) |
| Publisher | `WattCurbSharedState::update_from_report()` (`src/ipc/tray_shared_state.hpp`) |
| Consumer | `TrayClient::render_tooltip()` CPU/GPU bars, title total, dashboard |
| Hover guard | `TrayClient::probe_sensors_for_hover()` (`src/tray/tray_client.cpp`) |
| Report reuse | `report::get_effective_total_watts()` → the same method |

The tray's own `sys_mw = system_drain_mw > 0 ? system_drain_mw : 1` floor is kept
as a division guard, but it is no longer reachable from a real daemon report: the
effective total is at least the domain sum whenever any domain is measured.

## 4. Blast Radius & Failure Modes

- **Display semantics change on AC.** The tooltip title and the dashboard total
  now show the measured domain sum (e.g. `4.1 W`) instead of `0.00 W`, and the
  CPU/GPU shares are relative to that. This is the project's existing report
  definition, so it is more consistent, but it is a visible change.
- **No actuation.** This requirement touches only the display/IPC path; it writes
  no sysfs/procfs node and changes no policy.
- **The Seqlock POD layout is unchanged.** Only the value written into the
  existing `system_drain_mw` field changes, so no client/daemon struct mismatch is
  introduced.
- **Stale clients.** A tray from before this change reading a new daemon still
  reads a valid `system_drain_mw`; it simply benefits from the fallback too.

## 5. Verification & Oracle Gate Standards (REF-TEST-074)

`tests/test_units.cpp::test_effective_total_power_fallback()` proves, without
AC hardware:

1. `effective_total_watts()` returns the domain sum on AC (`0` rail), the rail
   while discharging, and honest `0.0` when nothing is measured;
2. `update_from_report()` publishes `4110 mW` for a `2.93 W CPU + 0.68 W GPU +
   0.5 W uncore` AC report;
3. `render_tooltip()` on that state shows the `4.1 W` total and a `71%` CPU share
   with **no** 8-block saturated bar - the pre-fix output;
4. a deliberately constructed raw-`0`-rail state does produce a full bar, guarding
   the invariant that a `0` rail is what caused the 100% symptom.

The test asserts the display arithmetic and the IPC value. It does not measure the
physical wall power on AC; the fallback is the project's existing report
definition, not a new physical measurement.
