# REF-REQ-078: Deep Battery Drain Telemetry & Standalone Report Window

## 1. Overview & Operational Context
WattCurb continuously accumulates system, hardware domain, and battery discharge telemetry inside a 7-day in-memory lockless ring buffer (`HistoryRingBufferShm` at `/dev/shm/wattcurb_history.shm`). While the real-time matrix dashboard provides instantaneous insight into current power consumption, users require a comprehensive historical diagnostic audit—a standalone deep report window—that examines the entire accumulated log stream.

This requirement defines the **Deep Battery Drain Analytics Engine** and the **Standalone Battery Drain Report Window (`BatteryReportWindow`)**, empowering users to identify exact hardware components and runaway background processes responsible for battery drain over extended operational windows.

---

## 2. Functional Requirements

### 2.1 Full In-Memory Battery Log Extraction & Integration
1. ** 전수 히스토리 데이터 추출 (Full Extraction)**:
   - The engine must retrieve all valid telemetry entries (up to 60,480 samples, representing 7 continuous days at 10-second intervals) from `/dev/shm/wattcurb_history.shm`.
   - Lockless Seqlock reading (`read_snapshot`) must be utilized without locking or blocking the root daemon.
2. ** 방전 세션 식별 (Discharge Session Segmentation)**:
   - Identify continuous discharging intervals where `battery_state == 1`.
   - Compute cumulative discharge metrics:
     - Total discharged energy in Watt-hours ($Wh$), milliampere-hours ($mAh$), and Joules ($J$).
     - Total battery capacity drop ($\Delta Bat\%$) and total active discharge duration (Hours & Minutes).
     - Global average discharge rate ($W_{avg}$) and peak instantaneous discharge rate ($W_{peak}$) with exact timestamps.

### 2.2 Physical Hardware Domain Drain Breakdown
Attributed battery energy consumption must be segregated into physical hardware domains:
1. **CPU Subsystem**: Package, Compute Cores, Uncore, and DRAM memory bus energy ($Wh$ and $\%$ of total discharge).
2. **GPU Silicon Subsystem**: Integrated AMD/Intel or discrete GPU compute/render engine and VRAM power ($Wh$ and $\%$).
3. **Display & Backlight Subsystem**: Screen illumination power consumption based on active brightness levels ($Wh$ and $\%$).
4. **Storage Subsystem**: NVMe SSD active/APST power consumption and disk write/read activity ($Wh$ and $\%$).
5. **Platform & Unaccounted Loss**: WiFi wireless transceiver, PCIe bus ASPM overhead, motherboard VRM conversion losses, and thermal fan power ($Wh$ and $\%$).

### 2.3 Top Process Drain Culprits & Causation Mechanisms
1. **상위 배터리 소모 프로세스 순위화 (Ranking)**:
   - Rank top background and foreground processes responsible for draining battery reserves.
   - For each culprit process, display:
     - PID, Process Name (`comm`), User ID (`uid`).
     - Cumulative attributed battery drain ($Wh$) and average power draw ($W$).
     - Energy share percentage ($\%$) relative to total process power.
     - WattCurb Drain Index (`wdi_score`).
     - Primary hardware domain affected (`primary_hw_domain`, e.g. "CPU Compute", "GPU Silicon", "C-State Wakeup Tax").
     - Exact hardware causation mechanism (`hardware_mechanism`, e.g. "Non-blocking Timer Wakeups (450/s)", "GPU GFX Loop (98% load)", "Cross-CCX Thread Thrashing").
2. **진단 및 완화 가이드 (Diagnostic Insights & Actionable Guidance)**:
   - Deep Sleep (C3+ C-State) residency evaluation: Alert if system spent $< 70\%$ of time in deep C-states due to excessive wakeups.
   - Targeted recommendations: Suggest cgroup throttling, timer slack alignment, or Ultra Battery profile switching.

### 2.4 Standalone KDE Plasma 6 Native Report Window (`BatteryReportWindow`)
1. **독립 창 실행 (Standalone Window)**:
   - The report must open as a distinct, dedicated floating window (`QQuickWindow` / `Window`), allowing the user to examine the report side-by-side with other applications or maximize it independently of the main dashboard.
2. **대시보드 상호작용 (Dashboard Trigger)**:
   - The main dashboard header bar and battery card must feature a prominent **[⚡ 배터리 정밀 분석 리포트 (Deep Report)]** action button.
3. **CLI 독립 실행 (CLI Invocation)**:
   - Support launching the report window directly via terminal:
     `wattcurb-dashboard --report` or `wattcurb-dashboard -r`.
   - Support textual report dump via daemon CLI:
     `wattcurb --battery-report`.
4. **리포트 텍스트 복사/내보내기 (Export & Clipboard)**:
   - Provide a 1-click button to copy the executive markdown/text diagnostic summary to the desktop clipboard for issue reporting and bug filing.

---

## 3. Non-Functional Requirements & Performance Constraints
1. **Zero-Wakeup & Zero-UI-Lag**:
   - Analysis of 60,480 samples must execute in $< 10\text{ ms}$ on modern x86-64 processors using contiguous memory traversals and SIMD vectorization.
   - Zero dynamic heap allocation in the core analysis loop.
2. **13-Language Internationalization (l10n)**:
   - All report headers, metrics, columns, and recommendations must be fully localized via `src/core/l10n.hpp`.
3. **Cyberpunk Aesthetic Consistency**:
   - Dark btop-inspired theme (`#0b0e12` background, neon cyan, emerald, amber, and crimson accents) matching the existing WattCurb visual standard.
