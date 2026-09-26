# [REF-ARCH-079] Matrix Dashboard Memory Telemetry & Pipeline Architecture

**Status**: Implemented · **Date**: 2026-09-26  
**Related**: [`REF-REQ-132`](../requirements/REQ-132-matrix-dashboard-memory-telemetry.md), [`REF-ARCH-036`](ARCH-036-circular-power-share-breakdown.md), [`REF-ARCH-076`](ARCH-076-process-memory-shm-persistence.md)  
**Verification**: [`REF-TEST-086`](../../tests/test_units.cpp)

---

## 1. 아키텍처 다이어그램 (Dataflow Pipeline)

```
[Linux Kernel /proc/meminfo, /proc/<pid>/statm]
                  │
                  ▼
[MemoryPressureGuard / ProcessAnalyzer]
                  │
                  ▼
[DaemonRunner: cached_report_ & mem_guard_]
                  │
                  ├────────────────────────┐
                  ▼                        ▼
      [IPC Datagram: FULL_TELEMETRY]    [SHM: 128B Seqlock & 32B HistoryRingBuffer]
      • mem_total_mb, mem_used_mb       • SharedCulprit (rss_mb, pss_mb)
      • swap_total_mb, swap_used_mb     • HistoryPoint (mem_used_mb, swap_used_mb)
      • processes[]: pss_mb, rss_mb
                  │
                  ▼
[DashboardBackend (Qt6 QObject Bridge)]
  • Ingest: pss_mb -> map["pssMb"], rss_mb -> map["rssMb"]
  • Properties: memTotalMb, memUsedMb, swapUsedMb, memUsedPercent
                  │
                  ▼
[DashboardWindow.qml (KDE Plasma 6 / QtQuick View)]
  • Card 1: 💻 CPU & Memory Subsystem (RAM 사용량/총량 바, Swap 지표)
  • Table: PSS Column (예: "358M") & Hover Card (PSS / RSS)
```

---

## 2. 모듈별 역할 및 구현 상세

### 2.1. 데몬 소켓 방출 계층 (`src/core/daemon_runner.cpp`)
- `cached_report_` 생성 시 `mem_guard_.last_sample()`의 시스템 메모리 카운터와 `ProcessAttributedPower`의 `pss_kib`, `rss_kib`를 JSON 직렬화에 바인딩.
- 추가 메모리 할당 없이 기존 스트링스트림 버퍼에 시스템 메모리 및 프로세스 메모리 키-값 쌍을 직접 포맷팅.

### 2.2. 백엔드 브릿지 계층 (`src/ui/dashboard_backend.cpp`, `dashboard_backend.hpp`)
- `Q_PROPERTY`로 `memTotalMb`, `memUsedMb`, `memAvailMb`, `swapTotalMb`, `swapUsedMb`, `memUsedPercent`, `memorySummaryString` 노출.
- `ingestTelemetryJson`: JSON 내 `processes` 객체 순회 시 `map[QStringLiteral("pssMb")] = p.value("pss_mb").toInt(0)` 및 `rssMb`를 정확히 대입.
- `updateFallbackTelemetry`: IPC 질의 대기 중에도 `/proc/meminfo`를 파싱하여 기본 RAM 지표 즉각 제공.

### 2.3. QML 뷰 계층 (`src/ui/qml/DashboardWindow.qml`)
- **Card 1**: 패키지 전력과 클럭 하단에 시스템 RAM(사용량/총량, 사용률, 프로그레스 바) 및 Swap 사용량을 표시.
- **프로세스 리스트**: `modelData["pssMb"]`를 통해 실제 메모리 점유량을 M단위로 표시하고 툴팁에 PSS/RSS 병기.
