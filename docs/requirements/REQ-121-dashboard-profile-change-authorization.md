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

## 3. Mechanism

| Item | Location |
| :--- | :--- |
| Resolver | `resolve_dashboard_bin()` (`src/tray/tray_client.cpp`) |
| Call sites | dashboard action (id 10), report action (id 11) |
| Authorization | `peer_is_authorized_client()` (`src/core/daemon_runner.cpp`) |

## 4. Blast Radius & Failure Modes

- **Launch path changes for the GUI.** The tray now starts the system binary; the
  per-user copy is only a fallback. On a host with both installed, the dashboard
  now runs from `/usr/local/bin` - the same binary the installer stages, so
  behaviour is identical apart from the authorization outcome.
- **A host with only a `~/.local/bin` install** still cannot change profiles from
  the dashboard, by design. The tray menu remains the way to change profiles
  there.
- **Not measured.** The end-to-end click was not driven programmatically; the fix
  is verified by the resolved path being an allowlisted, root-owned binary, which
  is exactly the condition `peer_is_authorized_client()` tests.

## 5. Verification & Oracle Gate Standards (REF-TEST-077)

The denial was reproduced from the audit log before the fix. After the fix the
resolved launch path is asserted to be `/usr/local/bin/wattcurb-dashboard`, which
satisfies the daemon's `ALLOWED_CLIENTS` entry and root-ownership check. The
release acceptance step is a profile change from the dashboard with no new
`[ALERT:DENY]` line in `/var/log/wattcurb/audit.log`.
