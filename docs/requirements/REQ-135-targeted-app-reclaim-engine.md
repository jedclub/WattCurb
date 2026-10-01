# REF-REQ-135: Targeted Memory Pressure Smart GC & Application Cgroup Reclaim

## 1. Overview & Operational Need

Modern interactive desktop workloads frequently execute web-engine and Electron-based applications (e.g. ChatGPT / Codex Desktop, Chromium, Slack, Discord, IDEs) for tens or hundreds of consecutive hours. Over extended runtimes, these environments suffer chronic memory bloat:
1. **Accumulated V8 Heap & Detached DOM Trees**: Un-garbage-collected JavaScript closures, terminal streaming buffers, and Monaco diff elements inflate resident and swap allocations beyond 1–3 GiB.
2. **Absence of Autonomous In-App Purges**: Without external memory pressure notifications, V8 and Blink retain decoded image buffers, font rasterizations, and compiled code caches indefinitely.
3. **The Oscillating Reclaim Anti-Pattern**: Naive continuous background memory reclaim or frequent signals introduce CPU churn, thread stalls, and UI frame drops (jank).

To permanently resolve this while respecting the Zero-Kill (`REF-REQ-044`) and Zero-Wakeup (`REF-REQ-012`) guarantees, WattCurb introduces the **Targeted App Reclaim & Smart GC Engine**.

---

## 2. Functional Requirements

### REF-REQ-135.1: Pressure-Gated Trigger (Zero Overhead at Idle)
* The targeted reclaim engine must **never** execute periodic wakeups or polling loops.
* Actuation is evaluated strictly when the system is under genuine memory pressure:
  * `MemoryPressureTier != Normal` (`Advisory` or `Throttle`), OR
  * System PSI `memory.pressure` full avg10 exceeds 5.0%, OR
  * Free swap drops below 35.0% (when swap is configured), OR
  * Available physical RAM drops below 20.0%.
* When memory is healthy, evaluation evaluates to an early `return 0` in $< 10\,\text{ns}$ with zero heap allocations.

### REF-REQ-135.2: Strict 20-Minute Cooldown Guard (Anti-Churn Lock)
* After any actuation pass, the engine enters a mandatory, immutable **20-minute (1,200 seconds)** cooldown:
  $$\Delta t_{\text{cooldown}} = 1200\,\text{s}$$
* Subsequent memory pressure events within this 1,200-second window must be rejected immediately without dispatching D-Bus signals or issuing cgroup writes.
* This guarantees that runaway loops, oscillating GC stalls, and disk/ZRAM write-storms are mathematically impossible.

### REF-REQ-135.3: Non-Destructive Two-Stage Actuation Protocol
When the pressure gate and 20-minute cooldown conditions are simultaneously satisfied, the engine executes a discrete, non-destructive two-stage actuation:
1. **Stage 1 (Cooperative In-App GC via D-Bus)**:
   * Dispatches a single `LowMemoryWarning(LEVEL_MODERATE = 100)` broadcast across the system D-Bus conforming to `org.freedesktop.LowMemoryMonitor`.
   * Instructs participating Chromium, Electron, Firefox, and WebKitGTK runtimes to execute V8 Major GC sweeps and discard decoded asset caches.
2. **Stage 2 (Bounded Kernel Cgroup Reclaim)**:
   * Identifies up to two top bloat candidate processes with $\text{PSS} \ge 256\,\text{MiB}$.
   * Writes a bounded target of $256\,\text{MiB}$ to each candidate's cgroup `memory.reclaim` interface:
     $$\text{Target} = 256 \times 1024 \times 1024\,\text{bytes} \quad (\text{swappiness default})$$
   * Immediately reclaims the unreferenced pages and inactive caches just released by Stage 1 into the OS available memory pool.

### REF-REQ-135.4: Process Immunity & Safety
* Critical desktop components (`ProcessSafetyTier::CriticalImmune`, `DesktopCore`, `DesktopShell`) are strictly exempt from cgroup reclaim.
* Active focused windows and media/audio producing processes are protected from destructive throttling; cooperative D-Bus GC remains safe and non-blocking for all desktop clients.

---

## 3. Verification Criteria & Ref-IDs

* Requirement: `REF-REQ-135`
* Architecture: `REF-ARCH-082`
* Unit & Regression Tests: `REF-TEST-089` (validates pressure gate, 20-minute cooldown invariance, candidate filtering, and bounded actuation).
