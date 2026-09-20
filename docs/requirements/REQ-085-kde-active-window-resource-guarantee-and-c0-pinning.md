# REF-REQ-085: KDE Active Window Resource Guarantee & C0 Latency Pinning (PM QoS)

- **Status**: Approved
- **Ref ID**: `REF-REQ-085`
- **Related Requirements**: [`REF-REQ-084`](REQ-084-adaptive-c1-c2-cluster-dispersion-and-terminal-shield.md), [`REF-REQ-033`](REQ-033-window-aware-dynamic-suppression-ladder.md), [`REF-REQ-044`](REQ-044-absolute-zero-kill-and-non-halting-safety.md)
- **Related Research**: [`REF-RES-021`](../research/RES-021-kde-active-window-c0-qos-and-resource-guarantee.md)
- **Related Architecture**: [`REF-ARCH-062`](../architecture/ARCH-062-active-window-resource-guarantee-and-pm-qos.md)
- **Created**: 2026-09-21
- **Category**: Interactive Ergonomics, Kernel PM QoS, Active Window Priority, CPU Scheduling

---

## 1. Problem Statement

Even with background compute workloads dispersed to secondary cores (Cluster 2), the currently active and focused KDE application (e.g. terminal, text editor, code IDE, or browser) may experience typing latency spikes and micro-stutter due to two major factors:
1. **CPU Deep C-State Sleep**: During sub-millisecond pauses between user keystrokes or frame deadlines, CPU idle governors push cores into C2/C3/C6 states. Waking up requires $20\text{--}250\mu\text{s}$ (exit latency + voltage ramp-up).
2. **Resource Contention**: The active window competes on the CFS runqueue with other threads if not granted priority elevation and dedicated core affinity.

---

## 2. Functional Requirements

### 2.1 Hardware-Level C0 Latency Pinning via Linux PM QoS
1. The daemon MUST support kernel PM QoS actuation via `/dev/cpu_dma_latency`.
2. When an active KDE window is engaged:
   - Open `/dev/cpu_dma_latency` with `O_WRONLY | O_CLOEXEC`.
   - Write a 32-bit signed integer value of `0` (or $\le 1\mu\text{s}$) to request $0\mu\text{s}$ maximum exit latency.
   - This holds active CPU cores in **C0 (Active) / C1 (Halt, $< 2\mu\text{s}$)** state, completely bypassing high-latency C2/C3/C6 power gating during active user interaction.
3. The open file descriptor MUST be held open as long as the active window is engaged, and closed immediately upon focus loss or system idle to restore power-saving states.

### 2.2 Dedicated Resource Guarantee for Active Foreground Window
1. The active window's process (and its thread group) MUST receive guaranteed resources:
   - **Spatial Core Pinning**: Affine to **Cluster 1 (C1: Cores 0..7 or Headroom 0..3)**.
   - **CFS Priority Elevation**: Elevate `nice` to `-10` (or `-7`), ensuring instantaneous preemption over background threads.
   - **Timer Slack Tightening**: Set timer slack to $10\mu\text{s}$ (`PR_SET_TIMERSLACK`, `10000ULL`) for precision rendering and event dispatch.
   - **I/O Priority**: Set Block I/O priority to `IOPRIO_CLASS_BE` with level 0.

### 2.3 Focus Lifecycle & Zero-Leak Rollback
1. WattCurb MUST track the snapshot of the active process prior to modification (`original_nice`, `original_affinity`, `original_timerslack`).
2. When focus transitions away from the window or the window is closed:
   - Restore original `nice`, `affinity`, and `timerslack`.
   - Close `/dev/cpu_dma_latency` file descriptor, restoring normal kernel C-state governors.
   - Clean up snapshot state without leaving memory or resource leaks.

### 2.4 IPC Datagram Fast-Path
1. Support Unix domain socket IPC datagram commands in `wattcurb`:
   - `ACTIVE_WINDOW <pid> [comm]`
   - `WINDOW_STATE <pid> <minimized> <active> [is_audio]`
2. Actuation latency MUST be under $0.5\text{ms}$ upon message arrival.
