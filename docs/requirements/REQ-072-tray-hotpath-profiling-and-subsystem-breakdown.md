# REQ-072: Detailed Fine-Grained Profiling Scopes and Hot-Path Identification for Desktop Tray Client

**Ref-ID**: `REF-REQ-072`  
**Subsystem**: Desktop Tray Client / Scoped Profiler / IPC Telemetry  
**Priority**: High  
**Status**: Approved  

---

## 1. Context & Motivation

WattCurb provides a standalone unprivileged desktop StatusNotifierItem (SNI) tray client (`wattcurb-tray`) that displays live battery telemetry, power drain attribution, and dynamic Cyber HUD tooltips on KDE Plasma, GNOME, and Wayland environments.

While the tray client adheres to a zero-heap, event-driven model sleeping in `sd_bus_wait()`, user interactions—specifically **mouse hovering over the tray icon** (`property_get_tooltip`) and **menu layout querying** (`dbusmenu_method_get_layout`)—trigger instantaneous live hardware sensor probing, Unicode progress bar synthesis, and HTML Cyber HUD formatting.

To uncover micro-architectural bottlenecks, syscall stalls, and hot-path execution costs with sub-microsecond precision, fine-grained `WATTCURB_PROFILE_SCOPE` instrumentation must be integrated across all critical execution stages of the desktop tray client.

---

## 2. Functional Requirements

1. **Sub-Phase Scope Granularity (`REF-REQ-072.1`)**:
   Every discrete computational step in the tray client hot path must be wrapped in a dedicated, named profiling scope:
   - `tray.read_state`: Seqlock atomic read from shared memory (/dev/shm).
   - `tray.probe_sensors.bat_uevent`: Battery uevent sysfs read and parse.
   - `tray.probe_sensors.thermal`: CPU thermal zone sysfs read.
   - `tray.probe_sensors.cpufreq`: CPU scaling frequency sysfs read.
   - `tray.resolve_icon`: Battery icon quantization and naming.
   - `tray.tooltip.render_total`: Full tooltip rendering workflow.
   - `tray.tooltip.build_bars`: Unicode block progress bar synthesis (BAT, CPU, GPU).
   - `tray.tooltip.snprintf_hud`: HTML Cyber HUD string formatting.
   - `tray.tooltip.sanitize_utf8`: Multi-byte UTF-8 boundary validation.
   - `tray.property_get_tooltip.dbus_pack`: D-Bus container serialization `(sa(iiay)ss)`.
   - `tray.menu.get_layout`: Full context menu layout query.
   - `tray.menu.build_nodes`: D-Bus Menu node population.
   - `tray.loop.poll_shm`: Event loop periodic SHM poll & diff check.
   - `tray.loop.emit_signals`: D-Bus property change signal emission.

2. **Runtime & Interactive Profile Inspection (`REF-REQ-072.2`)**:
   - The tray client must support a `--profile` CLI flag and `SIGUSR1` signal handler to print a formatted profiling table to `stdout` at any point during desktop operation.
   - In automated testing, an empirical audit suite (`REF-TEST-037`) must simulate 5,000 hover cycles and 10,000 SHM reads to report exact per-operation latencies, cycle counts, and percentage shares.

3. **Zero Overhead in Production (`REF-REQ-072.3`)**:
   - In production release builds (`-DNDEBUG`), all `WATTCURB_PROFILE_SCOPE` macros must strictly collapse to `((void)0)`, leaving zero instruction or binary footprint.

---

## 3. Verification Criteria

- [ ] All 14 target profiling scopes are properly registered in `ScopedProfilerRegistry`.
- [ ] Automated evaluation (`REF-TEST-037`) produces a clear execution breakdown table showing exact nanosecond / microsecond execution costs for each tray sub-phase.
- [ ] No heap allocations (`malloc`, `new`, `std::string`) are introduced within any scoped path.
- [ ] All unit tests pass and compile cleanly.
