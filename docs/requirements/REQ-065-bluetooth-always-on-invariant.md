# REQ-065: Bluetooth Always-On Invariant Specification

- **Status**: Approved
- **Ref ID**: `REF-REQ-065`
- **Related Architecture**: `REF-ARCH-041`
- **Related Test**: `REF-TEST-030`
- **Created**: 2026-09-18
- **Category**: Physical Hardware Power Profiling, Mitigation Actuation, Hardware Invariant

---

## 1. Background & Problem Statement

In previous iterations, the `UltraEndurance` power profile attempted to aggressively minimize hardware leakage by disabling the Bluetooth radio subsystem via `rfkill block bluetooth` and soft-blocking the controller.
However, in practical daily usage:
1. Disabling Bluetooth interrupts peripheral connectivity (Bluetooth mice, keyboards, wireless headphones, and smartwatch/phone sync).
2. Users who leave Bluetooth enabled often find themselves unable to power on the adapter easily or experience authentication/firmware sync failures (`Failed to set mode: Authentication Failed (0x05)`) when rfkill states toggle asynchronously with `bluetoothd`.
3. The power draw of an idle Bluetooth LE (Low Energy) controller with adaptive sleep states is negligible (~15mW to 50mW).
4. Therefore, the user explicitly mandates: **"Bluetooth must remain strictly Always-On across all power profiles."**

---

## 2. Requirements & Invariant Enforcement

### 2.1 Absolute Bluetooth Non-Blocking Invariant
- Under **all** power profiles (`Performance`, `Balanced`, `PowerSaver`, and `UltraEndurance`), the daemon and profile actuation scripts **must never** execute `rfkill block bluetooth` or soft-block the Bluetooth device.
- `MitigationEngine::apply_power_profile(PowerProfileMode::UltraEndurance)` is explicitly updated to call:
  `set_bluetooth_blocked(false);`
- `power-profile-manager`'s `set_ultra` routine is updated to explicitly enforce:
  `rfkill unblock bluetooth` and ensure `bluetoothctl power on` is active.

### 2.2 Self-Healing & Controller Recovery Mechanism
- When unblocking Bluetooth (`set_bluetooth_blocked(false)`), the daemon triggers asynchronous user-space adapter activation:
  `execute_user_desktop_cmd("bluetoothctl power on 2>/dev/null &")`
- In recovery/failsafe scripts, if the kernel driver loses synchronization with `bluetoothd`, a safe, non-disruptive reset of the `btusb` transport module and service restart is executed:
  `modprobe -r btusb; sleep 0.2; modprobe btusb; systemctl restart bluetooth`

---

## 3. Verification & Oracle Gate Standards (`REF-TEST-030`)

1. **State Persistence Verification**:
   - Transition to `UltraEndurance` mode.
   - Verify `rfkill list bluetooth` shows `Soft blocked: no` across all interfaces (`tpacpi_bluetooth_sw`, `hci0`).
   - Verify `bluetoothctl show` reports `Powered: yes` and `PowerState: on`.
2. **Rollback Idempotency**:
   - Transitioning between `Performance` $\leftrightarrow$ `UltraEndurance` maintains uninterrupted Bluetooth peripheral links.
