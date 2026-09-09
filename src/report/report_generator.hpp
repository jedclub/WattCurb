#pragma once

#include "core/types.hpp"
#include <iosfwd>

namespace wattcurb::report {

// Implements REF-REQ-005, REF-ARCH-002
class ReportGenerator {
public:
    static void render_terminal(const AnalysisReportData& report, std::ostream& out);
    static void render_json(const AnalysisReportData& report, std::ostream& out);
};

} // namespace wattcurb::report
