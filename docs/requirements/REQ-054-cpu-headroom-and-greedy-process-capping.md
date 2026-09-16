# [REF-REQ-054] CPU Headroom Guarantee & Greedy Process Anti-Starvation Specification

## 1. Context & Motivation

On multi-core, simultaneous multi-threading (SMT) systems (such as the AMD Ryzen 7 PRO 4750U with 8 physical cores and 16 logical threads), compute-heavy multithreaded workloads (e.g. compilation with `ninja -j16` or `cargo build`, video encoding, runaway scientific scripts, or machine learning workloads) frequently spawn 16 or more active worker threads, saturating 100% of all logical CPU cores.

Under Linux Completely Fair Scheduler (CFS), this full-capacity saturation introduces severe desktop degradations:
1. **Audio Dropouts & XRUNs**: PipeWire and WirePlumber operate on strict real-time buffers (e.g., 256 frames at 48kHz = 5.3ms deadline). When all 16 cores are running compute-intensive threads, scheduler runqueue latency and SMT execution port contention prevent audio callback threads from executing within their deadline, causing audible clicks, pops, or complete sound drops.
2. **Desktop & Video Stuttering**: The Wayland display compositor (`kwin_wayland`) and video presentation threads (mpv, browser video decoders) require consistent 16.6ms (60Hz) or 6.9ms (144Hz) frame presentation times. When CFS queues are saturated, compositor frame rendering is delayed, causing severe frame drops, UI lag, and mouse freezing.
3. **Usability Violation**: Permitting non-interactive batch compute jobs to consume 100% of all logical cores destroys interactive usability while only yielding marginal throughput gains (< 5-10%).

To solve this, **WattCurb** mandates a proactive **CPU Headroom Guarantee & Anti-Starvation Subsystem** (`REF-REQ-054`).

---

## 2. Functional Requirements

### 2.1 Clean Headroom Core Reservation (`REF-REQ-054-01`)
1. The daemon must dynamically determine the host logical CPU topology:
   - Total online cores $N = \text{sysconf}(\_SC\_NPROCESSORS\_ONLN)$.
   - On systems with $N \ge 8$, reserve at least $R = 2$ logical CPUs (one complete physical SMT core pair, specifically CPUs $N-2$ and $N-1$, e.g. Cores 14 and 15 on a 16-core system).
   - On systems with $4 \le N < 8$, reserve $R = 1$ logical CPU (CPU $N-1$).
2. The reserved cores ($N-R \dots N-1$) form the **Clean Interactive / Audio Headroom Domain**.
3. Real-time interactive applications (KWin Wayland compositor, Xwayland, video renderers), audio daemons (PipeWire, WirePlumber, PulseAudio), and WattCurb itself have unrestricted access to ALL $N$ cores (including the clean headroom cores).

### 2.2 Dynamic Greedy Process Detection & Affinity Capping (`REF-REQ-054-02`)
1. During each observation cycle, WattCurb analyzes all active processes in `AnalysisReportData`.
2. A process is flagged as a **Greedy Starvation Candidate** if:
   - It is NOT immune (`safety_tier != CriticalImmune` and `safety_tier != DesktopCore` and not an immune audio/compositor process).
   - Its power/compute consumption exceeds the greedy threshold (`cpu_watts > 1.5W`, `wdi_score > 8.0`, or `is_runaway_candidate == true`).
   - Or its safety tier is `BackgroundWorker` (Tier 4) or `RunawayCandidate` (Tier 5) with elevated CPU activity (`cpu_watts > 1.0W`).
3. Actuation on Greedy Candidates:
   - **Thread-Wide Core Affinity Capping**: Set CPU affinity to the allowed mask ($0 \dots N-R-1$, e.g. CPUs 0..13) across the parent process **and all child worker threads** in `/proc/<pid>/task/` using `sched_setaffinity`.
   - **CFS Batch Degradation**: Apply `SCHED_BATCH` and `nice +10` across all threads in `/proc/<pid>/task/`. This signals CFS that the threads are non-interactive batch workloads, ensuring instant preemption whenever audio or compositor threads become runnable.
4. Throughput Guarantee:
   - On a 16-thread system, capping greedy batch jobs to 14 cores preserves 87.5% of full compute throughput (e.g. `ninja` compiles at ~90% speed), while guaranteeing 100% glitch-free audio and fluid desktop responsiveness.

### 2.3 Audio Stack Self-Healing & Priority Enforcement (`REF-REQ-054-03`)
1. The daemon must audit the audio stack (`pipewire`, `pipewire-pulse`, `wireplumber`, `pulseaudio`) at every observation cycle.
2. In multi-user and root daemon environments, the audit must dynamically discover user cgroup slices across all active user sessions (`/sys/fs/cgroup/user.slice/user-*.slice/user@*.service/session.slice/` and `app.slice/`).
3. For all identified audio processes:
   - Proactively elevate priority to `nice -19` (or `SCHED_RR`).
   - Ensure the scheduler is NOT set to `SCHED_IDLE`; restore to `SCHED_OTHER` immediately if compromised.
   - Ensure CPU affinity permits execution across all $N$ cores (unconstrained).

### 2.4 Graceful Hysteresis & Dynamic Restoration (`REF-REQ-054-04`)
1. When a capped process's CPU consumption drops below the de-escalation threshold (`cpu_watts < 0.3W` and `wdi_score < 3.0`) for 2 consecutive cycles, or upon process termination, its CPU affinity must be restored to all online cores ($0 \dots N-1$) and its scheduling restored to normal CFS (`SCHED_OTHER`, `nice 0`).
2. When the user explicitly selects **Performance Mode** (`PowerProfileMode::Performance`) or upon daemon shutdown/rollback:
   - All core affinity caps are immediately removed (`restore_all_affinity`).
   - All batch priorities are restored.

---

## 3. Non-Functional & Safety Constraints

1. **Absolute Zero-Kill Invariant (`REF-REQ-044`)**: Under NO circumstance may any greedy or runaway process be terminated (`SIGKILL`/`SIGTERM`) or frozen (`cgroup.freeze`). Throttling is strictly limited to affinity partitioning and CFS batch prioritization.
2. **Zero Dynamic Allocation in Hot Path**: Core affinity bitmasks and thread iteration buffers must use stack allocation (`cpu_set_t`, fixed buffers).
3. **Execution Latency**: Thread affinity traversal for a greedy process tree must complete within < 200 microseconds.
4. **Compositor & Audio Immunity**: KWin and PipeWire must never have their affinity masked or priority degraded.

---

## 4. Verification & Oracle Gate Standards (`REF-TEST-019`)

1. **Topological Math**: Verify that $N=16$ yields 2 reserved cores and an allowed mask of 14 cores (bits 0..13).
2. **Thread Affinity Traversal**: Verify that `apply_core_affinity_cap` and `restore_core_affinity` successfully iterate `/proc/<pid>/task/` and alter thread affinity.
3. **Audio Self-Healing**: Verify that audio stack detection accurately scans `user-*.slice` without relying on `getuid() == 0`.
4. **Oracle Gate Benchmark**: Core affinity mask computation and thread clamping must complete in < 50us per process.
