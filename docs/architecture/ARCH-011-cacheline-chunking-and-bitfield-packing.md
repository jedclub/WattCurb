# REF-ARCH-011: 64-Byte Hot/Cold Cacheline Alignment, Bit-Level Compaction & Sequence Probe

## 1. Subsystem Overview

To achieve sub-milliwatt daemon operation and eliminate CPU pipeline stalls caused by memory latency, WattCurb adopts an empirical cache-conscious architecture. By coupling **Sequence-Aware Hot/Cold Chunking** with **Bit-Level Field Packing**, all high-frequency process telemetry operations are guaranteed to fit within a single 64-byte hardware cache line (`alignas(64)`), ensuring $100\%$ L1 Data Cache (L1D) locality.

---

## 2. Structural Data Layout Specifications

### 2.1 `ProcessHotChunk` (Exact 64 Bytes, `alignas(64)`)

```cpp
struct alignas(64) ProcessHotChunk {
    int32_t pid{0};                     // Bytes 0..3
    int32_t ppid{0};                    // Bytes 4..7
    uint64_t utime_ticks{0};            // Bytes 8..15
    uint64_t stime_ticks{0};            // Bytes 16..23
    uint64_t voluntary_ctxt_switches{0};   // Bytes 24..31
    uint64_t nonvoluntary_ctxt_switches{0}; // Bytes 32..39
    uint32_t rss_kib{0};                // Bytes 40..43
    uint32_t pss_kib{0};                // Bytes 44..47
    uint32_t minflt{0};                 // Bytes 48..51
    uint32_t majflt{0};                 // Bytes 52..55

    // Bit-Packed Metadata Word (Bytes 56..63: exactly 8 bytes / 64 bits)
    int64_t cpu_core : 10 {-1};
    uint64_t num_threads : 16 {1};
    int64_t nice : 6 {0};
    int64_t priority : 8 {0};
    uint64_t open_sockets : 12 {0};
    uint64_t has_io_perm : 1 {1};
    uint64_t is_kthread : 1 {0};
    uint64_t cross_ccx_migrated : 1 {0};
    uint64_t reserved_flags : 9 {0};
};
static_assert(sizeof(ProcessHotChunk) == 64);
static_assert(alignof(ProcessHotChunk) == 64);
```

### 2.2 `CompactProcessHot` (Exact 32 Bytes, 2 Processes Per Cache Line)

For ultra-high-density stream evaluation where memory footprint must be halved:

```cpp
struct CompactProcessHot {
    uint32_t pid : 22 {0};
    uint32_t is_kthread : 1 {0};
    uint32_t has_io_perm : 1 {1};
    uint32_t reserved_bits : 8 {0};

    uint32_t delta_cpu_ticks{0};
    uint32_t delta_wakeups{0};
    uint32_t rss_kib{0};
    uint32_t pss_kib{0};
    uint32_t minflt{0};
    uint16_t majflt{0};

    int16_t cpu_core{-1};
    uint16_t num_threads{1};
    int8_t nice{0};
    int8_t priority{0};
};
static_assert(sizeof(CompactProcessHot) == 32);
```

### 2.3 `ProcessSample` (192 Bytes, Exactly 3 Cache Lines)
- **Line 0 (Bytes 0..63)**: 100% Hot fields identical to `ProcessHotChunk`.
- **Line 1 (Bytes 64..127)**: `comm` (16B), `uid` (4B), `pinned_drm_fd` (4B), `timerslack_ns` (8B), `read_bytes` (8B), `write_bytes` (8B), `io_syscalls` (8B), `drm_engine_gfx_ns` (8B).
- **Line 2 (Bytes 128..191)**: `drm_engine_compute_ns` (8B), `drm_engine_dec_ns` (8B), `drm_engine_enc_ns` (8B), `drm_vram_kib` (8B).

---

## 3. Deep Memory Sequence Analyzer Probe Specification

### 3.1 Design Invariants
1. **Preprocessed Zero-Cost Execution**:
   - Guarded by `#if defined(WATTCURB_MEMORY_PROBE) || !defined(NDEBUG)`.
   - In production release builds (`-DNDEBUG`), macros expand to `((void)0)` and the probe class consists of inline no-op functions with 0 memory or binary footprint.
2. **Cache Line Crossing Detection**:
   - Every recorded field access evaluates `cline = offset / 64`.
   - If `has_prev && cline != prev_cline`, an inter-line crossing hazard is logged.
3. **Statistical Transition Matrix**:
   - $P(F_j \mid F_i) = \frac{\text{Transitions}(F_i \rightarrow F_j)}{\sum_k \text{Transitions}(F_i \rightarrow F_k)}$.
   - Validates that sequential execution remains clustered within Line 0 ($100\%$ locality).

---

## 4. Verification & Testing

- Unit Test: `test::test_memory_sequence_probe_and_cache_chunking()` in [`tests/test_units.cpp`](file:///home/jedclub/Develop/WattCurb/tests/test_units.cpp).
- Compile-time Assertions:
  - `static_assert(sizeof(ProcessHotChunk) == 64);`
  - `static_assert(alignof(ProcessHotChunk) == 64);`
  - `static_assert(sizeof(CompactProcessHot) == 32);`
  - `static_assert(sizeof(ProcessSample) == 192);`
  - `static_assert(alignof(ProcessSample) == 64);`
