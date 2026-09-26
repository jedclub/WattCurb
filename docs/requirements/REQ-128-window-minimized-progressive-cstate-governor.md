# [REF-REQ-128] Window Minimized Progressive C-State Governor (C0 -> C1 -> C2)

**Status**: Implemented · **Date**: 2026-09-26  
**Related**: [`REF-REQ-033`](REQ-028-adaptive-power-state-machine-and-actuation.md), [`REF-REQ-044`](REQ-044-absolute-zero-kill-and-non-halting-safety.md), [`REF-REQ-085`](REQ-085-kde-active-window-resource-guarantee-and-c0-pinning.md), [`REF-ARCH-062`](../architecture/ARCH-062-active-window-resource-guarantee-and-pm-qos.md)  
**Verification**: [`REF-TEST-082`](../../tests/test_units.cpp)

---

## 1. 개요 및 배경 (Problem Statement)

웹 브라우저(Chrome/Firefox)나 Electron 애플리케이션(VS Code, 메신저 등)은 창이 최소화(Minimized)되어 화면에서 보이지 않는 상태에서도 백그라운드 자바스크립트 타이머, 렌더링 루프, IPC 폴링 등을 1~10ms 간격으로 빈번하게 실행한다.
이러한 고빈도 마이크로 웨이크업은 CPU 코어가 깊은 C-State 절전(C1 Halt, C2/C3 Package Deep Sleep)에 진입하는 것을 방해하고, 시스템을 불필요한 **C0 Active 발열 상태**에 지속적으로 묶어두어 배터리 소모와 팬 소음을 유발한다.

반면 사용자는 백그라운드에서 파일 다운로드, 데이터 스트리밍, 동기화 작업 등이 멈추지 않고 지속되기를 원하므로, 프로세스를 강제 동결(`SIGSTOP`, `cgroup.freeze`)하거나 강제 종료(`SIGKILL`)하는 것은 **절대 불가**하다 (`REF-REQ-044` Zero-Kill & Non-Halting Invariant).

따라서 WattCurb 데몬은 **창 최소화 시간과 호스트 전원 프로파일(Power Profile)**에 따라 프로세스를 부드럽게 감속시키고 타이머를 응집(Coalesce)하는 **2단계 점진적 C-State 유도 거버너(Progressive C-State Governor)**를 구현한다.

---

## 2. 세부 요구사항 (Specifications)

### REQ-128.1: Non-Halting 절대 불변식 (Zero-Freeze & Non-Halting)
- 최소화된 애플리케이션을 결코 정지(`SIGSTOP`), 동결(`cgroup.freeze`), 또는 종료(`SIGKILL`)하지 않는다.
- 프로세스는 항상 실행 가능(Runnable) 상태를 유지하여 백그라운드 다운로드, 웹소켓 통신, 알림 수신이 정상 작동해야 한다.

### REQ-128.2: 2단계 점진적 C-State 완화 단계 (Two-Stage Progressive Ladder)
1. **Stage 1 (Soft C1 Coalescing)**:
   - **목표 C-State**: C0 $\to$ C1 (Halt / 빠른 복구 C-State)
   - **스케줄러 조절**: CFS Nice +10 (선점 우선순위 완화)
   - **타이머 슬랙**: **100ms** (`100'000'000ULL` ns) 강제 적용 (`/proc/<pid>/timerslack_ns`)
   - **효과**: 고빈도 1~10ms 웨이크업을 100ms 주기로 병합하여 짧은 C1 유휴 상태 확보, 백그라운드 작업 속도 정상 유지.
2. **Stage 2 (Deep C2/C3 Idle Throttle)**:
   - **목표 C-State**: C1 $\to$ C2/C3 (Deep Package Sleep / 저전력 상태)
   - **스케줄러 조절**: `SCHED_IDLE` (시스템 유휴 시에만 CPU 사용) 및 `IOPRIO_CLASS_IDLE`
   - **타이머 슬랙**: **1,000ms (1초)** (`1'000'000'000ULL` ns) 극대화
   - **효과**: 시스템이 쉴 때 최저 주파수로만 조용히 연산하며 1초 단위로만 웨이크업하여 CPU 패키지 C2/C3 거주율 극대화.

### REQ-128.3: 전원 프로파일별 차등 전환 임계 시간 (Profile-Adaptive Transition)
창이 최소화된 상태를 유지할 때 Stage 1에서 Stage 2로 승격되는 시간 기준은 활성 전원 프로파일에 따라 차등 적용된다:
- **성능 모드 (`PowerProfileMode::Performance`)**:
  - `0초 ~ 10분 미만 (600초)`: Stage 1 (Soft C1 Coalescing) 유지
  - `10분 이상 (600초)` 지속: Stage 2 (Deep C2/C3 Idle) 승격
- **균형 모드 (`PowerProfileMode::Balanced`)**:
  - `0초 ~ 1분 미만 (60초)`: Stage 1 (Soft C1 Coalescing) 유지
  - `1분 이상 (60초)` 지속: Stage 2 (Deep C2/C3 Idle) 승격
- **절전 모드 (`PowerProfileMode::PowerSaver` / `PowerProfileMode::UltraEndurance`)**:
  - `0초 (즉시)`: 최소화되는 순간 지체 없이 **즉시 Stage 2** 적용

### REQ-128.4: 무지연 복구 (Zero-Latency Instant Unthrottle)
- 사용자가 최소화된 창을 화면에 복원(Un-minimize)하거나 포커스를 활성화하는 즉시, 0밀리초 무지연으로 원래의 스케줄러 정책(`SCHED_OTHER`), 원래 Nice 값, 표준 50µs 타이머 슬랙으로 즉각 복구한다.

### REQ-128.5: 활성 오디오 안전장치 (Active Audio Guard)
- 최소화된 창이라도 사운드 재생 중(`is_audio_active == true`)인 경우, 버퍼 언더런 및 음 끊김을 방지하기 위해 Stage 2(`SCHED_IDLE`, 1초 타이머)로 승격하지 않고 Stage 1 또는 오디오 안전 우선순위를 유지한다.
