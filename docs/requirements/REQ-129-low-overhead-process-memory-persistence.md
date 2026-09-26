# [REF-REQ-129] Low-Overhead Process Memory Telemetry & SHM Persistence

**Status**: Implemented · **Date**: 2026-09-26  
**Related**: [`REF-REQ-028`](REQ-028-adaptive-power-state-machine-and-actuation.md), [`REF-REQ-059`](REQ-059-lockless-in-memory-history-ring-buffer.md), [`REF-REQ-112`](REQ-112-boost-restoration-and-non-halting-memory-guard.md)  
**Verification**: [`REF-TEST-083`](../../tests/test_units.cpp)

---

## 1. 개요 및 목적 (Problem Statement)

기존 WattCurb 데몬은 `/proc/<pid>/statm` 및 `/proc/meminfo`를 통해 프로세스의 PSS/RSS 및 시스템 메모리를 수집하여 내부 어트리뷰션 계산에 활용하고 있었으나, **공유 메모리(SHM) Seqlock POD 및 7일 히스토리 링버퍼(HistoryRingBuffer)에는 메모리 지표가 저장되지 않고 예약 패딩(padding/reserved)으로 비워져 있었다.**

메모리 누수나 고갈 상황을 사후 분석하고, 데스크톱 트레이 및 CLI가 0-Syscall로 상위 프로세스의 실제 메모리 점유(RSS, PSS)와 시스템 스왑 사용량을 즉각 확인할 수 있도록, **0바이트 추가 메모리 할당(Zero-Allocation) 및 O(1) 비용으로 프로세스 메모리 지표를 SHM에 영구 기록**한다.

---

## 2. 세부 요구사항 (Specifications)

### REQ-129.1: 128B Seqlock SharedCulprit 메모리 필드 내장
- `SharedCulprit` (32바이트 POD)의 기존 6바이트 패딩(`padding[6]`)을 구조체 레이아웃 변경 없이 다음 필드로 재할당한다:
  - `uint16_t rss_mb`: 프로세스 실제 물리 메모리(Resident Set Size, MB 단위)
  - `uint16_t pss_mb`: 프로세스 공정 점유 메모리(Proportional Set Size, MB 단위)
  - `uint16_t majflt_s`: 초당 중대 페이지 폴트 횟수 (스왑/디스크 I/O 유발 지표)
- `sizeof(SharedCulprit) == 32` 및 `sizeof(WattCurbSharedState) == 128` 불변식을 100% 엄격히 유지한다.

### REQ-129.2: 32B HistoryPoint 시스템 및 프로세스 메모리 기록
- `HistoryPoint` (32바이트 POD)의 기존 7바이트 예약 공간(`reserved[7]`)을 다음 필드로 재할당한다:
  - `uint16_t mem_used_mb`: 시스템 전체 물리 RAM 실사용량 (MB 단위)
  - `uint16_t swap_used_mb`: 시스템 전체 스왑 사용량 (MB 단위)
  - `uint16_t top_proc_pss_mb`: 시스템 내 최대 메모리 점유 프로세스의 PSS (MB 단위)
  - `uint8_t reserved[1]`: 잔여 1바이트 예약 패딩
- `sizeof(HistoryPoint) == 32` 및 `HistoryRingBufferShm` 1.85 MiB 레이아웃을 100% 엄격히 유지한다.

### REQ-129.3: 제로 할당 및 제로 오버헤드 불변식 (Zero-Allocation Invariant)
- 메모리 지표 기록은 기존 데몬 관측 주기(10초 / 60초)의 Seqlock 원자적 갱신 루프 내에서 $O(1)$ 산술 연산으로 수행되며, 추가적인 힙 동적 할당(`malloc`/`new`)을 일절 발생시키지 않는다.
