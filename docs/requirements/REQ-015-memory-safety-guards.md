# [REF-REQ-018] Memory Safety Guards, Bounds Protection & Capacity Headroom Specification

## 1. Overview & Operational Context
- **Ref-ID**: `REF-REQ-018`
- **Module**: `core::containers`, `proc::ProcessAnalyzer`, `policy::AttributionEngine`
- **Related Requirements**: [`REF-REQ-017`](../requirements/REQ-014-custom-containers.md), [`REF-REQ-002`](../requirements/REQ-001-hardware-power-profiling.md#ref-req-002)
- **Status**: Approved

While extreme zero-allocation optimization (introduced in `REF-REQ-017`) eliminates dynamic heap churn and halves L1 cache misses, fixed-capacity contiguous containers must be fortified against memory corruption, buffer overflows, and capacity exhaustion under extreme multi-tenant or containerized server workloads.

---

## 2. Functional & Safety Requirements

### 2.1. Generous Static Memory Pool Capacity Headroom
1. **Host Process Capacity Expansion**:
   - The default `ProcessSnapshot` capacity must expand from **1024 to 2048 entries** (`FixedVector<ProcessSample, 2048>`), providing > 400% headroom above normal desktop/laptop process counts (~450 PIDs).
   - Accommodates burst workloads (heavy compilation, multi-tab browsers, container/Kubernetes node environments) without dropping process telemetries.
2. **Kernel Thread (`kthread`) Cache Expansion**:
   - `kthread_pids_` capacity must expand from **256 to 512 entries** (`FixedVector<int32_t, 512>`), accommodating heavily threaded enterprise kernels.
3. **Attribution Engine Internal Capacity**:
   - All intermediate vectors (`IntermediateProc`, `attributed`, `accumulated_procs`, `next_accum`, `proc_zero`, `proc_delta`) must support **2048 entries**.
4. **Report Registry Capacity**:
   - `AnalysisReportData::top_processes` capacity expanded from **32 to 64 entries**.
   - `AnalysisReportData::domain_culprits` expanded from **8 to 16 entries**.
   - `DomainCulprit::top_culprits` expanded from **5 to 8 entries**.

### 2.2. Bounds Protection & Saturating Clamping
1. **Safe Bound Accessors**:
   - `FixedVector::at(size_type idx)`: Throws no exceptions, but safely asserts in debug mode and returns a clamped valid reference (`data()[min(idx, size_ - 1)]`) in production to prevent out-of-bounds pointer dereference.
   - `FixedVector::front()` and `FixedVector::back()`: Must check `empty()` invariant. If empty, return a safe dummy static instance instead of executing undefined memory access on `data()[-1]`.
2. **Saturating Overflow Detection & Drop Telemetry**:
   - `FixedVector::push_back` and `FixedVector::emplace_back` must track `overflow_count()`.
   - When capacity is reached, insertion is rejected gracefully without memory corruption, and `overflow_occurred()` flag is latched for diagnostic visibility.

### 2.3. Memory Integrity Canary Guards
1. **End-of-Buffer Integrity Canary**:
   - Containers must feature an end-of-storage 64-bit Canary Magic word (`0xDEADBEEFCAFE0001ULL`).
   - The method `bool check_integrity() const noexcept` verifies that no buffer overrun has corrupted adjacent memory.
2. **Null-Termination Invariant**:
   - `FixedString<Capacity>` must enforce null-termination (`\0`) under all truncation, assignment, and append edge cases.

---

## 3. Verification & Oracle Gate Standards (`REF-TEST-007`)
1. **Buffer Overflow Stress Test**:
   - Attempt to push 3000 elements into `FixedVector<int, 2048>`.
   - Verify that exactly 2048 elements are stored, `overflow_occurred()` returns `true`, and canary check passes.
2. **Empty Container Front/Back Test**:
   - Verify that calling `front()` or `back()` on an empty container does not fault or produce undefined behavior.
3. **PMU Zero-Degradation Guard**:
   - Adding bounds guards and canary checks must not increase steady-state task-clock beyond the 0.05% threshold.
