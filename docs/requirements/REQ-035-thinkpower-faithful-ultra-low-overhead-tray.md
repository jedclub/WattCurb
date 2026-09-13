# [REF-REQ-035] ThinkPower-Faithful Ultra-Low-Overhead Desktop Tray Client Specification

## 1. Context & Motivation
- **Ref-ID**: `REF-REQ-035`
- **Related Requirements**: [`REF-REQ-025`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-025-desktop-tray-and-bidirectional-control.md), [`REF-REQ-026`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-026-binary-seqlock-data-supply-and-json-elimination.md)
- **Related Research**: [`REF-RES-012`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-012-thinkpower-domain-survey-and-silicon-mechanisms.md), [`REF-RES-017`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-017-thinkpower-tray-ux-and-zero-overhead-client.md)
- **Target Target**: `wattcurb-tray` (Standalone Executable)

The desktop tray client is the user's primary interface to WattCurb. It must faithfully preserve the **rich user experience of ThinkPower** (real-time discharge/charge feedback, dense process hover tooltips, and one-click profile switching) while strictly enforcing **WattCurb's extreme optimization rules**: zero polling, zero heap allocations in hot paths, and sub-2MB resident set size.

---

## 2. Functional Requirements

### REQ-035.1: StatusNotifierItem (SNI) Protocol Compliance
- The client shall register the D-Bus service `org.kde.StatusNotifierItem-wattcurb` on the session bus.
- The client shall register with `org.kde.StatusNotifierWatcher` so that KDE Plasma, Waybar, and other desktop panels automatically embed the tray icon.
- Supported categories: `Hardware`, Status: `Active`.

### REQ-035.2: ThinkPower-Faithful Telemetry Tooltip (On-Demand Hover)
- When the desktop shell queries the `ToolTip` property over D-Bus:
  - The client shall read the latest state from `/dev/shm/wattcurb_state.shm` via `read_atomic()`.
  - The client shall format a dense technical tooltip containing:
    1. **System Power**: Current drain in Watts (e.g. `7.4 W`) and status (`Discharging` / `Charging` / `AC Online`).
    2. **Battery State**: Capacity percent, health percent, and estimated time to empty/full.
    3. **Silicon & Thermal**: CPU package power, GPU power, CPU temperature, and fan RPM.
    4. **Top 2 Drain Culprits**: Process name (`comm`), PID, and attributed drain in $mW$.
    5. **Current Profile**: `Balanced`, `PowerSaver`, or `UltraEndurance`.
  - Formatting must occur inside a stack-allocated buffer (`char buf[512]`) with **zero dynamic heap allocation**.

### REQ-035.3: Bidirectional Action Menu (Click & ContextMenu)
- Left Click:
  - Cycle to the next power profile (`Balanced` $\rightarrow$ `PowerSaver` $\rightarrow$ `UltraEndurance` $\rightarrow$ `Balanced`).
- Right Click (ContextMenu / DBusMenu):
  - Radio items for explicit profile selection.
  - "Rescan Now" option to request an immediate 5-second observation window from the daemon.
  - "Quit Tray" option (closes only the tray UI; daemon continues running).

### REQ-035.4: Zero-Wakeup Polling Ban
- The tray client shall **NEVER** run a periodic timer to poll telemetry or redraw the icon.
- The client shall sleep in `epoll_wait` on the D-Bus file descriptor, waking **only when the user hovers, clicks, or the daemon explicitly notifies state changes**.

### REQ-035.5: Visual Battery Percentage & Gauge in Tray Icon (Breeze Dynamic Status Icons)
- The client must dynamically resolve the exact battery level (quantized to 10% steps: `000` to `100`), charging state, and active power profile into KDE Breeze official icons:
  - Discharging: `battery-%03d-profile-%s` (e.g. `battery-080-profile-balanced`, `battery-070-profile-powersave`)
  - Charging: `battery-%03d-charging-profile-%s` (e.g. `battery-080-charging-profile-balanced`)
  - AC Passthrough: `battery-100-profile-balanced` / `battery-080-charging-profile-balanced`
- The system tray icon must visually convey the battery level and charging state directly without hovering.

### REQ-035.6: Real-Time Panel Text Label (`XAyatanaLabel`)
- The client shall expose `XAyatanaLabel` and `XAyatanaLabelGuide` properties:
  - Label format: `"%u%% (%c%.1fW)"` (e.g., `80% (-14.2W)` or `80% (+25.0W)`).
  - Label guide: `" 100% (+00.0W)"`.
  - Enables desktop shells supporting Ayatana AppIndicator (including KDE Plasma) to render real-time percentage and Watts directly beside the icon in the panel text font.

### REQ-035.7: Full Native Context Menu Protocol (`com.canonical.dbusmenu`)
- The client shall implement `/MenuBar` with `com.canonical.dbusmenu` protocol:
  - Header Item 1 (Disabled, Bold): `⚡ 80% (Est: 4h 15m) | -14.2 W (On Battery)`
  - Header Item 2 (Disabled): `🔋 Battery Health: 94.2% (95 cycles) | 63°C Fan 4300 RPM`
  - Header Item 3 (Disabled): `🔥 Top 1: plasmashell (11.0 W) | Top 2: agy (5.9 W)`
  - Separator
  - Radio Item: `● Performance (고성능 모드 - 4.1GHz Boost)`
  - Radio Item: `● Balanced (균형 모드 - 기본 권장)`
  - Radio Item: `○ Smart Save (스마트 절전 모드 - 1.7GHz)`
  - Radio Item: `○ Ultra Save (초절전 모드 - 1.4GHz, 48Hz)`
  - Separator
  - Action Item: `🔍 지금 전력 소비 정밀 분석 (Rescan Now)`
  - Action Item: `📊 KDE 시스템 모니터 열기 (System Monitor)`

---

## 3. Non-Functional & Extreme Optimization Mandates

1. **Memory Footprint**:
   - Resident Set Size (RSS) must remain **$\le 2.0\text{MB}$** under active desktop interaction.
2. **Binary Footprint**:
   - Release binary size must remain **$\le 250\text{KB}$** via LTO, `-s`, `-fno-rtti`, and dead-code elimination.
3. **Execution Latency**:
   - Tooltip generation and D-Bus reply must complete in **$\le 100\mu\text{s}$** ($0.1\text{ms}$).
4. **Privilege Decoupling**:
   - Must run 100% unprivileged as the desktop user ($UID != 0$).
