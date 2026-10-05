# [REF-ARCH-085] Base Swap Lifecycle & Zero-Downtime Migration Architecture

**Implements**: [`REF-REQ-138`](../requirements/REQ-138-autonomous-base-swap-right-sizing-and-migration.md)  
**Date**: 2026-10-05  

---

## 1. Architectural Role & Placement

The Base Swap Right-Sizing Engine resides inside `wattcurb::policy::SwapExpander` as a complementary lifecycle controller:
* [`REF-REQ-113`](../requirements/REQ-113-performance-mode-dynamic-swap-expansion.md) / [`REF-ARCH-073`](ARCH-073-dynamic-swap-expansion.md) handles **expansion** (elastic addition and pruning of ephemeral slice files `wattcurb-dyn-*.swap`).
* [`REF-REQ-138`](../requirements/REQ-138-autonomous-base-swap-right-sizing-and-migration.md) / [`REF-ARCH-085`](ARCH-085-base-swap-lifecycle-and-zero-downtime-migration.md) handles **base sizing** (autonomous downsizing and seamless migration of bloated vendor/distro base swapfiles like `/swap/swapfile` from 32 GiB to 8 GiB).

```
                      Daemon Low-Frequency Loop (60s tick)
                                      │
              ┌───────────────────────┴───────────────────────┐
              ▼                                               ▼
     Elastic Slice Manager                         Base Swap Right-Sizer
     (REF-REQ-113 / ARCH-073)                     (REF-REQ-138 / ARCH-085)
   - Performance mode surge                    - 2-hour low-usage window
   - Dynamic +8 GiB slices                     - Downsize 32 GiB -> 8 GiB
   - Ephemeral prune on idle                   - Zero-downtime page migration
```

---

## 2. State Machine Pipeline

To adhere to the **Zero-Wakeup** and **Non-Blocking** invariants, migration progresses through an asynchronous 5-stage state machine:

```
  [State: IDLE] 
       │ (Window >= 7200 s && PeakUsed <= 5.6 GiB && BaseSize >= 16 GiB && FsFree >= 16 GiB)
       ▼
  [State: CREATING_NEW]
       │ Child forks: btrfs mkswapfile / fallocate 8 GiB at /swap/swapfile.new
       ▼
  [State: SWAPPING_ON_NEW]
       │ Parent reaps child: swapon("/swap/swapfile.new", 0) [DUAL ACTIVE SWAP]
       ▼
  [State: SWAPPING_OFF_OLD]
       │ Child forks: swapoff("/swap/swapfile") [Kernel page migration into .new + ZRAM]
       ▼
  [State: COMMITTING]
       │ Parent reaps child: unlink("/swap/swapfile"), rename(.new -> /swap/swapfile)
       ▼
  [State: COMPLETED] 
       │ 24 GiB NVMe space recovered; Cooldown engaged; Return to IDLE
```

---

## 3. Detailed Step Invariants

| State | Operation | Execution Context | Failure Recovery |
| :--- | :--- | :--- | :--- |
| **IDLE** | Track `max_swap_used_kb` over 7200 s | Main thread (O(1)) | Inaction cooldown |
| **CREATING_NEW** | Create 8 GiB file `/swap/swapfile.new` | Forked Child (non-blocking) | If exit != 0, unlink `.new`, abort to IDLE |
| **SWAPPING_ON_NEW**| `swapon(path_new, 0)` | Parent thread (< 1 ms syscall) | If failed, unlink `.new`, abort to IDLE |
| **SWAPPING_OFF_OLD**| `swapoff(path_old)` | Forked Child (non-blocking) | If failed, `swapoff(.new)`, unlink `.new`, abort |
| **COMMITTING** | `unlink(path_old)` & `rename(path_new, path_old)` | Parent thread (< 1 ms syscalls) | Atomic path swap ensures fstab stability |

---

## 4. Mathematical Decision Functions (Pure Oracle Gates)

All decision thresholds are implemented as pure, side-effect-free static member functions for deterministic verification:

```cpp
// Pure decision: is the observation window satisfied?
[[nodiscard]] static bool is_eligible_for_right_sizing(
    uint64_t window_duration_sec,
    uint64_t peak_used_kb,
    uint64_t current_base_size_bytes,
    uint64_t fs_free_bytes
) noexcept {
    constexpr uint64_t MIN_WINDOW_SEC = 7200ULL;                    // 2 hours
    constexpr uint64_t MAX_PEAK_USED_KB = 5600ULL * 1024ULL;        // 5.6 GiB
    constexpr uint64_t MIN_BASE_SIZE_BYTES = 16ULL << 30;           // 16 GiB
    constexpr uint64_t MIN_FS_FREE_BYTES = 16ULL << 30;             // 16 GiB

    return (window_duration_sec >= MIN_WINDOW_SEC) &&
           (peak_used_kb <= MAX_PEAK_USED_KB) &&
           (current_base_size_bytes >= MIN_BASE_SIZE_BYTES) &&
           (fs_free_bytes >= MIN_FS_FREE_BYTES);
}
```

---

## 5. Storage & Battery Coexistence

1. **NVMe Wearout Prevention**: Right-sizing runs at most **once** per boot or after a strict 24-hour backoff lockout. It does not churn disk files.
2. **AC / Battery Consideration**: If the host is on battery with remaining capacity < 30%, migration is deferred until AC connection to avoid draining power on 8 GiB zero-allocation and crypto-hashing.
3. **Emergency Fallback Guarantee**: If subsequent parallel workloads (e.g. `cc1plus` / `clang++`) require > 8 GiB swap, `SwapExpander::ensure_headroom()` automatically adds temporary dynamic slices without manual intervention.
