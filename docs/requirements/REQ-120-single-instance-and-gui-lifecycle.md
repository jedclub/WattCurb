# [REF-REQ-120] Single-Instance Programs & GUI Lifecycle (only tray + daemon persist)

**Status**: Implemented · **Date**: 2026-09-22
**Related**: [`REF-REQ-002`](REQ-001-hardware-power-profiling.md),
[`REF-REQ-036`](REQ-036-native-kde-dashboard-matrix.md),
[`REF-REQ-111`](REQ-111-audit-defect-remediation.md),
[`REF-REQ-119`](REQ-119-dashboard-and-report-window-visibility.md),
[`REF-TEST-077`](#5-verification--oracle-gate-standards-ref-test-077)

## 1. Why

Two behaviours were wrong in the field:

1. **The dashboard binary was not single-instance.** The lock was per-mode
   (`wattcurb-dashboard.lock` vs `wattcurb-report.lock`), so the matrix dashboard
   and the battery report ran as two processes at once - measured: **2
   processes**, two event loops, ~170 MB RSS each.
2. **A minimised window kept the whole process alive.** Measured: **167 MB RSS
   and a live event loop while minimised**.

The requirement is that **only the tray icon and the daemon persist**; every other
WattCurb program is a one-shot tool that must release all CPU, GPU and memory
when its window is gone.

## 2. Requirements

- **REQ-120.1 (One GUI instance)** The dashboard binary shall take a single
  singleton lock regardless of mode, so dashboard and report cannot run
  concurrently. The tray shall close the previously open GUI before launching
  the other one.
- **REQ-120.2 (Precise process matching)** Closing the previous GUI shall match
  the executable name token at the start of the command line, not a bare
  substring. The previous `pkill -f wattcurb-dashboard` also matched any command
  line mentioning the path - a diagnostic script in this repository killed
  itself with it.
- **REQ-120.3 (Window gone ⇒ process gone)** Closing the top-level window shall
  terminate the process. Application-side hiding shall terminate it as well.
- **REQ-120.4 (Persistent set)** Only `wattcurb --daemon` and `wattcurb-tray`
  are long-lived. The CLI queries (`-s`, `-b`, `-R`, `-L`, `-H`, `-F`) are
  one-shot and already exit.

## 3. Mechanism

| Item | Location |
| :--- | :--- |
| Unified GUI lock | `src/ui/main_dashboard.cpp` (`wattcurb-dashboard.lock`) |
| Close ⇒ quit | `QQuickWindow::closing` → `QCoreApplication::quit()` |
| Hide ⇒ quit | `QWindow::visibilityChanged` (Hidden/Minimized, after first show) |
| Minimise hint dropped | `win->setFlags(... & ~Qt::WindowMinimizeButtonHint)` |
| Precise GUI close | `close_existing_gui()` (`src/tray/tray_client.cpp`) |

### 3.1 The Wayland minimise limitation (measured, not assumed)

On this host (KDE Plasma 6 Wayland), when KWin minimises a window the client
receives **nothing**:

| Probe | Result after compositor minimise |
| :--- | :--- |
| `visibilityChanged` | never fires |
| `windowStateChanged` | never fires |
| `visibleChanged` | never fires |
| `isExposed()` | stays `1` |
| `visibility()` | stays `Windowed` (2) |

Core Wayland has no "you were minimised" event, so a client cannot detect a
compositor-side minimise. The minimise button hint is dropped so the window does
not advertise a state the process cannot react to, but a minimise triggered by a
keyboard shortcut or the taskbar menu still leaves the process running. This is a
platform limitation, stated rather than hidden.

## 4. Blast Radius & Failure Modes

- **Opening the report closes the dashboard** (and vice versa) because they share
  one lock. The tray closes the previous window first, so the user sees the new
  window rather than a refusal.
- **Minimise still leaks memory** when triggered by the compositor. The process
  is idle (no CPU, no frame callbacks while unmapped), but its RSS is retained
  until closed. Not fixable from the client on Wayland.
- **`setFlags()` after `show()`** can re-create the window on some platforms. It
  is called immediately after `show()` in the same callback; if a compositor
  mishandles it the window may flicker once at startup. Not measured.
- **The pkill pattern is anchored**, so a user-writable path containing the name
  is no longer matched. A process whose argv[0] is exactly `wattcurb-dashboard`
  from another directory would still match - accepted, it is the same program.

## 5. Verification & Oracle Gate Standards (REF-TEST-077)

Verified on the host with a KWin script that minimises/closes the window, because
window lifecycle needs a live compositor and cannot be exercised in the unit
suite:

1. close → process exits (verified: EXITED);
2. minimise → process stays (platform limitation, verified);
3. second GUI instance refused (verified: `[!] WattCurb GUI is already running`);
4. dashboard + report cannot coexist (verified: 1 process).

The unit suite covers the non-GUI parts: the tray's resolver and pattern logic
compile into the tray binary, and the existing tray/dashboard suites
(REF-TEST-018, REF-TEST-039, REF-TEST-053, REF-TEST-054) still pass.
