# REF-REQ-051: AMD APU Package Power Tracking (PPT) Disambiguation & Physical Duty-Cycle GPU Power Attribution

## 1. Overview & Problem Definition

In prior versions of WattCurb, physical power attribution on AMD APU systems suffered from two major mathematical and sensor-level flaws:
1. **AMD APU Package Power Tracking (PPT) Misclassification**:
   - On AMD APUs (e.g. Renoir Vega Series), the kernel DRM driver exposes total socket power (CPU Cores + iGPU + SoC) via `/sys/class/drm/card*/device/hwmon/hwmon*/power1_input` with `power1_label == "PPT"`.
   - The daemon treated this total socket power (typically 15W ~ 27W) as standalone discrete GPU power (`dGPU`).
2. **Proportional Duty-Cycle Attribution Failure ("Profiler Paradox / Heisenbug")**:
   - The engine computed per-process GPU wattage via relative share:
     $$P_{\text{proc\_gpu}} = P_{\text{gpu\_raw}} \times \left(\frac{\Delta t_{\text{proc\_gpu}}}{\sum \Delta t_{\text{gpu}}}\right)$$
   - When a lightweight GUI process (such as `wattcurb-dashboard`) rendered 1 frame using $2.47\,\text{ms}$ of GPU time in a 1-second interval, and other background tasks used $0\,\text{ms}$, the relative share was $1.0$ ($100\%$).
   - As a result, the GUI was blamed for $20.00\,\text{W}$ of GPU power—a **400x over-attribution error** that falsely categorized the energy profiler itself as a Tier 5 Runaway!

---

## 2. Functional Requirements

### 2.1 Hardware Sensor Disambiguation (`REF-REQ-051-A`)
1. The hardware probe must check `power1_label` when discovering GPU power sensors.
2. If `power1_label == "PPT"` or the device is an AMD APU iGPU, flag `gpu_is_apu_ppt = true`.
3. In APU PPT mode, decouple iGPU power from raw PPT power:
   - When `gpu_busy_percent == 0`, iGPU dynamic power is $0.0\,\text{W}$, and idle baseline leakage is clamped to $\le 0.05\,\text{W}$.
   - When active, $P_{\text{igpu}} = \min(P_{\text{ppt}} \times 0.70, P_{\text{ppt}} \times (\text{gpu\_busy\_percent} / 100.0) + 0.15\,\text{W})$.
   - If unprivileged (no RAPL permission), attribute remaining $P_{\text{ppt}} - P_{\text{igpu}}$ to CPU Package and SoC.

### 2.2 Physical Duty-Cycle Scaling (`REF-REQ-051-B`)
1. Per-process GPU attribution must be bounded by both relative share and absolute physical duty cycle:
   $$\text{duty\_cycle}_i = \min\left(1.0, \frac{\Delta t_{\text{gpu}, i}}{\Delta t_{\text{window}}}\right)$$
   $$P_{\text{max\_duty}} = P_{\text{gpu\_hw}} \times \text{duty\_cycle}_i$$
   $$P_{\text{proc\_gpu}} = \min(P_{\text{gpu\_dyn}} \times \text{share}_i, \ P_{\text{max\_duty}})$$
2. A process using $2.47\,\text{ms}$ of GPU in a $1\,\text{s}$ interval at $5\%$ GPU busy load can consume at most:
   $$20\,\text{W} \times 0.00247 = 0.0494\,\text{W} \ (49.4\,\text{mW})$$
   completely eliminating the 400x false attribution spike.

### 2.3 Daemon & Tool Self-Immunity (`REF-REQ-051-C`)
1. Process names starting with `wattcurb` (`wattcurb`, `wattcurb-tray`, `wattcurb-dashboard`, `wattcurb-dashbo`) are classified as **Tier 0 Critical Immune**.
2. Immune processes must never be flagged as `is_runaway_candidate` or subjected to scheduler throttling or cgroup freezing.

---

## 3. Verification & Oracle Gate

- Unit test: `test_apu_ppt_and_gpu_duty_cycle_attribution` in `tests/test_units.cpp`.
- Verifies that $2.47\,\text{ms}$ GPU usage in an idle 20W APU attributes $< 0.01\,\text{W}$ and retains Tier 0 immunity without runaway flags.
