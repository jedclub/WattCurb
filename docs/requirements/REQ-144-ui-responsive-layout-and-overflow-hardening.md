# [REF-REQ-144] UI Responsive Layout & Bottom Overflow Hardening Specification

## 1. 개요 및 배경 (Context & Problem Statement)

WattCurb btop-스타일 데스크톱 대시보드(`wattcurb-dashboard`) 및 커스텀 팬 제어 스튜디오(`FanCurveStudioView`), 배터리 전수 분석 리포트(`BatteryReportView`) 환경에서 다음과 같은 다수의 UI 레이아웃 결함이 발견되었다:

1. **대시보드 메인 HUD (Tab 0)의 하단 파묻힘 및 중첩 결함**:
   - Tab 0의 루트 `Item` 바로 아래에 상단 메인 워크스페이스 `RowLayout { anchors.fill: parent }`와 하단 전력 점유율 덱 `RowLayout`이 동일 계층에 배치되어 있었다.
   - `Item`은 QML Layout이 아니므로 하단 전력 점유율 덱(CARD A 하드웨어 도넛, CARD B 프로세스 도넛)이 상단 워크스페이스 뒤에 바닥으로 파묻히거나 겹치는 치명적 레이아웃 충돌이 발생했다.
   - 세로 해상도가 880px 미만이거나 창을 줄였을 때 스크롤이 지원되지 않아 하단 컨트롤 바 및 프로세스 테이블의 행들이 화면 바닥 밖으로 잘려 나갔다.
2. **도넛 차트 범례(Legend) 텍스트 잘림**:
   - 하단 CARD A(하드웨어 9개 도메인) 및 CARD B(프로세스 Top 11 + Other 12개 항목)의 2열 그리드에서 행당 높이와 열 너비가 부족하여, 긴 프로세스 명칭 및 하드웨어 명칭("PLATFORM", "DISPLAY")이 잘리거나 와트/퍼센트 수치와 겹치는 현상이 발생했다.
3. **팬 제어 스튜디오 (`FanCurveStudioView`)의 가로/세로 오버플로우**:
   - 상단 컨트롤 바의 긴 설명 텍스트("ThinkPad EC Monotone Hermite Spline · 30°C~70°C...")와 프로파일 알약(Pills), 커스텀 활성 배지, 하드웨어 연동 배지가 가로 1,190px 이상을 차지하여 최소 너비(1,080px) 환경에서 우측 배지가 잘려나갔다.
   - 하단 액션 바에서 피드백 배너가 가로 440px를 고정 점유하여 창이 좁을 때 우측의 핵심 버튼("기본값 복원", "커브 적용")이 찌그러지거나 잘렸다.
   - X축 온도 눈금 라벨 및 축 제목이 캔버스 하단 경계선에 딱 붙어 가독성이 저하되었다.
4. **배터리 리포트 뷰 (`BatteryReportView`)의 하단 진단 박스 바닥 깔림**:
   - 상단 KPI 카드(86px), 비교 매트릭스(60px), 메인 리스트(300px+), 하단 진단 및 최적화 권장 박스(110px) 등 총 높이가 730px 이상을 요구함에도 스크롤이 지원되지 않아, 창 높이가 660~780px일 때 최하단 진단 박스와 프로세스 순위표가 바닥에 깔려서 보이지 않았다.

---

## 2. 세부 요구사항 (Detailed Functional Requirements)

### REF-REQ-144-F01: Tab 0 ScrollView 감싸기 및 ColumnLayout 계층 정상화
- Tab 0의 루트를 `ScrollView`로 감싸고 내부를 `ColumnLayout { width: availableWidth }`로 단일화한다.
- 상단 메인 매트릭스 `RowLayout`은 `Layout.preferredHeight: Math.max(540, tab0Scroll.height - 198)`로 설정하여 세로 공간이 충분할 때는 화면을 꽉 채우고, 좁아질 때는 540px 이상을 유지하며 자연스러운 세로 스크롤을 제공한다.
- 하단 전력 점유율 덱 `RowLayout`은 `Layout.preferredHeight: 185`로 `ColumnLayout`의 두 번째 자식으로 정상 결합되어 상단 요소와 겹치거나 파묻히지 않아야 한다.

### REF-REQ-144-F02: 도넛 차트 범례 반응형 타이포그래피 및 간격 최적화
- CARD A (하드웨어 9개 도메인): 도넛 직경을 100px로 컴팩트화하고, 2열 그리드의 폰트를 11px, `spacing: 3`으로 정밀화하여 모든 9개 하드웨어 항목의 명칭이 클리핑 없이 선명하게 표시되어야 한다.
- CARD B (프로세스 12개 항목): 2열 그리드의 열 간격을 10px, 폰트를 11px로 설정하고 프로세스명에 `elide: Text.ElideRight`를 적용하여 긴 프로세스명 및 PID와 와트/퍼센트 수치가 겹치지 않아야 한다.

### REF-REQ-144-F03: 팬 제어 스튜디오 상단/하단 바 반응형 탄력 배치
- 상단 바: 서브타이틀 텍스트에 `elide: Text.ElideRight`, `Layout.maximumWidth: 260`을 적용하고 프로파일 알약 패딩을 축소하여 1,080px 해상도에서도 우측의 하드웨어 연동 배지가 완전히 표시되도록 보장한다.
- 하단 바: 피드백 배너에 `Layout.fillWidth: true`, `Layout.minimumWidth: 80`, `Layout.maximumWidth: 320`을 적용하여 가로 공간을 유연하게 배분하고, 우측 "기본값 복원" 및 "커브 적용" 버튼의 가시성을 100% 보장한다.
- 캔버스 패딩: `padBottom`을 56px로 확보하여 X축 온도 눈금(30°C~70°C) 및 축 제목이 바닥 경계선에 잘리지 않도록 한다.

### REF-REQ-144-F04: 배터리 리포트 뷰 전구간 세로 스크롤 보장
- `BatteryReportView` 전체를 `ScrollView`로 감싸 세로 해상도가 730px 미만으로 줄어들거나 대시보드 탭으로 임베드되었을 때도 하단 진단 요약 박스(110px)와 프로세스 순위표가 바닥에 깔리지 않고 세로 스크롤을 통해 온전히 접근 가능해야 한다.

---

## 3. 검증 기준 (Verification Criteria)

- **정적 레이아웃 불변식**: QML 구문 파싱 및 모든 중괄호 짝 100% 일치 (`Unclosed blocks: 0`).
- **가로 반응형 불변식**: 너비 1,080px에서 1,920px까지 창 크기 변경 시 헤더 배지 및 액션 버튼 가로 오버플로우 0건.
- **세로 반응형 불변식**: 높이 660px에서 1,080px까지 창 크기 변경 시 바닥 파묻힘/클리핑 없이 스크롤 정상 작동.
- **오라클 게이트 통과**: `REF-TEST-098` 단위 테스트 통과 및 회귀 버그 0건 검증.
