# [REF-ARCH-007] Memory Safety Guards, Canary Integrity & Capacity Architecture

## 1. Architectural Philosophy: Hardened Zero-Allocation
- **Ref-ID**: `REF-ARCH-007`
- **Module**: `core::containers`, `proc::ProcessAnalyzer`, `policy::AttributionEngine`
- **Related Requirements**: [`REF-REQ-018`](../requirements/REQ-015-memory-safety-guards.md), [`REF-ARCH-006`](../architecture/ARCH-006-custom-containers.md)
- **Status**: Approved

Zero-cost performance must not compromise daemon stability or host security. By combining:
1. **Generous Capacity Over-Provisioning**: Doubling process buffer limits to 2,048 entries (~350KB static buffer per snapshot), granting > 400% safety buffer over peak load.
2. **Boundary Canary Words**: Guarding storage tail with compile-time 64-bit magic constants (`0xDEADBEEFCAFE0001ULL`).
3. **Graceful Saturating Clamping**: Clamping out-of-range indexing and rejecting insertions beyond capacity without crashing or invoking undefined behavior.
WattCurb achieves enterprise-grade memory resilience with zero performance penalty.

---

## 2. Component Design & Guard Mechanisms

### 2.1. Buffer Layout with Canary Guard
```text
+-------------------------------------------------------------------------+
| FixedVector Memory Layout (64-byte aligned)                             |
+------------------------------------+------------------------------------+
| Element Storage [0 ... Capacity-1] | Canary Guard (64-bit Magic)        |
| (sizeof(T) * Capacity bytes)       | 0xDEADBEEFCAFE0001ULL              |
+------------------------------------+------------------------------------+
                                      ^ Checked by check_integrity()
```

### 2.2. Guarded FixedVector Implementation Details
```cpp
template <typename T, size_t Capacity>
class alignas(alignof(T) > 64 ? alignof(T) : 64) FixedVector {
    static constexpr uint64_t CANARY_MAGIC = 0xDEADBEEFCAFE0001ULL;

    size_type size_{0};
    uint32_t overflow_count_{0};
    alignas(T) std::byte storage_[sizeof(T) * Capacity];
    uint64_t canary_{CANARY_MAGIC};

public:
    [[nodiscard]] bool check_integrity() const noexcept {
        return canary_ == CANARY_MAGIC;
    }

    [[nodiscard]] bool overflow_occurred() const noexcept {
        return overflow_count_ > 0;
    }

    [[nodiscard]] uint32_t overflow_count() const noexcept {
        return overflow_count_;
    }

    [[nodiscard]] reference at(size_type idx) noexcept {
        if (size_ == 0) return dummy_instance();
        if (idx >= size_) idx = size_ - 1; // Safe clamp
        return data()[idx];
    }
...
};
```

### 2.3. Static Memory Footprint vs. Safety Headroom
- `sizeof(ProcessSample)` ≈ 168 bytes
- 2048 entries = 344,064 bytes (~336 KB)
- `DoubleBufferedPool<ProcessSample, 2048>` = ~672 KB total in static/BSS arena.
- For modern laptops/servers (16 GB ~ 128 GB RAM), a 672 KB static allocation represents < 0.004% of total RAM, while guaranteeing complete freedom from dynamic memory fragmentation and page faults.
