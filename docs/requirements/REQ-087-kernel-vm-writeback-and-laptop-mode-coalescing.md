# REF-REQ-087: Kernel VM Writeback & Laptop Mode Disk Flushing Coalescing Specification

- **Document ID**: `REF-REQ-087`
- **Related Requirements**: [`REF-REQ-001`](REQ-001-hardware-power-profiling.md), [`REF-REQ-031`](REQ-028-adaptive-power-state-machine-and-actuation.md), [`REF-REQ-034`](REQ-034-rapid-charge-and-profile-restoration-engine.md), [`REF-REQ-063`](REQ-063-ultra-endurance-hardware-and-desktop-power-capping.md)
- **Related Architecture**: [`REF-ARCH-064`](../architecture/ARCH-064-kernel-vm-sysctl-actuation-and-clean-ac-rollback.md)
- **Related Research**: [`REF-RES-024`](../research/RES-024-deep-power-log-audit-and-drain-analysis.md), [`REF-RES-025`](../research/RES-025-ultra-endurance-deep-silicon-and-kernel-power-minimization.md)
- **Status**: Approved
- **Date**: 2026-09-21

---

## 1. Objective & Background

Physical power audits ([`REF-RES-024`](../research/RES-024-deep-power-log-audit-and-drain-analysis.md)) revealed that background disk flusher threads continuously interrupt deep CPU Package C6 sleep states. By default, the Linux Virtual Memory subsystem commits dirty page cache pages to storage devices every 5 seconds (`dirty_writeback_centisecs = 500`), and expires dirty pages after 30 seconds (`dirty_expire_centisecs = 3000`).

In addition, Linux defaults to `laptop_mode = 0`, forcing storage devices (NVMe and SATA SSDs) to wake up periodically from autonomous power states (APST PS3/PS4) even for minor background file modifications.

`REF-REQ-087` mandates:
1. **Dynamic VM Writeback Extension in UltraEndurance**: Extending writeback timer intervals from 5s to **60s** (`6000 centiseconds`) and page expiration to **120s** (`12000 centiseconds`) to coalesce disk writes into rare, concentrated bursts.
2. **Linux Laptop Mode Activation (`laptop_mode = 5`)**: Forcing disk flushes to execute only opportunistically when a disk read has already woken the storage device, preserving deep APST sleep states.
3. **Idempotent Baseline Snapshot & Rapid Rollback**: Recording baseline VM parameters at startup and restoring them in $< 5\,\text{ms}$ when transitioning back to `Balanced`, `Performance`, or upon AC charger connection.

---

## 2. Functional Requirements

### 2.1 Hardware Baseline Snapshot (`REF-REQ-087-F01`)
- Upon daemon startup or first evaluation, `MitigationEngine` must record the active values of:
  - `/proc/sys/vm/dirty_writeback_centisecs`
  - `/proc/sys/vm/dirty_expire_centisecs`
  - `/proc/sys/vm/laptop_mode`
- Values must be preserved in `HardwareBaselineState`.

### 2.2 UltraEndurance Actuation (`REF-REQ-087-F02`)
- When transitioning into `PowerProfileMode::UltraEndurance`:
  1. Write `6000` to `/proc/sys/vm/dirty_writeback_centisecs`.
  2. Write `12000` to `/proc/sys/vm/dirty_expire_centisecs`.
  3. Write `5` to `/proc/sys/vm/laptop_mode`.
  4. Flag `vm_writeback_modified = true`.

### 2.3 Idempotent Restoration & Rollback (`REF-REQ-087-F03`)
- When transitioning to `PowerProfileMode::Balanced`, `Performance`, or `PowerSaver`:
  - If `vm_writeback_modified` is true, write the original baseline values back to `/proc/sys/vm/`.
  - Reset `vm_writeback_modified = false`.
- Rollback must complete synchronously without heap allocations in $< 2.0\,\text{ms}$.

---

## 3. Non-Functional & Oracle Gate Invariants (`REF-TEST-051`)

1. **Zero Heap Allocation**: All file parsing and writeback manipulations must use fixed stack buffers (`char buf[32]`) and zero runtime dynamic allocations.
2. **Filesystem Graceful Fallback**: If write permissions to `/proc/sys/vm/` are denied (e.g. non-root test environments), the daemon must log a zero-crash debug notice and proceed gracefully without throwing exceptions.
3. **Roundtrip Fidelity**: Unit tests must verify that capture, modification, and restoration preserve baseline integer values exactly.
