# [REF-REQ-117] User-Application Exemption from Mitigation Ladder

**Status**: Implemented · **Date**: 2026-09-22
**Related**: [`REF-REQ-031`](REQ-028-adaptive-power-state-machine-and-actuation.md),
[`REF-REQ-044`](REQ-044-absolute-zero-kill-and-non-halting-safety.md),
[`REF-REQ-085`](REQ-085-kde-active-window-resource-guarantee-and-c0-pinning.md),
[`REF-REQ-104`](REQ-104-ultimate-performance-unleash-actuation.md),
[`REF-REQ-116`](REQ-116-effective-total-power-fallback.md),
[`REF-TEST-075`](#5-verification--oracle-gate-standards-ref-test-075)

## 1. Why

On the reference host (Balanced profile, on battery) typing and scrolling into the
ChatGPT/Codex desktop application was visibly delayed while the machine was
otherwise loaded. The cause was not CPU frequency, memory or the compositor -
all were healthy. It was **this daemon's own mitigation ladder**:

`ProcessClassifierDB::classify()` matches `/proc/<pid>/comm`, which the kernel
truncates to 15 bytes. The ChatGPT/Codex Electron application was **not on the
UserInteractive allowlist**, so every one of its processes fell through to the
default **Tier 5 `RunawayCandidate`** (`default_action = SchedIdle`). Observed:

- `ChatGPT --type=renderer`, `--type=gpu-process` and `MainThread` at
  `SCHED_IDLE` (`policy=5`), while sibling renderer processes of the same app
  stayed `SCHED_OTHER` - i.e. per-PID selection, not an app-level setting.
- `kmscon`, the kernel-console terminal, also at `SCHED_IDLE`.
- Journal entries applying/rolling back `AntiStarvationCap` (nice=10 + affinity
  masked to 7 of 8 logical CPUs) to ChatGPT/Codex/chrome PIDs every 20-40 s.

A process on `SCHED_IDLE` gets the CPU only when nothing else runnable wants it,
so its input and paint path waits behind the machine's other work.

The intended safety net for the focused window does not work in production: the
`ACTIVE_WINDOW <pid>` / `WINDOW_STATE` IPC handlers exist in `DaemonRunner`, but
**no production client sends them**, so `WindowAwareGovernor::active_window_pid()`
is `0` in the daemon. KWin's window-query D-Bus API (`queryWindowInfo`) is an
interactive, user-click API (`org.kde.KWin.Error.UserCancel`), and Wayland
exposes no focus-PID protocol to unprivileged clients by design - so the daemon
cannot learn focus that way either.

## 2. Requirements

- **REQ-117.1 (Common interactive launchers are classified)** The classifier
  shall recognise the common Electron/Chromium-family desktop launchers by name,
  including `ChatGPT`, `codex`, `code`, `electron`, `vscode`, `code-insiders`,
  `signal-desktop`, `element`, `mattermost`, `notion`, `obsidian`, and the
  Electron renderer thread name `MainThread`, as `UserInteractive`.
- **REQ-117.2 (`kmscon` is DesktopCore)** `kmscon` is a terminal emulator and an
  input sink; it shall be classified `DesktopCore` (never throttled), beside
  `konsole`, `kitty`, `foot`, `alacritty`, `wezterm`.
- **REQ-117.3 (Ancestry exemption)** A process whose ancestry reaches a
  `UserInteractive` ancestor before `init` shall be exempt from mitigation, so an
  Electron app's subprocesses are protected even when their thread names are not
  allowlisted. The walk shall be bounded (max depth 8), read only `/proc`, and
  **terminate negatively** at any `BackgroundWorker` or `RunawayCandidate`
  ancestor, so an indexer's children are not protected by having been launched
  from a shell.
- **REQ-117.4 (Exempt from every process mitigation)** A Tier 3
  (`UserInteractive`) process, or one covered by REQ-117.3, shall be excluded
  from `SchedIdleThrottle`, `TimerSlackCoalescing`, `AntiStarvationHeadroom` and
  `ZenCcxAffinityPinning`, **in every non-Performance profile** (Balanced,
  PowerSaver, UltraEndurance). Performance already imposes nothing
  ([`REF-REQ-104`](REQ-104-ultimate-performance-unleash-actuation.md)).
- **REQ-117.5 (Both ladders agree)** The exemption shall hold in
  `FeatureManager::evaluate_and_actuate()` (the production path) **and**
  `MitigationEngine::evaluate_and_actuate()`, so the two policy ladders cannot
  disagree about who may be throttled.
- **REQ-117.6 (Background work stays throttleable)** The exemption shall not
  extend to `BackgroundWorker` or `RunawayCandidate` processes; they remain
  throttled exactly as before.

## 3. Mechanism

| Item | Location |
| :--- | :--- |
| Allowlist additions | `ProcessClassifierDB::classify()` (`src/policy/process_classifier.cpp`) |
| Ancestry walk | `FeatureManager::is_user_app_tree()` (`src/policy/battery_feature.cpp`) |
| ppid parse | `read_proc_ppid()` (field 4 of `/proc/<pid>/stat`) |
| Exemption (production) | `FeatureManager::evaluate_and_actuate()` |
| Exemption (engine) | `MitigationEngine::evaluate_and_actuate()` |

The ancestry signal replaces the non-functional focus-IPC signal. It protects the
whole application tree, which is what an input path actually spans (launcher,
renderer, GPU process), without needing to know which window is focused.

## 4. Blast Radius & Failure Modes

- **A greedy user application is now never throttled.** A browser or Electron app
  that genuinely drains the battery is no longer capped in any profile; the only
  remedy is the user selecting a saving profile's hardware caps, which are not
  per-process. This is the intended trade: an input consumer must not be put on
  `SCHED_IDLE`.
- **Ancestry walk cost.** Four to eight `read_small_file()` calls per candidate
  process per cycle in the worst case. It only runs for Tier 3-or-unknown
  candidates, and only for processes that survived the earlier tier checks.
- **Over-broad exemption risk.** Mitigated by the negative termination at
  background-worker ancestors and by the controlled negative in REF-TEST-075.
- **Not measured.** The typing/scroll latency improvement on the reference host
  was not instrumented before/after; the change removes the observed `SCHED_IDLE`
  and cap actuations on the app's PIDs, which is the mechanism, but the latency
  delta itself is not a measured number.

## 5. Verification & Oracle Gate Standards (REF-TEST-075)

`tests/test_units.cpp::test_user_app_tree_exemption()` proves, without touching
the scheduler:

1. `ChatGPT`, `codex`, `MainThread` classify as `UserInteractive`, `kmscon` as
   `DesktopCore`, and `baloo_file`/`updatedb` stay `BackgroundWorker` (controlled
   negative);
2. `is_user_app_tree()` is safely false for pid `1`, `0`, and `-1`;
3. from the production path (`FeatureManager::evaluate_and_actuate`, Balanced,
   sandboxed) two synthetic Tier 3 processes at 9 W and `wdi 40` produce
   `throttled_count == 0`.

The test asserts the classification and the absence of actuation. It does not
measure input latency, and it does not prove the ancestry walk on a live Electron
tree - that was observed on the host, not reproduced in the suite.
