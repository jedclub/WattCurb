# [REF-REQ-119] Dashboard & Battery Report Window Visibility

**Status**: Implemented · **Date**: 2026-09-22
**Related**: [`REF-REQ-036`](REQ-036-native-kde-dashboard-matrix.md),
[`REF-REQ-078`](REQ-078-battery-report-window.md),
[`REF-REQ-081`](REQ-081-battery-report-action-and-tactile-buttons.md),
[`REF-TEST-076`](#5-verification--oracle-gate-standards-ref-test-076)

## 1. Why

Both GUI windows failed to appear on KDE Plasma Wayland:

- the **power matrix dashboard** (`wattcurb-dashboard`) opened no window, and
- the **deep battery drain report** (`wattcurb-dashboard --report`) opened no
  window.

The processes were alive (`timeout` returned 124 after the full interval, so
`app.exec()` had been reached); `ldd` showed every Qt6 library resolved; the
Wayland platform plugin `libqwayland.so` was present; and `--report-cli`
(offscreen, no window) produced a correct report. So the data path, the Qt
runtime and the platform plugin were all fine - **only window presentation**
failed.

Root cause, in `src/ui/main_dashboard.cpp`:

```cpp
std::string lock_name = report_mode ? "wattcurb-report.lock" : "wattcurb-dashboard.lock";
wattcurb::core::SingletonLock mode_lock(lock_name);
if (!mode_lock.is_locked()) { ...; return 0; }   // <- not the cause
...
engine.load(qmlUrl);
```

The report window's QML declared a plain `Window` with `visible: false`
(`BatteryReportWindow.qml`). Unlike `ApplicationWindow`, a plain `Window` is
**never shown by `QQmlApplicationEngine`** - the engine creates it and leaves it
invisible. The only thing that ever showed it was the `objectCreated` callback:

```cpp
} else if (obj && report_mode) {
    obj->setProperty("visible", true);
}
```

That callback is connected with `Qt::QueuedConnection` and sets a QML
**property**, not the QWindow's shown state. It raced creation and never
reliably won, so the window was constructed and stayed hidden. The dashboard
window (`ApplicationWindow`, `visible: true`) was also being routed through the
same fragile path.

## 2. Requirements

- **REQ-119.1 (Top-level report window is visible)** The report window shall set
  `visible: true` in its own QML so that, when loaded as a top-level window by
  `--report`, it is shown without depending on a callback race.
- **REQ-119.2 (Embedded copy stays hidden)** The instance embedded in
  `DashboardWindow.qml` shall explicitly set `visible: false`, because it must
  only appear on an explicit user action (the three existing call sites set
  `visible`/`show()`/`raise()`/`requestActivate()` together).
- **REQ-119.3 (Explicit show)** `main_dashboard.cpp` shall show the created
  top-level window through the QWindow API (`QQuickWindow::show()`, `raise()`,
  `requestActivate()`), not by setting a QML property, so presentation does not
  depend on binding evaluation order.
- **REQ-119.4 (No behavioural change to the CLI)** `--report-cli` and
  `--benchmark` shall be unaffected; they deliberately use the offscreen
  platform and return before this code.

## 3. Mechanism

| Item | Location |
| :--- | :--- |
| Top-level visibility | `src/ui/qml/BatteryReportWindow.qml` (`visible: true`) |
| Embedded override | `src/ui/qml/DashboardWindow.qml` (`BatteryReportWindow { visible: false }`) |
| Explicit show | `src/ui/main_dashboard.cpp` `objectCreated` lambda |

## 4. Blast Radius & Failure Modes

- **The report window is now visible the moment it loads.** If a user launches
  it and does not want it, they close it; previously it never appeared at all,
  which is strictly worse.
- **QML change, no C++ behaviour change.** The dashboard's own logic, the tray
  integration and the report data are untouched.
- **Not measured.** Whether the window is properly focused and raised under
  KWin's focus-stealing prevention was not verified on the host; `show()` +
  `raise()` + `requestActivate()` is the standard sequence, but a compositor may
  still refuse activation, leaving the window opened but unfocused.

## 5. Verification & Oracle Gate Standards (REF-TEST-076)

The QML resources are embedded in the binary and validated by the existing
dashboard QML integrity checks (`REF-TEST-039`, `REF-TEST-053`, `REF-TEST-054`),
which load and exercise `DashboardWindow.qml` and `BatteryReportWindow.qml` and
would fail on a QML syntax or resource error. The window *presentation* itself
requires a live compositor and is therefore verified on the host after
installing the release, not in the unit suite - stated rather than implied.

The change is also covered by the release procedure: the dashboard binary is
rebuilt through the PGO pipeline and the window is opened on the running desktop
as the acceptance check.
