# REF-REQ-133: 배터리 히스토리 리포트 AC 텔레메트리 폴백 및 전원 상태별 투명 표시 (Battery History Report AC Telemetry Fallback and Multi-Power-State Transparency)

## 1. 개요 및 목적 (Overview & Motivation)
WattCurb의 인메모리 히스토리 링버퍼(`HistoryRingBufferShm`)는 시스템 파워 프로파일(Performance, Balanced, PowerSaver, UltraEndurance) 및 전원 상태(AC Connected, Discharging, AC Passthrough)에 관계없이 10초 주기로 모든 전력/하드웨어 텔레메트리를 7일간(~60,480 샘플) 누락 없이 기록한다.
그러나 기존 배터리 심층 드레인 분석기([`battery_history_analyzer.cpp`](file:///home/jedclub/Develop/WattCurb/src/report/battery_history_analyzer.cpp))는 순수 배터리 방전(`battery_state == 1`) 샘플만을 엄격하게 필터링하도록 설계되어 있어, 사용자가 AC 전원(충전기 연결 또는 80% 충전 보존 모드) 상태에서 특정 프로파일(예: PowerSaver)을 장시간(수십 시간, 수천 샘플) 운용했더라도 해당 모드의 방전 데이터가 없으면 `0h 0m (0 samples)`의 빈 리포트를 반환하고 비교 매트릭스에서 제외하는 결함이 존재했다.

본 요구사항은 방전 데이터가 부재하더라도 AC 전원 연결 중에 수집된 풍부한 하드웨어 텔레메트리를 자동으로 폴백(Fallback) 집계하여, 소비 전력(W), CPU 패키지 전력, GPU 전력, Deep Sleep(C3+), 온도 등을 투명하게 시각화하고 사용자에게 전원 상태(AC vs Battery)를 명확히 인지시키는 아키텍처를 정의한다.

---

## 2. 세부 요구사항 명세 (Functional Specifications)

### 2.1. 듀얼 소스(Dual-Source) 집계 및 자동 AC 폴백
- **REF-REQ-133.1 [크로스 프로파일 비교 매트릭스 보존]**:
  - 4대 파워 프로파일 비교 매트릭스(`ProfileComparisonEntry`) 생성 시, 각 프로파일별로 순수 방전(`battery_state == 1`) 샘플이 1개 이상 존재하면 방전 텔레메트리를 우선 적용한다.
  - 순수 방전 샘플이 0개인 프로파일이라도 전원 연결(AC / Passthrough, `battery_state != 1`) 샘플이 존재할 경우, 이를 0개로 폐기하지 않고 AC 텔레메트리 데이터를 기반으로 평균 소비 전력(W), 피크 전력, C3 Deep Sleep %, 온도를 정상 산출한다.
  - 해당 프로파일의 지속 시간 문자열(`duration_str`)에 `[⚡AC]` 배지를 부착하여 AC 전원 기반 데이터임을 명시한다.

### 2.2. 단일 프로파일 필터링 시 조기 반환 차단 및 상세 분석
- **REF-REQ-133.2 [비(非)방전 프로파일 분석 연속성]**:
  - `filter_mode >= 0` 필터링 시, `discharge_pts`가 비어있다는 이유로 분석을 조기 중단(`return result`)하지 않는다.
  - 해당 모드의 전체 샘플(`all_matching_pts`)이 존재할 경우, `is_pure_discharging = false` 상태로 전환하여 전체 샘플을 `active_pts`로 활용한다.
  - 진단 요약(`diagnostic_summary`)에 AC 전원 상태 텔레메트리 기반 분석임을 안내하고, 배터리 방전 시의 추가 분석 혜택을 권장 사항(`recommendation_text`)으로 제공한다.

### 2.3. UI 및 CLI 투명성 라벨링
- **REF-REQ-133.3 [QML 대시보드 및 CLI 동적 라벨]**:
  - `discharging_samples == 0`인 경우, QML 리포트 창의 상단 KPI 카드 라벨을 "총 방전 소모량" → "총 소비 전력량 (AC 전원)", "평균 방전율" → "평균 소비 전력 (AC 전원)"으로 동적 전환한다.
  - CLI(`wattcurb -R`) 및 마크다운 출력에서도 "Total Energy Consumed (AC)", "Average System Power"로 적응형 라벨을 표시한다.

---

## 3. 검증 및 오라클 게이트 기준 (Verification & Oracle Gate, REF-TEST-086)
1. **AC 폴백 단위 테스트**:
   - `pts` 배열에 방전 샘플이 0개이고 AC 전원 샘플(PowerSaver, 100개)만 인입되었을 때, `BatteryHistoryAnalyzer::analyze(..., filter_mode = 2)`가 100개 샘플을 정상 분석하여 `avg_discharge_watts > 0`, `duration_sec == 1000`을 산출해야 한다.
2. **크로스 프로파일 매트릭스 완결성**:
   - 4개 프로파일 중 3개는 방전, 1개는 AC 데이터만 존재하는 혼합 데이터셋에서 `profile_comparisons` 4개 엔트리 모두 `sample_count > 0`이어야 하며, AC 엔트리는 `duration_str`에 `⚡AC` 배지를 포함해야 한다.
3. **오버헤드 불변성**:
   - 듀얼 어큐뮬레이터 추가로 인한 연산 비용은 60,480개 링버퍼 기준 1회 순회(Single-Pass) 내에서 O(N)으로 처리되어야 하며, 힙 할당을 추가하지 않는다.
