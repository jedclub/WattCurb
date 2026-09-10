#pragma once

#include "core/types.hpp"
#include <vector>
#include <span>

namespace wattcurb::policy {

// Implements REF-REQ-004, REF-RES-003, REF-ARCH-002
class AttributionEngine {
public:
    AttributionEngine() = default;

    [[nodiscard]] AnalysisReportData compute_attribution(
        const HardwareSample& hw1,
        const HardwareSample& hw2,
        std::span<const ProcessSample> proc1,
        std::span<const ProcessSample> proc2,
        size_t top_n = 15
    ) const;

    // Implements REF-REQ-012 (Continuous Multi-Sample Window Evaluation)
    [[nodiscard]] AnalysisReportData compute_windowed_attribution(
        const std::vector<HardwareSample>& hw_samples,
        const std::vector<std::vector<ProcessSample>>& proc_samples,
        size_t top_n = 15
    ) const;

    [[nodiscard]] AnalysisReportData compute_windowed_attribution(
        std::span<const HardwareSample> hw_samples,
        std::span<const ProcessSnapshot> proc_samples,
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
