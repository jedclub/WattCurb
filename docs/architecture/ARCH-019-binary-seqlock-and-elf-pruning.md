# [REF-ARCH-019] Pure Binary Seqlock Shared Memory & Zero-ELF Residue Release Architecture

## 1. System Overview

This architecture document defines the zero-overhead data supply pipeline replacing JSON with a **128-byte Seqlock POD memory layout** and the **zero-ELF residue build pipeline** for WattCurb.

```
┌────────────────────────────────────────────────────────────────────────┐
│                        WattCurb Background Daemon                      │
│                                                                        │
│   [Observation Window] ──> [Hardware + Attribution Analysis]           │
│                                      │                                 │
│                                      ▼                                 │
│                   WattCurbSharedState::update_from_report              │
│                     (128-byte Seqlock Atomic Write)                    │
│                        │                          │                    │
└────────────────────────┼──────────────────────────┼────────────────────┘
                         │ mmap                     │ sendto (Datagram)
                         ▼                          ▼
               /dev/shm/wattcurb_state.shm     /run/wattcurb.sock
               (Zero-Wakeup Shared Memory)    (Binary Control Socket)
                         │                          │
                         ▼                          ▼
               ┌───────────────────┐      ┌────────────────────┐
               │ Desktop Tray Icon │      │ CLI Query Client   │
               │ (read_atomic)     │      │ (wattcurb --status)│
               └───────────────────┘      └────────────────────┘
```

---

## 2. Data Structure Architecture

### 2.1 Memory Map & Dual-Cacheline Layout

```
Cacheline 0 (Offset 0..63, 64 Bytes):
+---------------------+-------------------+------------------+------------------+
| seq_version (8B)    | system_drain (4B) | battery_pct (1B) | battery_state(1B)|
+---------------------+-------------------+------------------+------------------+
| time_to_empty (2B)  | mitigations (4B)  | cpu_drain (4B)   | gpu_drain (4B)   |
+---------------------+-------------------+------------------+------------------+
| wakeups_sec (4B)    | cpu_temp_c (2B)   | fan_rpm (2B)     | c3_pct (1B)      |
+---------------------+-------------------+------------------+------------------+
| batt_health_pct(1B) | reserved0 (22B)   | (End of Cacheline 0)                |
+---------------------+-------------------+-------------------------------------+

Cacheline 1 (Offset 64..127, 64 Bytes):
+-------------------------------------------------------------------------------+
| culprits[0]: comm(16B), pid(4B), drain_mw(4B), domain(1B), tier(1B), pad(6B)  |
+-------------------------------------------------------------------------------+
| culprits[1]: comm(16B), pid(4B), drain_mw(4B), domain(1B), tier(1B), pad(6B)  |
+-------------------------------------------------------------------------------+
```

### 2.2 Seqlock Synchronization Mechanics

1. **Daemon (Single Writer)**:
   ```cpp
   uint64_t ver = __atomic_load_n(&seq_version, __ATOMIC_RELAXED);
   __atomic_store_n(&seq_version, ver + 1, __ATOMIC_RELEASE); // Odd: Busy
   // Update all telemetry fields
   __atomic_store_n(&seq_version, ver + 2, __ATOMIC_RELEASE); // Even: Stable
   ```
2. **Tray / Reader (Lock-Free Multi-Reader)**:
   ```cpp
   uint64_t v1 = 0, v2 = 0;
   do {
       v1 = __atomic_load_n(&seq_version, __ATOMIC_ACQUIRE);
       if (v1 & 1) continue; // Busy writer, loop back
       std::memcpy(&out, this, sizeof(WattCurbSharedState));
       v2 = __atomic_load_n(&seq_version, __ATOMIC_ACQUIRE);
   } while ((v1 & 1) != 0 || v1 != v2);
   ```

---

## 3. Zero-ELF Residue Release Pipeline

Production builds enforce complete stripping of unused sections:

| Section Name | Original Purpose | Elimination Rationale in WattCurb |
| :--- | :--- | :--- |
| `.note.gnu.build-id` | Build identifier | Unused in distribution; saves ~36 bytes |
| `.note.ABI-tag` | OS ABI tag | Linux glibc defaults handle without note; saves ~32 bytes |
| `.note.gnu.property` | Intel CET / IBT notes | Pruned in native binary deployments |
| `.comment` | GCC compiler version string | Zero functional utility in runtime execution |
| `.sframe` | Compact unwinding tables | Exception-free daemon (`-fno-exceptions`) |
| `.eh_frame` | DWARF Call Frame Info | Strip safe when exceptions and backtraces are removed |
| `.eh_frame_hdr` | Binary search table for eh_frame | Pruned together with `.eh_frame` |

**Build Command**:
```bash
strip --strip-all \
    --remove-section=.note.gnu.build-id \
    --remove-section=.note.ABI-tag \
    --remove-section=.note.gnu.property \
    --remove-section=.comment \
    --remove-section=.sframe \
    --remove-section=.eh_frame \
    --remove-section=.eh_frame_hdr \
    output/wattcurb
```

---

## 4. Benchmark Results & Size Progression

- Standard `-O3` with JSON & Exception Tables: **225 KB**
- Stripping `.eh_frame` & `.comment`: **208 KB**
- Complete JSON Purge & 128B Seqlock POD Implementation: **204 KB (204,216 bytes)**
- **Net Release Binary Reduction**: **~21 KB (9.3% reduction)**
