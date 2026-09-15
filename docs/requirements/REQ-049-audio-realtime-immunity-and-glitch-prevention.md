# REQ-049: Audio Realtime Immunity & Glitch Prevention Invariant Specification

## 1. Overview & Problem Statement

### 1.1 The Issue
Under 100% all-core CPU workloads (such as multi-threaded compilation, 3D rendering, or computational benchmarks) in Performance or Balanced mode, audio playback (music, communication, media) would crackle, distort, and stutter heavily.

### 1.2 Root-Cause Forensic Analysis
Investigation into the running audio stack (`pipewire`, `pipewire-pulse`, `wireplumber`) revealed the exact culprit:
- The PulseAudio compatibility daemon for PipeWire runs with process comm name `pipewire-pulse`.
- In `src/policy/process_classifier.cpp`, the classification check used exact string matching:
  ```cpp
  comm == "pipewire" || comm == "wireplumber"
  ```
- Because `pipewire-pulse` did not strictly match `"pipewire"`, it was misclassified into `ProcessSafetyTier::RunawayCandidate` or `BackgroundWorker`.
- During previous battery conservation or runaway-mitigation cycles, WattCurb called `MitigationEngine::apply_sched_idle()` on `pipewire-pulse`.
- Under the Linux scheduler (CFS / EEVDF / BORE):
  - `SCHED_IDLE` has a static weight of 3 (compared to nice 0's weight of 1024, ~340x lower).
  - When all CPU cores hit 100% saturation, CFS starves `SCHED_IDLE` threads of CPU time slices.
  - The audio ringbuffer (typically 5ms~20ms DMA buffer) ran dry immediately, causing continuous ALSA buffer underruns (xruns) and audible glitching.
- Furthermore, because `pipewire-pulse` originally possessed an elevated nice level (`nice = -12`), subsequent unprivileged calls to `sched_setscheduler(pid, SCHED_OTHER, ...)` failed with `EPERM` (Linux security restriction prohibiting non-root processes from modifying negative nice threads back to normal without resetting priority first).

---

## 2. Functional Requirements (`REF-REQ-049`)

### 2.1 Absolute Audio Immunity (Tier 0 `CriticalImmune`)
1. All audio subsystem processes must be classified strictly as `ProcessSafetyTier::CriticalImmune`:
   - `comm.starts_with("pipewire")` (`pipewire`, `pipewire-pulse`, `pipewire-media-session`)
   - `comm.starts_with("wireplumber")`
   - `comm == "pulseaudio"`
   - `comm.starts_with("jackd")` and `comm == "jackdbus"`
   - `comm == "alsactl"`, `comm == "rtkit-daemon"`, `comm == "sndiod"`
2. For all audio stack processes:
   - `can_throttle_scheduler = false`
   - `can_reclaim_memory = false`
   - `can_freeze = false`
   - `default_action = MitigationAction::None`

### 2.2 Dual-Layer Actuation Defense in `MitigationEngine`
1. `MitigationEngine::apply_sched_idle(int32_t pid)` must perform an immutable second-layer invariant check:
   ```cpp
   if (pid <= 1 || is_immune_process(pid)) return false;
   ```
2. Any attempt by any policy layer to throttle an audio or core desktop compositor PID must be strictly rejected at the syscall barrier.

### 2.3 Sub-50us Self-Healing Audio Stack Audit
1. At daemon initialization (`MitigationEngine::MitigationEngine()`), on every evaluation cycle (`evaluate_and_actuate()`), and during all rollback sweeps (`rollback_all()`):
   - WattCurb conducts an ultra-fast direct scan of systemd audio user unit cgroups:
     `/sys/fs/cgroup/user.slice/user-%u.slice/user@%u.service/{session.slice,app.slice}/{pipewire,pipewire-pulse,wireplumber,pulseaudio}.service/cgroup.procs`
   - Reading `cgroup.procs` requires < 50us (compared to 10ms for full `/proc` scan).
   - If any audio daemon PID is detected in `SCHED_IDLE`, WattCurb immediately executes self-healing:
     1. Reset nice to 0 via `::setpriority(PRIO_PROCESS, pid, 0)` (bypassing unprivileged `EPERM`).
     2. Restore scheduler policy to CFS via `sched_setscheduler(pid, SCHED_OTHER, &sp)`.
     3. Restore block I/O priority via `syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, pid, prio_val)`.

---

## 3. Verification & Oracle Gate Standards

1. **Unit Test Verification (`tests/test_units.cpp`)**:
   - Verify `pipewire`, `pipewire-pulse`, `pipewire-media-session`, `wireplumber`, `pulseaudio`, `jackdbus`, `alsactl`, and `rtkit-daemon` all classify as `ProcessSafetyTier::CriticalImmune` with `can_throttle_scheduler == false`.
   - Verify `test_mitigation_engine` excludes mock `pipewire-pulse` from throttling even under critical battery / high WDI scores.
2. **Performance Budget**:
   - Fast cgroup lookup in `audit_and_heal_audio_stack()` must execute in < 100 microseconds.
   - Zero heap allocation in the audio check hot path.
3. **Live System Verification**:
   - `ps -eo pid,ni,cls,rtprio,comm | grep -E "pipewire|wireplumber|pulseaudio"` must report `TS` (SCHED_OTHER) and elevated priority (`nice <= 0`), never `IDL`.
