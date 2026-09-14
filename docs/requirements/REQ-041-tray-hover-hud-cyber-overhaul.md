# REQ-041: System Tray Hover Cyber Telemetry HUD Overhaul

## 1. Context & Motivation
- **Ref-ID**: `REF-REQ-041`
- **Component**: `src/tray/tray_client.cpp`, `src/tray/tray_client.hpp`
- **Issue**:
  - The previous StatusNotifierItem (SNI) ToolTip description rendered raw, unaligned HTML text littered with `<hr>` dividers and unthemed text bullets.
  - In modern desktop environments like KDE Plasma 6 (CachyOS/Breeze/Emerald), `<hr>` produces thick, dated 3D beveled lines and consumes excessive vertical line count, causing Plasma's tooltip delegate (`maximumLineCount: 8`) to truncate half the telemetry.
  - Furthermore, duplicate titles ("⚡ WattCurb" repeated both in heading and body) and internal jargon ("Tier 5", "활성 제어 게이트") degraded usability and visual polish.

## 2. Technical Specifications & Functional Requirements
1. **Zero-Truncation Single-Block HTML Table Card Architecture**:
   - Structure the entire tooltip body using a cohesive HTML `<table>` layout with inline CSS styling compatible with Qt Quick / `QTextDocument`.
   - Treat the entire HUD card as a unified structured block to guarantee that Plasma 6 does not truncate elements.
   - Completely eliminate all `<hr>` tags.
2. **Cyber HUD 4-Section Layout**:
   - **Section 1: Power & Battery Status Header**:
     - Dynamic 10-segment Unicode battery gauge (`[▰▰▰▰▰▱▱▱▱▱]`) with state-dependent color encoding (emerald `#10b981`, amber `#f59e0b`, red `#ef4444`).
     - AC status: "AC 패스스루 (마모 0%)", "AC 충전 중", or "배터리 방전 중 (시간 남음)".
     - Total system watts highlighted in large amber typography (`#f59e0b`, 16px).
   - **Section 2: 2x2 Physical Hardware Domains Matrix**:
     - CPU domain: Watts, dynamic temperature color (cyan/amber/red), fan RPM.
     - GPU domain: Watts, active rendering vs D3Cold sleep status.
     - Platform & DRAM / I/O: Watts derived from `total - (cpu + gpu)` with bus power management status.
     - C-State & Wakeups: C3 sleep percentage and wakeups/sec with Zero-Wakeup rating.
   - **Section 3: Real-Time Top Culprits Card**:
     - Distinct deep purple/rose container card (`#1a1025`) with border (`#4a1d48`).
     - Ranks top 2 energy-consuming processes with command name, PID, attributed watts, and system power percentage share (`%`).
     - Shows clean resting state when idle.
   - **Section 4: Profile & Active Mitigation Footer**:
     - Current power profile (Performance, Balanced, SmartSave, UltraSave).
     - Mitigation engine status without cryptic tier numbers.
3. **Zero Dynamic Allocation & High-Performance Latency**:
   - Strict adherence to `AGENTS.md` Rule 9 (C++23 Zero-Cost Abstractions, stack-only buffers).
   - Entire `render_tooltip` function must execute in under 6 microseconds (`< 10,000 CPU cycles`).
