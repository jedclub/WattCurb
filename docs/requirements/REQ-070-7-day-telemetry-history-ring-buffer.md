# [REF-REQ-070] 7-Day Ultra-Compact In-Memory Telemetry History Ring-Buffer Specification

## 1. Executive Summary & Objectives

Under [`REF-REQ-059`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-059-zero-disk-wakeup-logging-and-telemetry-history.md), WattCurb established an in-memory lockless ring buffer (`HistoryRingBufferShm`) sized at 600 slots (representing ~100 minutes of telemetry at 10-second cadence).

To enable weekly trend analysis, long-term battery degradation evaluation, overnight idle drain tracking, and historical hardware comparison, this specification expands the telemetry history retention window from 100 minutes to **7 full calendar days (168 hours)**.

Thanks to the ultra-compact 32-byte layout of `HistoryPoint` ([`REF-REQ-059`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-059-zero-disk-wakeup-logging-and-telemetry-history.md)), a 7-day ring buffer requires only **~1.85 MiB (1,935,424 bytes)** of RAM, preserving zero-disk-wakeup integrity and sub-50ns write latency while expanding temporal coverage by 100.8x.

---

## 2. Technical Specifications

### 2.1 Capacity & Geometry (`REF-REQ-070-1`)
- **Sampling Cadence**: 10.0 seconds (Background Tier 2 probe cadence).
- **Target Retention Window**: 7 days = $7 \times 24 \times 3600 = 604,800$ seconds.
- **Ring Buffer Capacity ($N$)**:
  $$\text{CAPACITY} = \frac{604,800\text{ s}}{10\text{ s/sample}} = 60,480\text{ entries}$$
- **Data Point Size**: Exactly 32 bytes (`HistoryPoint`, TriviallyCopyable POD).
- **Total Shared Memory Size**:
  $$\text{Size} = 64\text{ bytes (Header)} + 60,480 \times 32\text{ bytes} = 1,935,424\text{ bytes} \approx 1.8457\text{ MiB}$$

### 2.2 Shared Memory Lifetime & Version Migration (`REF-REQ-070-2`)
- Location: `/dev/shm/wattcurb_history.shm` (RAM-backed tmpfs).
- Backward Compatibility / Size Guard:
  - On daemon startup, `setup_history_shm()` must inspect the existing file size via `fstat()`.
  - If the existing file size does not equal `sizeof(HistoryRingBufferShm)` (e.g. legacy 19,264-byte file), the daemon must atomically unlink and recreate the backing file to prevent buffer overrun or memory misalignment.

### 2.3 Access Latency & Concurrency Invariants (`REF-REQ-070-3`)
- Single-Writer Multi-Reader (SWMR) 64-bit Seqlock protocol.
- Append latency must remain $< 50\text{ ns/op}$.
- Zero dynamic heap allocation on the hot monitoring path.

---

## 3. Verification & Oracle Gate Standards (`REF-TEST-035`)

1. **Geometry & Memory Footprint Assertion**:
   - `sizeof(HistoryRingBufferShm) == 1935424` bytes.
   - `HistoryRingBufferShm::CAPACITY == 60480`.
2. **Circular Wraparound Fidelity Test**:
   - Sequential insertion of $60,480 + 400 = 60,880$ items must result in:
     - `count == 60,480`
     - `head_index == 400`
     - Oldest readable sample timestamp == 401
     - Newest readable sample timestamp == 60,880
3. **Sequential Read Consistency**:
   - `read_snapshot()` returns samples strictly in monotonically increasing chronological order.
