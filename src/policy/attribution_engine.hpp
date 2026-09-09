#pragma once

#include "core/types.hpp"
#include <vector>

namespace wattcurb::policy {

// Implements REF-REQ-004, REF-RES-003, REF-ARCH-002
class AttributionEngine {
public:
    AttributionEngine() = default;

    [[nodiscard]] AnalysisReportData compute_attribution(
        const HardwareSample& hw1,
        const HardwareSample& hw2,
        const std::vector<ProcessSample>& proc1,
        const std::vector<ProcessSample>& proc2,
        size_t top_n = 15
    ) const;

private:
    [[nodiscard]] HardwarePowerBreakdown compute_hardware_power(
        const HardwareSample& hw1,
        const HardwareSample& hw2,
        double delta_sec
    ) const;
};

} // namespace wattcurb::policy
