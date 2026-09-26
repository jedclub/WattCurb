# [REF-ARCH-076] Low-Overhead Process Memory SHM Persistence Architecture

- **Status**: Approved · **Date**: 2026-09-26
- **Ref ID**: `REF-ARCH-076`
- **Related Requirements**: [`REF-REQ-129`](../requirements/REQ-129-low-overhead-process-memory-persistence.md), [`REF-REQ-028`](../requirements/REQ-028-adaptive-power-state-machine-and-actuation.md), [`REF-REQ-059`](../requirements/REQ-059-lockless-in-memory-history-ring-buffer.md)
- **Category**: Shared Memory IPC, Seqlock POD Architecture, Memory Profiling

---

## 1. Data Structure Layout Alignment

### 1.1 `SharedCulprit` (32 Bytes POD)
```
+---------------------------------------------------------------------------------+
| Offset | Field        | Type      | Size   | Description                        |
|:------:|:-------------|:----------|:------:|:-----------------------------------|
|   0    | comm         | char[16]  | 16 B   | Process command name (NUL-term)    |
|  16    | pid          | int32_t   |  4 B   | Process ID                         |
|  20    | drain_mw     | uint32_t  |  4 B   | Power drain in milliwatts          |
|  24    | domain_id    | uint8_t   |  1 B   | Primary attribution domain ID      |
|  25    | tier         | uint8_t   |  1 B   | ProcessSafetyTier                  |
|  26    | rss_mb       | uint16_t  |  2 B   | Resident Set Size (MB)             |
|  28    | pss_mb       | uint16_t  |  2 B   | Proportional Set Size (MB)         |
|  30    | majflt_s     | uint16_t  |  2 B   | Major Page Faults per second       |
+---------------------------------------------------------------------------------+
Total Size: 32 Bytes (Cacheline aligned, 0-padding)
```

### 1.2 `HistoryPoint` (32 Bytes POD)
```
+---------------------------------------------------------------------------------+
| Offset | Field              | Type      | Size   | Description                  |
|:------:|:-------------------|:----------|:------:|:-----------------------------|
|   0    | timestamp_sec      | uint64_t  |  8 B   | Monotonic epoch time (sec)   |
|   8    | total_system_mw    | uint32_t  |  4 B   | Total platform power (mW)    |
|  12    | cpu_package_mw     | uint16_t  |  2 B   | CPU Package RAPL power (mW)  |
|  14    | gpu_mw             | uint16_t  |  2 B   | GPU DRM power (mW)           |
|  16    | cpu_temp_c         | uint16_t  |  2 B   | CPU temperature (Celsius)    |
|  18    | cpu_freq_mhz       | uint16_t  |  2 B   | Realtime CPU clock (MHz)     |
|  20    | battery_percent    | uint8_t   |  1 B   | State of Charge (%)          |
|  21    | battery_state      | uint8_t   |  1 B   | AC / Battery / Passthrough   |
|  22    | power_profile_mode | uint8_t   |  1 B   | Active Power Profile         |
|  23    | cstate_c3_percent  | uint8_t   |  1 B   | Deep C3 Sleep Residency (%)  |
|  24    | active_mitigations | uint8_t   |  1 B   | Bitmask / active count       |
|  25    | mem_used_mb        | uint16_t  |  2 B   | System RAM in use (MB)       |
|  27    | swap_used_mb       | uint16_t  |  2 B   | System Swap in use (MB)      |
|  29    | top_proc_pss_mb    | uint16_t  |  2 B   | Top Process PSS (MB)         |
|  31    | reserved           | uint8_t[1]|  1 B   | Alignment padding            |
+---------------------------------------------------------------------------------+
Total Size: 32 Bytes (Exact power-of-two 32B alignment)
```
