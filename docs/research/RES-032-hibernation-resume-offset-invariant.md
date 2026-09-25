# [REF-RES-032] Hibernation `resume_offset` Invariant After Swapfile Retopology

**Date**: 2026-09-26 · **Host**: LENOVO 20U7S01000 (ThinkPad L15 Gen 1), CachyOS, systemd 261, Limine
**Related**: [`REF-REQ-112`](../requirements/REQ-112-boost-restoration-and-non-halting-memory-guard.md),
[`REF-ARCH-072`](../architecture/ARCH-072-memory-pressure-guard-and-ceiling-assertion.md),
[`REF-ARCH-073`](../architecture/ARCH-073-dynamic-swap-expansion.md)

## 1. Host hibernation configuration

| Interface | Value |
| :--- | :--- |
| `/swap/swapfile` | 32 GiB btrfs swapfile, subvol `/@swap` of `nvme0n1p2` (`UUID=86663e7b-a7ec-48f5-9457-aae5726966c3`), nocow, recreated **2026-09-22 13:58** (8 → 32 GiB, REF-ARCH-072 §3) |
| `zram0` | 6 GiB, priority 100 (zram-generator) |
| `/etc/fstab` | `/swap/swapfile none swap defaults,pri=-2 0 0` |
| Effective swap priority | **-1** (kernel default), not -2: util-linux `swapon` 2.42.3 only sets `SWAP_FLAG_PREFER` when `priority >= 0` (`sys-utils/swapon.c`), so the negative `pri=` is dropped; systemd 261 runs `/sbin/swapon --fixpgsz -o defaults,pri=-2` (`src/core/swap.c`, `swap_enter_activating`). This is not harmful - zram's 100 stays the fast tier, and with `resume=` set logind's device selection ignores priority. |
| `/etc/systemd/sleep.conf.d/10-hibernate-shutdown.conf` | `HibernateMode=shutdown` - the firmware advertises S4 but bounces straight back out of it (observed 2026-09-05), so the kernel writes the image itself and powers off |
| `/etc/default/limine` | `KERNEL_CMDLINE[default]+="... resume=UUID=86663e7b-... resume_offset=<N>"`; `limine-update` regenerates `/boot/limine.conf` from this file |
| `/usr/lib/systemd/system-sleep/50-shrink-before-hibernate` | pre: `drop_caches` + cgroup `memory.reclaim`; post: resume report; trace at `/var/log/hibernate-trace.log` |

## 2. The invariant and the failure mode

`resume_offset` must equal the physical offset of the **first extent** of
`/swap/swapfile`, divided by the 4 KiB page size:

```
N=$(btrfs inspect-internal map-swapfile -r /swap/swapfile)
```

Recreating, resizing-by-recreation, or relocating (btrfs balance/defrag of the
subvolume) the file changes that offset, so the stored value goes stale.

How a stale value removes hibernation (systemd 261,
`src/shared/hibernate-util.c`):

1. `find_suitable_hibernation_device_full()` reads `/sys/power/resume` and
   `/sys/power/resume_offset`, then enumerates `/proc/swaps` (zram entries are
   ignored).
2. For each swapfile it derives the devno and the first-extent offset, and with
   `resume=` set requires `devno == resume` **and** `offset == resume_offset`.
3. A stale offset matches nothing, the function returns `-ESTALE`,
   `hibernation_is_safe()` fails, and logind answers `CanHibernate` with `na`.
4. KDE Plasma hides the hibernate action when that answer is not `yes`.
   Suspend keeps working because `/sys/power/state` still lists `mem`.

## 3. Incident: 2026-09-22 swapfile retopology, discovered 2026-09-26

- A prior session's `swap_retopo.sh` (pkexec; journal shows the invocation) recreated
  the swapfile 8 → 32 GiB on 2026-09-22 13:58:41. Kernel log:
  `Adding 33554428k swap on /swap/swapfile. Priority:-1 extents:2 across:34718088k SS`.
- `resume_offset` stayed at **3577677**, the 8 GiB file's first-extent offset from the
  2026-09-05 hibernation setup, while the new file's first extent sits at **31634589**.
- Result: `CanHibernate` returned `na` and the hibernate action disappeared from the
  power menu. No hibernation had been attempted since (last trace entry 2026-09-20).
- Recovery, applied while the machine stayed up:
  - `printf 31634589 > /sys/power/resume_offset` → `CanHibernate` flipped
    `na` → `yes` immediately (logind re-reads the values per call).
  - `/etc/default/limine` updated (`resume_offset=31634589`) and `limine-update`
    run (rc=0); the two main `/boot/limine.conf` entries carry the new offset.
    The ~100 snapshot entries written by `limine-snapper-sync` keep the offset
    captured when each snapshot was made; booting one is unaffected, only
    resuming an image from it would need the same fix.
- Not verified: a full hibernate/resume cycle - it powers the machine off. What is
  verified is the availability gate (`CanHibernate=yes`) and the offset identity
  reported by `btrfs map-swapfile` against `/sys/power/resume_offset`.

## 4. Procedure after any `/swap/swapfile` recreation

```
N=$(pkexec btrfs inspect-internal map-swapfile -r /swap/swapfile)
echo "$N" | sudo tee /sys/power/resume_offset                 # runtime, restores CanHibernate now
sudo sed -i -E "s/resume_offset=[0-9]+/resume_offset=$N/" /etc/default/limine
sudo limine-update                                             # regenerates /boot/limine.conf
busctl call org.freedesktop.login1 /org/freedesktop/login1 \
    org.freedesktop.login1.Manager CanHibernate                # expect "yes"
```

Never recreate or relocate `/swap/swapfile` without this step. WattCurb's own
dynamic files (REF-ARCH-073) are separate (`/swap/wattcurb-dyn-*`), are never the
hibernation target, and their creation/release does not touch this invariant.

## 5. Self-healing guard (installed 2026-09-26)

A stale offset is not only a lost menu entry. If hibernation were allowed while
the kernel command line is stale, the image would be written using the live
offset and the next boot would resume from the stale one: systemd 261's
initramfs generator prefers the command line over the EFI `HibernateLocation`
variable (`acquire_hibernate_info()` in
`src/hibernate-resume/hibernate-resume-config.c` takes `info->cmdline` when
present), so the two must always agree.

Host-side units (deliberately kept under `/etc/systemd/system`, which is what
`system-config-backup` / `tp-backup` sweeps, so they are captured and restored):

| Path | Role |
| :--- | :--- |
| `/etc/systemd/system/hibernation-resume-offset-guard.sh` | the guard |
| `/etc/systemd/system/hibernation-resume-offset-guard.service` | oneshot, `After=swap-swapfile.swap`, runs before `graphical.target` |
| `/etc/systemd/system/hibernation-resume-offset-guard.path` | `PathChanged=/swap/swapfile`, heals mid-session as well |

Behaviour: recompute `N` with `btrfs inspect-internal map-swapfile -r`; repair
`/etc/default/limine` and `/boot/limine.conf` (single `.bak-hibernation-guard`
copy, `sed` output validated against the source line count and the new value,
snapshot entries included because every entry points at the same live file);
then write `/sys/power/resume_offset`. **Interlock**: when the persistent
locations cannot be repaired (UKI-embedded cmdline, read-only ESP, failed
validation), the runtime value is left untouched on purpose - hibernation stays
unavailable rather than producing an image the next boot cannot resume.
`StartLimitIntervalSec=0` on both units: the first draft used a directory watch
plus `PathExists=`, which fired three starts in 200 ms and tripped
`start-limit-hit`, wedging the path unit.

Verified 2026-09-26: writing `3577677` into `/sys/power/resume_offset` and
starting the service restored `31634589` and `CanHibernate` stayed `yes`;
`touch /swap/swapfile` triggered the path unit and the run was a no-op. Not
verified: boot ordering and the mandatory `swap-swapfile.swap` dependency (both
need a reboot) and, as in section 3, a real hibernate/resume cycle.
