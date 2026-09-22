# [REF-REQ-122] Graphical Session Environment for Tray-Launched Windows

**Status**: Implemented · **Date**: 2026-09-22
**Related**: [`REF-REQ-035`](REQ-035-native-kde-tray-indicator.md),
[`REF-REQ-120`](REQ-120-single-instance-and-gui-lifecycle.md),
[`REF-REQ-121`](REQ-121-dashboard-profile-change-authorization.md),
[`REF-TEST-077`](#5-verification--oracle-gate-standards-ref-test-077)

## 1. Why

"Open Dashboard" from the tray produced **no window at all** after a fresh
install. The tray forked `/usr/local/bin/wattcurb-dashboard` and the child died
immediately:

```
$ timeout 12 env -i HOME=/home/jedclub XDG_RUNTIME_DIR=/run/user/1000 \
      DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus \
      /usr/local/bin/wattcurb-dashboard
timeout: 감시 중인 명령에서 코어 덤프      # core dumped
```

`install.sh` starts the tray with a deliberately minimal environment:

```sh
sudo -u "${TARGET_USER}" \
    XDG_RUNTIME_DIR="${TARGET_RUNTIME}" \
    DBUS_SESSION_BUS_ADDRESS="unix:path=${TARGET_RUNTIME}/bus" \
    nohup /usr/local/bin/wattcurb-tray >/dev/null 2>&1 &
```

`WAYLAND_DISPLAY` and `DISPLAY` are absent. Qt **aborts** rather than degrades
when it cannot find a display: it calls `qFatal`, which is `abort()` - hence the
core dump, not a warning. Every window the tray launches inherited the broken
environment, because a forked child inherits the parent's environment.

Measured on this host: the tray's `/proc/<pid>/environ` contained `HOME` and
`XDG_RUNTIME_DIR` only; the live session had `WAYLAND_DISPLAY=wayland-0` and
`DISPLAY=:0` (from `plasmashell`).

## 2. Requirements

- **REQ-122.1 (Self-healing)** The tray shall reconstruct the display environment
  from the sockets that exist, so it works regardless of how it was started.
- **REQ-122.2 (No clobbering)** An already-set variable shall never be
  overwritten; the existing session value is authoritative.
- **REQ-122.3 (Native Wayland)** When the compositor socket exists,
  `QT_QPA_PLATFORM=wayland` shall be selected, so the dashboard gets a native
  Wayland surface rather than XWayland.
- **REQ-122.4 (Installer parity)** `install.sh` shall pass `WAYLAND_DISPLAY` and
  `DISPLAY` from the user's systemd manager when it launches the tray.

## 3. Mechanism

| Item | Location |
| :--- | :--- |
| `ensure_display_env()` | `src/tray/tray_client.cpp` |
| Called from | `TrayClient::initialize()` (before any fork/exec) |
| Installer session env | `install.sh` §5 (`systemctl --user show-environment`) |

Resolution order per variable:

| Variable | Source of truth |
| :--- | :--- |
| `XDG_RUNTIME_DIR` | existing value, else `/run/user/<uid>` |
| `WAYLAND_DISPLAY` | existing value, else `wayland-0` if `$XDG_RUNTIME_DIR/wayland-0` exists |
| `DISPLAY` | existing value, else `:0` if `/tmp/.X11-unix/X0` exists |
| `QT_QPA_PLATFORM` | existing value, else `wayland` when `WAYLAND_DISPLAY` is set |

## 4. Blast Radius & Failure Modes

- **The tray now sets four variables for itself and its children.** A user who
  intentionally points the tray at a specific display keeps their value
  (`setenv(..., 0)` never overwrites).
- **`QT_QPA_PLATFORM=wayland` is forced when the Wayland socket exists**, even on
  a session where the user normally runs XWayland applications. The dashboard is
  a native Qt Quick window; on an X11-only session the socket does not exist and
  nothing is forced. Not measured on an X11-only session.
- **The probe can pick a display the user is not looking at** if several
  compositors are running (e.g. a nested one that also owns `wayland-0`). The
  variable is only a fallback; the existing session value wins.
- **The installer's `systemctl --user show-environment`** is best-effort: it is
  wrapped so that a failure yields an empty string, and the tray's own probe then
  covers the case. The unquoted expansion of `SESSION_ENV` is deliberate - it
  becomes separate `VAR=value` words for `env`.

## 5. Verification & Oracle Gate Standards (REF-TEST-077)

Verified on the host:

1. `env -i` launch reproduced the core dump (the counterfactual);
2. after the fix, the tray's menu action was driven over D-Bus
   (`com.canonical.dbusmenu.Event` id 10) and the forked child reported
   `exe=/usr/local/bin/wattcurb-dashboard`;
3. the tray's environment now carries `WAYLAND_DISPLAY`, `DISPLAY` and
   `QT_QPA_PLATFORM`, and the launched window creates a Wayland surface.

The unit suite cannot cover this: it needs a live compositor and a real fork/exec
from a tray process started without a display environment.
