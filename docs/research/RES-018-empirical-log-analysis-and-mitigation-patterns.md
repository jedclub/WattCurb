# [REF-RES-018] Comprehensive Empirical Telemetry & Mitigation Log Analysis

## 1. Executive Summary

WattCurb 백그라운드 데몬 가동 이후 축적된 **6,735행의 시스템 저널 로그**와 **600개(최근 30분)의 RAM 텔레메트리 링버퍼 데이터**를 전수 추출하여 정밀 통계 및 거동 패턴 분석을 수행했습니다.

분석 결과:
1. **하드웨어 전력 거동**: 전체 시스템 소비 전력은 최저 **10.32W**에서 최고 **40.23W**, 평균 **14.38W** 수준을 기록하였으며, C3 심층 절전 체류율은 평균 **71.3%**를 유지하고 있습니다.
2. **프로세스 완화 진동(Flapping) 식별**: 총 **3,336회의 완화(Mitigation)**와 **3,231회의 롤백(Rollback)**이 발생하여 거의 1:1로 빠르게 진동하는 고빈도 플래핑 현상(총 49개 고빈도 프로세스)이 확인되었습니다.
3. **핵심 대화형 데스크톱 도구 오분류 발견**: 사용자가 직접 조작하는 Wayland 터미널(`foot`, 224회 진동), 한글 입력기(`fcitx5`, 48회 진동), 에디터/IDE(`opencode`, 92회), AI 코딩 도구(`agy`, 67회), KDE 데스크톱 데몬(`kded6`, 81회) 등이 순간 부하 시 폭주 태스크로 간주되어 스케줄링 캡(Nice=10, 코어 마스크 제한)을 당하고 바로 복원되는 불필요한 스케줄러 syscall 폭풍이 발생하고 있었습니다.
4. **브라우저 탭 전력 드레인**: Google Chrome 렌더러/워커 프로세스들이 전체 완화 이벤트의 50% 이상을 차지하며 주된 백그라운드 전력 누수원으로 확인되었습니다.

---

## 2. Telemetry Dataset Topology

| 항목 | 수집 출처 | 샘플 수 / 라인 수 | 분석 기간 |
| :--- | :--- | :--- | :--- |
| **시스템 저널 감사 로그** | `systemd journald` (`wattcurb.service`) | **6,735 줄** (이벤트 6,568건) | 2026-09-17 ~ 2026-09-18 |
| **RAM 히스토리 링버퍼** | `/dev/shm/wattcurb_history.shm` | **600개 샘플** (30분 슬라이딩 윈도우) | 최근 실시간 30분 |
| **추적된 고유 PID 수** | 커널 프로세스 이벤트 | **833개 PID** | 누적 전체 |
| **고빈도 플래핑 PID (>=10회)**| 스케줄링 완화 진동 PID | **49개 PID** | 누적 전체 |

---

## 3. Hardware Power & Silicon Behavioral Patterns

RAM 링버퍼 600개 샘플 전수 통계 분석 결과:

```text
=== Power & Silicon Telemetry Statistics (N=600 samples) ===
  System Power (W): Min=10.32, Mean=14.38, Max=40.23
  CPU Power    (W): Min= 0.30, Mean= 4.89, Max=19.21
  GPU Power    (W): Min= 0.05, Mean= 0.63, Max= 9.10
  CPU Temp   (°C) : Min= 41.0, Mean= 45.3, Max=71.0
  Frequency  (MHz): Min= 1347, Mean= 1572, Max=3214
  C3 Deep Sleep % : Min=   0%, Mean=71.3%, Max= 88.0%
```

### 3.1 하드웨어 전력 분석 인사이트
1. **유휴 기본 전력 바닥 (10.32 W)**:
   - 디스플레이 패널(약 2.5~3.0W), 플랫폼 & 손실(DRAM, VRM, Wi-Fi 약 3.8W), CPU 정적 전력(약 0.3~1.0W)의 합계가 최저 10.3W로 수렴합니다.
2. **작업 시 피크 전력 (40.23 W)**:
   - 컴파일, 빌드, 복잡한 웹 렌더링 시 CPU Package가 19.2W까지 상승하며 시스템 전력이 40W까지 도달합니다.
3. **iGPU 전력 기여도**:
   - 평상시 0.05W~0.6W 수준이나, 브라우저 스크롤 및 Wayland 합성 시 순간적으로 9.1W까지 상승합니다.
4. **C3 C-State 심층 절전 건전성**:
   - 평균 체류율이 **71.3%**로 매우 양호하게 깊은 절전 상태를 유지하고 있습니다.

---

## 4. Empirical Process Mitigation & Flapping Analysis

### 4.1 상위 완화/롤백 프로세스 식별 (Top Oscillating PIDs)

| PID | 프로세스 이름 (`comm`) | 이벤트 수 (완화+롤백) | 진동 횟수 (Oscillations) | 프로세스 성격 및 분류 |
| :--- | :--- | :--- | :--- | :--- |
| **582406** | `(exited)` | 584 회 | 292 회 | 빌드/컴파일러 서브프로세스 (종료됨) |
| **1689** | `foot` | 449 회 | **224 회** | **사용자 활성 Wayland 터미널** |
| **616344** | `chrome` | 251 회 | 125 회 | 웹 브라우저 탭 렌더러 |
| **616879** | `chrome` | 237 회 | 118 회 | 웹 브라우저 탭 렌더러 |
| **616685** | `chrome` | 218 회 | 109 회 | 웹 브라우저 탭 렌더러 |
| **393928** | `claude` | 199 회 | **99 회** | **Claude 데스크톱 앱 (Electron/UI)** |
| **616282** | `chrome` | 187 회 | 93 회 | 웹 브라우저 탭 렌더러 |
| **224879** | `opencode` | 185 회 | **92 회** | **OpenCode IDE / 에디터** |
| **1243** | `kded6` | 163 회 | **81 회** | **KDE Plasma 코어 서비스 데몬** |
| **2096** | `agy` | 134 회 | **67 회** | **Antigravity CLI 에이전트 워커** |
| **1884** | `foot` | 108 회 | 54 회 | 보조 Wayland 터미널 창 |
| **1160** | `fcitx5` | 97 회 | **48 회** | **한글/한자 입력기 데몬 (IME)** |

---

## 5. Root Cause Analysis (근본 원인 규명)

### 5.1 원인 1: 대화형 데스크톱 핵심 도구 오분류 (Classification Deficit)
- `fcitx5`(한글 입력기), `foot`(터미널), `kded6`(KDE 서비스), `opencode`(IDE), `agy`(에이전트)는 사용자가 직접 상호작용하는 핵심 도구입니다.
- 사용자가 빠르게 타이핑하거나 터미널 출력을 쏟아낼 때 순간적으로 CPU 점유율이 50%~100%를 초과할 수 있습니다.
- 현재의 `ProcessClassifier` 규칙에서 이들이 완전 면제 티어(Tier 0: CriticalImmune, Tier 1: DesktopCore)에 등록되어 있지 않아 일반 폭주 태스크로 판정되었습니다.
- 결과적으로 사용자가 타이핑하거나 터미널을 쓰는 도중에 **Nice=10으로 강등당하고 헤드룸 코어 마스크가 씌워지는 렉 현상**이 유발되었습니다.

### 5.2 원인 2: 단발성 스파이크 필터 부재 (Zero-Hysteresis Flapping)
- 현재 알고리즘은 **단 1회의 관측 사이클(1 tick)**에서 CPU 임계치를 넘으면 즉시 `AntiStarvationCap`을 적용하고, 다음 사이클에서 부하가 조금만 떨어지면 즉시 `restore`를 호출합니다.
- 이로 인해 `foot` 터미널 하나에서만 224회의 캡과 224회의 롤백이 교대로 반복되어 스케줄러 syscall 폭풍을 일으켰습니다.

### 5.3 원인 3: 로거 커널 식별자(`comm`) 하드코딩 결함
- `battery_feature.cpp` 내부 `actuate_anti_starvation_cap` 호출 시 실제 프로세스 이름 대신 하드코딩된 `"runaway-task"` 문자열을 전달하여 저널 로그에서 정확한 comm을 남기지 못하고 있었습니다.

---

## 6. Strategic Mitigation Patterns & Action Plan (`REF-ARCH-045`)

분석된 실측 로그를 바탕으로 다음과 같은 4대 대응 패턴을 아키텍처 및 소스 코드에 즉시 구현합니다:

### 패턴 1: 대화형 데스크톱 핵심 도구 면역 티어 강화 (Interactive Core Immunity)
- **대상**: `fcitx5`, `ibus`, `foot`, `kitty`, `alacritty`, `konsole`, `kded6`, `plasmashell`, `kwin_wayland`, `opencode`, `agy`
- **조치**: `ProcessClassifier`에서 이들 프로세스를 **Tier 1 (DesktopCore) 및 Tier 0 (CriticalImmune)**으로 명시적 격상.
- **효과**: 사용자의 타이핑, 터미널 명령, 에디터 작업 시 스케줄러 캡을 일절 배제하여 100% 무지연(Zero-Lag) 타이핑 및 작업 환경 보장.

### 패턴 2: 2단계 시간적 히스테리시스 래더 (Anti-Flapping Temporal Hysteresis Ladder)
- **진입 조건 (Spike Filter)**: 단발성 1회 스파이크는 무시하고, **연속 2사이클 이상 지속적으로 임계치(150% CPU 또는 높은 WDI)를 초과할 때만** 완화 적용.
- **유지 및 해제 조건 (Cooldown Hold)**: 완화가 한 번 발동되면 **최소 15초(Cooldown Window)** 동안 상태를 유지하며, 부하가 안정적으로 가라앉았음이 확인된 후에만 1회 롤백 실행.
- **효과**: 플래핑 진동 횟수를 현재 3,336회에서 **-90% 이상 격감**시켜 커널 스케줄러 오버헤드 소멸.

### 패턴 3: 감사 저널 프로세스 실제 comm 연동 (True Comm Journaling)
- `actuate_anti_starvation_cap`과 `actuate_anti_starvation_restore` 시그니처에 `const char* comm`을 인자로 전달하여, 저널 로그에 실제 PID와 실행 파일명이 명확하게 남도록 수정.

### 패턴 4: 브라우저 탭 백그라운드 선별 억제 (Browser Background Tab Isolation)
- Chrome 브라우저 프로세스 중 포그라운드 활성 창이 아닌 백그라운드 렌더러 워커에 한해서만 CFS nice 조정을 선별 적용하여 웹 브라우징 반응성 유지.
