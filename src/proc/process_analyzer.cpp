#include "proc/process_analyzer.hpp"

#include <array>
#include <charconv>
#include <cstring>
#include <fcntl.h>
#include <system_error>
#include <unistd.h>

namespace wattcurb::proc {

namespace {

std::string read_small_file(const std::filesystem::path& path, size_t max_bytes = 4096) {
    int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return {};

    std::string content(max_bytes, '\0');
    ssize_t bytes = ::read(fd, content.data(), max_bytes);
    ::close(fd);

    if (bytes <= 0) return {};
    content.resize(static_cast<size_t>(bytes));
    return content;
}

} // namespace

ProcessAnalyzer::ProcessAnalyzer(std::filesystem::path procfs_root)
    : procfs_root_(std::move(procfs_root)) {}

std::vector<ProcessSample> ProcessAnalyzer::capture_active_processes() const {
    std::vector<ProcessSample> samples;
    samples.reserve(256);

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(procfs_root_, ec)) {
        if (!entry.is_directory(ec)) continue;

        const auto filename = entry.path().filename().string();
        int32_t pid = 0;
        auto [ptr, conv_ec] = std::from_chars(filename.data(), filename.data() + filename.size(), pid);
        if (conv_ec != std::errc() || pid <= 0) continue;

        ProcessSample sample;
        sample.pid = pid;
        if (read_pid_details(pid, sample)) {
            inspect_pid_drm_fds(pid, sample);
            samples.push_back(std::move(sample));
        }
    }

    return samples;
}

bool ProcessAnalyzer::read_pid_details(int32_t pid, ProcessSample& sample) const {
    auto pid_dir = procfs_root_ / std::to_string(pid);

    // 1. Read /proc/[pid]/stat
    auto stat_content = read_small_file(pid_dir / "stat", 1024);
    if (stat_content.empty() || !parse_proc_stat(stat_content, sample)) {
        return false;
    }

    // 2. Read /proc/[pid]/status
    auto status_content = read_small_file(pid_dir / "status", 2048);
    if (!status_content.empty()) {
        parse_proc_status(status_content, sample);
    }

    // 3. Read /proc/[pid]/io (may fail if permissions are restricted, gracefully ignore)
    auto io_content = read_small_file(pid_dir / "io", 1024);
    if (!io_content.empty()) {
        parse_proc_io(io_content, sample);
    }

    return true;
}

void ProcessAnalyzer::inspect_pid_drm_fds(int32_t pid, ProcessSample& sample) const {
    auto fd_dir = procfs_root_ / std::to_string(pid) / "fd";
    auto fdinfo_dir = procfs_root_ / std::to_string(pid) / "fdinfo";

    std::error_code ec;
    if (!std::filesystem::exists(fd_dir, ec)) return;

    for (const auto& fd_entry : std::filesystem::directory_iterator(fd_dir, ec)) {
        auto target = std::filesystem::read_symlink(fd_entry.path(), ec);
        if (ec) continue;

        auto target_str = target.string();
        // Check if pointing to DRM render node or card
        if (target_str.find("/dev/dri/") != std::string::npos) {
            auto fd_name = fd_entry.path().filename().string();
            auto info_file = fdinfo_dir / fd_name;
            auto fdinfo_content = read_small_file(info_file, 2048);
            if (!fdinfo_content.empty()) {
                parse_drm_fdinfo(fdinfo_content, sample);
            }
        }
    }
}

bool ProcessAnalyzer::parse_proc_stat(std::string_view content, ProcessSample& out_sample) {
    // Format: pid (comm) state ppid ...
    auto open_paren = content.find('(');
    auto close_paren = content.rfind(')');
    if (open_paren == std::string_view::npos || close_paren == std::string_view::npos || close_paren <= open_paren) {
        return false;
    }

    // Parse PID
    int32_t pid = 0;
    auto [p_ptr, p_ec] = std::from_chars(content.data(), content.data() + open_paren, pid);
    if (p_ec == std::errc()) {
        out_sample.pid = pid;
    }

    // Parse comm
    out_sample.comm = std::string(content.substr(open_paren + 1, close_paren - open_paren - 1));

    // Parse fields after ')'
    // State is right after ') '
    auto remaining = content.substr(close_paren + 1);
    const char* cur = remaining.data();
    const char* end = remaining.data() + remaining.size();

    // Skip state (single char) and 10 preceding numbers to reach field 14 (utime)
    // Field 3: state
    // Field 4: ppid
    // Field 5: pgrp
    // Field 6: session
    // Field 7: tty_nr
    // Field 8: tpgid
    // Field 9: flags
    // Field 10: minflt
    // Field 11: cminflt
    // Field 12: majflt
    // Field 13: cmajflt
    // Field 14: utime
    // Field 15: stime

    // Skip whitespace and tokens
    int token_count = 2; // pid and comm are tokens 1 & 2
    while (cur < end && token_count < 13) {
        while (cur < end && (*cur == ' ' || *cur == '\t')) ++cur;
        if (cur >= end) break;
        while (cur < end && *cur != ' ' && *cur != '\t') ++cur;
        ++token_count;
    }

    // Now at field 14: utime
    while (cur < end && (*cur == ' ' || *cur == '\t')) ++cur;
    if (cur < end) {
        uint64_t utime = 0;
        auto [u_ptr, u_ec] = std::from_chars(cur, end, utime);
        if (u_ec == std::errc()) {
            out_sample.utime_ticks = utime;
            cur = u_ptr;
        }
    }

    // Now at field 15: stime
    while (cur < end && (*cur == ' ' || *cur == '\t')) ++cur;
    if (cur < end) {
        uint64_t stime = 0;
        auto [s_ptr, s_ec] = std::from_chars(cur, end, stime);
        if (s_ec == std::errc()) {
            out_sample.stime_ticks = stime;
        }
    }

    return true;
}

bool ProcessAnalyzer::parse_proc_status(std::string_view content, ProcessSample& out_sample) {
    size_t pos = 0;
    while (pos < content.size()) {
        auto next_nl = content.find('\n', pos);
        if (next_nl == std::string_view::npos) next_nl = content.size();
        auto line = content.substr(pos, next_nl - pos);
        pos = next_nl + 1;

        if (line.rfind("Uid:", 0) == 0) {
            const char* cur = line.data() + 4;
            const char* end = line.data() + line.size();
            while (cur < end && (*cur == ' ' || *cur == '\t')) ++cur;
            uint32_t uid = 0;
            auto [ptr, ec] = std::from_chars(cur, end, uid);
            if (ec == std::errc()) out_sample.uid = uid;
        } else if (line.rfind("voluntary_ctxt_switches:", 0) == 0) {
            const char* cur = line.data() + 24;
            const char* end = line.data() + line.size();
            while (cur < end && (*cur == ' ' || *cur == '\t')) ++cur;
            uint64_t val = 0;
            auto [ptr, ec] = std::from_chars(cur, end, val);
            if (ec == std::errc()) out_sample.voluntary_ctxt_switches = val;
        } else if (line.rfind("nonvoluntary_ctxt_switches:", 0) == 0) {
            const char* cur = line.data() + 27;
            const char* end = line.data() + line.size();
            while (cur < end && (*cur == ' ' || *cur == '\t')) ++cur;
            uint64_t val = 0;
            auto [ptr, ec] = std::from_chars(cur, end, val);
            if (ec == std::errc()) out_sample.nonvoluntary_ctxt_switches = val;
        }
    }
    return true;
}

bool ProcessAnalyzer::parse_proc_io(std::string_view content, ProcessSample& out_sample) {
    size_t pos = 0;
    while (pos < content.size()) {
        auto next_nl = content.find('\n', pos);
        if (next_nl == std::string_view::npos) next_nl = content.size();
        auto line = content.substr(pos, next_nl - pos);
        pos = next_nl + 1;

        if (line.rfind("read_bytes:", 0) == 0) {
            const char* cur = line.data() + 11;
            const char* end = line.data() + line.size();
            while (cur < end && (*cur == ' ' || *cur == '\t')) ++cur;
            uint64_t val = 0;
            auto [ptr, ec] = std::from_chars(cur, end, val);
            if (ec == std::errc()) out_sample.read_bytes = val;
        } else if (line.rfind("write_bytes:", 0) == 0) {
            const char* cur = line.data() + 12;
            const char* end = line.data() + line.size();
            while (cur < end && (*cur == ' ' || *cur == '\t')) ++cur;
            uint64_t val = 0;
            auto [ptr, ec] = std::from_chars(cur, end, val);
            if (ec == std::errc()) out_sample.write_bytes = val;
        }
    }
    return true;
}

bool ProcessAnalyzer::parse_drm_fdinfo(std::string_view content, ProcessSample& out_sample) {
    size_t pos = 0;
    while (pos < content.size()) {
        auto next_nl = content.find('\n', pos);
        if (next_nl == std::string_view::npos) next_nl = content.size();
        auto line = content.substr(pos, next_nl - pos);
        pos = next_nl + 1;

        if (line.rfind("drm-engine-gfx:", 0) == 0) {
            const char* cur = line.data() + 15;
            const char* end = line.data() + line.size();
            while (cur < end && (*cur == ' ' || *cur == '\t')) ++cur;
            uint64_t val = 0;
            auto [ptr, ec] = std::from_chars(cur, end, val);
            if (ec == std::errc()) out_sample.drm_engine_gfx_ns += val;
        } else if (line.rfind("drm-engine-compute:", 0) == 0) {
            const char* cur = line.data() + 19;
            const char* end = line.data() + line.size();
            while (cur < end && (*cur == ' ' || *cur == '\t')) ++cur;
            uint64_t val = 0;
            auto [ptr, ec] = std::from_chars(cur, end, val);
            if (ec == std::errc()) out_sample.drm_engine_compute_ns += val;
        } else if (line.rfind("drm-memory-vram:", 0) == 0 || line.rfind("drm-resident-vram:", 0) == 0) {
            auto colon = line.find(':');
            if (colon != std::string_view::npos) {
                const char* cur = line.data() + colon + 1;
                const char* end = line.data() + line.size();
                while (cur < end && (*cur == ' ' || *cur == '\t')) ++cur;
                uint64_t val = 0;
                auto [ptr, ec] = std::from_chars(cur, end, val);
                if (ec == std::errc() && val > out_sample.drm_vram_kib) {
                    out_sample.drm_vram_kib = val;
                }
            }
        }
    }
    return true;
}

} // namespace wattcurb::proc
