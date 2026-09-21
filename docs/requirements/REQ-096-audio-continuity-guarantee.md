# REF-REQ-096: Audio Continuity Guarantee

## 1. Context & Measured Cause
Music stuttered in the saving profiles. The daemon already healed the sound
server's scheduling (`audit_and_heal_audio_stack()` sets nice -19 and full-core
affinity on PipeWire), and the process allowlist already made it immune to
throttling - yet dropouts persisted.

The cause is not scheduling, it is **idle exit latency**. Measured on this
platform (`/sys/devices/system/cpu/cpu0/cpuidle/state*/latency`):

| State | Exit latency | Target residency |
| :--- | ---: | ---: |
| POLL | 0 us | 0 us |
| C1 | 1 us | 2 us |
| C2 | 18 us | 36 us |
| **C3** | **350 us** | 700 us |

A `cpu_dma_latency` constraint was held **only in Performance mode** (at 0 us).
In Balanced, PowerSaver and UltraEndurance nothing constrained the idle governor,
so it was free to park in C3. A 350 us wake-up against a PipeWire quantum is
enough to miss the deadline and underrun the buffer - audible as a dropout.

## 2. Functional Requirements

### REQ-096.1: Detect actual playback, not the mere presence of a sound server
Active playback is determined from ALSA's own substream status
(`/proc/asound/card*/pcm*p/sub*/status`): `state: RUNNING` means samples are
moving. The same file yields `owner_pid`, the process holding the PCM.
The probe must cost far less than one evaluation cycle.

### REQ-096.2: Hold a bounded latency ceiling in EVERY profile
While a playback stream is RUNNING, the daemon holds `/dev/cpu_dma_latency` at
**100 us**, in all four profiles. The value must sit strictly between the
shallowest non-zero idle state and the deepest one, so that:
- POLL / C1 / C2 remain available - idle power saving is not abandoned;
- C3 is excluded - the state that breaks the deadline cannot be entered.

This is a *ceiling*, not the Performance clamp. The kernel takes the **minimum**
of all `cpu_dma_latency` holders, so the audio fd (100 us) and the Performance fd
(0 us) compose without any interaction logic, and releasing one leaves the other
intact.

### REQ-096.3: Release when playback stops
The constraint is dropped as soon as no stream is RUNNING, so an idle machine
returns to full C3 residency. Engage and release must be idempotent and must not
leak descriptors.

### REQ-096.4: Immunity by stream ownership, not by name
Whatever currently owns a running PCM stream is immune to every mitigation,
regardless of its process name. The previous allowlist covered
`pipewire`/`wireplumber`/`pulseaudio`/`jackd` only.

### REQ-096.5: Codec runtime suspend must not arm under a live stream
UltraEndurance sets `snd_hda_intel power_save=10` with
`power_save_controller=Y`. While a stream is RUNNING this is parked at `0`, and
the prior value is restored once playback stops.

### REQ-096.6: Shield the media pipeline, not just the sound server
`owner_pid` names the process holding the PCM, which is the sound server. A
player demoted to `SCHED_IDLE` or frozen stops refilling its buffer just as
surely, and a media player classifies as Tier 3 (`UserInteractive`), which the
mitigation ladder is allowed to throttle.

While a stream is RUNNING, therefore:
- no process at Tier 0..3 may be demoted to `SCHED_IDLE` or frozen;
- Tier 4/5 background workers (`baloo`, `updatedb`, runaway scripts) remain fully
  throttleable, so playback does not suspend power saving wholesale.

## 3. Verification
[`REF-TEST-059`](file:///home/jedclub/Develop/WattCurb/tests/test_units.cpp)
checks the ceiling against the **host's real cpuidle table** rather than a
hardcoded constant, the probe cost, owner/active coherence, owner immunity, the
floor's engage/release lifecycle, and tier shielding - the last by renaming the
test process itself, so the result is deterministic rather than dependent on
whatever happens to be running.

## 4. Known Limitation
Shielding is by process **tier**, not by membership of PipeWire's client graph.
A Tier 4/5 process that genuinely feeds audio would still be throttleable, and a
Tier 3 process that has nothing to do with playback is shielded for the duration.
Resolving this exactly needs `libpipewire` / `pw-dump` to enumerate real stream
clients, which would add a dependency the tray and daemon do not otherwise carry.
