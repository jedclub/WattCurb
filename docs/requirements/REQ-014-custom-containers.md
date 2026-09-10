# [REF-REQ-017] Custom Zero-Allocation High-Performance Containers

- **Ref-ID**: `REF-REQ-017`
- **Module**: `src/core/custom_containers.hpp`
- **Related Requirements**: [`REF-REQ-004`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-002-zero-allocation-procfs.md), [`REF-REQ-009`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-005-kernel-interface-specialization.md)
- **Related Architecture**: [`REF-ARCH-006`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-006-custom-containers.md)
- **Status**: Approved / In-Progress

---

## 1. Executive Summary & Problem Statement

Standard C++ template library containers (`std::vector`, `std::string`, `std::unordered_map`) provide general-purpose flexibility but suffer from critical performance drawbacks in ultra-low-overhead real-time telemetry:
1. **Dynamic Heap Overhead (`malloc`/`free`)**: `std::vector` and `std::string` allocate buffer storage from the system heap, triggering page table lookups, memory fragmentation, and lock contention in multithreaded runtimes.
2. **Cache Locality Degradation & Pointer Chasing**: Heap memory is located far from stack/BSS memory pages, causing avoidable Data TLB (dTLB) evictions and L1 Data Cache (L1D) miss penalties.
3. **Non-TriviallyCopyable Invalidation**: Structures containing `std::string` or `std::vector` cannot be `std::is_trivially_copyable_v`, preventing compilers from employing SIMD AVX2/AVX-512 block memmove and vector register shuffling.
4. **Predictable Bounded Domains**: In WattCurb, cardinality is known at compile time:
   - Monitored processes per system: $\le 1024$
   - Hardware domains: $\le 8$
   - Domain culprits: $\le 5$
   - Process dashboard top rankings: $\le 32$
   - Text descriptions & mechanism strings: $\le 96$ bytes

---

## 2. Technical Specifications & Functional Requirements

### 2.1 `FixedVector<T, Capacity>` (Zero-Allocation Stack/Inline Vector)
- **Requirement 1 (Zero Heap Allocation)**: All storage must reside inline inside the container (`std::byte storage_[sizeof(T) * Capacity]`), requiring 0 bytes of dynamic heap allocation.
- **Requirement 2 (TriviallyCopyable Support)**: When `T` satisfies `std::is_trivially_copyable_v<T>`, `FixedVector<T, Capacity>` must automatically enable SIMD/`memcpy` bulk operations.
- **Requirement 3 (STL-Compatible API)**: Provide `push_back()`, `emplace_back()`, `pop_back()`, `operator[]`, `data()`, `size()`, `capacity()`, `empty()`, `clear()`, and standard iterator interfaces (`begin()`, `end()`).
- **Requirement 4 (Bounds Safety)**: Safe insertion that avoids buffer overruns and operates in `noexcept` mode where possible.

### 2.2 `FixedString<Capacity>` (Zero-Allocation Inline String)
- **Requirement 1 (Inline Storage)**: Fixed-size null-terminated inline buffer (`std::array<char, Capacity>`) with zero heap allocation.
- **Requirement 2 (TriviallyCopyable POD)**: Must be trivially copyable so structures containing it (`ProcessAttributedPower`, `DomainCulprit`) remain TriviallyCopyable.
- **Requirement 3 (Fast Append & Formatting)**: Support assignment from `std::string_view`, `const char*`, and fast append of numbers via `std::to_chars`.

### 2.3 `TopKHeap<T, K, Compare>` (Register-Resident Top-K Selector)
- **Requirement 1 (O(N log K) In-Place Selection)**: Select the top $K$ elements from an $N$-element stream without sorting the entire input or allocating memory.
- **Requirement 2 (Minimal Working Set)**: Maintain a min-heap of size $K$ directly on the stack ($K \le 32$, typically $K = 5$), fitting entirely into CPU L1 cache and registers.

---

## 3. Non-Functional & Telemetry Verification Criteria

1. **Heap Allocation Free**: 0 heap allocations during steady-state attribution and reporting loops.
2. **Binary Size & Compilation**: Reduction in template bloat compared to generic `std::vector` / `std::string`.
3. **Hardware PMU Guardrail**:
   - Instruction count must remain $\le 17\text{M}$ instructions over a 30-second window.
   - User CPU active time must remain $\le 10\text{ ms}$ across 30 seconds.
   - Resident Set Size (RSS) must remain strictly flat at $9.9\text{ MB}$.
