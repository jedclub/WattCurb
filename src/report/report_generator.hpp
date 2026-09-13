#pragma once

#include "core/types.hpp"
#include <iosfwd>

namespace wattcurb::report {

// Implements REF-REQ-005, REF-REQ-019, REF-ARCH-002, REF-ARCH-008
class ReportGenerator {
public:
    // Part 1: Human-readable Executive Text Briefing (REF-REQ-019, REF-REQ-020)
    static void render_executive_briefing(const AnalysisReportData& report, std::ostream& out);

    // Part 2: Detailed Developer Terminal Dashboard
    static void render_terminal(const AnalysisReportData& report, std::ostream& out);

    // Part 3: Feature Metadata Catalog Briefing (REF-REQ-020, REF-ARCH-010)
    static void render_feature_catalog(std::ostream& out);

    // Part 4: Extreme 30s Battery & Hardware Causation Profile for LLM Feature Synthesis (REF-REQ-021, REF-ARCH-010)
    static void render_extreme_profile(const AnalysisReportData& report, std::ostream& out);
};

} // namespace wattcurb::report
