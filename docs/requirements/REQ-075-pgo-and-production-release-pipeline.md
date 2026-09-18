# [REF-REQ-075] Comprehensive PGO Profiling & Production Release Pipeline

## 1. Overview & Operational Scope
To guarantee maximum execution throughput, minimum energy consumption, and zero-residual runtime overhead, WattCurb enforces a 2-stage Profile-Guided Optimization (PGO) compilation pipeline coupled with Link-Time Optimization (LTO) and aggressive binary stripping across all production artifacts.

- **Requirement Identifier**: `REF-REQ-075`
- **Related Requirements**: `REF-REQ-003`, `REF-REQ-005`, `REF-REQ-008`, `REF-REQ-035`, `REF-REQ-036`, `REF-REQ-073`, `REF-REQ-074`
- **Related Architecture**: `REF-ARCH-003`, `REF-ARCH-019`, `REF-ARCH-052`
- **Verification Gate**: `REF-TEST-040` (PGO Multi-Target Stripped Binary & PMU Audit)

---

## 2. Functional & Non-Functional Requirements

### 2.1 Multi-Target Production Suite
The PGO pipeline must optimize and output all components of the WattCurb system:
1. `wattcurb` (Background Daemon & CLI engine)
2. `wattcurb-tray` (Zero-Overhead StatusNotifierItem Desktop Tray Client)
3. `wattcurb-dashboard` (High-Density QML/C++23 Precision Matrix Dashboard)

### 2.2 Profile Generation (Stage 1)
- Must compile all binaries with `-fprofile-generate`, `-O3`, `-march=native`, and `-flto=auto`.
- Must exercise realistic, representative operational workloads during profile training:
  - Full algorithmic unit test suite (`wattcurb_tests`): exercises procfs parsing, RAPL/hwmon sensors, battery fuel gauges, attribution engines, and math routines.
  - Multi-mode CLI execution: `--features`, `--interval 1 --top 30`, `--briefing`, and `--extreme-profile -w 2 -i 1`.
  - Desktop Tray & Dashboard state transitions.

### 2.3 Profile Feedback & Release Compilation (Stage 2)
- Must recompile the entire suite with:
  - `-fprofile-use -fprofile-correction -Wno-error=coverage-mismatch`
  - `-flto=auto` (Full cross-module Link-Time Optimization)
  - `-march=native` (Hardware-tailored vectorization & instruction scheduling)
  - `-DNDEBUG` (Runtime assertion stripping)
  - `-DWATTCURB_DEV_PROFILE=OFF` (Absolute zero-cost ScopedProfiler elimination)
  - `-fvisibility=hidden -fvisibility-inlines-hidden -fno-rtti -fno-exceptions` (Zero metadata/symbol leakage)
  - `-ffunction-sections -fdata-sections -Wl,--gc-sections` (Dead code and data elimination)

### 2.4 Binary Stripping & Section Pruning
All production binaries staged into `output/` must be aggressively stripped:
- `strip --strip-all`
- Elimination of non-loadable metadata sections: `--remove-section=.note.gnu.build-id`, `--remove-section=.note.ABI-tag`, `--remove-section=.note.gnu.property`, `--remove-section=.comment`, `--remove-section=.sframe`, `--remove-section=.eh_frame`, `--remove-section=.eh_frame_hdr`.

### 2.5 Hardware PMU & Assembly Audit
- Run hardware PMU telemetry via `perf stat` tracking instructions, cycles, IPC, L1D-misses, dTLB-misses, and branch mispredictions.
- Generate Intel-syntax assembly dumps (`build/asm/*.s`) to audit L1I loop alignments (`.p2align 4`), SIMD vectorization, and elimination of runtime heap allocations (`_Znwm`/`malloc`).

---

## 3. Verification & Oracle Gate Standards (`REF-TEST-040`)
1. **Binary Generation**: `output/wattcurb`, `output/wattcurb-tray`, and `output/wattcurb-dashboard` must exist and be fully executable.
2. **Binary Diet**:
   - `output/wattcurb` <= 220 KB.
   - `output/wattcurb-tray` <= 60 KB.
   - `output/wattcurb-dashboard` <= 150 KB (excluding dynamic Qt6 runtime libraries).
3. **PGO Correctness**: Zero crashes or functional regressions under representative workloads.
4. **PMU Hardware Assertions**: High IPC (> 1.0 on compute loops) and low dTLB miss rate (< 0.1%).
