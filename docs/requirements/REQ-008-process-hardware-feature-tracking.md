# [REF-REQ-011] Process-to-Hardware Feature Attribution & Physical Causation Tracking Engine

- **Ref-ID**: `REF-REQ-011`
- **Related Requirements**: [`REF-REQ-001`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-001-hardware-power-profiling.md), [`REF-REQ-004`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-001-hardware-power-profiling.md#24-ref-req-004-hardware-to-process-power-attribution), [`REF-REQ-010`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-007-extreme-hardware-telemetry.md)
- **Related Architecture**: [`REF-ARCH-002`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-002-profiler-implementation.md)
- **Related Research**: [`REF-RES-002`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-002-hardware-power-measurement-mechanisms.md), [`REF-RES-003`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-003-hardware-to-process-attribution.md), [`REF-RES-005`](file:///home/jedclub/Develop/WattCurb/docs/research/PMU_BENCHMARKS.md)
- **Status**: Approved

---

## 1. Executive Summary & Objective

Monolithic power attribution that simply dumps numbers into a single "Watts" column fails to provide actionable insight to users and power management daemons. 

This specification mandates that WattCurb tracks the **exact physical hardware mechanism** exercised by each process, establishing clear, bi-directional causation:
1. **Hardware-to-Process**: For every physical watt dissipated by silicon or mechanical apparatus (GPU, CPU Package, C-State sleep disruption, NVMe controller, or cooling fan), attribute the exact percentage share and list the responsible processes.
2. **Process-to-Hardware**: For every running process, explicitly identify its primary hardware drain domain (`primary_hw_domain`) and output detailed telemetry detailing how it interacts with the physical silicon (e.g. AMDGPU GFX Engine ns, VRAM KiB, C3 deep sleep disruption frequency, NVMe read/write throughput, and fan thermal induction).

---

## 2. Multi-Domain Physical Causation Model

WattCurb evaluates five physical causation vectors for each process:

### 2.1 Graphics & Compute Silicon (AMDGPU / DRM)
- **Hardware Telemetry Node**: `/sys/class/drm/card*/device/hwmon/*/power1_input` ($\mu W$)
- **Process Accounting Interface**: `/proc/[pid]/fdinfo/<drm_fd>`
  - `drm-engine-gfx`: 3D rendering pipeline active nanoseconds.
  - `drm-engine-compute`: OpenCL / ROCm GPGPU compute nanoseconds.
  - `drm-engine-dec` & `drm-engine-enc`: Hardware video decoder / encoder active nanoseconds.
  - `drm-memory-vram`: Dedicated GPU high-bandwidth VRAM residency.
- **Attribution Model**:
  $$P_{\text{GPU}, i} = P_{\text{GPU\_PPT}} \times \left( \frac{\Delta T_{\text{engine}, i}}{\sum_k \Delta T_{\text{engine}, k}} \right)$$

### 2.2 CPU C-State Deep Sleep Disruption (Wakeup Tax)
- **Hardware Impact**: High-frequency timer interrupts, IPC polling, and short context switches force the CPU package to repeatedly exit deep $C_2/C_3$ sleep into $C_0$ active state.
- **Process Accounting Interface**: `/proc/[pid]/status` (`voluntary_ctxt_switches` + `nonvoluntary_ctxt_switches`).
- **Attribution Model**:
  $$P_{\text{WakeTax}, i} = \min\left(1.5\text{ W}, \Delta \text{Wakeups/s}_i \times 0.0012\text{ W}\right) \quad (\text{when } \text{Wakeups/s} > 15)$$

### 2.3 Mechanical Cooling Fan Thermal Induction
- **Hardware Impact**: When silicon junction temperatures rise ($T_{\text{cpu}} > 65^\circ\text{C}$, $T_{\text{gpu}} > 55^\circ\text{C}$), the ThinkPad Embedded Controller ramps fan speed up to 4,300+ RPM, consuming $1.5 \sim 2.5\text{ W}$ in mechanical motor energy.
- **Physical Causation**: Processes generating high thermal heat (CPU compute + GPU load) are directly responsible for the fan power.
- **Attribution Model**:
  $$P_{\text{Fan}, i} = P_{\text{Fan\_Mechanical}} \times \left( \frac{P_{\text{CPU}, i} + P_{\text{GPU}, i}}{\sum_k (P_{\text{CPU}, k} + P_{\text{GPU}, k})} \right)$$

### 2.4 Storage & NVMe APST Disruption
- **Hardware Impact**: NVMe Autonomous Power State Transitions (APST) put the SSD into PS3/PS4 standby ($5\text{mW}$). Frequent writes or I/O syscalls force the SSD into PS0 active ($3\sim 7\text{W}$).
- **Process Accounting Interface**: `/proc/[pid]/io` (`read_bytes`, `write_bytes`, `syscr`, `syscw`).
- **Attribution Model**:
  $$P_{\text{NVMe}, i} = (\text{MB/s}_i \times 0.015\text{ W}) + (0.05\text{ W} \times \mathbb{I}(\Delta \text{IOPS}_i > 50))$$

### 2.5 Dynamic CPU Execution Quantum
- **Process Accounting Interface**: `/proc/[pid]/stat` (`utime`, `stime`).
- Proportional share of the dynamic CPU Package RAPL counter or decomposed CPU rail.

---

## 3. Telemetry Output & Reporting Directives

1. **Software Attribution Table**:
   - Must expose explicit separate columns for:
     `PID | Process Name | CPU(W) | GPU(W) | NVMe(W) | WakeTax(W) | Fan(W) | Total(W) | WDI | Primary Hardware Mechanism`
2. **Domain-Specific Culprit Section**:
   - Dedicate a clear dashboard section listing:
     - **GPU Power Culprits** (Ranked by GPU engine occupancy and VRAM).
     - **C-State Sleep Breakers** (Ranked by context switches per second and Wakeup Tax).
     - **Cooling Fan Thermal Drivers** (Ranked by heat contribution inducing fan RPM).
     - **Storage APST Disrupters** (Ranked by I/O throughput and syscall frequency).
3. **Structured JSON Telemetry**:
   - Export all per-process hardware vectors and domain culprit groupings in JSON format for automated daemon mitigation and programmatic telemetry.
