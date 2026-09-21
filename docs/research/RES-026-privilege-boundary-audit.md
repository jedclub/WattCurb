# REF-RES-026: Root Daemon Privilege-Boundary Audit

**Date**: 2026-09-21 · **Scope**: every path by which a less-privileged actor can influence, or be influenced by, the root `wattcurb` daemon.

## 1. Method

| Technique | Coverage |
| :--- | :--- |
| `clang-tidy` (`bugprone-*`, `clang-analyzer-*`, `core.*`, `unix.Stream`) | 16 daemon translation units |
| Shell-execution enumeration | every `system` / `popen` / `exec*` call site, then backward taint of each argument |
| Trust-boundary review | `/dev/shm` POD, abstract AF_UNIX command socket, procfs/sysfs parsers |
| Empirical kernel-validation probe | `unshare -Urn` + `ip link add` to establish which characters `dev_valid_name()` accepts |
| `systemd-analyze security --offline` | service exposure scoring |

Static analysis produced **no correctness defects**: 12 `implicit-widening` hits were constants far inside `int` range, 6 `branch-clone` hits were intentional hysteresis, and all 4 descriptor-leak candidates were `if (fd < 0) return` false positives.

## 2. Findings

### S1 - Symlink following plus forced 0666 in the root daemon (HIGH)
`src/core/daemon_runner.cpp` (3 sites) opened a hardcoded
`/home/<dev>/.cache/power_profile_mode` with `O_CREAT | O_TRUNC`, **no `O_NOFOLLOW`**,
then called `fchmod(fd, 0666)`.

The daemon runs as root (`ps`: `root /usr/local/bin/wattcurb --daemon`), while the
path is inside a user-owned directory. Any code executing as that desktop user
could replace the file with a symlink to an arbitrary root-owned target; the
daemon would then truncate it, **chmod it world-writable** and write to it as
root. The `fchmod` is the sharper edge: aimed at `/etc/shadow` it makes the file
0666 regardless of content. CWE-59 + CWE-732, local user to root.

The matching read had no `O_NOFOLLOW` either, so a FIFO could block daemon startup.

### S2 - Command injection via interface name (MEDIUM)
`iw dev %s set txpower ...` was formatted into a `::system()` string with `%s`
taken from a `readdir()` of `/sys/class/net`. Kernel `dev_valid_name()` rejects
only whitespace and `/`, verified empirically in a private netns:

```
허용: [a;id]  [a$(id)]  [a`id`]  [a|id]  [a&b]  [a>b]
거부: [a b]   [a/b]
```

Assembled result for an interface named `wlan0;id>/tmp/pwn`:

```
iw dev wlan0;id>/tmp/pwn set txpower limit 1200 >/dev/null 2>&1 &
```

Naming an interface in the root netns needs CAP_NET_ADMIN, so this was
defence-in-depth rather than a live escalation.

### S3 - Developer paths outranking installed ones (MEDIUM)
- `src/ui/main_dashboard.cpp` preferred `/home/<dev>/Develop/WattCurb/src/ui/*.qml`
  over the compiled-in QRC. QML executes JavaScript, so whoever can write that
  directory supplies the dashboard's code.
- `src/tray/tray_client.cpp` tried `/home/<dev>/.local/bin/wattcurb-dashboard`
  before `/usr/local/bin`.

Present in all three shipped binaries (`strings | grep -c`: 1 each).

### S4 - No service hardening (LOW)
`scripts/wattcurb.service` carried zero hardening directives and no pinned
`PATH`, while the daemon resolves `bluetoothctl`, `kscreen-doctor`, `qdbus6`,
`balooctl6`, `setpriv` and `iw` through the inherited environment.
`systemd-analyze security`: **9.4 UNSAFE**.

### S5 - Hardcoded desktop identity (LOW)
`execute_user_desktop_cmd()` hardcoded `--reuid=1000 --regid=1000`,
`XDG_RUNTIME_DIR=/run/user/1000` and `WAYLAND_DISPLAY=wayland-0`. On a host whose
desktop user is not uid 1000, the root daemon drops into an unrelated account.
All seven callers pass string literals, so `sh -c '%s'` was not injectable in
practice, but nothing enforced that and truncation of the 512-byte buffer could
unbalance the quoting.

### S6 - World-writable IPC (LOW, found while fixing S1)
Both `/dev/shm` regions were created `0666` and `fchmod`ed to `0666`, yet every
consumer (tray, dashboard, CLI, history analyzer) opens `O_RDONLY` and maps
`PROT_READ`. The daemon is the sole writer, so the write bit only allowed a
local user to forge daemon telemetry.

## 3. Test-Harness Ambient Coupling

Engaging the actuation sandbox exposed that the Oracle Gate had been reading a
**live daemon** through four channels. Two are fixed, two remain:

| Channel | Status |
| :--- | :--- |
| `/dev/shm/wattcurb_state.shm` via `DashboardBackend::mapSharedMemory()` | fixed |
| Abstract AF_UNIX command socket via `queryDaemonTelemetry()` | fixed |
| `/dev/shm/wattcurb_history.shm` via `analyze_shm()` | **open** - `REF-TEST-043` asserts on live content |
| Live CPU topology, which the daemon itself mutates via SMT control | **open** - `REF-TEST-...` cluster assertions failed twice under build load |

See [`REF-REQ-093`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-093-daemon-privilege-boundary-hardening.md)
and [`REF-ARCH-070`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-070-privilege-boundary-and-actuation-isolation.md).

## 4. Not Covered

QML (2,755 lines), algorithmic correctness of the attribution and battery models,
and Seqlock memory-ordering under ThreadSanitizer.
