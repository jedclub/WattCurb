# [REF-ARCH-033] Multi-Profile Affinity Partitioning, Compositor Shielding & CFS Preemption Architecture

## 1. Architectural Overview
This architecture eliminates all-core 100% CPU lockups by creating a structural separation between background compute workloads and interactive desktop threads.

```
       +-------------------------------------------------------------+
       |             Hardware Cores Topology (16 Logical Threads)     |
       +-------------------------------------------------------------+
       | Cores 0 .. 7 | Cores 8 .. 11 | Cores 12 .. 13 | Cores 14 .. 15 |
       +--------------+---------------+----------------+----------------+
Ultra: | Greedy Cap   | FREE HEADROOM | FREE HEADROOM  | CLEAN DESKTOP  |
Save:  | <---------- Greedy Allowed ----------> | FREE | CLEAN DESKTOP  |
Bal:   | <---------------- Greedy Allowed ------------>| CLEAN DESKTOP  |
Perf:  | <---------------- Greedy Allowed ------------>| CLEAN DESKTOP  |
       +-----------------------------------------------+----------------+
                                                       | KWin Wayland   |
                                                       | PipeWire Audio |
                                                       | Terminal Shell |
                                                       +----------------+
```

## 2. Core Components

### 2.1 Profile-Tiered Allowed Bitmask Generator
- Function: `MitigationEngine::get_headroom_allowed_cpuset(PowerProfileMode mode)`
- Computes compile-time and run-time bitmasks on stack `cpu_set_t`.
- Zero heap allocation, $O(1)$ evaluation time ($< 20\text{ns}$).

### 2.2 Thread-Tree Traversal Engine
- When a process is flagged for affinity capping, the daemon iterates `/proc/<pid>/task/` to discover all child thread IDs (TIDs).
- Calls `sched_setaffinity` on every TID to enforce the allowed mask across the entire thread pool (e.g. all 16 worker threads of `ninja`).
- Applies `SCHED_BATCH` and `nice +5` to `+15` via `sched_setscheduler` and `setpriority`.

### 2.3 Interactive Stack Shielding
- Periodically checks compositor and audio cgroups/PIDs:
  - Audio daemons: `nice -19`, full 16-core mask.
  - Compositor (`kwin_wayland`): `nice -10`, full 16-core mask.
- Under 100% all-core load, CFS immediately yields to interactive threads because they possess higher static priority and dedicated clean headroom cores.

## 3. Dynamic Life Cycle
```
[ High Load Detected ] (Threads >= 4 || Watts > Thresh || WDI > 6.0)
          |
          v
[ Apply Profile-Tiered Cap ] ---> sched_setaffinity(Cores 0..13) + SCHED_BATCH
          |
          v
[ Workload Subsides ] (Watts < Deescalate_W && Threads < 4)
          |
          v
[ Full Dynamic Restore ] ---> sched_setaffinity(All 16 Cores) + SCHED_OTHER (nice 0)
```
