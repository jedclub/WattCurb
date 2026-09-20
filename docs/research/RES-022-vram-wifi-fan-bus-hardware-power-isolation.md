# REF-RES-022: Physical Power Isolation & Telemetry Decomposition for VRAM, Wi-Fi, Fan, and PCIe/Fabric Bus

- **Status**: Approved
- **Ref ID**: `REF-RES-022`
- **Related Research**: [`REF-RES-019`](RES-019-granular-platform-loss-decomposition-and-kernel-interfaces.md), [`REF-RES-020`](RES-020-amd-zen-ccx-topology-and-c1-c2-latency-shield.md)
- **Related Requirements**: [`REF-REQ-082`](../requirements/REQ-082-granular-platform-loss-hardware-telemetry.md), [`REF-REQ-064`](../requirements/REQ-064-wifi-txpower-capping-and-decomposed-platform-loss.md)
- **Created**: 2026-09-21
- **Category**: Physical Hardware Telemetry, Component Power Isolation, Kernel Interfaces

---

## 1. Physical Hardware Reality vs. Telemetry Isolation

In modern laptop platforms (such as Lenovo ThinkPad with AMD Renoir/Cezanne or Intel Core architectures), hardware manufacturers do not place physical current-sensing shunt resistors (ADC PMICs) on every individual trace (e.g. discrete VRAM traces, PCIe bus lines, or Wi-Fi M.2 connectors) due to BOM cost and motherboard PCB area constraints.

Only primary power rails have dedicated hardware current sensors:
1. **Total System Battery Rail**: Fuel gauge shunt resistor reporting $V_{\text{bat}} \times I_{\text{bat}}$ via `/sys/class/power_supply/BAT0/`.
2. **CPU Package & Cores**: Intel/AMD RAPL MSRs and `/sys/class/powercap/intel-rapl/`.
3. **GPU Core & ASIC**: AMDGPU / NVIDIA NVML sensors via `/sys/class/hwmon/hwmon*/power1_input`.

However, the Linux kernel exposes **rich, real-time physical telemetry states** that permit high-accuracy mathematical and physical isolation of sub-domains:
- **VRAM**: Dedicated Video RAM vs Compute Core.
- **Wi-Fi**: Baseband quiescent + RF Tx Amplifier power.
- **Fan**: Aerodynamic mechanical shaft power.
- **Bus**: PCIe link PHYs, Infinity Fabric (FCLK), and SoC interconnect.

---

## 2. Component-by-Component Isolation Mechanisms

### 2.1 VRAM (Video Memory Power Isolation)

#### Kernel Interfaces:
- **Allocation Telemetry**:
  - `/sys/class/drm/card*/device/mem_info_vram_used`: Active dedicated VRAM (bytes).
  - `/sys/class/drm/card*/device/mem_info_vram_total`: Total physical/carved VRAM.
  - `/sys/class/drm/card*/device/mem_info_gtt_used`: Graphics Translation Table system memory allocations.
- **Clock & DPM Telemetry**:
  - `/sys/class/drm/card*/device/pp_dpm_mclk`: Memory controller and VRAM clock state (e.g. 400MHz, 800MHz, 1200MHz, 1333MHz with active `*` indicator).
- **Core Power Telemetry**:
  - `/sys/class/drm/card*/device/pp_dpm_sclk`: Shader core clock state (e.g. 200MHz ~ 1600MHz).
  - `/sys/class/hwmon/hwmon4/power1_input`: Total GPU ASIC / APU PPT power.

#### Isolation Formulation:
GPU total power $P_{\text{gpu}}$ is decomposed into Core Compute ($P_{\text{gfx}}$) and Memory Subsystem ($P_{\text{vram}}$):
$$P_{\text{vram}} = P_{\text{vram\_static}} + k_{\text{mclk}} \cdot \left(\frac{f_{\text{mclk}}}{f_{\text{mclk\_max}}}\right) \cdot V_{\text{mem}}^2 + k_{\text{util}} \cdot \left(\frac{\text{VRAM}_{\text{used}}}{\text{VRAM}_{\text{total}}}\right) \cdot \alpha_{\text{gpu\_busy}}$$

- On integrated APUs (Renoir/Cezanne), $P_{\text{vram}}$ represents the graphics memory controller, PHY termination, and DRAM traffic slice.
- On discrete GPUs (dGPU), $P_{\text{vram}}$ represents physical GDDR6/HBM chips and VDDQ/VPP rails.

---

### 2.2 Wi-Fi Subsystem (Baseband & RF Power Isolation)

#### Kernel Interfaces:
- **Die Temperature**: `/sys/class/hwmon/hwmon8/temp1_input` (`iwlwifi` die thermal sensor).
- **RF Configuration**: `nl80211` / `mac80211` (`iw dev wlan0 info`) reporting `txpower` (e.g. $12.00\text{ dBm} \approx 15.85\text{ mW}$ RF power output).
- **Traffic Duty Cycle**: `/sys/class/net/wlan0/statistics/tx_bytes` and `rx_bytes`.
- **Bus Power State**: `/sys/bus/pci/devices/0000:03:00.0/power/runtime_status` (PCIe D0 vs D3cold).

#### Isolation Formulation:
RF Power Amplifier (PA) efficiency $\eta_{\text{PA}}$ typically ranges between $15\% \sim 25\%$.
$$P_{\text{RF\_Tx}} = \frac{10^{\frac{\text{txpower\_dBm}}{10}} \times 10^{-3}}{\eta_{\text{PA}}} \cdot \text{Duty}_{\text{Tx}}$$
$$P_{\text{wifi}} = P_{\text{quiescent}}(\text{D0/D3}) + P_{\text{RF\_Tx}} + P_{\text{baseband}}(\text{Rx\_rate})$$
- Idle / Power Save enabled: $60\text{--}120\text{ mW}$.
- Active Web / Medium Traffic: $250\text{--}500\text{ mW}$.
- Sustained Throughput (160MHz 2x2 MIMO): $1.2\text{--}1.8\text{ W}$.

---

### 2.3 Cooling Fan (Mechanical Shaft Power Isolation)

#### Kernel Interfaces:
- **EC Tachometer**: `/sys/class/hwmon/hwmon3/fan1_input` (ThinkPad EC RPM sensor).
- **PWM Actuation**: `/sys/class/hwmon/hwmon3/pwm1` (0 ~ 255 duty cycle).

#### Isolation Formulation:
Centrifugal cooling fans follow the aerodynamic affinity laws, where aerodynamic resistance and motor shaft power scale with the **cube of angular velocity**:
$$P_{\text{fan}}(\text{RPM}) = \begin{cases} 0.0\text{ W} & (\text{RPM} < 500) \\ P_{\text{bearing\_loss}} + k_{\text{aero}} \cdot \left(\frac{\text{RPM}}{\text{RPM}_{\text{rated}}}\right)^3 & (\text{RPM} \ge 500) \end{cases}$$

For standard 5V, 0.5A laptop fans (e.g. ThinkPad T14 / P14s):
- **0 RPM**: $0.00\text{ W}$ (Fan stopped).
- **2,000 RPM**: $\sim 0.22\text{ W}$.
- **3,500 RPM**: $\sim 0.85\text{ W}$.
- **4,500 RPM**: $\sim 1.85\text{ W}$.
- **5,200 RPM (Full throttle)**: $\sim 2.45\text{ W}$.

---

### 2.4 BUS (PCIe Links, Infinity Fabric & SoC Interconnect Isolation)

#### Kernel Interfaces:
- **PCIe Link Speed & Width**: PCIe Configuration Space Capability 0x10 Link Status (Gen 1/2/3/4 @ x1, x2, x4, x16).
- **PCIe ASPM State**: `/sys/bus/pci/devices/*/power/runtime_status` (D0 Active vs D3hot/D3cold Suspended).
- **AMD Infinity Fabric (FCLK)**: `/sys/class/drm/card1/device/pp_dpm_fclk` (400MHz ~ 1333MHz).
- **SoC Clock (SOCCLK)**: `/sys/class/drm/card1/device/pp_dpm_socclk` (400MHz ~ 975MHz).
- **RAPL Uncore**: $P_{\text{uncore}} = P_{\text{package}} - P_{\text{core}}$.

#### Isolation Formulation:
$$P_{\text{bus}} = P_{\text{fabric}}(f_{\text{fclk}}) + \sum_{i \in \text{PCIe}} P_{\text{phy}}(\text{Gen}_i, \text{Width}_i, \text{ASPM}_i)$$
- When PCIe devices sleep in D3cold (L1.2 ASPM), bus transceiver loss drops to $< 10\text{ mW}$.
- Active NVMe Gen3 x4 or dGPU links draw $400\text{--}800\text{ mW}$ in PCIe transceivers alone.
- AMD Infinity Fabric draws $0.4\text{ W}$ at 400MHz, scaling to $1.2\text{--}1.5\text{ W}$ at 1333MHz.
