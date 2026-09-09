# Global Agent Guidelines for WattCurb

## 1. Project Overview & Mission

**WattCurb** is an ultra-low-overhead, singleton Linux background daemon written in modern C++23. 
Its primary mission is to:
1. Maximize battery life and minimize overall system power consumption.
2. Conduct physical hardware-level power profiling (identifying exact physical components responsible for power drain: CPU Package/Cores/Uncore/DRAM via RAPL, GPU via hwmon/sysfs, Display/Backlight, Storage/NVMe APST, and PCIe/WiFi power states).
3. Dynamically analyze, throttle, freeze, or terminate runaway or unneeded background processes without introducing CPU wakeup overhead (adhering strictly to the Zero-Wakeup design principle).
4. Maintain long-term system stability and predictability.

---

## 2. Directory Structure Conventions

The repository strictly enforces the following directory layout:

```
WattCurb/
├── AGENTS.md               # Global directives and agent operational standards (this file)
├── .gitignore              # Ignores build artifacts, temporary files, and output binaries
├── docs/                   # Central repository for project documentation
│   ├── research/           # External research, paper summaries, prior art, kernel interface surveys
│   ├── architecture/       # System design, module architecture, component interfaces
│   └── requirements/       # Indexed requirements and specifications
├── src/                    # All production C++23 source code, headers, and implementation units
├── tests/                  # Unit tests, integration tests, and benchmark suites
├── build/                  # Intermediate build files and object files (GIT-IGNORED)
├── output/                 # Final compiled binaries, packages, and distribution artifacts (GIT-IGNORED)
└── tmp/                    # Temporary scratchpads, test logs, and ephemeral runtime data (GIT-IGNORED)
```

- **`docs/`**: Master documentation tree.
- **`docs/research/`**: Dedicated storage for collected research papers, external benchmark studies, competitor analysis (TLP, auto-cpufreq, powertop, systemd-oomd), and kernel documentation.
- **`src/`**: Pure production source code.
- **`build/`**: CMake/Ninja build working directory. Must NEVER be committed.
- **`output/`**: Binary releases and staging artifacts. Must NEVER be pushed to remote.
- **`tmp/`**: Ephemeral workspace. Must NEVER be pushed to remote.

---

## 3. Git Workflow & Prompt-Level Local Commit Discipline

1. **Prompt-by-Prompt Commit Requirement**:
   - Whenever any documentation, research file, configuration, or source code is updated during a user prompt/interaction, a **local Git commit must be executed within that turn** before concluding the response.
   - Commit messages must be structured, meaningful, and reference the corresponding `REF-ID` when applicable.
2. **Repository Cleanliness (`.gitignore`)**:
   - Build artifacts (`build/`), temporary files (`tmp/`), and final target distributions (`output/`) must remain excluded from git tracking.
   - Local commits may track newly introduced docs, research notes, and source code. Remote pushes must remain pristine and devoid of transient files.

---

## 4. Documentation-Driven Development & Ref-ID System

Every code change and design decision must be anchored in documentation to preserve structural integrity:

1. **Documentation Evaluation on Every Prompt**:
   - Upon receiving any user prompt, evaluate whether documentation or research needs to be created or updated.
   - Code and documentation must maintain strict bi-directional synchronization. Code must never diverge from documented specifications.
2. **Ref-ID Indexing Standards**:
   - All documented items must have a unique, searchable identifier (`ref-id`) to enable cross-referencing:
     - `REF-REQ-xxx`: Functional and non-functional requirements (e.g., `REF-REQ-001`).
     - `REF-RES-xxx`: Research entries, external paper analyses, competitor benchmarks (e.g., `REF-RES-001`).
     - `REF-ARCH-xxx`: Architecture designs, component interfaces, state machines (e.g., `REF-ARCH-001`).
     - `REF-TEST-xxx`: Unit test specifications and evaluation test cases (e.g., `REF-TEST-001`).
3. **Cross-Referencing Rule**:
   - Code comments, test cases, and commit logs should reference their corresponding `ref-id` (e.g., `// Implements REF-REQ-002; see docs/requirements/power_profiler.md`).

---

## 5. Automated Testing & The "Oracle Gate" Regression Loop

Code correctness and extreme energy efficiency must be verified through automated pipelines:

1. **Granular Unit Testing**:
   - Write small, isolated, and deterministic unit tests for every logical unit (e.g., parsing procfs/sysfs lines, Netlink socket decoders, rule matching, singleton lock acquisition).
2. **Automated Evaluation (Eval) Telemetry**:
   - In addition to standard assertion tests, code must produce quantifiable evaluation metrics:
     - CPU Wakeup Count (wakeups per minute).
     - Daemon CPU consumption (target: < 0.1% CPU under idle/monitoring).
     - Memory RSS footprint (target: < 10 MB, zero-allocation during steady-state monitoring).
     - Power profiling sampling latency and accuracy against kernel RAPL counters.
3. **Oracle Gate Verification**:
   - The "Oracle Gate" is the automated validation barrier that checks test pass rates and evaluation thresholds.
   - If an edit causes a failure in tests or degrades evaluation metrics (e.g., introducing heap allocations in the hot path or uncoordinated CPU wakeups), the change must be rejected or placed into an automated correction loop until the regression is resolved.

---

## 6. Physical Hardware Profiling Directives

WattCurb must not treat power as a monolithic system-wide number. It must attribute power draw to physical hardware domains:
1. **CPU & Memory Subsystem**:
   - Intel/AMD RAPL (`/sys/class/powercap/intel-rapl/`): Package (`package-0`), Core (`core`), Uncore (`uncore`), and DRAM (`dram`).
2. **Graphics Processing Units (GPU)**:
   - Dedicated/Integrated GPU power sensors via `hwmon`, `sysfs` (`/sys/class/drm/card*/device/hwmon/`), AMDGPU power metrics, and NVIDIA NVML/sysfs.
3. **Display & Backlight**:
   - Backlight power and brightness levels via `/sys/class/backlight/`.
4. **Storage & NVMe**:
   - Autonomous Power State Transitions (APST) and active states via `/sys/class/nvme/` and block device sysfs.
5. **Bus & Peripheral Devices (PCIe / USB / WiFi)**:
   - PCIe ASPM (Active State Power Management) states, runtime PM (`power/control`), and wireless power saving modes (`iw dev <dev> set power_save on`).

---

## 7. Agent Operational Directives & Language Policy

1. **User Interaction Language**:
   - **All conversational prompts, responses, explanations, and user interactions must be in Korean (한국어).**
2. **Code & Internal Documentation Language**:
   - `AGENTS.md`, source code identifiers, comments, and formal technical docs are maintained in English (or bilingual as needed) to ensure technical precision and compatibility with global engineering standards.
3. **Non-Intrusive Execution**:
   - The daemon must never compromise system responsiveness. Throttling and freezing actions must be progressive and adhere strictly to safe priority levels (`SCHED_IDLE`, cgroups v2 freezing, affinity masking) before considering process termination.

---

## 8. Release Build Optimization, PMU/ASM Analysis & PGO Pipeline

Release builds must adhere to an empirical, hardware-verified optimization workflow:

1. **PMU Hardware Counter & Assembly (ASM) Performance Audit**:
   - Release binaries must undergo hardware Performance Monitoring Unit (PMU) analysis (`perf stat` tracking instructions, cycles, IPC, L1-dcache-load-misses, dTLB-load-misses, branch-misses).
   - Generate and inspect assembly dumps (`-S -fverbose-asm` / `objdump -d -M intel`) to verify:
     - Vectorization (SIMD / AVX2 / AVX-512 where applicable).
     - Elimination of dead stores, unnecessary branch jumps, and function inlining verification.
     - Confirmation that no unintended runtime heap allocations or exception handling landing pads are generated in the inner monitoring loop.
2. **Profile-Guided Optimization (PGO) Build Pipeline**:
   - The production build system must support a 2-stage PGO pipeline:
     - **Stage 1 (Instrumentation)**: Compile with `-fprofile-generate`, `-O3`, and `-flto=auto`.
     - **Stage 2 (Representative Profile Training)**: Execute automated representative workloads (profiling typical desktop/server process loads).
     - **Stage 3 (Optimized Compilation)**: Compile the final production artifact with `-fprofile-use`, `-fprofile-correction`, and Link-Time Optimization (`-flto=auto`).
   - Deliver an empirical performance report documenting IPC gains, cache miss reductions, and latency improvements over standard `-O3`.

---

## 9. C++23 Zero-Cost Abstractions, Cache-Locality & Minimal Memory Footprint

To achieve sub-milliwatt daemon overhead and preserve host battery, software architecture must be tailored to CPU hardware realities:

1. **Clean Layered Separation via Zero-Cost Abstractions**:
   - Decompose code cleanly into modular subsystems (Hardware Probes, Process Analyzers, Attribution Policy, Mitigation Actuators).
   - Enforce abstractions using compile-time C++23 mechanisms:
     - C++23 Concepts and constraints instead of runtime `vtable` virtual dispatch in hot paths.
     - `constexpr` and `consteval` for compile-time lookup tables, unit conversions, and bitmask calculations.
     - `std::expected` and `std::optional` for zero-overhead, non-allocating error handling.
2. **L1 Data (L1D) & Instruction (L1I) Cache Hit Maximization**:
   - Layout hot structures contiguously to fit inside L1 Data Cache (typically 32 KB ~ 48 KB per core).
   - Struct-of-Arrays (SoA) or Hot/Cold field splitting: isolate frequently accessed counters (ticks, engine ns, wakeups) from cold string names and metadata.
   - Cache-line alignment: align hot structures to 64 bytes (`alignas(64)`) to avoid false sharing and misaligned cache line splits.
   - Keep loop bodies small and tight to fit entirely inside the CPU's L1 Instruction Cache (L1I) and decoded loop buffer.
3. **TLB & dTLB Miss Minimization**:
   - Zero dynamic heap allocation (`new`, `malloc`, reallocation of `std::vector`/`std::string`) during steady-state profiling loops.
   - Eliminate pointer chasing: replace node-based structures (`std::map`, `std::list`, pointers to pointers) with contiguous flat arrays, `std::span`, and fixed-capacity stack buffers.
   - Restrict the working set memory footprint to a single memory page or small contiguous arena to prevent TLB cache evictions.

---

## 10. Continuous PMU Milestone Telemetry & LLM Optimization Feedback Loop

To guarantee that WattCurb converges toward zero overhead rather than suffering gradual performance degradation:

1. **Feature-Level PMU Documentation Mandate**:
   - For every major functional unit, algorithmic revision, or subsystem refactor, the agent must run an empirical hardware PMU audit (`perf stat` tracking task-clock, cycles, instructions, IPC, L1D-misses, dTLB-misses, memory RSS, and power overhead).
   - Results must be formally documented in `docs/research/PMU_BENCHMARKS.md` ([`REF-RES-005`](file:///home/jedclub/Develop/WattCurb/docs/research/PMU_BENCHMARKS.md)) with clear milestone versioning.
2. **LLM Optimization Feedback & Guardrail**:
   - The AI Agent (LLM) must actively reference historical PMU metrics as design constraints when implementing subsequent features.
   - If a new feature or refactor regresses CPU active time, introduces heap allocations, or degrades IPC, the agent must perform assembly/syscall analysis and iteratively optimize the implementation until the regression is eliminated.
3. **Metric Conversion Standards**:
   - All PMU records must report both raw hardware counters and converted real-world metrics:
     - Active CPU Task-Clock (ms) & Core Utilization Percentage.
     - System-wide CPU Percentage (normalized across all host threads).
     - Peak Resident Set Size (RSS in MB).
     - Energy Consumption ($J$) & Average Power Overhead ($mW$).


