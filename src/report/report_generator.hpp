#pragma once

#include "core/types.hpp"
#include <iosfwd>

namespace wattcurb::report {

// Implements REF-REQ-005, REF-REQ-019, REF-ARCH-002, REF-ARCH-008
class ReportGenerator {
public:
    // Part 1: Human-readable Executive Text Briefing (REF-REQ-019)
    static void render_executive_briefing(const AnalysisReportData& report, std::ostream& out);

    // Part 2: Detailed Developer Terminal Dashboard & Machine Struct/JSON
    static void render_terminal(const AnalysisReportData& report, std::ostream& out);
    static void render_json(const AnalysisReportData& report, std::ostream& out);
};

} // namespace wattcurb::report
