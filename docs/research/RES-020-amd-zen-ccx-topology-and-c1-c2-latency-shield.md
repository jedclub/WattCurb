# [REF-RES-020] Multi-Cluster (C1/C2) Microarchitecture Topology, Cross-CCX Contention & Terminal Latency Shielding

- **Ref-ID**: `REF-RES-020`
- **Related Requirements**: [`REF-REQ-054`](../requirements/REQ-054-cpu-headroom-and-greedy-process-capping.md), [`REF-REQ-057`](../requirements/REQ-057-multi-profile-anti-monopoly-and-headroom-protection.md), [`REF-REQ-084`](../requirements/REQ-084-adaptive-c1-c2-cluster-dispersion-and-terminal-shield.md)
- **Related Architecture**: [`REF-ARCH-061`](../architecture/ARCH-061-adaptive-c1-c2-cluster-dispersion-architecture.md)
- **Author**: Antigravity Autonomous Agent
- **Status**: Complete / Active Reference

---

## 1. Executive Summary

On modern multi-core x86-64 and ARM64 processors—particularly AMD Zen multi-CCX topologies (e.g. Zen 2 Renoir 8-core/16-thread dual-CCX) and Intel hybrid architectures (P-cores vs E-cores)—the CPU is partitioned into distinct hardware clusters:
- **Cluster 1 (C1 / CCX 0)**: First core complex with its own dedicated L3 Cache (e.g. CPUs 0..7).
- **Cluster 2 (C2 / CCX 1)**: Second core complex with its own dedicated L3 Cache (e.g. CPUs 8..15).

When the system operates in **High Performance Mode**, standard Linux CFS (Completely Fair Scheduler) and power managers remove CPU frequency constraints and allow unrestricted thread spawning. Consequently, compute-heavy multi-threaded process groups (such as compilers, build systems, language runtimes, browser worker pools, and AI inference engines) saturate all 16 hardware execution threads simultaneously across both C1 and C2.

Because interactive programs (terminal emulators, shells, GUI compositors, and text editors) default to standard CFS priority (`nice 0`), they are forced to compete on identical runqueues against dozens of active compiler threads. This results in severe scheduling latency ($> 80\text{ ms}$ queuing delay), input lag, dropped keystrokes, and UI sluggishness.

This research paper surveys the physical mechanisms of cross-cluster contention and formulates the **Adaptive C1/C2 Spatial Dispersion & Terminal Latency Shielding** architecture.

---

## 2. Microarchitecture Analysis: Why Terminals Starve in High Performance Mode

### 2.1 The Cross-CCX Interconnect & Cache Eviction Bottleneck
On AMD Zen multi-CCX architectures (such as the AMD Ryzen 7 PRO 4750U host):
1. **L3 Cache Isolation**:
   - CCX 0 (C1: CPUs 0..7) has a dedicated 4MB L3 cache slice.
   - CCX 1 (C2: CPUs 8..15) has an independent 4MB L3 cache slice.
   - Communication between C1 and C2 traverses the on-die Infinity Fabric (IF) bus, which incurs approximately $3\times$ higher latency ($\sim 35\text{ ns}$ vs $\sim 10\text{ ns}$ intra-CCX L3 access) and bus energy penalties.
2. **Cache Pollution from Unconstrained Heavy Process Groups**:
   - When a multi-threaded build (e.g., `make -j16`, `ninja`, `cargo`, `rustc`, `python3`) spans all 16 cores, compiler ASTs, symbols, and object buffers rapidly evict the terminal emulator’s rendering glyph cache and PTY buffer lines from L1D, L2, and L3 caches across **both** C1 and C2.
3. **CFS Runqueue Saturation**:
   - In Performance mode with 16 heavy threads running at `nice 0`, the Linux CFS runqueue length per core averages 2 to 4 runnable tasks.
   - When the user types a character in `konsole` or `alacritty`:
     - The X11/Wayland event is queued.
     - The terminal process wakes up.
     - Because the terminal and the compiler threads have identical `nice 0` priority, the scheduler will not immediately preempt the compiler task if its `sysctl_sched_min_granularity` timeslice is unexpired.
     - The user experiences visible typing stutter and lag (50ms ~ 200ms delay).

---

## 3. The Physical Solution: Dual-Cluster Spatial Dispersion

```
Physical Processor (16 Execution Threads)
┌─────────────────────────────────────────────────────────────────────────────┐
│ Cluster 1 (C1 / CCX 0: CPUs 0..7) - 4MB L3 Cache                            │
│ ┌───────────────────────────────────┐ ┌───────────────────────────────────┐ │
│ │ Cores 0..3: INTERACTIVE SHIELD    │ │ Cores 4..7: AUXILIARY POOL        │ │
│ │  - Terminals (konsole, kitty, etc)│ │  - Desktop apps, light workers    │ │
│ │  - Shells (bash, zsh, tmux)       │ │  - Balanced secondary compute     │ │
│ │  - Compositor (kwin_wayland -10)  │ │                                   │ │
│ │  - CFS nice -5 ~ -10 (Fast Thaw)  │ │                                   │ │
│ └───────────────────────────────────┘ └───────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────────┘
                                  Infinity Fabric
┌─────────────────────────────────────────────────────────────────────────────┐
│ Cluster 2 (C2 / CCX 1: CPUs 8..15) - 4MB L3 Cache                           │
│ ┌─────────────────────────────────────────────────────────────────────────┐ │
│ │ Cores 8..15: COMPUTE WORKLOAD ISOLATION ENCLAVE                         │ │
│ │  - Compilers (gcc, clang, rustc) & Build systems (ninja, make)          │ │
│ │  - Runtime interpreters (python3, node, java) & Heavy compute           │ │
│ │  - SCHED_BATCH / nice 5 (Full throughput, zero interactive preemption)  │ │
│ └─────────────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 3.1 Principle 1: Interactive Terminal & Shell Shielding
1. **Process Discovery**:
   - Terminal emulators: `konsole`, `alacritty`, `kitty`, `foot`, `wezterm`, `ptyxis`, `gnome-terminal`, `xterm`, `rxvt`.
   - Interactive shells: `bash`, `zsh`, `fish`, `tmux`, `screen`, `ssh`.
   - Compositors: `kwin_wayland`, `kwin_x11`, `mutter`, `sway`, `hyprland`, `Xorg`, `Xwayland`.
   - Text Editors / IDEs: `code`, `cursor`, `zed`, `nvim`, `vim`, `kate`, `kwrite`.
2. **Scheduling Priority Elevation**:
   - Promote to `nice -5` (and `nice -10` for compositor). Under Linux CFS, a negative nice value scales the process's dynamic weight by $1.25^{|nice|}$. At `nice -5`, the interactive terminal receives $\approx 3.05\times$ greater CPU scheduling weight, triggering immediate preemption of compute tasks upon keystroke/PTY wakeups.
3. **Core Affinity Pinning**:
   - Confine/bias interactive applications to **Cluster 1 (C1: Cores 0..7)**, with Cores 0..3 preserved as clean low-latency headroom.

### 3.2 Principle 2: Dynamic C2 Cluster Confinement for Heavy Compute Tasks
1. **Target Process Identification**:
   - Process safety tiers: `BackgroundWorker`, `RunawayCandidate`, or heavy `UserInteractive`.
   - Physical criteria: `cpu_watts > 0.8W`, `num_threads >= 4`, or `wdi_score > 4.0`.
   - Explicit binary heuristics: `gcc`, `g++`, `clang`, `rustc`, `cargo`, `ninja`, `make`, `python3`, `node`, `java`, `ffmpeg`, `Isolated Web Co`.
2. **Spatial Confinement**:
   - Bind heavy compute process groups to **Cluster 2 (C2: Cores 8..15)** via `sched_setaffinity`.
   - Modulate scheduling policy to `SCHED_BATCH` with `nice 5`. `SCHED_BATCH` informs the Linux scheduler that the task is throughput-oriented rather than latency-sensitive, eliminating wakeup latency boosts that would otherwise preempt desktop rendering.
3. **Cache Coherency Gains**:
   - By confining compiler and worker threads to C2, all worker threads share C2's 4MB L3 cache locally, eliminating cross-CCX cache invalidation traffic across the Infinity Fabric!

### 3.3 Principle 3: Adaptive Variable Application ("가변적 적용")
- **Hysteresis & Dynamic Scaling**:
  - When the system is idle or lightly loaded ($P_{\text{cpu}} < 0.5\text{W}$), heavy process restrictions are relaxed, allowing unconstrained burst across all 16 cores.
  - When heavy compute contention occurs ($P_{\text{cpu}} > 1.2\text{W}$ or process thread count $\ge 4$ under active terminal presence), the C1/C2 partition activates within the light monitoring cadence ($< 2\text{s}$).
  - When the workload terminates or subsides ($< 0.3\text{W}$ for 5 seconds), the daemon seamlessly rolls back affinity to all cores (`get_all_cores_cpuset()`) and restores baseline nice levels.
