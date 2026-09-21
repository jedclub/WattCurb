# REF-REQ-108: UltraEndurance Liveness Guarantee

## 1. Contract
> UltraEndurance may be **slow**. It may not **stop responding**.

Slowness is the profile's stated bargain: a 1.4 GHz ceiling, boost disabled, a
40% GPU clock cap and a 48 Hz panel. Those are not defects and are not relaxed
here. What is forbidden is any actuation whose failure mode is the machine
appearing hung - input that does not register, a window that stops repainting,
an application that has to be waited out.

## 2. Functional Requirements

### REQ-108.1: Tier 0..3 are never demoted to the idle class
`MitigationEngine::is_stall_shielded()` refuses `SCHED_IDLE` demotion and cgroup
freezing for every process classified Tier 0..3 (`CriticalImmune` ..
`UserInteractive`), **in every profile and whether or not audio is playing**.

Before this, the equivalent shield
([`REF-REQ-096.6`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-096-audio-continuity-guarantee.md))
was conditional on a PCM stream being RUNNING. Outside playback the same
processes could be moved to `SCHED_IDLE` and given 100 ms of timer slack, which
is indistinguishable from a stall at the keyboard.

Tier 4/5 background workers (`baloo`, `updatedb`, runaway scripts) remain fully
throttleable, so the saving is confined to work nobody is waiting on.

### REQ-108.2: Writeback coalescing is bounded
UltraEndurance sets `vm.dirty_writeback_centisecs = 1500` (15 s),
`vm.dirty_expire_centisecs = 3000` (30 s) and `vm.laptop_mode = 2`, replacing
60 s / 120 s / 5.

A 60-second window lets a full minute of dirty pages accumulate and discharge in
one burst, blocking every `fsync` behind it, and it widens the data-loss window
on power cut to the same 60 seconds. The long window was chosen to let a
spinning disk stay parked; the only block device on this class of machine is
NVMe, which has no spin-up to amortise.

### REQ-108.3: Input devices are never suspended
`apply_usb_runtime_pm_auto()` skips any USB device or interface whose class is
HID (`03`) or Bluetooth (`e0`), at both the device and the interface level.
Already implemented; restated because a first input after resume that takes
hundreds of milliseconds is exactly the failure this requirement exists to
prevent.

### REQ-108.4: No process is ever frozen
`apply_cgroup_freeze()` refuses every freeze request and falls back to graceful
idle throttling
([`REF-REQ-044`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-044-absolute-zero-kill-and-non-halting-safety.md)).
Already implemented and unchanged.

### REQ-108.5: SMT stays enabled
Offlining half the logical CPUs in the profile whose failure mode is stalling
removes the scheduling capacity the compositor and input path need
([`REF-REQ-098`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-098-system-liveness-invariant.md)).
Already implemented and unchanged.

## 3. Known Limitation
Shielding is by classifier **tier**, not by what the user is actually
interacting with. A Tier 4/5 process the user is waiting on is still
throttleable, and a Tier 3 process they have forgotten about is shielded. The
graphical-session check (REQ-101) covers most of the first case.

## 4. Verification
[`REF-TEST-061`](file:///home/jedclub/Develop/WattCurb/tests/test_units.cpp)
covers the shield's independence from playback state and the writeback bounds.
Input latency and flush behaviour are host properties and are re-measured per
REQ-106.6, not asserted in the sandboxed suite.
