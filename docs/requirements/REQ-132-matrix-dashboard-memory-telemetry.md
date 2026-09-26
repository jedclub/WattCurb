# [REF-REQ-132] Matrix Dashboard System & Process Memory Telemetry Visualization

**Status**: Implemented · **Date**: 2026-09-26  
**Related**: [`REF-REQ-036`](REQ-036-native-kde-dashboard-matrix.md), [`REF-REQ-037`](REQ-037-btop-dense-matrix-dashboard.md), [`REF-REQ-129`](REQ-129-low-overhead-process-memory-persistence.md), [`REF-REQ-130`](REQ-130-safe-non-destructive-memory-reclaim.md)  
**Verification**: [`REF-TEST-086`](../../tests/test_units.cpp)

---

## 1. 개요 및 목적 (Problem Statement)

WattCurb의 정밀 분석 매트릭 창(`wattcurb-dashboard`, Matrix Dashboard)은 시스템의 물리 하드웨어 전력과 주요 소모 프로세스를 실시간으로 시각화하는 핵심 GUI 도구이다.

그러나 기존 구현에서 다음과 같은 결함이 확인되었다:
1. **프로세스 테이블 PSS 메모리 누락**: 데몬은 `FULL_TELEMETRY` JSON으로 각 프로세스의 `"pss_mb"`를 정상 송출하고 있었으나, UI 백엔드 브릿지(`DashboardBackend::ingestTelemetryJson`)에서 이를 `processList` 매핑 딕셔너리로 추출하는 코드가 누락되어 프로세스 테이블의 PSS 열 및 호버 툴팁에 메모리 크기가 전혀 표시되지 않거나 0M으로 출력되었다.
2. **시스템 메모리(RAM/스왑) 크기 부재**: 좌측 상단 카드(Card 1: `CPU & Memory Subsystem`)에 패키지/코어/언코어/DRAM 전력 소모(W)와 온도는 존재했으나, 시스템의 실제 물리 메모리 총량, 사용량, 사용률(%), 스왑 점유량 크기가 표시되지 않아 시스템 메모리 상태를 파악할 수 없었다.

본 요구사항은 데몬의 `FULL_TELEMETRY` 데이터그램과 UI 백엔드, 그리고 QML 렌더링 계층을 보강하여 **시스템 전체 메모리(RAM/Swap) 및 프로세스별 PSS/RSS 메모리 크기를 매트릭 대시보드에 고밀도로 완벽히 시각화**하는 것을 목표로 한다.

---

## 2. 세부 요구사항 (Specifications)

### REQ-132.1: 데몬 FULL_TELEMETRY 시스템 및 프로세스 메모리 JSON 규격 확장
- `src/core/daemon_runner.cpp`의 소켓 데이터그램 송출 로직에 다음 필드를 추가한다:
  - `mem_total_mb`: 호스트 전체 물리 RAM 크기 (MB)
  - `mem_used_mb`: 호스트 현재 실제 사용 중인 물리 RAM 크기 (MB, MemTotal - MemAvailable)
  - `mem_avail_mb`: 즉각 할당 가능한 가용 물리 RAM 크기 (MB)
  - `swap_total_mb`: 시스템 스왑 총 크기 (MB)
  - `swap_used_mb`: 시스템 현재 스왑 점유 크기 (MB)
- `processes` 배열의 각 프로세스 항목에 `"pss_mb"`에 더해 `"rss_mb"`를 명시적으로 함께 송출한다.

### REQ-132.2: DashboardBackend Qt6 프로퍼티 노출 및 파싱 브릿지 구축
- `DashboardBackend` 클래스에 다음 Q_PROPERTY 및 게터를 신설한다:
  - `int memTotalMb`: 총 RAM (MB)
  - `int memUsedMb`: 사용 RAM (MB)
  - `int memAvailMb`: 가용 RAM (MB)
  - `int swapTotalMb`: 총 스왑 (MB)
  - `int swapUsedMb`: 사용 스왑 (MB)
  - `double memUsedPercent`: RAM 사용률 (%)
  - `QString memorySummaryString`: 포맷된 메모리 요약 문자열 (예: `7.4G / 15.1G (49%)`)
- `ingestTelemetryJson`에서 프로세스 객체의 `pss_mb` 및 `rss_mb`를 읽어 `map["pssMb"]` 및 `map["rssMb"]`로 적재한다.
- 데몬 IPC 질의 전 초기 단계(Fallback)에서도 `/proc/meminfo` 파싱을 통해 실시간 메모리 수치를 지체 없이 즉각 제공한다.

### REQ-132.3: QML 정밀 분석 매트릭 뷰(Matrix View) 고밀도 렌더링
- `DashboardWindow.qml`의 **Card 1 (CPU & Memory Subsystem)**:
  - RAM 사용량 / 총용량 및 사용률 퍼센트, 고밀도 미니 시각화 바, 스왑 사용량 지표를 전력 수치 하단에 명확하게 배치한다.
- `DashboardWindow.qml`의 **프로세스 테이블(Process ListView)**:
  - `PSS` 열에 각 프로세스의 실제 메모리 크기(예: `358M`, `57M`)가 누락 없이 출력되도록 보장한다.
  - 마우스 호버 세부 정보 툴팁에 `정격 PSS: XXX MB (RSS: YYY MB)` 형태로 공정 메모리와 물리 상주 메모리를 동시 표기한다.

---

## 3. 검증 기준 (Verification Criteria)

1. `tests/test_units.cpp`의 단위 테스트(`REF-TEST-086`)에서 `DashboardBackend`가 시스템 메모리 지표(`memTotalMb`, `memUsedMb`, `memUsedPercent`) 및 프로세스 `pssMb`, `rssMb`를 정확히 인제스트하는지 검증한다.
2. 실기 환경에서 `wattcurb-dashboard` 가동 시 Card 1 및 프로세스 테이블 PSS 컬럼에 메모리 수치가 실시간 갱신되어야 한다.
