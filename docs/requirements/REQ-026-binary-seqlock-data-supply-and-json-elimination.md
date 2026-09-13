# [REF-REQ-029] Elimination of JSON in Production & 128-Byte Seqlock POD Binary Data Supply Specification

## 1. Executive Summary & Problem Definition

In prior iterations, WattCurb supported an optional `--json` output flag and internal `render_json` formatting helpers. While textual formatting is acceptable for one-off CLI debugging, emitting JSON from an ultra-low-overhead resident daemon violates the core mission of **sub-milliwatt power efficiency and zero CPU wakeup overhead**:
1. **Instruction & Code Bloat**: JSON serialization (`render_json`) comprised over 8,072 bytes of dense machine code and dozens of string literal constants in `.rodata`.
2. **Dynamic Heap Allocation & String Formatting**: Streaming JSON via `std::ostringstream` or manual string appending introduces heap allocation risk and non-trivial CPU execution active cycles (~0.5ms per report).
3. **Consumer Inefficiency**: A desktop tray icon or monitoring agent receiving JSON must execute a heavy parser (DOM/SAX) on every cycle, waking the CPU and consuming battery.

**Specification Mandate**:
All runtime external data supply to GUI desktop trays, CLI query utilities, or monitoring agents must occur exclusively via an **atomic, zero-copy, 128-byte Seqlock POD binary protocol** over shared memory (`/dev/shm/wattcurb_state.shm`) and Unix Domain Datagram Sockets (`/run/wattcurb.sock`). JSON serialization is entirely expunged from production code.

---

## 2. Technical Requirements

### 2.1 Complete Elimination of JSON Serialization
- **[REQ-29.1]**: `render_json()` and the `--json` CLI argument must be completely purged from `ReportGenerator`, `main.cpp`, and daemon event loops.
- **[REQ-29.2]**: Release builds must contain zero JSON key strings or formatting buffers in `.rodata`.

### 2.2 128-Byte Seqlock POD Shared State (`WattCurbSharedState`)
- **[REQ-29.3] Fixed 128-Byte Dual-Cacheline Footprint**:
  - Exactly 128 bytes (`sizeof(WattCurbSharedState) == 128`).
  - Cache-aligned to 64 bytes (`alignas(64)`).
  - Cacheline 0: Seqlock version counter (`uint64_t seq_version`) and full system electrical telemetry (drain mW, SoC %, AC/Discharging state, remaining minutes, active feature count, CPU/GPU drain, fan RPM, temperatures).
  - Cacheline 1: Top 2 dominant battery drain culprit processes (`SharedCulprit culprits[2]`, each exactly 32 bytes).
- **[REQ-29.4] Strict TriviallyCopyable Invariant**:
  - `WattCurbSharedState` must satisfy `std::is_trivially_copyable_v<WattCurbSharedState>`.
  - Zero heap pointers, zero virtual dispatch, and no non-trivial assignment operators.
  - Allows raw byte transfer via `std::memcpy`, `mmap`, and `sendto`.

### 2.3 Lock-Free Seqlock Concurrency & Atomic Memory Ordering
- **[REQ-29.5] Daemon Writer Semantics**:
  - Before modifying state: `__atomic_store_n(&seq_version, ver + 1, __ATOMIC_RELEASE)`. Odd sequence marks write in progress.
  - After updating fields: `__atomic_store_n(&seq_version, ver + 2, __ATOMIC_RELEASE)`. Even sequence signals stable data.
  - Zero locking overhead: Writer never blocks or context switches.
- **[REQ-29.6] Consumer Reader Semantics (`read_atomic`)**:
  - Non-blocking loop reading `seq_version` via `__ATOMIC_ACQUIRE`.
  - If `seq_version` is odd or changes during `std::memcpy`, retry with exponential backoff (up to 100 iterations).
  - Typical read latency: < 15 nanoseconds.

### 2.4 Dual Communication Channels
- **[REQ-29.7] Passive Zero-Wakeup Polling (`/dev/shm/wattcurb_state.shm`)**:
  - Daemon exposes the 128-byte POD via POSIX shared memory (`mmap`).
  - Tray icon reads directly from memory without issuing any syscall to the daemon. Daemon CPU wakeup: **0 wakeups**.
- **[REQ-29.8] Active Binary Datagram Socket (`/run/wattcurb.sock`)**:
  - IPC control queries directly return the 128-byte raw datagram payload via `sendto` without string formatting.

### 2.5 Exhaustive ELF Metadata Pruning
- **[REQ-29.9] Complete Removal of Non-Essential ELF Sections**:
  - All `.note.gnu.build-id`, `.note.ABI-tag`, `.note.gnu.property`, `.comment`, `.sframe`, `.eh_frame`, and `.eh_frame_hdr` sections must be stripped from production binaries.
  - Target production binary size: **< 210 KB**.

---

## 3. Verification & Oracle Gate Invariants

1. **Static Invariants**:
   - `static_assert(sizeof(WattCurbSharedState) == 128);`
   - `static_assert(alignof(WattCurbSharedState) == 64);`
   - `static_assert(std::is_trivially_copyable_v<WattCurbSharedState>);`
   - `static_assert(sizeof(SharedCulprit) == 32);`
2. **Runtime Concurrency Test (`test_tray_binary_shared_state`)**:
   - Validate odd-sequence detection during write collision.
   - Validate even-sequence atomic copy integrity.
   - Validate round-trip `std::memcpy` datagram deserialization.
