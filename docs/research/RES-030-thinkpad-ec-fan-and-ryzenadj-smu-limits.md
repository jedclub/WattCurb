# REF-RES-030: ThinkPad EC Fan Control & RyzenAdj SMU Limit Interfaces

- **Status**: Approved
- **Ref ID**: `REF-RES-030`
- **Related Research**: [`REF-RES-022`](RES-022-vram-wifi-fan-bus-hardware-power-isolation.md), [`REF-RES-012`](RES-012-thinkpower-domain-survey-and-silicon-mechanisms.md), [`REF-RES-027`](RES-027-performance-mode-clock-collapse-incident.md)
- **Related Requirements**: [`REF-REQ-114`](../requirements/REQ-114-thinkpad-fan-thermal-assist-curve.md), [`REF-REQ-115`](../requirements/REQ-115-smu-thermal-power-limit-raise.md), [`REF-REQ-092`](../requirements/REQ-092-ultimate-performance-unleash-actuation.md)
- **Created**: 2026-09-22
- **Category**: Kernel Interfaces, ThinkPad Embedded Controller, AMD SMU, Thermal Management

---

## 1. Why the Fan and the SMU Are the Same Problem

The Performance profile of [`REF-REQ-092`](../requirements/REQ-092-ultimate-performance-unleash-actuation.md)
raises the CPU clock ceiling, the platform profile and (from
[`REF-REQ-115`](../requirements/REQ-115-smu-thermal-power-limit-raise.md)) the
SMU thermal limit. Raising a thermal limit without adding cooling converts a
*clock* limit into a *temperature* limit: the part boosts until it reaches the
new ceiling, then sits against it. On this class of machine the cooling headroom
already exists but is not used, because the embedded controller (EC) selects its
own conservative fan curve independently of the OS.

There is no OS-visible "boost the fan" flag. There are two orthogonal
interfaces, and both must be treated as optional actuations:

1. **`thinkpad_acpi` fan control** (`/proc/acpi/ibm/fan`): lets the OS set the
   fan level directly.
2. **AMD SMU limits** (`ryzenadj`): the power/thermal limits the firmware applies
   to the SoC, which the EC's own fan curve is often tuned against.

## 2. ThinkPad EC Fan Control (`thinkpad_acpi`)

### 2.1 Interface

When the `thinkpad_acpi` module is loaded with `fan_control=1`, the EC exposes a
writable text interface:

```
$ cat /proc/acpi/ibm/fan
status:         enabled
speed:          5385
level:          disengaged
commands:       level <level> (<level> is 0-7, auto, disengaged, full-speed)
commands:       enable, disable
commands:       watchdog <timeout> (<timeout> is 0 (off), 1-120 (seconds))
```

- `level 0`..`level 7` select the EC's discrete fan steps (0 = stopped,
  7 = highest discrete step).
- `level auto` returns control to the EC's automatic curve.
- `level full-speed` is a distinct top step above 7 (reported as
  `level: full-speed`).
- `level disengaged` is the "no restrictions" EC state.

**Without `fan_control=1` the node is read-only and every write is refused**; the
interface is therefore optional by construction, not merely by convention.

### 2.2 Observed behaviour on the reference host

- The EC's automatic curve tops out around **4.3k RPM** under sustained load on
  this model, while the fan itself is capable of roughly **5.4k RPM**
  (the host above reports `speed: 5385`).
- Aerodynamic shaft power scales with the **cube** of angular velocity
  ([`REF-RES-022`](RES-022-vram-wifi-fan-bus-hardware-power-isolation.md) §2.3):
  the fan at full throttle is worth on the order of **2.4 W**, which is real
  battery cost. This is why the interface is used only when the thermal ceiling
  has been raised, and not in the saving profiles.

### 2.3 Design consequence

Because a write takes full ownership of the fan, there is no "raise a floor"
mode: writing level *n* forces level *n*. WattCurb therefore applies a curve
while it needs the cooling (the raised-SMU profiles) and otherwise writes the
captured baseline back, so the EC resumes control.

## 3. AMD SMU Limits (`ryzenadj`)

### 3.1 Interface

`ryzenadj` writes SMU power-management tables through the AMD PCI mailbox. It is
a userspace tool outside this repository and is **not** a stable kernel ABI.
`ryzenadj -i` prints the current limits; the fields used here are:

| Field | Meaning |
| :--- | :--- |
| `STAPM LIMIT` | Sustained average power limit (mW) |
| `PPT LIMIT FAST` | Short-burst package power limit (mW) |
| `PPT LIMIT SLOW` | Sustained package power limit (mW) |
| `THM LIMIT CORE` | Core thermal limit (C) |

### 3.2 Availability

The reference host has `ryzenadj` at `~/.local/bin/ryzenadj`, which is **not** on
the system path. The daemon searches only root-owned system locations
(`/usr/local/bin`, `/usr/bin`) and treats the tool as absent otherwise, so the
SMU actuation is a no-op on such a host. This is deliberate: a root daemon must
not execute a user-writable binary during a privileged routine.

### 3.3 Risk

SMU limits directly bound package power and junction temperature. A wrong value
can cause thermal shutdown, unstable clocks, or (if the limit is raised without
cooling) accelerated wear. The actuation therefore captures the pre-change
baseline verbatim and restores it on every path out of the raised-limit profiles
and on daemon shutdown.

## 4. Summary of Adopted Interfaces

| Domain | Interface | Optionality | Baseline captured |
| :--- | :--- | :--- | :--- |
| Fan level | `/proc/acpi/ibm/fan` (`level N`/`auto`/`full-speed`) | Requires `thinkpad_acpi fan_control=1` | `level:` field read before the first write |
| SMU limits | `ryzenadj --tctl-temp/--stapm-limit/--fast-limit/--slow-limit` | Requires `ryzenadj` on a root-owned path | `ryzenadj -i` parsed before the first write |

## 5. Thermal Lever Topology on the Reference Host (2026-09-23)

Asked: *is everything now depending on one cooling fan?* The honest answer has two
halves - the hardware fact and the control fact.

### 5.1 The hardware fact: there is exactly one fan

| Evidence | Result |
| :--- | :--- |
| `ls /sys/class/hwmon/hwmon*/fan*_input` | **1** (`hwmon3` = `thinkpad`, `fan1_input`) |
| `/proc/acpi/ibm/fan` | a single `speed:` field |
| Chassis | ThinkPad L15 Gen 1 - one blower, no second fan header |

There is no second active cooling device to enlist, and no software path to add
one. Active cooling is a single-sourced dependency by construction.

### 5.2 The control fact: the fan is not the dominant lever

Comparing the two actuations that have actually been measured on this host:

| Lever | Range | Measured effect |
| :--- | :--- | :--- |
| **SMU STAPM** (power limit) | 6-10 W (EC default) -> 22-25 W (WattCurb) | 600 MHz -> **2.2-2.8 GHz** all-core; roughly **4x** |
| **Fan** | `auto` 3490 RPM -> `full-speed` 5348 RPM (both at Tctl 70 C) | holds the raised power budget under the ceiling; the clock delta was **not isolated** (see 5.5) |

The power limit sets how much heat the package may generate; the fan sets how
much of that budget can be spent before the ceiling is reached. The measured
order of magnitude is not close - WattCurb's performance recovery came from the
STAPM raise, and the fan keeps it from being clipped early. It would be wrong to
describe the current design as "everything rides on the fan"; it is accurate to
say the fan is *necessary but not sufficient*.

### 5.3 The OS is not in the CPU thermal loop at all

| Observation | Implication |
| :--- | :--- |
| Only one kernel thermal zone exists: `thermal_zone0` = `iwlwifi_1` (`step_wise`) | the CPU is **not** a kernel thermal zone on this host |
| 16 `Processor` cooling devices exist, **all at `cur_state = 0`** | the kernel's P-state throttling is idle - nothing is being throttled by Linux |
| `platform_profile` and `/proc/acpi/ibm/fan` are the only OS-writable thermal interfaces | firmware (EC + SMU) owns the die temperature; the OS owns only the fan level |

The CPU's thermal fate is decided inside the SMU/EC against a `Tctl` ceiling
(**70 C** on this host, firmware-fixed - see
[`REF-RES-031`](RES-031-bios-access-and-settings.md) section 3), not against a
Linux policy. The consequence: `thinkpad_acpi` fan control is the **only**
OS-visible thermal actuation available, and it operates *under* the firmware's
ceiling rather than replacing it.

### 5.4 The other half of the thermal budget is heat generation

Every non-fan lever WattCurb uses on the generation side, with the actuation that
implements it:

| Domain | Actuation |
| :--- | :--- |
| SoC power/thermal limits | `apply_smu_performance_limits` (STAPM/PPT/TDC via `ryzenadj`) |
| CPU clock ceiling | `assert_unrestricted_cpu_ceiling`, profile frequency caps, `set_cpu_boost` |
| Platform thermal table | `apply_power_profile` (`/sys/firmware/acpi/platform_profile`) |
| GPU | `set_gpu_dpm_level` / `set_gpu_power_profile_mode` |
| Storage | `set_nvme_apst_latency` / `set_nvme_power_control` |
| Wireless | `set_wifi_powersave` / `set_wifi_txpower` |
| Display | `cap_display_backlight` / refresh-rate control |
| Devices | `set_audio_codec_power_save`, `set_bluetooth_blocked` |
| Work itself | `apply_cgroup_cpu_quota`, `apply_cgroup_freeze`, `apply_core_affinity_cap`, `apply_sched_idle` / `apply_sched_batch`, `apply_timer_slack`, `apply_memory_reclaim`, `set_baloo_suspended` |

The last row is the only lever that reduces heat without touching any hardware
setting: less work scheduled means less power dissipated. That path is what makes
the saving profiles possible at all.

### 5.5 What is *not* established

- **The isolated clock benefit of `full-speed` vs `auto` was not measured.** Both
  fan states hold `Tctl` at 70 C under sustained all-core load, so the difference
  appears as higher sustained clock at the same temperature - but every
  measurement this session was taken while an unrelated project held the host at
  load 5-18, which is not a controlled comparison. The STAPM delta (~4x) is
  measured; the fan delta is not.
- **Failure behaviour is inferred, not tested.** If the fan stalls or WattCurb is
  stopped, the SMU should simply throttle against the 70 C ceiling (the fan
  command is not persistent across a daemon stop either - `restore_fan_level`
  returns the EC to `auto`). That is the designed safety path, but no fan-failure
  injection test has been run.
