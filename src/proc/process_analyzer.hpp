#pragma once

#include "core/types.hpp"
#include <filesystem>
#include <vector>
#include <string_view>

namespace wattcurb::proc {

// Implements REF-REQ-004, REF-ARCH-002
class ProcessAnalyzer {
public:
    explicit ProcessAnalyzer(std::filesystem::path procfs_root = "/proc");

    [[nodiscard]] std::vector<ProcessSample> capture_active_processes(
        const std::vector<ProcessSample>* prev_samples = nullptr) const;

    // Exposed for granular unit testing (REF-TEST-002)
    static bool parse_proc_stat(std::string_view content, ProcessSample& out_sample);
    static bool parse_proc_status(std::string_view content, ProcessSample& out_sample);
    static bool parse_proc_io(std::string_view content, ProcessSample& out_sample);
    static bool parse_drm_fdinfo(std::string_view content, ProcessSample& out_sample);

private:
    std::filesystem::path procfs_root_;

    bool read_pid_details(int32_t pid, ProcessSample& sample) const;
    void inspect_pid_drm_fds(int32_t pid, ProcessSample& sample) const;
};

} // namespace wattcurb::proc
