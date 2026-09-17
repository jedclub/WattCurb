# REF-ARCH-032: Ping-Pong Double-Buffered Delta Attribution & QML Telemetry Sync

## 1. Architectural Context
WattCurb employs zero-allocation ring and ping-pong buffers (`DoubleBufferedPool<T, Capacity>`) in `src/core/custom_containers.hpp` to ensure that periodic procfs/sysfs snapshots generate zero dynamic allocations or TLB invalidations in the steady-state monitoring path.

```
       Cycle N:
       [ next() buffer ] <--- Populated with fresh ProcessSample list
              |
         swap() call
              |
              v
       std::swap(current_, previous_);
       previous_->clear();  // <--- Cleans old consumed buffer, keeps current_ intact!
              |
              v
       Cycle N+1:
       prev_snapshot = proc_pool_.current();  // Size > 0, Valid baseline!
       cur_snapshot  = proc_pool_.next();     // Newly captured snapshot
              |
              v
       AttributionEngine::compute_attribution(prev_snapshot, cur_snapshot)
              |
              v
       [ Sorted Top Processes ] ---> IPC FULL_TELEMETRY ---> QML Matrix Dashboard
```

## 2. Buffer State Transition Table

| Phase | `current_` (Active Snapshot) | `previous_` (Working Buffer) |
|---|---|---|
| Initial (Cycle 0) | Empty (size = 0) | Empty (size = 0) |
| Cycle 1 Sampling | Empty | Populated via `next()` (e.g. 150 procs) |
| Cycle 1 Post-Swap | Points to Cycle 1 data (150 procs) | Old buffer, cleared to 0 |
| Cycle 2 Sampling | Retains Cycle 1 data (Baseline) | Populated via `next()` (155 procs) |
| Cycle 2 Delta Match | `current_.span()` (150 procs) | `next().span()` (155 procs) |
| Cycle 2 Post-Swap | Points to Cycle 2 data (155 procs) | Old Cycle 1 buffer, cleared to 0 |

## 3. Telemetry IPC Pipeline
1. **Daemon Side (`daemon_runner.cpp`)**:
   - Stores sorted `top_processes` in `AttributionReport`.
   - On IPC `FULL_TELEMETRY` request, formats top processes into JSON array including `pid`, `comm`, `total_w`, `cpu_w`, `gpu_w`, `wdi_score`, `pss_mb`, `tier`, `wakeups_sec`, etc.
2. **Dashboard Backend Side (`dashboard_backend.cpp`)**:
   - `DashboardBackend::queryDaemonTelemetry()` queries `@wattcurb.lock` abstract Unix datagram socket.
   - Timeout set to 300ms.
   - Deserializes JSON `processes` array and maps to `QVariantMap` rows.
   - Emits `processListChanged()` notifying QML `ListView`.
3. **GUI Side (`DashboardWindow.qml`)**:
   - Renders live process matrix table with dynamic resource attribution and severity coloring.
