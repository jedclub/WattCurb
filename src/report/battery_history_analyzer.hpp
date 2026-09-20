#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include "ipc/history_ring_buffer.hpp"
#include "core/types.hpp"

namespace wattcurb::report {

// Implements REF-REQ-078 & REF-ARCH-055: Deep Battery Drain Telemetry Analyzer
struct HardwareDomainDrain {
    std::string name;
    std::string icon;
    double wh{0.0};
    double avg_watts{0.0};
    double percent{0.0};
    std::string color_hex;
    std::string saving_tip;
};

struct ProcessDrainCulprit {
    int rank{0};
    int pid{0};
    std::string comm;
    uint32_t uid{0};
    std::string domain;
    double drain_wh{0.0};
    double avg_watts{0.0};
    double share_percent{0.0};
    double wdi_score{0.0};
    std::string mechanism;
    std::string action_str;
};

struct BatteryDischargeSummary {
    uint32_t total_samples_analyzed{0};
    uint32_t discharging_samples{0};
    uint64_t total_discharge_duration_sec{0};
    std::string duration_str;
    
    double total_discharge_wh{0.0};
    double total_discharge_mah{0.0};
    double total_discharge_joules{0.0};
    
    int battery_start_pct{0};
    int battery_end_pct{0};
    int battery_drop_pct{0};
    
    double avg_discharge_watts{0.0};
    double peak_discharge_watts{0.0};
    uint64_t peak_timestamp_sec{0};
    std::string peak_time_str;
    
    double avg_cstate_c3_percent{0.0};
    double avg_cpu_temp_c{0.0};
    
    std::string primary_culprit_comm;
    std::string primary_culprit_domain;
    double primary_culprit_share_pct{0.0};
    
    std::string diagnostic_summary;
    std::string recommendation_text;
};

struct BatteryDrainReportResult {
    BatteryDischargeSummary summary;
    std::vector<HardwareDomainDrain> hardware_shares;
    std::vector<ProcessDrainCulprit> process_culprits;
    
    std::string to_markdown() const;
    std::string to_plain_text() const;
};

class BatteryHistoryAnalyzer {
public:
    // Core analytical engine (testable with mock data)
    static BatteryDrainReportResult analyze(
        const ipc::HistoryPoint* points,
        size_t count,
        const std::vector<ProcessAttributedPower>& top_procs,
        double current_voltage_v = 11.4
    );

    // Read directly from live /dev/shm/wattcurb_history.shm
    static BatteryDrainReportResult analyze_shm(
        const std::vector<ProcessAttributedPower>& top_procs,
        double current_voltage_v = 11.4
    );
};

} // namespace wattcurb::report
