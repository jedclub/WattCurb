# ARCH-051: Architecture of High-Density Matrix Dashboard Profiling, Zero-Copy Ingestion & Delta-Signaling

**Ref-ID**: `REF-ARCH-051`  
**Subsystem**: Precision Analysis Matrix Dashboard / Architecture & Profiling  
**Dependencies**: `REF-REQ-074`, `REF-REQ-036`, `REF-REQ-060`, `REF-ARCH-027`, `REF-ARCH-049`  
**Status**: Approved  

---

## 1. Architectural Overview

The Precision Analysis Matrix Dashboard is re-architected into a pipeline combining Seqlock delta gating, zero-copy process attribution, and fine-grained `ScopedProfiler` telemetry:

```
[1.5s QTimer Tick: onPollTimer()]
               │
               ▼
   [Scoped: dashboard.poll.total]
               │
               ▼
   [Scoped: dashboard.shm.read] ── (read_atomic Seqlock)
               │
      (seq_version changed?)
        ├── NO  ──► [Fast Path: Instant Bypass (0 socket IPC, 0 JSON allocations)]
        └── YES ──► [Slow Path: Active Sample Ingestion]
                         │
                         ▼
             [Scoped: dashboard.daemon.query_ipc]
             (query_daemon: FULL_TELEMETRY)
                         │
                         ▼
             [Scoped: dashboard.json.parse]
             (QJsonDocument::fromJson)
                         │
                         ▼
             [Scoped: dashboard.json.extract_fields]
             (Extract battery, cpu, gpu, cstates, pmu)
                         │
                         ▼
             [Scoped: dashboard.json.processes]
             (Single-pass build + Inlined Power Share Accumulation)
                         │
                         ▼
             [Scoped: dashboard.history.update]
             (Sliding 35-sample FIFO)
                         │
                         ▼
             [Scoped: dashboard.power_shares.total]
             ├── dashboard.power_shares.device (Hardware constituents)
             └── dashboard.power_shares.process (Zero-copy top culprits)
                         │
                         ▼
             [Scoped: dashboard.qml.signal_emit]
             (Delta-guarded emit ...Changed() only on real updates)
```

---

## 2. Technical Specifications

### 2.1 Fine-Grained Profiler Registry Integration

`DashboardBackend` integrates `wattcurb::core::ScopedProfiler` with the following micro-scopes:

| Scope Name | Execution Phase | Target Optimization |
| :--- | :--- | :--- |
| `dashboard.poll.total` | Total `onPollTimer` execution | Full poll gate < 50 µs on warm cache |
| `dashboard.shm.read` | 128B Seqlock atomic read | < 50 ns lock-free access |
| `dashboard.daemon.query_ipc` | Unix domain socket transaction | Bypassed when `seq_version` matches |
| `dashboard.json.parse` | `QJsonDocument::fromJson` | Executed only on new daemon frame |
| `dashboard.json.extract_fields`| Numeric & string field extraction | Inlined double/int conversion |
| `dashboard.json.processes` | Matrix process list creation | Single-pass accumulation |
| `dashboard.history.update` | 35-sample sliding window | Ring-buffer / deque push |
| `dashboard.power_shares.device`| Platform & constituent math | Floating-point sum invariant |
| `dashboard.power_shares.process`| Process attribution decomposition | **0 `.toMap()` deep copies** |
| `dashboard.qml.signal_emit` | Qt QML signal notification | Elided if no state delta |

### 2.2 Zero-Copy Process Attribution & Elimination of `QVariantMap` Detachments

Previously, `update_power_shares()` iterated over `process_list_` and called `item.toMap()`. In Qt, calling `.toMap()` on a `QVariant` creates a detached copy of the entire underlying map:

```cpp
// OLD INEFFICIENT CODE:
for (const auto& item : process_list_) {
    proc_sum += item.toMap().value("totalWatts").toDouble(); // 25 deep map copies!
}
for (int i = 0; i < count; ++i) {
    QVariantMap p = process_list_[i].toMap(); // 7 deep map copies!
}
```

This is replaced with inlined tracking during `queryDaemonTelemetry()`:
```cpp
struct CachedProcessShare {
    int pid{0};
    QString comm;
    double total_watts{0.0};
    QString color;
};

// Maintained with ZERO map copies during JSON array traversal:
std::vector<CachedProcessShare> cached_proc_shares_;
double cached_proc_sum_{0.0};
```

`update_power_shares()` directly iterates over `cached_proc_shares_`, achieving $O(1)$ memory overhead and zero dynamic heap reallocations.

### 2.3 Seqlock Delta-Gated IPC Suppression

Since the WattCurb daemon updates shared memory atomically at a fixed cadence (10s in steady state, 2s in interactive mode), polling at 1.5s frequently lands between updates. Checking `cur.seq_version != prev_seq_version_` eliminates **up to 75% of socket queries and JSON parsing**.
