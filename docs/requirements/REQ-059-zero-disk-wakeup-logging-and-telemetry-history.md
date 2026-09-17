# REQ-059: Zero-Disk-Wakeup Logging & In-Memory Telemetry History Specification

- **Ref-ID**: `REF-REQ-059`
- **Category**: Telemetry, Logging Architecture & Power Storage Physics
- **Title**: Zero-Disk-Wakeup Event-Driven Audit Logging & Lockless In-Memory Telemetry History
- **Status**: Approved
- **Domain**: Linux I/O, NVMe APST, PCIe ASPM, IPC Shared Memory, systemd-journald
- **Dependencies**: [`REF-REQ-058`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-058-ultra-endurance-frequency-capping-and-zero-io-logging-audit.md), [`REF-REQ-028`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-026-binary-seqlock-data-supply-and-json-elimination.md), [`REF-ARCH-018`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-018-seqlock-binary-shm-and-json-streaming.md)

---

## 1. Problem Statement & Storage Power Physics

### 1.1 The Battery Penalty of Continuous Disk Logging
Modern NVMe SSDs utilize Autonomous Power State Transitions (APST) and PCIe Active State Power Management (ASPM L1.1 / L1.2).
- **Non-Operational Deep Sleep (PS4 / L1.2)**: Power consumption is typically **< 5 mW**.
- **Operational Active State (PS0 / L0)**: Power consumption jumps to **1.5 W ~ 3.5 W**.
- **Exit & Re-entry Penalty**: When an application writes to a log file every 1 to 3 seconds:
  1. The kernel page cache is dirtied.
  2. Ext4 journaling (`jbd2`) or synchronous writes force PCIe link training from L1.2 to L0.
  3. The NVMe controller transitions to PS0, preventing entry into PS4.
  4. With an idle dwell timeout of 100ms ~ 5000ms, the SSD controller **never sleeps**, continuously burning 1.5W~2.0W. On a laptop with an 8W idle platform budget, this single defect depletes **15%~25% of battery life**.

### 1.2 The Observability Gap of Complete Log Elimination
Prior to this requirement, WattCurb eliminated all periodic disk writes by publishing solely to a 128-byte `/dev/shm/wattcurb_state.bin` Seqlock file.
However, this introduced an observability deficit:
- Administrators and users could not audit **when** or **why** an aggressive background process was throttled, capped, or restored.
- System transitions (e.g. AC disconnect, profile drop into UltraEndurance) left zero historical traces in `journalctl`.
- No historical time-series data was preserved to inspect power discharge curves over the last 30~60 minutes without triggering disk I/O.

---

## 2. Functional Requirements

### 2.1 Pillar 1: Event-Driven Audit Journaling (`REF-REQ-059-P1`)
1. **Zero Steady-State Writes**: During normal steady-state monitoring loops (3.0s cadence), zero disk writes (`write(2)`, `pwrite(2)`) or syslog emissions shall occur.
2. **State-Transition Triggered**: Logging shall be strictly event-driven, triggered only upon:
   - **Profile Transitions (`[PROFILE]`)**: Mode changes between Performance, Balanced, PowerSaver, and UltraEndurance, documenting the trigger (battery threshold, AC event, user request) and hardware limits enforced.
   - **Mitigation Actuation (`[MITIGATION]`)**: Throttling of a runaway process, documenting target PID, comm, CFS nice adjustment, core affinity mask (e.g. 50% cap), and cgroup CPU quota.
   - **Mitigation Rollback (`[ROLLBACK]`)**: Restoration of an unthrottled process back to baseline CFS policy, nice, affinity, and quota.
   - **Hardware/Battery Alerts (`[ALERT]`)**: Battery milestone crossings (20%, 10%), AC connect/disconnect, and thermal throttle warnings.
3. **Structured systemd & File Journaling**: Output shall be emitted to `journald` (via stdout/syslog) and append-only `/var/log/wattcurb/audit.log` (when directory exists), ensuring zero heap allocation during formatting.

### 2.2 Pillar 2: In-Memory Lockless Telemetry History (`REF-REQ-059-P2`)
1. **RAM-Only Circular History Buffer**: WattCurb shall maintain a fixed-capacity circular ring buffer in shared memory (`/dev/shm/wattcurb_history.bin`) containing the most recent 600 telemetry points (representing the last 30 minutes at a 3.0s cadence).
2. **Compact 32-Byte POD Point**: Each historical sample must be a 32-byte `TriviallyCopyable` struct (`HistoryPoint`) capturing timestamp, total mW, CPU mW, GPU mW, CPU temp, CPU clock, battery %, battery state, and active profile.
3. **Cache-Friendly Working Set**: Total history footprint shall not exceed 20 KB ($600 \times 32\text{ bytes} + 64\text{ bytes header} = 19,264\text{ bytes}$), fitting entirely inside the L1/L2 data cache.
4. **Interactive CLI & Dashboard Inspection**:
   - `wattcurb --history` / `wattcurb -H` reads `/dev/shm/wattcurb_history.bin` and outputs a compact terminal time-series table.
   - `wattcurb --logs` / `wattcurb -L` dumps recent event audit logs.

### 2.3 Pillar 3: AC-Coalesced Long-Term Persistence (`REF-REQ-059-P3`)
1. While operating on battery, deep historical archives shall never be flushed to physical storage.
2. Long-term log archiving (if configured) shall only occur when AC power is attached or during orderly daemon termination.

---

## 3. Verification & Oracle Gate Standards (`REF-TEST-024`)

1. **Zero-Allocation Assertion**:
   - `EventLogger` formatting routines must allocate 0 bytes on the heap (using fixed stack buffers and `std::snprintf`).
2. **Sub-Microsecond Latency Gate**:
   - Appending a sample to the `HistoryRingBuffer` must execute in **< 50 nanoseconds**.
   - Formatting an audit log event must execute in **< 2.0 microseconds**.
3. **Circular Wraparound Consistency**:
   - Validate that ring buffer head indexing correctly wraps around 600 elements without buffer overflows or memory corruption.
4. **Physical Disk I/O Invariant**:
   - Under steady-state monitoring with no profile/mitigation events, daemon `/proc/[pid]/io` `write_bytes` must remain constant (0 bytes written).
