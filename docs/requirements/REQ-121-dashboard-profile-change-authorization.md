# [REF-REQ-121] Dashboard Profile Change Rejected by the Daemon

**Status**: Implemented · **Date**: 2026-09-22
**Related**: [`REF-REQ-111`](REQ-111-audit-defect-remediation.md),
[`REF-REQ-036`](REQ-036-native-kde-dashboard-matrix.md),
[`REF-REQ-120`](REQ-120-single-instance-and-gui-lifecycle.md),
[`REF-TEST-077`](#5-verification--oracle-gate-standards-ref-test-077)

## 1. Why

Changing the power profile from inside the **Precision Analysis Matrix** dashboard
did nothing. The buttons called `DashboardBackend::setProfile()`, which sends
`PROFILE <n>` to the daemon over the abstract socket, and the daemon answered with
a denial that was only visible in the audit log:

```
[ALERT:DENY] Rejected PROFILE command from wattcurb-dashbo[3445221] uid=1000
             - not an installed WattCurb client binary
```

`REF-REQ-111` hardened the command socket so that a `PROFILE` datagram is
accepted only from an **installed, root-owned client binary**. The allowlist is:

```
/usr/local/bin/wattcurb{,-tray,-dashboard}
/usr/bin/wattcurb{,-tray,-dashboard}
```

The tray, however, launched the dashboard from `$HOME/.local/bin` **first**:

```cpp
if (user_dashboard_path(dash_bin, ...) && ::access(dash_bin, X_OK) == 0) {
    ::execl(dash_bin, "wattcurb-dashboard", nullptr);   // ~/.local/bin
} else {
    // /usr/local/bin only as a fallback
}
```

`~/.local/bin/wattcurb-dashboard` is not on the allowlist, so `/proc/<pid>/exe`
did not match and every profile change was refused. The buttons looked dead while
the tray's own profile menu (tray is an allowed client) worked - which is why the
symptom was specific to the dashboard.

Note the comment above `user_dashboard_path()` already argued that the per-user
path must not outrank the installed binary; the code did the opposite.

A second entry point had the same defect and was found while verifying the first.
`install.sh` writes the application-menu entry to
`/usr/share/applications/wattcurb-dashboard.desktop` with the correct
`Exec=/usr/local/bin/wattcurb-dashboard`, but an **older** installer had also left
`~/.local/share/applications/wattcurb-dashboard.desktop` with
`Exec=${HOME}/.local/bin/wattcurb-dashboard`. A user-level entry **shadows** the
system one, so a menu launch took the unauthorized path and the profile buttons
were dead there too.

The refusal was also **silent**: `setProfile()` latched `local_override_mode_`
before sending, so the dashboard highlighted the requested profile while the
hardware never left the old one. The daemon does answer refusals
(`ERROR: not an authorized WattCurb client (REF-REQ-111)`); the dashboard was
discarding the reply.

## 2. Requirements

- **REQ-121.1 (Authority order)** The tray shall resolve the dashboard binary as
  `/usr/local/bin/wattcurb-dashboard`, then `/usr/bin/wattcurb-dashboard`, and
  only then `$HOME/.local/bin/wattcurb-dashboard`.
- **REQ-121.2 (No silent dead buttons)** When only a per-user copy exists, the
  daemon still refuses by design (`REF-REQ-111`: a user-writable directory is not
  trustworthy for commanding a root daemon). That is a documented limitation, not
  a bug to be worked around by loosening the check.
- **REQ-121.3 (Authorization unchanged)** The daemon's allowlist and the
  root-ownership requirement shall not be weakened.
- **REQ-121.4 (Refusal is visible)** The dashboard shall read the daemon's reply.
  If it begins with `ERROR`, the optimistic local override shall not be latched
  and the UI shall fall back to the daemon's actual mode, so a refused change
  shows as refused instead of as applied.
- **REQ-121.5 (One launch path)** The installer shall remove the legacy per-user
  desktop entry that points at `~/.local/bin`, leaving the system entry as the
  single source of truth for the menu launch path.

## 3. Mechanism

| Item | Location |
| :--- | :--- |
| Resolver | `resolve_dashboard_bin()` (`src/tray/tray_client.cpp`) |
| Call sites | dashboard action (id 10), report action (id 11) |
| Authorization | `peer_is_authorized_client()` (`src/core/daemon_runner.cpp`) |
| Reply handling | `sendDaemonCommandQuery()` + `setProfile()` (`src/ui/dashboard_backend.cpp`) |
| Legacy entry cleanup | `install.sh` §4.4 |

## 4. Blast Radius & Failure Modes

- **Launch path changes for the GUI.** The tray now starts the system binary; the
  per-user copy is only a fallback. On a host with both installed, the dashboard
  now runs from `/usr/local/bin` - the same binary the installer stages, so
  behaviour is identical apart from the authorization outcome.
- **A host with only a `~/.local/bin` install** still cannot change profiles from
  the dashboard, by design. The tray menu remains the way to change profiles
  there.
- **The legacy entry is deleted, not rewritten.** A user who deliberately created
  their own `~/.local/share/applications/wattcurb-dashboard.desktop` loses it on
  the next install. The name is WattCurb's own, and keeping it was the defect.
- **`setProfile()` now blocks up to 50 ms** waiting for the daemon's answer
  (the existing `query_daemon` timeout) where it previously fired and returned.
  The call runs on the GUI thread; 50 ms is the worst case when the daemon is
  wedged, and it is the same budget the read path already uses.
- **Not measured.** The end-to-end click was not driven programmatically; the fix
  is verified by the resolved path being an allowlisted, root-owned binary, which
  is exactly the condition `peer_is_authorized_client()` tests.

## 5. Verification & Oracle Gate Standards (REF-TEST-077)

The denial was reproduced from the audit log before the fix:

```
[ALERT:DENY] Rejected PROFILE command from wattcurb-dashbo[3445221] uid=1000
             - not an installed WattCurb client binary
```

After the fix, the tray's menu action was driven over D-Bus
(`com.canonical.dbusmenu.Event` id 10, the "Open Dashboard" item) and the process
it forked reported `exe=/usr/local/bin/wattcurb-dashboard` - the allowlisted,
root-owned path. The release acceptance step is a profile change from the
dashboard with no new `[ALERT:DENY]` line in `/var/log/wattcurb/audit.log`.
