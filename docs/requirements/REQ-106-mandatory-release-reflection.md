# REF-REQ-106: Mandatory Release Reflection

## 1. Context & Observed Defect
The repository and the running system had silently diverged. At the time this
requirement was written:

| Artifact | State |
| :--- | :--- |
| `/usr/local/bin/wattcurb` | built 09-22 00:00 |
| `/usr/local/bin/wattcurb-tray` | built 09-20 18:44 - **two days stale** |
| `/usr/local/bin/wattcurb-dashboard` | built 09-20 18:44 - **two days stale** |
| `~/.local/bin/wattcurb-tray` | built 09-21 22:56 - a third, different build |
| `wattcurb.service` | `enabled` but **inactive**; the daemon was not running at all |
| Running tray process | `~/.local/bin/wattcurb-tray`, i.e. none of the above |

Every feature committed in that window existed only in git. Behaviour reported
as "implemented" was not behaviour the machine exhibited, and three different
builds of the same suite were installed side by side, each with its own idea of
the `/dev/shm` Seqlock POD layout.

## 2. Functional Requirements

### REQ-106.1: A commit that changes shipped code triggers a release
Any modification to `src/`, `CMakeLists.txt`, `scripts/wattcurb.service`,
`src/ui/qml/` or `install.sh` must be followed, in the same turn, by a full
release reflection. Documentation-only changes are exempt.

### REQ-106.2: Total shutdown precedes the rebuild
`wattcurb.service` is stopped and every `wattcurb-tray` / `wattcurb-dashboard`
process is terminated before the new release is built or installed. A surviving
daemon continues actuating hardware under the previous policy, and a surviving
client keeps the previous `ipc::TraySharedState` layout mapped - a POD change
then makes it decode the new daemon's bytes through the old struct.

### REQ-106.3: The release is the PGO artifact
Only `scripts/build_pgo.sh` may produce the installed binaries. Installing from
`build/` ships a non-PGO, unstripped binary and violates AGENTS.md Sec 8 and
Sec 11. The same run regenerates `docs/research/PGO_PMU_REPORT.md`, satisfying
the PMU audit mandate of AGENTS.md Sec 10 for the change being released.

### REQ-106.4: One source of truth for the systemd unit
`install.sh` installs `scripts/wattcurb.service` verbatim. It must not carry an
inline copy of the unit: the inline copy predated
[`REF-REQ-093`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-093-daemon-privilege-boundary-hardening.md)
and would have reverted the entire service hardening
(`NoNewPrivileges`, `ProtectSystem`, `ProtectHome`, `MemoryDenyWriteExecute`,
`RestrictAddressFamilies`, `StateDirectory`) on the next install, without any
error.

### REQ-106.5: Privileged steps use polkit, not sudo
The agent has no TTY, so `sudo` cannot read a password on this host. Root steps
are performed with `pkexec` against an absolute path.

### REQ-106.6: The reflection is verified, not assumed
After restart the following are checked and reported: `systemctl is-active`,
the installed binaries' `mtime` against the freshly staged `output/`, a live
`wattcurb --status` response, and the tray process running the new image.
Divergent mtimes between `/usr/local/bin` and `~/.local/bin`, or between the
three binaries, mean a partial rollout and are a defect.

### REQ-106.7: Skipped steps are stated
If Qt6 is absent, no polkit agent is running, or a stage fails, the report says
which step did not run. A partial rollout must never be reported as complete.

## 3. Verification
Verified by inspection at the end of each release turn (REQ-106.6), not by a
unit test: the property being checked is a property of the host, not of the
binary.
