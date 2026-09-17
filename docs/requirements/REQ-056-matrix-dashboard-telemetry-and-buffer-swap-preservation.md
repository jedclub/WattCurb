# REF-REQ-056: Matrix Dashboard Telemetry Pipeline & Ping-Pong Buffer Swap Data Preservation

## 1. Overview & Problem Statement
Users observed that the WattCurb Detailed Analysis Window (Matrix Dashboard, `wattcurb-dashboard`) showed no process telemetry contents (empty process table, 0 active power-draining processes reported).

### Root Cause Analysis
During internal state investigation:
1. IPC queries (`FULL_TELEMETRY`) against the running daemon returned `"processes": []` on every cycle after startup.
2. In `src/core/daemon_runner.cpp`, process telemetry is captured using a `DoubleBufferedPool`:
   ```cpp
   prev_snapshot = proc_pool_.current();
   cur_snapshot = proc_pool_.next();
   engine_.compute_attribution(..., prev_snapshot.span(), cur_snapshot.span(), 20);
   proc_pool_.swap();
   ```
3. In `src/core/custom_containers.hpp`, `DoubleBufferedPool::swap()` contained a fatal logic bug:
   ```cpp
   void swap() noexcept {
       std::swap(current_, previous_);
       current_->clear(); // BUG: Cleared the newly populated active buffer!
   }
   ```
   Because `current_->clear()` was invoked immediately after swapping pointers, the freshly populated snapshot was instantly wiped out. As a consequence, on every subsequent observation cycle, `prev_snapshot` was empty (size 0).
4. `AttributionEngine::compute_attribution()` performs a two-pointer delta matching (`while (idx1 < sz1 && idx2 < sz2)`). With `sz1 == 0`, the matching loop terminated immediately with 0 matches, resulting in `report.top_processes` being completely empty.
5. In addition, `DashboardBackend::queryDaemonTelemetry()` had a tight 80ms socket polling timeout, risking timeout drops during transient scheduling stalls.

## 2. Functional Requirements

### REQ-056-1: DoubleBufferedPool Data Preservation Invariant
- `DoubleBufferedPool::swap()` must guarantee that after the pointer swap:
  - `current()` holds and retains the newly populated data captured during the current observation cycle.
  - `previous()` (which now points to the old, consumed snapshot) is cleared and reset for future writes.
  - Zero heap allocation or deallocation during the swap operation.

### REQ-056-2: Continuous Multi-Cycle Attribution Telemetry
- Over successive observation cycles (Cycle 1, Cycle 2, ..., Cycle $N$), `AttributionEngine` must continuously receive non-empty baseline snapshots (`prev_snapshot`) to calculate process deltas.
- `report.top_processes` must reliably report active system processes, ranking their power consumption ($W$), CPU attribution, GPU attribution, DRAM attribution, WDI score, and memory PSS ($MB$).

### REQ-056-3: Resilient IPC Polling Window
- `DashboardBackend::queryDaemonTelemetry()` must provide an adequate polling timeout window (minimum 300ms) to ensure guaranteed IPC delivery across varying system load and scheduling states.

## 3. Verification & Oracle Gate Standards
- A dedicated unit test (`REF-TEST-021` in `tests/test_units.cpp`) must verify `DoubleBufferedPool` behavior:
  - Writing to `next()` and calling `swap()` must leave `current()` intact with the expected elements.
  - `previous()` must be empty following `swap()`.
- End-to-end verification via IPC datagram query must confirm continuous process lists across multiple observation cycles.
