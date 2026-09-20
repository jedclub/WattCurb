# REF-RES-021: Linux PM QoS C0 Latency Pinning & KDE Active Window Resource Guarantee

- **Status**: Approved
- **Ref ID**: `REF-RES-021`
- **Created**: 2026-09-21
- **Category**: Linux Kernel PM QoS, CPU C-States, KDE Plasma 6 Wayland, Interactive Ergonomics

---

## 1. Executive Summary

In desktop operating systems, input responsiveness is dominated not by peak clock frequencies, but by **tail latency in event dispatch**. When a user types in a terminal, edits code, or scrolls a web page, the active application often sleeps between keystrokes or frame deadlines (16.6ms for 60Hz, 8.3ms for 120Hz). During these brief sub-millisecond pauses, modern CPU power management aggressively places CPU cores into deep power-gated C-states (C2, C3, C6, C7).

Waking up from these deep C-states incurs significant latency:
- **C1 (Halt)**: $\sim 1\text{--}2\mu\text{s}$ exit latency.
- **C2 (Intermediate)**: $\sim 10\text{--}30\mu\text{s}$ exit latency.
- **C3 / C6 (Deep Package Sleep)**: $\sim 50\text{--}250\mu\text{s}$ exit latency, including PLL relock, voltage rail ramp-up, and L3 cache segment un-gating.

When combined with CFS runqueue scheduling delays under heavy background load, this introduces noticeable input jitter, micro-stutter, and dropped frames.

This research analyzes:
1. Linux PM QoS (`/dev/cpu_dma_latency`) for hardware-level active C0/C1 latency guarantees.
2. KDE Plasma 6 KWin active window lifecycle and PID resolution.
3. Dedicated spatial core isolation and CFS latency-nice prioritisation.

---

## 2. Linux PM QoS Architecture & `/dev/cpu_dma_latency`

### 2.1 The Kernel PM QoS Subsystem
The Linux kernel provides the Power Management Quality of Service (PM QoS) framework (`drivers/base/power/qos.c`, `kernel/power/qos.c`). It allows userspace applications and system daemons to specify maximum tolerable hardware latency.

The primary userspace interface is `/dev/cpu_dma_latency` (character device major 10, minor 259):
- Opening `/dev/cpu_dma_latency` and writing a 32-bit signed integer value $L$ (in microseconds) registers a latency constraint.
- Writing `0` (or $L \le 1$) requests a maximum exit latency of $0\mu\text{s}$.
- The kernel CPU idle governor (e.g. `menu` or `teo`) compares the target residency and exit latency of each C-state against all active PM QoS constraints:
  $$\text{ExitLatency}(\text{State}_k) \le L_{\text{qos}}$$
- When $L_{\text{qos}} = 0$, the governor completely skips C2, C3, and C6, restricting the processor strictly to **C0 (Active execution)** and **C1 (Instant halt, $< 2\mu\text{s}$)**.

### 2.2 Inode-Bound Automatic Cleanup
A crucial architectural advantage of `/dev/cpu_dma_latency` is its **file-descriptor binding**:
- The constraint remains active **only while the file descriptor remains open**.
- If the daemon crashes, restarts, or explicitly closes the FD, the kernel automatically removes the QoS request and restores normal deep C-states.
- This guarantees zero residual power drain even in anomalous failure modes.

---

## 3. KDE Plasma 6 KWin Active Window Lifecycle

In KDE Plasma 6 (Wayland & X11), KWin manages window focus through its internal `Workspace` model:
- `workspace.activeWindow`: Points to the currently focused window client.
- `workspace.windowActivated`: Signal emitted whenever window focus shifts.
- Properties exposed by KWin:
  - `client.pid`: The Linux Process ID (PID) owning the window.
  - `client.caption`: Human-readable window title.
  - `client.minimized`: Window minimization state.
  - `client.active`: True if focused.

### 3.1 IPC Fast-Path Integration
To maintain WattCurb's strict sub-millisecond actuation without requiring heavy D-Bus introspection on every tick:
1. `wattcurb-tray` (running natively inside the user graphical session) or KWin script sends a lightweight abstract Unix datagram:
   `ACTIVE_WINDOW <pid> [comm]`
2. `wattcurb` daemon receives this datagram via `/run/wattcurb.sock` and actuates the resource guarantee immediately.

---

## 4. Multi-Layer Active Window Resource Guarantee

When a window gains user focus, WattCurb applies four complementary hardware and kernel levers:

| Layer | Kernel Actuation | Effect on Active Window | Effect on Background |
|---|---|---|---|
| **C-State PM QoS** | Write `0` to `/dev/cpu_dma_latency` | Locks core in C0/C1 readiness; zero wakeup stutter | Deep C-states temporarily suspended during interaction |
| **Spatial Core Pinning** | `sched_setaffinity` to Cluster 1 (`0..7` or Headroom `0..3`) | Guaranteed L3 cache lines, zero cross-CCX interconnect penalty | Background compute stays confined to Cluster 2 (`8..15`) |
| **CFS Priority** | `setpriority(PRIO_PROCESS, pid, -10)` | Instant preemption over any normal/batch background task | Background tasks yield runqueue without starving |
| **Wakeup Timer Slack** | `prctl(PR_SET_TIMERSLACK, 10'000ULL)` | Timers accurate to $10\mu\text{s}$, perfect frame delivery | Minimized windows retain $50\text{ms}$ coalescing |
| **Block I/O Priority** | `SYS_ioprio_set(IOPRIO_CLASS_BE, 0)` | Highest best-effort disk/storage priority | Minimized background tasks remain `IOPRIO_CLASS_IDLE` |

---

## 5. De-Escalation & Power Preservation

Resource guarantees must be **transient and responsive**, never rigid:
1. **Focus Shift**: When the user switches windows, the previous window's custom priority and timerslack are restored.
2. **Idle Window**: If the active window receives zero user input or draws $< 0.1\text{W}$ for $\ge 10$ seconds, PM QoS is conditionally released, allowing the system to sleep deeply while the user is away.
