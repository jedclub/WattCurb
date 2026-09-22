# REF-REQ-084: Adaptive C1/C2 Dual-Cluster Spatial Load Dispersion & Interactive Terminal Latency Shield

- **Status**: Approved
- **Ref ID**: `REF-REQ-084`
- **Related Requirements**: [`REF-REQ-054`](REQ-054-cpu-headroom-and-greedy-process-capping.md), [`REF-REQ-057`](REQ-057-multi-profile-anti-monopoly-and-headroom-protection.md), [`REF-REQ-049`](REQ-049-audio-realtime-immunity-and-glitch-prevention.md)
- **Related Research**: [`REF-RES-020`](../research/RES-020-amd-zen-ccx-topology-and-c1-c2-latency-shield.md)
- **Related Architecture**: [`REF-ARCH-061`](../architecture/ARCH-061-adaptive-c1-c2-cluster-dispersion-architecture.md)
- **Created**: 2026-09-20
- **Category**: CPU Scheduling, Multi-Core Spatial Partitioning, Terminal Latency, Ergonomics

---

## 1. Problem Statement

In High Performance Mode, multi-threaded compute process groups (compilers, build systems, parallel language runtimes, browser worker pools) saturate all logical CPU threads indiscriminately across all physical CCX clusters. On multi-CCX CPUs (such as the AMD Ryzen 7 PRO 4750U with CCX 0 Cores 0..7 and CCX 1 Cores 8..15), this causes severe cross-CCX cache thrashing and CFS runqueue starvation.

Interactive programs—specifically terminal emulators (`konsole`, `alacritty`, `kitty`, `ptyxis`), interactive shells (`bash`, `zsh`, `fish`, `tmux`), text editors, and the desktop compositor—suffer noticeable input latency ($> 50\text{ms}$ typing delay) and sluggish responsiveness.

---

## 2. Functional Requirements

### 2.1 Hardware Topology Auto-Detection (C1 vs C2)
The daemon MUST detect the physical L3 cache / CCX cluster topology at initialization:
1. Parse `/sys/devices/system/cpu/cpu*/cache/index3/shared_cpu_list`.
2. Map online logical threads into:
   - **Cluster 1 (C1)**: Primary core complex (e.g. Cores 0..7 on 16-thread Renoir).
   - **Cluster 2 (C2)**: Secondary core complex (e.g. Cores 8..15 on 16-thread Renoir).
3. If the host CPU has a single uniform CCX (e.g. Cezanne/Barcelo), partition logically:
   - C1 = Cores $0 \dots \frac{N}{2}-1$
   - C2 = Cores $\frac{N}{2} \dots N-1$

### 2.2 Proactive Interactive Terminal & Shell Latency Shield
1. Automatically classify interactive terminal emulators and shells into the `DesktopCore` tier:
   - Terminals: `konsole`, `alacritty`, `kitty`, `foot`, `wezterm`, `ptyxis`, `gnome-terminal`, `xterm`, `rxvt`
   - Shells: `bash`, `zsh`, `fish`, `tmux`, `screen`, `ssh`
2. Apply latency shielding in High Performance mode (and across all active profiles):
   - Priority elevation: Set CFS priority to `nice -5` for terminals/shells and `nice -10` for compositors (`kwin_wayland`, `mutter`).
   - Affinity pinning: Pin interactive applications to **Cluster 1 (C1)**, ensuring immediate wakeup without cross-CCX interconnect penalty.

### 2.3 Adaptive C1 / C2 Spatial Load Dispersion for Heavy Compute Workloads
1. Automatically detect heavy compute process groups:
   - Criteria: `proc.cpu_watts > 0.8W`, `proc.num_threads >= 4`, or `proc.wdi_score > 4.0` in Performance mode.
   - Identified signatures: `gcc`, `g++`, `clang`, `rustc`, `cargo`, `ninja`, `make`, `python3`, `node`, `java`, `ffmpeg`, `Isolated Web Co`.
2. Confinement Policy:
   - Confine heavy compute workloads to **Cluster 2 (C2: Cores 8..15)** via `sched_setaffinity`.
   - Modulate task scheduler policy to `SCHED_BATCH` with `nice 5`.
   - Preserve clean headroom on C1 (Cores 0..3) strictly for interactive terminal input and UI rendering.
3. **Profile exception (REF-REQ-104)**: this confinement is **NOT applied in Performance mode**. That profile's contract is that no workload is held back, so a compile or render keeps every core; interactive latency there is protected by priority elevation (terminal/compositor shield and the active-window guarantee), not by capping the heavy work. The dispersion remains in force for Balanced, PowerSaver and UltraEndurance.

### 2.4 Dynamic & Variable Hysteresis Loop ("가변적 적용")
1. Confinement MUST NOT be permanent or rigid.
2. If total CPU load drops below $0.4\text{W}$ or the heavy process ceases active computation for 5 consecutive seconds:
   - Seamlessly de-escalate and restore affinity to all cores (`sched_setaffinity(pid, get_all_cores_cpuset())`).
   - Restore original `nice` and `sched_policy`.
3. Actuation latency MUST be sub-millisecond, executing during light probe and deep sweep intervals without interrupting running workflows.
