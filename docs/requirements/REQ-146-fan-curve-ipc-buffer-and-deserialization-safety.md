# REF-REQ-146: 팬 커브 IPC 수신 버퍼 확장 및 영속화 역직렬화 안전성 보장

## 1. 개요 및 배경
대시보드(Fan Curve Studio)에서 사용자가 사용자 정의(커스텀) 팬 커브를 작성하고 [💾 커브 적용 (데몬 반영)]을 수행할 때, 전체 4개 프로필(Performance, Balanced, PowerSaver, UltraEndurance)의 직렬화 페이로드는 약 450~850바이트에 달한다. 그러나 데몬의 `handle_ipc_datagram` 수신 버퍼가 레거시 128바이트(`char buf[128]`)로 고정되어 있어 `recvmsg` 호출 시 초과 바이트가 커널 수준에서 절단(Truncate)되었다. 이로 인해 데몬 측의 `deserialize_curves` 파싱이 실패하여 데몬 메모리와 디스크(`/var/lib/wattcurb/fan_curves.conf`)에 커스텀 값이 반영되지 않고, 대시보드 재시작 시 데몬의 팩토리 기본값을 읽어와 기존 설정이 롤백되는 문제가 발생했다.

## 2. 세부 요구사항

### 2.1 IPC 데이터그램 수신 버퍼 4096바이트 확장
- `DaemonRunner::handle_ipc_datagram`의 `buf` 크기를 기존 128바이트에서 최소 4096바이트로 확장한다.
- 4개 프로필의 최대 10개 제어점(총 40포인트) 및 커스텀 메타데이터를 완전하게 손실 없이 수신해야 한다.

### 2.2 클라이언트 추상 소켓 주소 무충돌 보장
- `SingletonLock::query_daemon`에서 임시 클라이언트 바인딩 주소 생성 시, 초 단위 `time(nullptr)` 대신 `clock_gettime(CLOCK_MONOTONIC)`의 나노초와 `std::atomic<uint32_t>` 시퀀스 카운터를 결합하여 동일 밀리초 내 연속 쿼리 시 `EADDRINUSE` 충돌을 원천 차단한다.

### 2.3 안전한 역직렬화 및 부분 누락 방어
- `FanCurveEngine::deserialize_curves`는 유효하게 파싱된 프로필 개수를 추적하고, 최소 1개 이상의 온전한 프로필이 복원되었을 때만 성공을 반환한다.
- 중간 절단 스트림 수신 시 안전하게 거부하여 기존 인메모리 커브 오염을 방지한다.
- `ensure_dir_exists`는 다단계 디렉터리(`mkdir -p`) 재귀 생성을 지원하여 임의 경로 저장 시 실패를 방지한다.

### 2.4 하드웨어 반영 및 감사 로깅
- `SET_FAN_CURVES` 성공 시 적용된 ThinkPad EC 레벨, CPU 온도, 요청 클라이언트 정보를 저널에 명확히 기록한다(`EventLogger::log_alert("FAN", ...)`).
- 파싱 실패 시에도 실패 바이트 수와 요청자 정보를 기록하여 디버깅 투명성을 제공한다.

## 3. 검증 기준 (Oracle Gate)
- `REF-TEST-100`: 직렬화 페이로드 크기(> 128B, < 4096B) 검증, 중간 절단 스트림 안전 거부 검증, 4개 프로필 완전 복원 검증, 다단계 중첩 경로 디렉터리 생성 및 저장/로드 검증.
