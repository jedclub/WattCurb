# [REF-REQ-057] Multi-Profile Anti-Monopoly Core Capping & CPU Headroom Protection

## 1. Executive Summary & Problem Definition
On multi-core, simultaneous multi-threading (SMT) systems such as the AMD Ryzen 7 PRO 4750U (8 physical cores, 16 logical threads), compute-heavy or runaway processes frequently spawn 16 or more worker threads. Under default Linux Completely Fair Scheduler (CFS), this full saturation monopolizes 100% of all logical CPU cores, causing severe interactive degradations:
1. **Compositor & Display Starvation**: `kwin_wayland` misses its 16.6ms frame presentation deadlines, causing severe mouse cursor stuttering, window frame freezes, and complete desktop lockups.
2. **Audio Dropouts (XRUNs)**: Real-time audio pipelines (`pipewire`, `wireplumber`) miss buffer deadlines, producing audible glitches, clicks, or total sound cutoffs.
3. **Low-Clock Latency Catastrophe in Power Saving Modes**: In Ultra (1.4GHz, 4W) and Save (1.7GHz, 10W) modes, lower core clock speeds dramatically increase runqueue latencies. Runaway tasks queue up, completely stalling user input and `Ctrl+C` interrupt handling.
4. **Performance Mode Vulnerability**: Prior implementations suppressed throttling in Performance Mode, allowing greedy jobs to fully monopolize all 16 cores and trigger desktop lockups.

To guarantee that no single process or job tree can monopolize the system, **WattCurb** mandates **Multi-Profile Anti-Monopoly Core Capping & CPU Headroom Protection** (`REF-REQ-057`).

---

## 2. Functional Requirements

### REQ-057-1: Clean Headroom Core Isolation (Cores 14 & 15)
- On an $N=16$ system, Cores 14 & 15 (one physical SMT core pair) are strictly reserved as the **Clean Interactive & Audio Headroom Domain**.
- No greedy batch compute process shall ever be allowed to execute on Cores 14 or 15.
- The clean headroom cores guarantee that `kwin_wayland`, `pipewire`, `wireplumber`, and shell processes always have immediate, queue-free CPU compute resources.

### REQ-057-2: Profile-Tiered Core Monopoly Limits
The maximum number of cores permitted for any greedy compute workload is dynamically clamped according to the active power profile:

| Power Profile | Hardware TDP & Clock | Max Permitted Cores | Headroom Reserved | Scheduler & Priority |
|---|---|---|---|---|
| **UltraEndurance** | 1.4GHz / 4W | **Max 8 Cores (50%)** | 8 Cores Free (0..7 allowed) | `SCHED_BATCH` (`nice +15`) / `SCHED_IDLE` |
| **PowerSaver** | 1.7GHz / 10W | **Max 12 Cores (75%)** | 4 Cores Free (0..11 allowed) | `SCHED_BATCH` (`nice +10`) |
| **Balanced** | Dynamic / 18W | **Max 14 Cores (87.5%)** | 2 Cores Free (0..13 allowed) | `SCHED_BATCH` (`nice +10`) |
| **Performance** | Boost 4.1GHz / 25W | **Max 14 Cores (87.5%)** | 2 Cores Free (0..13 allowed) | `SCHED_BATCH` (`nice +5`) |

### REQ-057-3: Profile-Scaled Greedy Process Detection
Detection shall not rely on monolithic watt numbers, but adapt to active TDP boundaries:
- **TDP-Scaled Thresholds**:
  - UltraEndurance (4W): CPU attribution $> 0.35\text{W}$
  - PowerSaver (10W): CPU attribution $> 0.70\text{W}$
  - Balanced (18W): CPU attribution $> 1.20\text{W}$
  - Performance (25W): CPU attribution $> 2.00\text{W}$
- **Multi-Thread Monopoly Flag**: Any process with `num_threads >= 4` and non-trivial compute (`cpu_watts > 0.25W`) is immediately classified as a parallel compute job and subjected to core affinity capping.
- **WDI & Runaway Flag**: Any process with `wdi_score > 6.0` or `is_runaway_candidate == true`.

### REQ-057-4: Proactive Compositor & Audio Stack Shielding
- `pipewire`, `wireplumber`, and `pulseaudio` are proactively prioritized to `nice -19` with unrestricted access to all 16 cores.
- `kwin_wayland`, `kwin_x11`, `mutter`, and `Xwayland` are prioritized to `nice -10` with unrestricted access to all 16 cores.

### REQ-057-5: Dynamic De-escalation & Restoration
- When a capped process's CPU consumption subsides below profile de-escalation levels for consecutive cycles, its CPU affinity is cleanly restored to all $N$ cores and its original scheduling policy/nice value is restored.

---

## 3. Non-Functional Constraints
1. **Absolute Zero-Kill Invariant (`REF-REQ-044`)**: No process is terminated or frozen; mitigation is strictly achieved via affinity partitioning and CFS batch prioritization.
2. **Compute Throughput Preservation**: On 16-core systems, capping greedy batch jobs to 14 cores preserves 87.5% of peak compute throughput (SMT scalability yields ~92% actual speed).
3. **Zero Dynamic Allocation**: Affinity bitmask calculations and thread traversal operate entirely on the stack.

---

## 4. Verification & Testing Standards (`REF-TEST-022`)
- Unit tests (`tests/test_units.cpp`) verify:
  - Multi-profile allowed cpuset calculation across Ultra (8 cores), Save (12 cores), Balanced (14 cores), and Performance (14 cores).
  - Cores 14 & 15 are never allocated to capped batch jobs.
  - Compositor and audio services receive priority shielding and full-core affinity.
