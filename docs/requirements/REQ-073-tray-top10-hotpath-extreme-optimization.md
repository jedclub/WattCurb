# REQ-073: Extreme Optimization of Desktop Tray Client Top Hotspots

**Ref-ID**: `REF-REQ-073`  
**Subsystem**: Desktop Tray Client / Hot-Path Optimization / Syscall Elision  
**Priority**: High  
**Status**: Approved  

---

## 1. Context & Motivation

Through fine-grained micro-architectural profiling (`REF-REQ-072`, `REF-ARCH-049`, `REF-TEST-037`), the exact computational costs of the desktop StatusNotifierItem (SNI) tray client (`wattcurb-tray`) were exposed:

1. **Hardware Sysfs VFS & ACPI Bottleneck (93.8% of hover latency)**:
   - Repeated `open()` / `close()` syscall cycles on `/sys/class/thermal/thermal_zone0/temp` (~112 µs/op) and `/sys/class/power_supply/BAT0/uevent` (~46 µs/op).
   - High event frequency during mouse hovering: KDE Plasma sends multiple `ToolTip` D-Bus property queries within a few hundred milliseconds while the cursor moves over the icon.
2. **String Formatting & Loop Inefficiencies (6.2% of hover latency)**:
   - Repeated multi-byte UTF-8 loop synthesis in `build_unicode_bar`.
   - String formatting with `std::snprintf` for icon names that have only 66 deterministic permutations.
   - Redundant `std::strlen()` passes in `sanitize_utf8_inplace`.

WattCurb's design principles dictate radical syscall elimination, persistent descriptor retention, branchless lookup tables (LUT), and zero-cost memoization to achieve sub-microsecond latency.

---

## 2. Functional & Technical Requirements

1. **Persistent Sysfs File Descriptors & `pread()` Reuse (`REF-REQ-073.1`)**:
   - Eliminate all `open()` and `close()` syscalls during mouse hover.
   - Maintain static, lazily initialized persistent file descriptors with `O_RDONLY | O_CLOEXEC` for `thermal_zone0/temp`, `BAT0/uevent`, and `scaling_cur_freq`.
   - Ingest data via zero-seek `pread(fd, buf, size, 0)`.

2. **Hover Telemetry Hysteresis & Subsampling Guard (`REF-REQ-073.2`)**:
   - Introduce a 500ms monotonic time gate for physical sysfs reads.
   - If consecutive `ToolTip` queries occur within 500ms (typical KDE cursor hover jitter), bypass physical hardware sysfs reading entirely and reuse the Seqlock state.
   - This reduces repetitive ACPI EC bus stalls to zero during continuous hover.

3. **Precomputed Unicode Progress Bar Lookup Table (`REF-REQ-073.3`)**:
   - Replace the loop-based byte-by-byte construction in `build_unicode_bar` with a `constexpr` 9-element 24-byte UTF-8 table (`BAR_LUT[0..8]`).
   - Copy bar strings in a single 24-byte memory operation (`std::memcpy`).

4. **Precomputed Icon Name Lookup Table (`REF-REQ-073.4`)**:
   - Eliminate runtime `std::snprintf` formatting in `resolve_icon_name`.
   - Precompute icon strings into a flat constant table indexed by `(battery_bracket, profile_index, is_charging)`.
   - Return static string literals or perform single pointer copies.

5. **`strlen()` Elimination in UTF-8 Sanitization (`REF-REQ-073.5`)**:
   - Utilize the length returned by `snprintf` directly to validate UTF-8 boundaries, avoiding $O(N)$ string traversals.

6. **Fast Branchless Integer Parsing (`REF-REQ-073.6`)**:
   - Replace generic `std::strtol` with an inlined pointer-advancing numeric scanner for sysfs temperature and frequency buffers.

---

## 3. Verification & Oracle Gate Standards (`REF-TEST-038`)

- Total hover tooltip latency must drop from ~175 µs to **< 5 µs** on warm cache (a > 30x throughput improvement).
- Sysfs open/close syscall count during steady-state hover must be **0**.
- `build_bars` latency must drop to **< 15 ns/op**.
- `resolve_icon` latency must drop to **< 20 ns/op**.
- All unit tests and existing invariants must pass without regression.
