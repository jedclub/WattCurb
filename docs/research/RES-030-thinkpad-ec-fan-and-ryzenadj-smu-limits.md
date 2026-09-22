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
