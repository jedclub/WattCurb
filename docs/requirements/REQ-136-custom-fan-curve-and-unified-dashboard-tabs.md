# REF-REQ-136: Unified Tabbed Dashboard & SIMD-Vectorized Profile Fan Curve Studio

## 1. Operational Need & Motivation

WattCurb currently provides two separate desktop GUI windows:
1. `wattcurb-dashboard` (Real-time btop-style power and hardware domain matrix)
2. `wattcurb-report` (`BatteryReportWindow`, deep battery drain audit and profile comparison)

Managing two separate top-level windows increases window manager clutter, doubles GPU surface allocations under Wayland, and disrupts continuous performance monitoring. Furthermore, users require **fine-grained manual control over the cooling fan curve per power profile** (`Performance`, `Balanced`, `PowerSaver`, `UltraEndurance`) to balance acoustic noise against thermal headroom, with an interactive visual curve.

---

## 2. Functional Requirements

### REF-REQ-136.1: Unified Tabbed Dashboard Architecture
* `wattcurb-dashboard` must unify all diagnostic, auditing, and control views into a single cohesive window managed by an integrated tab switcher:
  * **Tab 0: 📊 Power Matrix Dashboard** (Real-time CPU/GPU/Memory/NVMe btop telemetry)
  * **Tab 1: 📑 Battery Drain Audit Report** (Deep historical drain analysis & profile comparison)
  * **Tab 2: 🌀 Fan Curve Studio** (Interactive profile-specific fan curve tuner)
* CLI `--report` execution must seamlessly open the unified dashboard window with **Tab 1** pre-selected.

### REF-REQ-136.2: Per-Profile Manual Fan Curve Domain & Boundaries
* The fan curve tuner must maintain independent, isolated fan curves for each of the 4 power profiles:
  * Profile 0: `Performance`
  * Profile 1: `Balanced`
  * Profile 2: `PowerSaver`
  * Profile 3: `UltraEndurance`
* **Temperature Domain Limits**:
  * Minimum start temperature: **$30^\circ\text{C}$** ($T_{\min} = 30$)
  * Maximum anchor temperature: **$70^\circ\text{C}$** ($T_{\max} = 70$)
* **Control Point Constraints**:
  * Minimum 2 control points, **maximum 5 control points** ($2 \le N \le 5$).
  * Each control point $P_i = (T_i, S_i)$ where $T_i \in [30^\circ\text{C}, 70^\circ\text{C}]$ and fan speed $S_i \in [0\%, 100\%]$.
  * Points must be strictly ordered by temperature: $T_0 < T_1 < \dots < T_{N-1}$.

### REF-REQ-136.3: Failsafe Thermal Ceiling & Hardware Protection
* **Hard Hardware Safety Invariant**:
  * Regardless of user-configured curves, whenever CPU package temperature reaches or exceeds **$70^\circ\text{C}$**, the fan must be commanded to **$100\%$ full speed (`FAN_LEVEL_FULL_SPEED`)** without exception.
  * For $T < 30^\circ\text{C}$, the fan speed is clamped to the first control point $S_0$ (or 0 RPM for `UltraEndurance`).

### REF-REQ-136.4: Monotone Cubic Hermite Spline with SIMD Vectorization
* To prevent unnatural oscillations or speed dips (where higher temperatures produce lower fan speeds), curve interpolation must employ a **Monotone Cubic Hermite Spline (Fritsch-Carlson)** algorithm.
* **SIMD Vectorization Mandate**:
  * The evaluation of the 41-element integer lookup table ($T \in [30^\circ\text{C}, 70^\circ\text{C}]$) must be vectorized using 8-wide AVX2 / FMA (or 4-wide SSE) SIMD intrinsics.
  * Table generation must complete in $< 100\,\text{ns}$ with zero heap allocation.
  * During monitoring cycles, daemon fan speed lookup must be strictly **$O(1)$** via fixed stack/cache array indexing.

---

## 3. Verification Criteria & Ref-IDs

* Requirement: `REF-REQ-136`
* Architecture: `REF-ARCH-083`
* Test Specification: `REF-TEST-090` (Monotone spline correctness, SIMD lookup equivalence, boundary clamping, 5-point constraint, 70°C failsafe).
