# REF-REQ-130: Safe Non-Destructive Memory Recovery & Runtime GC Notification Specification

## 1. Context & Motivation

Current Linux desktop and server distributions suffer from severe usability defects when memory approaches saturation:
- Traditional Out-Of-Memory (OOM) mechanisms rely on destructive termination (`SIGKILL`), terminating parent terminal shells, developer IDEs, or active web sessions.
- In modern computing environments, 40% to 65% of memory allocated by applications is **inactive or fragmented heap** (e.g. uncollected JavaScript/V8 objects in Electron/Chromium, idle background tabs, un-trimmed glibc memory pools).
- Terminative OOM responses violate WattCurb's core mission: **Absolute Zero-Kill & Non-Halting Invariant** ([`REF-REQ-044`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-044-absolute-zero-kill-and-non-halting-safety.md)) and **User-Application Exemption** ([`REF-REQ-117`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-117-user-app-exemption-from-mitigation.md)).

WattCurb must shift from reactive killing to **proactive, safe, non-destructive memory recovery** based on empirical research ([`REF-RES-033`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-033-safe-memory-reclamation-and-smart-gc.md)).

---

## 2. Functional Requirements

### 2.1 REF-REQ-130.1: Cooperative D-Bus LowMemoryMonitor Notification
- The daemon must implement or trigger the desktop standard `org.freedesktop.LowMemoryMonitor` D-Bus system bus interface:
  - **Path**: `/org/freedesktop/LowMemoryMonitor`
  - **Interface**: `org.freedesktop.LowMemoryMonitor`
  - **Signal**: `LowMemoryWarning(uint8 level)`
- **Threshold Escalation**:
  - **Level 100 (Moderate)**: Emitted when system Available Memory drops below 18% or PSI memory `some.avg10` > 8.0. Prompts compliant applications (Chromium, Firefox, GTK4, WebKit) to drop ephemeral image/font caches and run minor GC.
  - **Level 255 (Critical)**: Emitted when system Available Memory drops below 8% or PSI memory `full.avg10` > 15.0. Prompts compliant applications to perform major full GC, discard inactive browser tab renderers, and invoke `malloc_trim(0)`.
- If another daemon (e.g. Canonical's `low-memory-monitor`) already owns the D-Bus well-known name, WattCurb must detect this gracefully and avoid registration conflicts.

### 2.2 REF-REQ-130.2: Targeted `process_madvise(MADV_PAGEOUT)` for Minimized Windows
- When window state transitions to minimized or inactive ([`REF-REQ-128`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-128-window-minimized-progressive-cstate-governor.md)):
  - WattCurb opens a `pidfd` via `pidfd_open(pid, 0)`.
  - Parses the target's `/proc/<pid>/maps` to identify writable, anonymous VMA ranges (excluding executable code or locked pages).
  - Issues `process_madvise(pidfd, iov, vlen, MADV_PAGEOUT, 0)`.
- **Non-Destructive Invariant**: The process continues running seamlessly. Paged-out memory is compressed into `zram` and transparently paged back in upon user re-activation with sub-millisecond latency.

### 2.3 REF-REQ-130.3: Cgroup v2 `memory.reclaim` Interface Integration
- Under severe memory pressure (Available Memory < 10%), the daemon issues targeted write requests to `/sys/fs/cgroup/<slice>/memory.reclaim`:
  - Target: Background slices (`user.slice`, background services) up to 256 MiB per cycle.
  - Exemption: The currently focused/active window cgroup is strictly exempted from direct reclamation.

### 2.4 REF-REQ-130.4: High-Performance Compressed In-Memory Swap Prioritization
- The daemon inspects active swap devices (`/proc/swaps`):
  - Ensures `/dev/zram0` (compressed in-memory swap using `zstd`) has higher priority than disk-backed swapfiles (e.g. priority 100 vs -1).
  - Monitors zram saturation; when zram usage exceeds 85%, triggers secondary disk swap creation ([`REF-REQ-113`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-113-performance-mode-dynamic-swap-expansion.md)) without latency spikes.

---

## 3. Non-Functional & Safety Constraints

1. **Zero-Kill Guarantee**: No user process may be terminated (`SIGKILL`, `SIGTERM`) under any memory pressure scenario handled by WattCurb.
2. **Audio & Input Protection**: Processes with active PulseAudio/PipeWire audio streams or input focus are 100% exempt from `MADV_PAGEOUT` and cgroup reclamation.
3. **Bounded CPU Overhead**: Scanning `/proc/<pid>/maps` and calling `process_madvise` must operate within bounded chunks (maximum 16 VMAs per batch) to avoid CPU spikes.

---

## 4. Verification & Oracle Gate Standards (`REF-TEST-084`)

- **`test_low_memory_monitor_dbus_signaling`**: Verify D-Bus signal encoding, level escalation (100 and 255), and fallback behavior.
- **`test_process_madvise_pageout_safety`**: Verify `process_madvise` with `MADV_PAGEOUT` on child test process; verify child process survives and heap integrity is preserved.
- **`test_cgroup_memory_reclaim_actuation`**: Verify simulated `memory.reclaim` generation and focused PID exemption.
