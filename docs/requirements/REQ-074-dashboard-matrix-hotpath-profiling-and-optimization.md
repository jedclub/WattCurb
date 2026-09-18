# REQ-074: Fine-Grained Profiling & Extreme Optimization for Precision Analysis Matrix Dashboard

**Ref-ID**: `REF-REQ-074`  
**Subsystem**: Precision Analysis Matrix Dashboard / GUI Backend / Profiling & Ingestion  
**Priority**: High  
**Status**: Approved  

---

## 1. Context & Motivation

The WattCurb Precision Analysis Matrix Dashboard (`wattcurb-dashboard`) provides an ultra-dense, btop-style real-time hardware telemetry and process attribution matrix for KDE Plasma 6 Wayland desktop environments.

While the desktop StatusNotifierItem (SNI) tray client (`wattcurb-tray`) has been optimized down to sub-microsecond latency (`REF-REQ-073`, `REF-ARCH-050`), the Matrix Dashboard backend (`DashboardBackend`) exhibits multiple latent hotspots:

1. **Unconditional 1.5-Second Unix Socket Querying**:
   - Every 1,500ms, `onPollTimer()` issues a `FULL_TELEMETRY\n` command over Unix domain sockets to the root daemon, transmitting and receiving ~10 KB of text payload even when the daemon has not generated a new telemetry frame (`seq_version` unchanged).
2. **Heavy JSON Deserialization & Allocation Overhead**:
   - `QJsonDocument::fromJson` parses the 10 KB string, creating dozens of intermediate tree nodes.
   - Iterating over 25 processes creates 25 `QVariantMap` objects, each containing 30 string keys ($25 \times 30 = 750$ string allocations and map insertions per 1.5s cycle).
3. **Map Detachment & Deep Copy Storm in Power Share Decomposition**:
   - In `update_power_shares()`, looping over `process_list_` and calling `.toMap()` causes Qt to detach and deep-copy every 30-entry `QVariantMap` twice ($25 + 7 = 32$ detached map copies per poll tick).
4. **Unconditional QML Signal Emissions**:
   - `telemetryChanged()`, `processListChanged()`, `historyChanged()`, and `powerSharesChanged()` are emitted unconditionally every 1.5s, forcing QML delegates, Canvas power charts, and ListView layouts to re-evaluate and trigger GPU re-renders even when data is 100% invariant.

To achieve WattCurb's Zero-Wakeup and sub-milliwatt desktop standards, the Matrix Dashboard must be rigorously profiled with microsecond `ScopedProfiler` scopes and subjected to radical zero-copy optimization.

---

## 2. Functional & Technical Requirements

1. **Full-Scope Microsecond Profiler Instrumentation (`REF-REQ-074.1`)**:
   - Instrument granular `WATTCURB_PROFILE_SCOPE` markers across all execution phases in `DashboardBackend`:
     - `dashboard.poll.total`: Overall poll tick duration.
     - `dashboard.shm.read`: Atomic Seqlock shared memory ingestion.
     - `dashboard.daemon.query_ipc`: Socket communication latency.
     - `dashboard.json.parse`: JSON document parsing.
     - `dashboard.json.extract_fields`: Hardware metric extraction.
     - `dashboard.json.processes`: Process matrix construction.
     - `dashboard.history.update`: 35-sample sliding window maintenance.
     - `dashboard.power_shares.device`: Hardware constituent share calculation.
     - `dashboard.power_shares.process`: Process power share decomposition.
     - `dashboard.qml.signal_emit`: Signal emission and Qt event dispatch.

2. **Seqlock Delta-Driven IPC Ingestion (`REF-REQ-074.2`)**:
   - Before executing `queryDaemonTelemetry()`, inspect `shm_state_->read_atomic(cur)`.
   - If `cur.seq_version == prev_seq_version_`, skip the Unix socket IPC query and JSON deserialization completely, reusing the cached matrix state with 0 syscalls and 0 memory allocations.

3. **Zero-Copy Process Attribution & Power Share Math (`REF-REQ-074.3`)**:
   - Eliminate all `.toMap()` detached copies in `update_power_shares()`.
   - Accumulate process power sums directly during the single ingestion pass and store top culprits in lightweight C++ structs.

4. **Delta-Guarded Signal Emissions (`REF-REQ-074.4`)**:
   - Guard `emit ...Changed()` signals behind value inequality checks to eliminate unnecessary QML binding re-evaluations and Wayland compositor redraws.

---

## 3. Verification & Oracle Gate Standards (`REF-TEST-039`)

- Complete poll cycle latency with cache hit must drop to **< 5 µs/op**.
- Uncached full telemetry ingestion latency must not exceed **< 1.0 ms/op**.
- Deep map copy count during power share calculations must be strictly **0**.
- Granular breakdown table must be fully populated and verified in the automated test harness.
