# REF-REQ-093: Root Daemon Privilege-Boundary Hardening Specification

## 1. Context
The audit in [`REF-RES-026`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-026-privilege-boundary-audit.md)
found a local privilege-escalation path in the root daemon, a latent command
injection, developer paths shipped in release binaries, an unhardened service
unit, and world-writable IPC. This specification states the invariants that must
hold so those classes cannot recur.

## 2. Functional Requirements

### REQ-093.1: The root daemon owns no state under `/home`
- The daemon must not read, write, create or `chmod` any path inside a user's
  home directory. Its own persistence lives in `/var/lib/wattcurb`, provisioned
  by `StateDirectory=wattcurb`, created `0644`.
- The live profile handoff from tray and dashboard travels over the existing
  command socket (`PROFILE <n>`); no file is shared across the privilege boundary.

### REQ-093.2: Privileged file operations refuse symlinks
- Every `open()` the daemon performs on a path it does not exclusively own must
  carry `O_NOFOLLOW`, for reads as well as writes.
- A privileged writer must never `fchmod()` a file to a mode wider than the
  narrowest one its consumers need.

### REQ-093.3: No shell command may embed data read from the system
- Interface names, process names, paths and any other value obtained from
  procfs, sysfs, netlink or a directory listing must never be formatted into a
  string passed to `system()`/`popen()`. Kernel `dev_valid_name()` permits
  `;`, `$()`, backticks, `|`, `&` and `>` in interface names.
- Wireless actuation uses nl80211 generic netlink, not `iw`.

### REQ-093.4: Release binaries contain no developer paths
- A filesystem path must never take precedence over a compiled-in resource. The
  QML development override is compiled out under `NDEBUG` and, in debug builds,
  requires an explicit `WATTCURB_QML_DEV_ROOT`.
- Per-user helper binaries are resolved from `$HOME`, never a literal home path.

### REQ-093.5: The privileged desktop bridge derives its target
- The uid, gid, `XDG_RUNTIME_DIR` and `WAYLAND_DISPLAY` used to drop privileges
  must be discovered from `/run/user/*` (preferring a runtime directory that
  carries a compositor socket), never hardcoded.
- The command body must be rejected if it contains a single quote, and a
  truncated command must never reach a shell.

### REQ-093.6: Service-level confinement
- The unit pins `PATH`, and sets `NoNewPrivileges`, `ProtectSystem=full`,
  `ProtectHome=read-only`, `PrivateTmp`, `RestrictSUIDSGID`,
  `RestrictNamespaces`, `RestrictRealtime`, `LockPersonality`,
  `MemoryDenyWriteExecute`, `SystemCallArchitectures=native` and
  `RestrictAddressFamilies=AF_UNIX AF_NETLINK`.
- Directives that would break core function are documented in the unit rather
  than silently omitted: `ProtectKernelTunables`, `ProtectControlGroups`,
  `PrivateDevices`, `ProtectKernelModules`.

### REQ-093.7: The Oracle Gate does not observe a live daemon
- No test may read a running daemon's shared memory or command socket. Ambient
  channels are gated by `WATTCURB_TEST_NO_DAEMON_SHM`.

## 3. Verification
- `systemd-analyze security --offline` exposure must stay at or below **7.0**
  (measured: 9.4 UNSAFE to 6.9 MEDIUM).
- No `/home/<literal-user>` string may appear in `src/` or in any shipped binary.
- No non-literal value may appear in a shell format string.
