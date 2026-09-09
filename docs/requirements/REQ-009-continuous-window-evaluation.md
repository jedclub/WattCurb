# [REF-REQ-012] Multi-Sample Continuous Window Evaluation & Steady-State Telemetry Specification

- **Ref-ID**: `REF-REQ-012`
- **Related Requirements**: [`REF-REQ-001`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-001-hardware-power-profiling.md), [`REF-REQ-004`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-001-hardware-power-profiling.md#24-ref-req-004-hardware-to-process-power-attribution), [`REF-REQ-006`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-003-pgo-pmu-optimization.md), [`REF-REQ-011`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-008-process-hardware-feature-tracking.md)
- **Related Architecture**: [`REF-ARCH-002`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-002-profiler-implementation.md), [`REF-ARCH-004`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-004-resident-daemon-event-loop.md)
- **Status**: Approved

---

## 1. Problem Statement: The Limits of 1-Second Profiling

Evaluating hardware power attribution over an ultra-short snapshot window (e.g. 1.0s or 2.0s) introduces several physical hardware telemetry errors and software aliasing artifacts:

### 1.1 Physical Hardware Sampling Incompatibilities
1. **Battery Fuel Gauge IC Update Latency (10s ~ 30s)**:
   - Modern laptop battery management ICs (e.g., TI BQ40Z50, BQ24780S) sample coulomb counters and integrate voltage/current over slow ADC integration periods (typically 10 to 30 seconds).
   - In a 1-second window, `/sys/class/power_supply/BAT*/current_now` returns either a static cached register value or an instantaneous spike uncorrelated with the window's actual energy drain.
2. **Thermal Inertia & Cooling Fan Hysteresis (5s ~ 20s)**:
   - Laptop cooling assemblies (heatpipes, fins, Embedded Controller EC fan tables) exhibit significant thermal mass.
   - An instantaneous 1-second GPU or CPU burst does not immediately cause the fan RPM to spin up; conversely, after a heavy workload ceases, the fan continues spinning at 4,000+ RPM for 15 seconds to dissipate residual heat. A 1-second sample risks attributing residual fan power to an idle process.
3. **NVMe APST (Autonomous Power State Transitions)**:
   - NVMe SSD controllers require an idle latency threshold (typically 100ms to 2,000ms) before stepping down from Active State (PS0, ~3.5W) to Non-Operational Low Power States (PS3/PS4, ~5mW). Short snapshot windows cannot determine true steady-state sleep efficiency.

### 1.2 Software Workload Aliasing
1. **Transient Bursts vs. Sustained Drain**:
   - Daemons such as `systemd`, `dbus-broker`, and `polkitd` periodically exchange batches of IPC messages. A 1-second capture that coincides with an IPC burst can record 3,000+ wakeups/sec, falsely branding the process as a catastrophic battery drainer when its 30-second average wakeup rate is negligible.
   - Conversely, periodic cron jobs or browser background syncs that run for 2 seconds every 30 seconds have a ~93% chance of being missed entirely in a 1-second snapshot.
2. **Ephemeral Process Loss**:
   - In a simple 2-snapshot delta ($T_2 - T_1$), short-lived processes (such as compilers, git hooks, or shell utilities) that spawn after $T_1$ and exit before $T_2$ are completely invisible.

---

## 2. Functional Requirements

### 2.1 Continuous Multi-Sample Window Evaluation (`--duration`, `-c, --count`)
- The profiler CLI must support:
  - `-w, --duration <sec>`: Total evaluation duration in seconds (e.g., `--duration 10` or `--duration 30`).
  - `-c, --count <num>`: Total number of intermediate sampling intervals to accumulate.
- When multi-sample evaluation is active, the engine must perform step-by-step delta captures across all intervals:
  $$\Delta t_k = t_k - t_{k-1}$$
- Intermediate process deltas ($\Delta \text{ticks}_k$, $\Delta \text{wakeups}_k$, $\Delta \text{gpu\_ns}_k$, $\Delta \text{io\_bytes}_k$) must be accumulated into a persistent aggregation map. Even if a process terminates between intervals, its accumulated energy drain must be retained in the final attribution report.

### 2.2 Time-Integrated Energy Attribution ($Joules$)
- Along with instantaneous/average power ($W$), the report must report total dissipated energy in Joules ($J$):
  $$E_{\text{total}} = \int_{0}^{T} P(t) \, dt \approx \sum_{k=1}^{N} \bar{P}_k \cdot \Delta t_k$$
- For each process:
  $$E_i = P_{\text{total}, i} \cdot T$$
- The CLI output must present both total Joules and sustained average Watts.

### 2.3 Short Window Notice
- If the evaluated time window is less than 3.0 seconds, the profiler must display a clear, non-intrusive warning notice:
  `[!] Notice: Short evaluation window (<duration>s). Physical hardware telemetry (Battery Fuel Gauge, EC Fan, NVMe APST) may have transient variance. Use '--duration 10' or '-c 5' for sustained attribution.`

### 2.4 Live Interactive Top Monitoring Mode (`-l, --live`)
- To allow interactive real-time inspection, the CLI must provide a `-l, --live` mode.
- In live mode, the terminal clears and redraws the software-to-hardware attribution table at every sampling interval until interrupted (`SIGINT` / `Ctrl+C`).

---

## 3. Steady-State PMU Benchmark Requirement (Milestone M4)

- A standalone 1-shot execution includes initialization, dynamic linking, and cold-cache effects.
- To rigorously verify the Zero-Wakeup and sub-milliwatt daemon overhead in steady-state:
  - **Milestone M4 Benchmark Protocol**:
    1. Run WattCurb daemon or continuous profiler over a **30-second continuous evaluation window**.
    2. Audit with hardware PMU counters via `perf stat` tracking instructions, cycles, IPC, L1D misses, dTLB misses, and task-clock.
    3. Calculate true sustained CPU utilization:
       $$\text{CPU Utilization} = \frac{\text{task-clock (ms)}}{30,000 \text{ ms}} \times 100\%$$
    4. Guardrail Target: Sustained CPU Utilization must be **$< 0.1\%$** of a single core (and $< 0.01\%$ host-wide across 16 threads). Peak RSS memory must remain flat without leaks.
