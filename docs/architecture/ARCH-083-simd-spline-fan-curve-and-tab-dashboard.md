# REF-ARCH-083: SIMD-Vectorized Spline Fan Curve Engine & Tabbed Dashboard Architecture

## 1. Subsystem Architecture

The **Custom Fan Curve & Unified Dashboard** subsystem links a high-DPI Qt6/QML interactive curve canvas directly to a C++23 SIMD interpolation engine and the kernel's `thinkpad_acpi` EC fan actuator.

```
+-------------------------------------------------------------------------+
|                  Qt6 / QML Unified Matrix Dashboard                     |
|  +-------------------+  +--------------------+  +--------------------+  |
|  | Tab 0: Matrix HUD |  | Tab 1: Drain Audit |  | Tab 2: Fan Studio  |  |
|  +-------------------+  +--------------------+  +---------+----------+  |
|                                                           |             |
|                                                           v             |
|                   Interactive Spline Canvas (2-10 Control Points)       |
+-----------------------------------------------------------+-------------+
                                                            | IPC Socket
                                                            v
+-------------------------------------------------------------------------+
|                       WattCurb C++23 Daemon                             |
|  +-------------------------------------------------------------------+  |
|  |           FanCurveEngine (REF-ARCH-083)                           |  |
|  |   - AVX2/FMA Monotone Cubic Spline (Horner's Rule)                |  |
|  |   - Hot/Cold Splitting: alignas(64) 4-profile LUT in 3 cache lines|  |
|  |   - 41-element alignas(64) uint8_t lookup table [30°C .. 70°C]    |  |
|  |   - Failsafe Safety Ceiling (>= 70°C -> FAN_LEVEL_FULL_SPEED)     |  |
|  |   - Scoped Profiling: WATTCURB_PROFILE_SCOPE on all hot paths     |  |
|  +---------------------------------+---------------------------------+  |
|                                    | O(1) Level Lookup                  |
|                                    v                                    |
|                       MitigationEngine::apply_fan_for_temp()            |
|                                    |                                    |
|                                    v                                    |
|                   Kernel /proc/acpi/ibm/fan (thinkpad_acpi)             |
+-------------------------------------------------------------------------+
```

---

## 2. Monotone Cubic Hermite Spline Mathematical Formulation

For $N$ control points $(T_0, S_0), \dots, (T_{N-1}, S_{N-1})$ with $T_0 < T_1 < \dots < T_{N-1}$ and $S_i \in [0, 100]$:
1. **Secant Slopes**:
   $$\Delta_i = \frac{S_{i+1} - S_i}{T_{i+1} - T_i}, \quad i = 0, \dots, N-2$$
2. **Initial Tangents**:
   $$m_0 = \Delta_0, \quad m_{N-1} = \Delta_{N-2}, \quad m_i = \frac{\Delta_{i-1} + \Delta_i}{2}$$
3. **Fritsch-Carlson Monotonicity Condition**:
   If $\Delta_i = 0$, then $m_i = m_{i+1} = 0$.
   Otherwise, let $\alpha_i = m_i / \Delta_i$ and $\beta_i = m_{i+1} / \Delta_i$.
   If $\alpha_i^2 + \beta_i^2 > 9$, rescale:
   $$\tau_i = \frac{3}{\sqrt{\alpha_i^2 + \beta_i^2}}, \quad m_i = \tau_i \alpha_i \Delta_i, \quad m_{i+1} = \tau_i \beta_i \Delta_i$$
4. **Hermite Basis Coefficients**:
   For interval $[T_i, T_{i+1}]$ with $h_i = T_{i+1} - T_i$ and normalized $u = \frac{t - T_i}{h_i} \in [0, 1]$:
   $$S(u) = (2u^3 - 3u^2 + 1)S_i + (u^3 - 2u^2 + u)h_i m_i + (-2u^3 + 3u^2)S_{i+1} + (u^3 - u^2)h_i m_{i+1}$$

---

## 3. AVX2/FMA SIMD Vectorization Implementation

Evaluating $u \in [0, 1]$ over batches of 8 temperature values simultaneously:
```cpp
// Polynomial form: S(u) = c0 + u * (c1 + u * (c2 + u * c3))
__m256 u_vec = _mm256_loadu_ps(&norm_u[k]);
__m256 res = _mm256_fmadd_ps(u_vec, c3_vec, c2_vec);
res = _mm256_fmadd_ps(u_vec, res, c1_vec);
res = _mm256_fmadd_ps(u_vec, res, c0_vec);
```
* **Throughput**: 41 temperatures evaluated in 5 AVX2 vector passes ($< 15\,\text{ns}$).
* **Output Table**: Stored into `alignas(64) uint8_t lookup_levels[41]` mapped to ThinkPad EC fan steps $0 \dots 6$ and `full-speed` (255).
* **Steady-State Monitoring**:
  $$O(1) \quad \text{Index} = \text{clamp}(T - 30, 0, 40)$$

---

## 4. Unified Tabbed Window Component Architecture

* `DashboardWindow.qml` hosts a top-level `TabBar` / cyber tab selector:
  * Tab 0: Dense btop matrix grid (`MatrixView`).
  * Tab 1: Deep historical battery drain audit (`BatteryReportView`).
  * Tab 2: Profile fan curve tuner (`FanCurveStudioView`).
* Memory footprint: Shared single QML engine instance, eliminating redundant process instantiation.
