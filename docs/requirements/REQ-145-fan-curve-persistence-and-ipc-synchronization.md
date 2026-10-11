# [REQ-145] StateDirectory-Backed Fan Curve Persistence & Bi-Directional IPC Synchronization

## 1. 개요 (Overview)

* **Ref-ID**: `REF-REQ-145`
* **유형**: Functional Requirement / Fault-Tolerance & Persistence Hardening
* **연관 컴포넌트**: `FanCurveEngine`, `DaemonRunner`, `DashboardBackend`, `systemd.service`
* **선행 참조**: [`REF-REQ-136`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-136-custom-fan-curve-and-unified-dashboard-tabs.md), [`REF-REQ-140`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-140-privilege-separated-fan-curve-ipc-persistence.md), [`REF-REQ-141`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-141-fan-curve-actuation-feedback.md), [`REF-REQ-093`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-093-daemon-privilege-boundary-hardening.md)

---

## 2. 문제 배경 및 장애 분석 (Problem Statement)

사용자가 Fan Curve Studio 대시보드에서 커스텀 팬 제어 곡선을 구성하고 "커브 적용"을 수행하였음에도 불구하고, 대시보드 창을 닫았다가 다시 열거나 시스템 재부팅/서비스 재시작 시 **커스텀 설정이 유지되지 않고 팩토리 기본값(Default)으로 롤백**되는 결함이 발견되었다.

### 2.1. 원인 1: `ProtectSystem=full`에 의한 `/etc` 쓰기 거부 (`EROFS`)
* `wattcurb.service`는 권한 격리 및 보안 하드닝([`REF-REQ-093`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-093-daemon-privilege-boundary-hardening.md))을 위해 `ProtectSystem=full` 지시자를 활성화하고 있다.
* `ProtectSystem=full`은 systemd 마운트 네임스페이스 격리를 통해 데몬 프로세스에 대해 `/etc` 파일 시스템을 **읽기 전용(`ro,nosuid,noatime`)** 으로 마운트한다.
* 기존 구현은 `/etc/wattcurb/fan_curves.conf`에 직접 파일 쓰기(`save_to_file`)를 시도하였으며, 커널 VFS 계층에서 `EROFS (Read-only file system)` 에러가 발생하여 파일 영속화가 실패하고 있었다.

### 2.2. 원인 2: 데몬 및 대시보드 부트스트랩 시 파일 로드 누락
* `DaemonRunner::run()` 및 `DashboardBackend::DashboardBackend()` 생성자에서 `fan_curve_engine_.initialize()`만 호출하고, 영속 저장소로부터 설정을 복원하는 `load_from_file()` 호출이 누락되어 있었다.
* 이로 인해 대시보드가 열릴 때마다 항상 하드코딩된 기본 커브가 메모리에 올라와 화면에 표시되었고, 사용자가 저장한 커브가 소실된 것처럼 보이는 착시와 실제 롤백을 유발하였다.

### 2.3. 원인 3: 양방향 IPC 동기화(`GET_FAN_CURVES`) 프로토콜 부재
* 대시보드는 일반 사용자(UID 1000) 권한으로 실행되므로 시스템 전역 상태 디렉터리에 대한 직접 접근 권한이 달라질 수 있다.
* 데몬이 메모리 및 디스크에 정상 보관 중인 커스텀 커브를 대시보드가 시작 즉시 UDS 소켓으로 쿼리하여 수신하는 `GET_FAN_CURVES` 프로토콜이 존재하지 않아, 클라이언트-데몬 간 상태 불일치가 발생하였다.

---

## 3. 기능 요구사항 (Functional Requirements)

### 3.1. 영속 저장소 경로 표준화 (`/var/lib/wattcurb/fan_curves.conf`)
1. **Primary Persistence Path**:
   * systemd `StateDirectory=wattcurb`가 안전하게 제공하는 쓰기 가능 디렉터리인 `/var/lib/wattcurb/fan_curves.conf`를 1순위 영속 파일 경로로 표준화한다.
2. **Fallback Load Ladder**:
   * 설정 파일 로드 시 다음 순서로 탐색하여 하위 호환성을 보장한다:
     1. `/var/lib/wattcurb/fan_curves.conf` (데몬 기본 저장소)
     2. `/etc/wattcurb/fan_curves.conf` (수동/레거시 설정 파일)
     3. `$HOME/.config/wattcurb/fan_curves.conf` (사용자별 로컬 캐시)
3. **Multi-Tier Save Ladder**:
   * 쓰기 권한에 따라 `/var/lib/wattcurb/` 쓰기를 시도하고, 권한이 부족한 일반 사용자 클라이언트 단독 실행 시 `$HOME/.config/wattcurb/`로 자동 폴백 저장한다.

### 3.2. 양방향 UDS IPC 프로토콜 (`GET_FAN_CURVES`)
1. **GET_FAN_CURVES 명령어 지원**:
   * 데몬은 `GET_FAN_CURVES\n` 명령 수신 시, 현재 메모리에 활성화된 전체 4개 프로파일 커브의 직렬화 페이로드를 `OK\n<payload>` 형식으로 즉시 응답한다.
   * 조회성(Read-only) 명령이므로 불필요한 클라이언트 거부 없이 즉각 반환한다.
2. **대시보드 시작 시 자동 동기화**:
   * `DashboardBackend`는 초기화 시 `GET_FAN_CURVES`를 데몬에 질의하여 최신 커브를 역직렬화하고, UI 뷰에 즉시 반영한다.
   * 데몬 응답이 없을 경우 로컬 파일 폴백(`load_from_file()`)을 수행한다.

### 3.3. 데몬 부트스트랩 시 자동 복원
* 데몬 초기화 시점(`DaemonRunner::run()`)에 `fan_curve_engine().load_from_file()`을 호출하여, 서비스 재시작이나 시스템 리부팅 후에도 사용자가 정의한 팬 커브가 ThinkPad EC 하드웨어에 즉시 재적용되도록 보장한다.

### 3.4. IPC 쿼리 타임아웃 현실화
* `DashboardBackend::sendDaemonCommandQuery()`의 기본 타임아웃을 50ms에서 1000ms로 상향하여, 디스크 I/O 및 ACPI EC 제어 시 발생하는 일시적 지연으로 인한 쿼리 드롭을 원천 차단한다.
