# [REF-RES-031] BIOS Access on the Reference Host (ThinkPad L15 Gen 1)

**Date**: 2026-09-23 · **Host**: LENOVO 20U7S01000 (ThinkPad L15 Gen 1), AMD Ryzen 7 PRO 4750U
**Related**: [`REF-REQ-123`](../requirements/REQ-123-ec-smu-power-cap-diagnosis.md),
[`REF-REQ-125`](../requirements/REQ-125-fan-full-speed-keyword.md)

## 1. Identity

| Field | Value | Source |
| :--- | :--- | :--- |
| Vendor | LENOVO | `/sys/class/dmi/id/sys_vendor` |
| Product | **ThinkPad L15 Gen 1** (20U7S01000) | `product_version` / `product_name` |
| BIOS | **R19ET56W (1.40)**, dated **2026-05-26** | `bios_version` / `bios_date` |

Earlier notes in this repository called the host a "T14/P14s class" machine. It is
an **L15 Gen 1**; the fan and EC behaviour recorded in REF-REQ-114/118/123/125
belongs to this model.

## 2. Access paths available on this host

| Path | Scope | Privilege |
| :--- | :--- | :--- |
| `/sys/class/dmi/id/*` | Vendor, model, BIOS version/date | any user |
| **`/sys/class/firmware-attributes/thinklmi/`** | **77 BIOS settings, read AND write** | root (`current_value` is 0600) |
| `fwupdmgr` | Firmware/BIOS update inventory, HSI security attributes | any user |
| `/sys/firmware/acpi/tables/` | DSDT + 11 SSDTs (EC/thermal methods live here) | root |
| `/sys/firmware/efi/efivars/` | 133 EFI variables | root |

`think-lmi` is the useful one: the kernel's
`sysfs-class-firmware-attributes` ABI exposes every BIOS setup item as
`attributes/<name>/current_value` (writable), with `possible_values`, `type` and
`display_name` alongside it.

### 2.1 Write conditions

- **No BIOS password is set**: `authentication/Admin/is_enabled = 0`,
  `System/is_enabled = 0`, `Power-on/is_enabled = 0`. Per the kernel ABI
  documentation, *"On Dell, Lenovo and HP systems, if Admin password is set, then
  all BIOS attributes require password validation"* - with no Admin password there
  is nothing to validate, so root may write.
- **Lenovo save limitation**: `attributes/save_settings = single`. Lenovo firmware
  allows only **48 attribute saves** in single mode; `echo bulk > save_settings`
  followed by `echo save > save_settings` lifts the limit. Exceeding 48 without
  bulk mode requires entering the BIOS setup to clear the error.
- **A reboot is required** for changes to take effect (`attributes/pending_reboot`
  currently reads `0`).

## 3. Thermal and power settings (the reason this survey exists)

| BIOS setting | Current value | Most permissive |
| :--- | :--- | :--- |
| `AdaptiveThermalManagementAC` | **MaximizePerformance** | MaximizePerformance |
| `AdaptiveThermalManagementBattery` | Balanced | MaximizePerformance |
| `AMDPowerNowTechnologyAC` | MaximumPerformance | MaximumPerformance |
| `AMDPowerNowTechnologyBettery` | BatteryOptimized | MaximumPerformance |
| `AMDPowerNowTechnology` | Enable | Enable |
| `CPUPowerManagement` | Enable | Enable |

**Conclusion for REF-REQ-123:** the AC thermal and power settings are already at
their most permissive values, and the firmware still holds `THM LIMIT CORE` (Tctl)
at **70 C** and STAPM at 6-10 W. There is therefore **no BIOS setting that raises
the 70 C ceiling** - it is a fixed policy of this model's EC firmware, not a
misconfigured thermal mode. The only remaining levers are:

1. `AdaptiveThermalManagementBattery` -> `MaximizePerformance` (affects battery
   operation only; the host runs on AC for the reported defect);
2. `attributes/debug_cmd` - a write-only, vendor-only interface the kernel docs
   describe as *"should only be used when recommended by the BIOS vendor"*;
3. a BIOS update that changes EC behaviour.

## 4. BIOS updates

`fwupdmgr refresh --force` + `get-updates` on 2026-09-23 offered:

- **Secure Boot dbx** (Microsoft, 20260707)
- Samsung SSD 980 PRO firmware

and **no System Firmware (BIOS) update**: the `System Firmware` device reports
`0.1.40` with no candidate. BIOS 1.40 is the newest available through LVFS for
this model at this time.

## 5. Other observations from the dump

- `SecureBoot = Disable`, `SecureRollBackPrevention = Disable`,
  `TSME = Disable` (Transparent SME memory encryption off).
- `BIOSUpdateByEndUsers = Enable`, `WindowsUEFIFirmwareUpdate = Enable`.
- `KeyboardLayout = Korean`, `SetupUI = SimpleText`.
- `WakeOnLAN = ACOnly`, `AlwaysOnUSB = Enable`, `OnByAcAttach = Disable`.

## 6. How to re-run this survey

```sh
# Identity (no privilege needed)
cat /sys/class/dmi/id/product_version /sys/class/dmi/id/bios_version

# Full BIOS dump (root)
sudo sh -c 'for a in /sys/class/firmware-attributes/thinklmi/attributes/*/; do
  n=$(basename "$a"); v=$(cat "$a/current_value" 2>/dev/null | head -1)
  [ -n "$v" ] && printf "%-34s = %s\n" "$n" "$v"; done' | sort

# Thermal/power subset
for n in AdaptiveThermalManagementAC AdaptiveThermalManagementBattery \
         AMDPowerNowTechnologyAC CPUPowerManagement; do
  printf '%s = %s\n' "$n" "$(sudo cat /sys/class/firmware-attributes/thinklmi/attributes/$n/current_value)"
done
```

Note that the attribute files are root-only (`current_value` is mode 0600), so a
non-root reader sees empty values rather than an error - which is why the first
attempt in this session returned blanks for every attribute.
