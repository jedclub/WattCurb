# [REF-ARCH-006] Custom Zero-Allocation Container Architecture

- **Ref-ID**: `REF-ARCH-006`
- **Module**: `src/core/custom_containers.hpp`
- **Related Requirements**: [`REF-REQ-017`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-014-custom-containers.md)
- **Status**: Implemented

---

## 1. Architectural Philosophy: Cache-Line Aligned Flat Structures

In ultra-low-power systems, cache misses and dynamic heap calls are the dominant sources of uncoordinated CPU wakeups and instructions.
Standard containers introduce pointer indirection and heap metadata overhead:

```
[Standard std::vector<T>]
 Stack Frame: [begin_ptr | end_ptr | capacity_ptr] (24 bytes)
                      │
                      └──> Heap Page: [ T0 | T1 | T2 ... ] (Cache Miss & dTLB Traversal)

[WattCurb FixedVector<T, N>]
 Stack Frame: [ size_t size | alignas(64) T storage[N] ] (Contiguous in L1D Cache!)
```

By guaranteeing that `storage_` is contiguous with the container's control block, the CPU prefetcher fetches both the container metadata and data payload in the same cache-line fill.

---

## 2. Core Components

### 2.1 `FixedVector<T, Capacity>`
- Contiguous inline array without dynamic heap allocation.
- Trivially copyable when `T` is trivially copyable.
- Fast bounds-checked `push_back()` that drops out-of-bounds elements safely.

### 2.2 `FixedString<Capacity>`
- Trivially copyable string container using a fixed-size char buffer.
- Eliminates `std::string` allocations when generating mechanism labels and domain names.

### 2.3 `TopKHeap<T, K, Compare>`
- Replaces full array sort and partial sort with an $O(N \log K)$ streaming priority queue.
- Working set is strictly bounded to $K$ elements ($K \le 32$), fitting directly into CPU registers or L1D cache.
