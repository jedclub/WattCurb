# REF-ARCH-061: Adaptive C1/C2 Dual-Cluster Spatial Load Dispersion & Interactive Latency Shield Architecture

- **Status**: Approved
- **Ref ID**: `REF-ARCH-061`
- **Related Requirements**: [`REF-REQ-084`](../requirements/REQ-084-adaptive-c1-c2-cluster-dispersion-and-terminal-shield.md), [`REF-REQ-054`](../requirements/REQ-054-cpu-headroom-and-greedy-process-capping.md), [`REF-REQ-057`](../requirements/REQ-057-multi-profile-anti-monopoly-and-headroom-protection.md)
- **Related Research**: [`REF-RES-020`](../research/RES-020-amd-zen-ccx-topology-and-c1-c2-latency-shield.md)
- **Created**: 2026-09-20
- **Category**: CPU Scheduling Architecture, Cluster Spatial Partitioning, Terminal Latency

---

## 1. Architectural Overview

```
+-----------------------------------------------------------------------------------+
|               Hardware Topology Discovery (sysfs index3 shared_cpu_list)          |
+-----------------------------------------------------------------------------------+
             |                                                  |
             v                                                  v
+------------------------------------+          +------------------------------------+
|  Cluster 1 (C1 / CCX 0: Cores 0..7)|          |  Cluster 2 (C2 / CCX 1: Cores 8..15|
+------------------------------------+          +------------------------------------+
             |                                                  |
             v                                                  v
+------------------------------------+          +------------------------------------+
|      INTERACTIVE LATENCY SHIELD     |          |       COMPUTE DISPERSION ENCLAVE   |
|                                    |          |                                    |
| - Terminals (konsole, kitty, etc)  |          | - Build systems (ninja, make)      |
| - Shells (bash, zsh, fish, tmux)   |          | - Compilers (gcc, clang, rustc)    |
| - Compositor (kwin_wayland)        |          | - Heavy runtimes (python3, node)   |
| - NICE: -5 ~ -10 (Fast Preemption) |          | - NICE: +5, SCHED_BATCH            |
| - AFFINITY: C1 Cores (0..7)        |          | - AFFINITY: C2 Cores (8..15)       |
+------------------------------------+          +------------------------------------+
```

---

## 2. Dynamic Affinity Set Formulations

For an $N$-thread processor with dual-CCX topology:
- **C1 Affinity Mask**:
  $$\text{CPU\_SET}(0 \dots 7, \mathcal{M}_{\text{C1}})$$
- **C2 Affinity Mask**:
  $$\text{CPU\_SET}(8 \dots 15, \mathcal{M}_{\text{C2}})$$
- **Interactive Headroom Mask** (Preserved within C1):
  $$\text{CPU\_SET}(0 \dots 3, \mathcal{M}_{\text{shield}})$$

In `MitigationEngine`:
```cpp
struct CpuClusterTopology {
    int32_t total_cpus{16};
    int32_t cluster_count{2};
    cpu_set_t c1_cpuset{}; // Primary / Interactive Cluster (e.g. Cores 0..7)
    cpu_set_t c2_cpuset{}; // Secondary / Compute Cluster (e.g. Cores 8..15)
    cpu_set_t interactive_shield_cpuset{}; // Clean headroom within C1 (e.g. Cores 0..3)
    cpu_set_t all_cores_cpuset{};
};
```

---

## 3. Dynamic Hysteresis & Adaptive Execution Loop

During every monitoring tick (both light probe and deep sweep):
1. **Interactive Shield Sweep**:
   - Traverse active task comms. If matching `konsole`, `alacritty`, `kitty`, `bash`, `zsh`, `kwin_wayland`, elevate priority to `nice -5` ~ `nice -10` and restrict to $\mathcal{M}_{\text{C1}}$.
2. **Compute Dispersion Sweep**:
   - **Performance mode does NOT run this sweep (REF-REQ-104).** That profile holds nothing back, so heavy compute keeps all cores; its interactive latency comes from the elevation in step 1. The sweep runs in `Balanced`/`PowerSaver`/`UltraEndurance` under contention.
   - If a heavy process is detected ($P_{\text{proc}} > 0.8\text{W}$ or $\text{threads} \ge 4$), bind to $\mathcal{M}_{\text{C2}}$ with `SCHED_BATCH` and `nice 5`.
   - Log mitigation action: `EventLogger::log_mitigation(pid, comm, "C2_CLUSTER_DISPERSION", "Confined heavy compute to C2 (Cores 8..15) to shield terminal interactivity")`.
3. **De-Escalation Sweep**:
   - If a confined process drops below $0.3\text{W}$ for 5 seconds, restore original affinity $\mathcal{M}_{\text{all}}$ and baseline nice.
