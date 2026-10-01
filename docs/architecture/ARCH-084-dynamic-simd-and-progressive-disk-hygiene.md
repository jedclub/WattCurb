# REF-ARCH-084: Dynamic SIMD Dispatch & Tiered Storage Hygiene Architecture

## 1. Subsystem Topology & Architecture

```
+-------------------------------------------------------------------------+
|                      WattCurb Policy Core Subsystem                     |
|                                                                         |
|  +--------------------------------+   +------------------------------+  |
|  |       FanCurveEngine           |   |      DiskPressureGuard       |  |
|  |  +--------------------------+  |   |  +------------------------+  |  |
|  |  | Dynamic SIMD Dispatcher  |  |   |  | statvfs Cadence (30s)  |  |  |
|  |  |  - AVX2+FMA (8-wide FMA) |  |   |  +------------+-----------+  |  |
|  |  |  - SSE2 (4-wide vector)  |  |   |               |              |  |
|  |  |  - Scalar Horner's Rule  |  |   |               v              |  |
|  |  +--------------------------+  |   |  Usage >= 90.0% & Cooldown?  |  |
|  +--------------------------------+   +---------------+--------------+  |
|                                                       |                 |
|                                                       v                 |
|                                       +---------------+--------------+  |
|                                       | 3-Tier Progressive Reclaim   |  |
|                                       |  - Tier 1: journal / dumps   |  |
|                                       |  - Tier 2: pkg cache deb/rpm |  |
|                                       |  - Tier 3: /tmp (> 7 days)   |  |
|                                       | (Max 512 MiB batch budget)   |  |
|                                       +------------------------------+  |
+-------------------------------------------------------------------------+
```

---

## 2. Dynamic SIMD Dispatch Mechanics

To satisfy the C++23 Zero-Cost Abstraction mandate:
1. **Load-Time Resolution**:
   A function pointer `SplineEvalFn` is bound during process initialization via CPUID detection (`has_runtime(CpuFeature::AVX2)` and `has_runtime(CpuFeature::FMA)`).
2. **Zero-Branch Invariant**:
   Subsequent invocations incur no runtime CPUID tests or branch penalties, jumping directly to the target vector instruction stream.

---

## 3. Storage Reclaim Safety Invariants & Boundary Guards

1. **Root Isolation**:
   Target scanning paths are strictly constrained to:
   - `/var/log/journal`
   - `/var/lib/systemd/coredump`
   - `/var/cache/apt/archives`
   - `/var/cache/pacman/pkg`
   - `/tmp`, `/var/tmp`
2. **Exclusion Mask**:
   Files matching any of the following are immune:
   - Sockets (`S_ISSOCK`), FIFOs (`S_ISFIFO`), block/char devices.
   - Suffixes: `.lock`, `.pid`, `.sock`.
   - Age $< 7$ days (for `/tmp`).
3. **Budget Cap**:
   Cumulative bytes unlinked per sweep are monitored via a monotonic accumulator; when `reclaimed_bytes >= 512 MiB`, iteration yields immediately.
