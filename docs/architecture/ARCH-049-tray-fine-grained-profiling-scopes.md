# ARCH-049: Architecture of Fine-Grained Profiling Scopes in Desktop Tray Client

**Ref-ID**: `REF-ARCH-049`  
**Subsystem**: Desktop Tray Client / Profiling Architecture  
**Dependencies**: `REF-REQ-072`, `REF-REQ-014`, `REF-ARCH-025`  
**Status**: Approved  

---

## 1. Architectural Overview

The StatusNotifierItem (SNI) tray client (`wattcurb-tray`) operates on an asynchronous D-Bus event loop backed by `sd-bus`. When a user moves their mouse cursor over the tray icon in KDE Plasma or Wayland bar, the compositor issues a synchronous D-Bus property query for `ToolTip`.

To diagnose and isolate hot paths, the tooltip pipeline is decomposed into hierarchical micro-benchmarking scopes:

```
[KDE Plasma / Compositor Hover Event]
               │
               ▼
   [tray.property_get_tooltip] 
        ├── [tray.read_state] (128B Seqlock SHM read)
        │
        ├── [tray.probe_sensors.total]
        │    ├── [tray.probe_sensors.bat_uevent] (BAT0/BAT1 sysfs read & parse)
        │    ├── [tray.probe_sensors.thermal]    (thermal_zone0 read & parse)
        │    └── [tray.probe_sensors.cpufreq]    (scaling_cur_freq read & parse)
        │
        ├── [tray.resolve_icon] (Profile & charge quantization)
        │
        ├── [tray.tooltip.render_total]
        │    ├── [tray.tooltip.build_bars]       (3x 8-block Unicode progress bars)
        │    ├── [tray.tooltip.snprintf_hud]     (HTML Cyber HUD string formatting)
        │    └── [tray.tooltip.sanitize_utf8]    (Multi-byte UTF-8 boundary scan)
        │
        └── [tray.property_get_tooltip.dbus_pack] (sd-bus container serialization)
```

---

## 2. Scope Instrumentation Map

| Scope Identifier | Target Method / Function | Purpose / Measured Operation | Target Latency |
| :--- | :--- | :--- | :--- |
| `tray.read_state` | `TrayClient::read_state` | Seqlock memory barrier & 128B struct copy | < 50 ns |
| `tray.probe_sensors.bat_uevent` | `probe_live_sensors_on_hover` | `fs::read_small_file` on BAT0/BAT1 uevent & SIMD parse | < 5.0 µs |
| `tray.probe_sensors.thermal` | `probe_live_sensors_on_hover` | `fs::read_small_file` on thermal_zone0 | < 2.0 µs |
| `tray.probe_sensors.cpufreq` | `probe_live_sensors_on_hover` | `fs::read_small_file` on scaling_cur_freq | < 2.0 µs |
| `tray.resolve_icon` | `TrayClient::resolve_icon_name` | Icon naming calculation & battery rounding | < 100 ns |
| `tray.tooltip.build_bars` | `TrayClient::render_tooltip` | Generation of 3x UTF-8 block bar strings | < 500 ns |
| `tray.tooltip.snprintf_hud` | `TrayClient::render_tooltip` | Large HTML table string generation | < 3.0 µs |
| `tray.tooltip.sanitize_utf8` | `TrayClient::render_tooltip` | Reverse lookback byte validation | < 50 ns |
| `tray.property_get_tooltip.dbus_pack` | `TrayClient::property_get_tooltip` | `sd_bus_message_open_container` & string packing | < 2.5 µs |
| `tray.menu.get_layout` | `TrayClient::dbusmenu_method_get_layout` | Complete menu tree query | < 8.0 µs |
| `tray.menu.build_nodes` | `append_menu_node` | DBusMenu node variant serialization | < 5.0 µs |
| `tray.loop.poll_shm` | `TrayClient::run` | 1s periodic Seqlock diff detection | < 100 ns |
| `tray.loop.emit_signals` | `TrayClient::run` | D-Bus signal emission (NewIcon, NewToolTip) | < 10.0 µs |

---

## 3. Interactive Signal & CLI Telemetry

The tray client executable registers a `SIGUSR1` signal handler.
When triggered (`kill -USR1 $(pidof wattcurb-tray)`), it prints the formatted `ScopedProfilerRegistry` breakdown table to `stdout`.
If `--profile` is passed at startup, it also logs cumulative profiling summaries every 10 seconds or after every 50 user hover interactions.

```bash
wattcurb-tray --profile
```

---

## 4. Empirical Evaluation Protocol (`REF-TEST-037`)

An automated evaluation function in `tests/test_units.cpp` executes:
- 5,000 iterations of live hover tooltip generation.
- 5,000 iterations of DBusMenu layout generation.
- 20,000 iterations of Seqlock SHM atomic reads.

It asserts that total hover latency remains below **50 µs** and outputs the complete hotspot rank table.
