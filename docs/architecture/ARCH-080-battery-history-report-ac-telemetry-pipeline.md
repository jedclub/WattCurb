# REF-ARCH-080: 배터리 히스토리 분석기 AC 텔레메트리 듀얼 소스 파이프라인 (Battery History Analyzer AC Telemetry Dual-Source Pipeline)

## 1. 아키텍처 배경 및 설계 원칙 (Architectural Context)
기존 `BatteryHistoryAnalyzer`는 방전(`battery_state == 1`) 샘플만을 1차 타깃으로 삼아 설계되어, AC 전원 연결 중(`battery_state == 0 || 2`)에 축적된 텔레메트리를 분석 대상에서 배제하였다.
그러나 사용자가 AC 전원에 연결된 채로 특정 프로파일(예: 절전 모드)을 장시간 사용할 경우, 해당 프로파일의 실제 하드웨어 소비 전력(CPU RAPL W, GPU W, C3 Deep Sleep) 데이터가 엄연히 존재함에도 불구하고 리포트에서는 `0개`로 표기되는 비직관성이 발생했다.

본 아키텍처는 **단일 순회(Single-Pass) 듀얼 어큐뮬레이터(Dual-Accumulator)** 구조를 도입하여 O(N) 시간 복잡도와 제로 힙 할당을 유지하면서 방전 및 AC 텔레메트리를 동시에 수집하고 유연하게 폴백하는 파이프라인을 구축한다.

---

## 2. 파이프라인 구조 및 데이터 흐름 (Data Flow & Pipeline Architecture)

```
                            [HistoryRingBufferShm (60,480 samples)]
                                              │
                                              ▼ (Single O(N) Pass)
                       ┌──────────────────────┴──────────────────────┐
                       │                                             │
                       ▼ (battery_state == 1)                        ▼ (battery_state != 1)
         [discharge_accs[4] (Discharging)]              [ac_accs[4] (AC / Passthrough)]
                       │                                             │
                       └──────────────────────┬──────────────────────┘
                                              │
                                              ▼
                             [Resolve ProfileComparisonEntry[4]]
                             - If discharge_accs[m].count > 0:
                                   Use Discharging (pure drain)
                             - Else if ac_accs[m].count > 0:
                                   Use AC Telemetry with "[⚡AC]" Badge
                                              │
                                              ▼
                                 [Active Points Resolution]
                             - If discharge_pts empty for filter:
                                   Fallback to all_matching_pts (AC)
                             - Update Summary Labels & Diagnostics
```

### 2.1. 듀얼 어큐뮬레이터 구조체
```cpp
struct ModeAccumulator {
    uint32_t count{0};
    double total_sys_energy_wh{0.0};
    double peak_watts{0.0};
    double c3_sum{0.0};
    double temp_sum{0.0};
    bool is_ac{false};
};

ModeAccumulator discharge_accs[4]{};
ModeAccumulator ac_accs[4]{};
```
단일 루프 내에서 프로파일 모드 `m`에 따라:
- `points[i].battery_state == 1`: `discharge_accs[m]`에 에너지, C3, 온도, 피크 전력을 가산.
- `points[i].battery_state != 1`: `ac_accs[m]`에 에너지, C3, 온도, 피크 전력을 가산.

### 2.2. 매트릭스 해결(Resolution) 규칙
- 각 모드 `m`에 대해:
  ```cpp
  ModeAccumulator mode_accs[4]{};
  for (uint8_t m = 0; m < 4; ++m) {
      if (discharge_accs[m].count > 0) {
          mode_accs[m] = discharge_accs[m];
          mode_accs[m].is_ac = false;
      } else if (ac_accs[m].count > 0) {
          mode_accs[m] = ac_accs[m];
          mode_accs[m].is_ac = true;
      }
  }
  ```
  이를 통해 4개 프로파일 중 방전 데이터가 없는 프로파일도 AC 전력 통계가 온전히 채워진다.

### 2.3. 상세 리포트 필터링 로직 개선
기존:
```cpp
if (filter_mode >= 0 && discharge_pts.empty()) return result; // Defect: early drop
```
개선:
```cpp
std::vector<const ipc::HistoryPoint*> discharge_pts;
std::vector<const ipc::HistoryPoint*> all_matching_pts;
discharge_pts.reserve(count);
all_matching_pts.reserve(count);

for (size_t i = 0; i < count; ++i) {
    if (filter_mode < 0 || points[i].power_profile_mode == static_cast<uint8_t>(filter_mode)) {
        all_matching_pts.push_back(&points[i]);
        if (points[i].battery_state == 1) {
            discharge_pts.push_back(&points[i]);
        }
    }
}

if (all_matching_pts.empty()) {
    // True empty: no samples at all under this profile
    ...
    return result;
}

bool is_pure_discharging = !discharge_pts.empty();
const auto& active_pts = is_pure_discharging ? discharge_pts : all_matching_pts;
```

---

## 3. UI 및 텍스트 렌더링 일관성 보장
- `is_pure_discharging` 여부에 따라:
  - 방전 모드: `dur_ss << hours << "h " << mins << "m (" << active_pts.size() << " samples) [" << mode_names[filter_mode] << "]"`
  - AC 모드: `dur_ss << hours << "h " << mins << "m (" << active_pts.size() << " samples) [⚡ " << mode_names[filter_mode] << " (AC)]"`
- 마크다운 및 QML 카드 라벨의 적응형 텍스트:
  - `discharging_samples > 0` ? "Total Energy Discharged" : "Total Energy Consumed (AC)"
  - `discharging_samples > 0` ? "Average Discharge Power" : "Average System Power"
