# [REF-ARCH-091] UI Responsive Layout & Overflow Hardening Architecture

## 1. 아키텍처 개요 (Architectural Overview)

본 문서는 [`REF-REQ-144`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-144-ui-responsive-layout-and-overflow-hardening.md)를 충족하기 위해 WattCurb Qt6/QML UI 프레임워크의 3대 핵심 뷰(`DashboardWindow.qml`, `FanCurveStudioView.qml`, `BatteryReportView.qml`)에 적용된 계층적 레이아웃 아키텍처와 오버플로우 방지 설계를 명세한다.

---

## 2. 계층 구조 개선 (Layout Hierarchy Transformation)

### 2.1 DashboardWindow.qml Tab 0 계층 구조

#### [기존 구조 (결함)]
```
Item (Tab 0, anchors.fill: parent)
├── RowLayout (anchors.fill: parent)  <-- 상단 메인 매트릭스가 Item 전체 점유
│   ├── ColumnLayout (Left Column, 460px)
│   └── ColumnLayout (Right Column, fillWidth)
└── RowLayout (Layout.fillWidth, 190px)  <-- 부모가 Item이어서 상단 뷰 뒤에 중첩/바닥 파묻힘!
    ├── Rectangle (CARD A Donut)
    └── Rectangle (CARD B Donut)
```

#### [개선 아키텍처 (안정화)]
```
ScrollView (id: tab0Scroll, anchors.fill: parent, clip: true)
└── ColumnLayout (width: availableWidth, spacing: 8)
    ├── RowLayout (Layout.fillWidth, preferredHeight: max(540, height - 198))
    │   ├── ColumnLayout (Left Column, 460px)
    │   │   ├── CARD 1: CPU & Memory (240px)
    │   │   ├── CARD 2: Battery BAT0 (174px)
    │   │   └── CARD 3: GPU & Display & Storage (fillHeight)
    │   └── ColumnLayout (Right Column, fillWidth)
    │       ├── TIMELINE Waveform (90px)
    │       └── Process Matrix Table (fillHeight)
    └── RowLayout (Layout.fillWidth, preferredHeight: 185px)  <-- ColumnLayout의 2번째 자식으로 정상 배치!
        ├── Rectangle (CARD A Donut 100px + 2-Column 11px Grid)
        └── Rectangle (CARD B Donut 100px + 2-Column 11px Grid)
```

### 2.2 FanCurveStudioView.qml 반응형 가로 폭 분배

```
ColumnLayout (anchors.fill: parent, margins: 14)
├── Rectangle (Top Control Bar, height: 56)
│   └── RowLayout (margins: 16, spacing: 12)
│       ├── Title & Subtitle (Subtitle: elide: Text.ElideRight, maxWidth: 260)
│       ├── Item (fillWidth: true)
│       ├── Profile Pills (4개, height: 32, compact padding)
│       ├── Status Badge (compact padding)
│       └── Hardware Sync Badge (compact padding)
├── Rectangle (Canvas Container, fillWidth, fillHeight, padBottom: 56)
└── Rectangle (Bottom Action Bar, height: 52)
    └── RowLayout (margins: 12, spacing: 8)
        ├── Point Editors / Add Point (compact)
        ├── Item (fillWidth: true)
        ├── Feedback Banner (fillWidth: true, minWidth: 80, maxWidth: 320, elide: Text.ElideRight)
        ├── Reset Button (implicitHeight: 32)
        └── Apply Button (implicitHeight: 32, highlighted)
```

### 2.3 BatteryReportView.qml 세로 스크롤 아키텍처

```
ScrollView (id: reportScroll, anchors.fill: parent, clip: true)
└── ColumnLayout (width: availableWidth, spacing: 10)
    ├── Header Bar (52px)
    ├── Filter Toolbar (42px)
    ├── 4 KPI Cards (86px)
    ├── Cross-Profile Matrix (60px)
    ├── Main Split (fillWidth, preferredHeight: max(340, height - 380))
    │   ├── Hardware Domains List (420px)
    │   └── Process Culprits Ranking Table (fillWidth)
    └── Diagnostic & Recommendations Box (110px)  <-- 스크롤 영역 내 포함으로 바닥 잘림 원천 방지!
```

---

## 3. 오라클 게이트 테스트 설계 (`REF-TEST-098`)

- `test_ui_responsive_layout_and_overflow_hardening()`:
  1. `DashboardWindow.qml`의 Tab 0가 `ScrollView` 및 직계 `ColumnLayout` 구조로 감싸져 있는지 검증.
  2. 하단 도넛 덱 `RowLayout`이 `ColumnLayout`의 자식으로 상단 매트릭스와 분리 계층화되어 있는지 검증.
  3. `FanCurveStudioView.qml`의 상단 타이틀 엘리전 및 하단 피드백 배너 유연 너비 제약 검증.
  4. `BatteryReportView.qml`이 `ScrollView`로 감싸져 하단 110px 진단 박스가 클리핑되지 않음을 검증.
