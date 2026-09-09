#include "proc/process_analyzer.hpp"
#include "core/cpu_features.hpp"

#include <array>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <system_error>
#include <unistd.h>

namespace wattcurb::proc {

namespace {

// Fast, zero-allocation stack buffer reader (REF-REQ-007, REF-ARCH-005)
bool read_file_to_stack_buf(const char* path, char* buf, size_t max_len, size_t& out_bytes) noexcept {
    int fd = ::open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;

    ssize_t bytes = ::read(fd, buf, max_len - 1);
    ::close(fd);

    if (bytes <= 0) return false;
    buf[bytes] = '\0';
    out_bytes = static_cast<size_t>(bytes);
    return true;
}

} // namespace

ProcessAnalyzer::ProcessAnalyzer(std::filesystem::path procfs_root)
    : procfs_root_(std::move(procfs_root)) {}

std::vector<ProcessSample> ProcessAnalyzer::capture_active_processes() const {
    std::vector<ProcessSample> samples;
    samples.reserve(384);

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
            // Only inspect DRM fds for user processes (kernel threads and system daemons don't render)
            if (sample.ppid != 2 && sample.uid >= 1000) {
                inspect_pid_drm_fds(pid, sample);
            }
            samples.push_back(std::move(sample));
        }
    }

    return samples;
}

bool ProcessAnalyzer::read_pid_details(int32_t pid, ProcessSample& sample) const {
    char path_buf[128];
    alignas(64) char read_buf[2048];
    size_t bytes = 0;

    // 1. Read /proc/[pid]/stat
    std::snprintf(path_buf, sizeof(path_buf), "%s/%d/stat", procfs_root_.c_str(), pid);
    if (!read_file_to_stack_buf(path_buf, read_buf, sizeof(read_buf), bytes)) {
        return false;
    }

    if (!parse_proc_stat(std::string_view(read_buf, bytes), sample)) {
        return false;
    }

    // Lazy Deep Inspection: Kernel threads (ppid == 2) have no user status/io/drm
    if (sample.ppid == 2) {
        return true;
    }

    // 2. Read /proc/[pid]/status
    std::snprintf(path_buf, sizeof(path_buf), "%s/%d/status", procfs_root_.c_str(), pid);
    if (read_file_to_stack_buf(path_buf, read_buf, sizeof(read_buf), bytes)) {
        parse_proc_status(std::string_view(read_buf, bytes), sample);
    }

    // 3. Read /proc/[pid]/io (may be permission restricted, gracefully ignore)
    std::snprintf(path_buf, sizeof(path_buf), "%s/%d/io", procfs_root_.c_str(), pid);
    if (read_file_to_stack_buf(path_buf, read_buf, sizeof(read_buf), bytes)) {
        parse_proc_io(std::string_view(read_buf, bytes), sample);
    }

    return true;
}

void ProcessAnalyzer::inspect_pid_drm_fds(int32_t pid, ProcessSample& sample) const {
    char fd_dir_path[128];
    char fdinfo_dir_path[128];
    std::snprintf(fd_dir_path, sizeof(fd_dir_path), "%s/%d/fd", procfs_root_.c_str(), pid);
    std::snprintf(fdinfo_dir_path, sizeof(fdinfo_dir_path), "%s/%d/fdinfo", procfs_root_.c_str(), pid);

    std::error_code ec;
    std::filesystem::path fd_dir(fd_dir_path);
    if (!std::filesystem::exists(fd_dir, ec)) return;

    alignas(64) char fdinfo_buf[2048];

    for (const auto& fd_entry : std::filesystem::directory_iterator(fd_dir, ec)) {
        auto target = std::filesystem::read_symlink(fd_entry.path(), ec);
        if (ec) continue;

        auto target_str = target.string();
        if (target_str.find("/dev/dri/") != std::string::npos) {
            auto fd_name = fd_entry.path().filename().string();
            char info_path[160];
            std::snprintf(info_path, sizeof(info_path), "%s/%s", fdinfo_dir_path, fd_name.c_str());

            size_t bytes = 0;
            if (read_file_to_stack_buf(info_path, fdinfo_buf, sizeof(fdinfo_buf), bytes)) {
                parse_drm_fdinfo(std::string_view(fdinfo_buf, bytes), sample);
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
    auto remaining = content.substr(close_paren + 1);
    const char* cur = remaining.data();
    const char* end = remaining.data() + remaining.size();

    // Field 3: state
    core::simd::skip_whitespace_simd(cur, end);
    if (cur < end) ++cur; // skip single state char

    // Field 4: ppid
    core::simd::skip_whitespace_simd(cur, end);
    if (cur < end) {
        int32_t ppid = 0;
        auto [pp_ptr, pp_ec] = std::from_chars(cur, end, ppid);
        if (pp_ec == std::errc()) {
            out_sample.ppid = ppid;
            cur = pp_ptr;
        }
    }

    // Skip fields 5 through 13 to reach field 14 (utime)
    // Field 5: pgrp
    // Field 6: session
    // Field 7: tty_nr
    // Field 8: tpgid
    // Field 9: flags
    // Field 10: minflt
    // Field 11: cminflt
    // Field 12: majflt
    // Field 13: cmajflt
    int token_count = 4; // We already passed tokens 1, 2, 3, 4
    while (cur < end && token_count < 13) {
        core::simd::skip_whitespace_simd(cur, end);
        if (cur >= end) break;
        core::simd::find_whitespace_simd(cur, end);
        ++token_count;
    }

    // Now at field 14: utime
    core::simd::skip_whitespace_simd(cur, end);
    if (cur < end) {
        uint64_t utime = 0;
        auto [u_ptr, u_ec] = std::from_chars(cur, end, utime);
        if (u_ec == std::errc()) {
            out_sample.utime_ticks = utime;
            cur = u_ptr;
        }
    }

    // Now at field 15: stime
    core::simd::skip_whitespace_simd(cur, end);
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
    const char* cur = content.data();
    const char* end = cur + content.size();

    while (cur < end) {
        const char* next_nl = core::simd::find_char_fast(cur, end, '\n');
        std::string_view line(cur, static_cast<size_t>(next_nl - cur));
        cur = (next_nl < end) ? next_nl + 1 : end;

        if (line.rfind("Uid:", 0) == 0) {
            const char* l_cur = line.data() + 4;
            const char* l_end = line.data() + line.size();
            core::simd::skip_whitespace_simd(l_cur, l_end);
            uint32_t uid = 0;
            auto [ptr, ec] = std::from_chars(l_cur, l_end, uid);
            if (ec == std::errc()) out_sample.uid = uid;
        } else if (line.rfind("voluntary_ctxt_switches:", 0) == 0) {
            const char* l_cur = line.data() + 24;
            const char* l_end = line.data() + line.size();
            core::simd::skip_whitespace_simd(l_cur, l_end);
            uint64_t val = 0;
            auto [ptr, ec] = std::from_chars(l_cur, l_end, val);
            if (ec == std::errc()) out_sample.voluntary_ctxt_switches = val;
        } else if (line.rfind("nonvoluntary_ctxt_switches:", 0) == 0) {
            const char* l_cur = line.data() + 27;
            const char* l_end = line.data() + line.size();
            core::simd::skip_whitespace_simd(l_cur, l_end);
            uint64_t val = 0;
            auto [ptr, ec] = std::from_chars(l_cur, l_end, val);
            if (ec == std::errc()) out_sample.nonvoluntary_ctxt_switches = val;
        }
    }
    return true;
}

bool ProcessAnalyzer::parse_proc_io(std::string_view content, ProcessSample& out_sample) {
    const char* cur = content.data();
    const char* end = cur + content.size();

    while (cur < end) {
        const char* next_nl = core::simd::find_char_fast(cur, end, '\n');
        std::string_view line(cur, static_cast<size_t>(next_nl - cur));
        cur = (next_nl < end) ? next_nl + 1 : end;

        if (line.rfind("read_bytes:", 0) == 0) {
            const char* l_cur = line.data() + 11;
            const char* l_end = line.data() + line.size();
            core::simd::skip_whitespace_simd(l_cur, l_end);
            uint64_t val = 0;
            auto [ptr, ec] = std::from_chars(l_cur, l_end, val);
            if (ec == std::errc()) out_sample.read_bytes = val;
        } else if (line.rfind("write_bytes:", 0) == 0) {
            const char* l_cur = line.data() + 12;
            const char* l_end = line.data() + line.size();
            core::simd::skip_whitespace_simd(l_cur, l_end);
            uint64_t val = 0;
            auto [ptr, ec] = std::from_chars(l_cur, l_end, val);
            if (ec == std::errc()) out_sample.write_bytes = val;
        }
    }
    return true;
}

bool ProcessAnalyzer::parse_drm_fdinfo(std::string_view content, ProcessSample& out_sample) {
    const char* cur = content.data();
    const char* end = cur + content.size();

    while (cur < end) {
        const char* next_nl = core::simd::find_char_fast(cur, end, '\n');
        std::string_view line(cur, static_cast<size_t>(next_nl - cur));
        cur = (next_nl < end) ? next_nl + 1 : end;

        if (line.rfind("drm-engine-gfx:", 0) == 0) {
            const char* l_cur = line.data() + 15;
            const char* l_end = line.data() + line.size();
            core::simd::skip_whitespace_simd(l_cur, l_end);
            uint64_t val = 0;
            auto [ptr, ec] = std::from_chars(l_cur, l_end, val);
            if (ec == std::errc()) out_sample.drm_engine_gfx_ns += val;
        } else if (line.rfind("drm-engine-compute:", 0) == 0) {
            const char* l_cur = line.data() + 19;
            const char* l_end = line.data() + line.size();
            core::simd::skip_whitespace_simd(l_cur, l_end);
            uint64_t val = 0;
            auto [ptr, ec] = std::from_chars(l_cur, l_end, val);
            if (ec == std::errc()) out_sample.drm_engine_compute_ns += val;
        } else if (line.rfind("drm-memory-vram:", 0) == 0 || line.rfind("drm-resident-vram:", 0) == 0) {
            const char* colon = core::simd::find_char_fast(line.data(), line.data() + line.size(), ':');
            if (colon != line.data() + line.size()) {
                const char* l_cur = colon + 1;
                const char* l_end = line.data() + line.size();
                core::simd::skip_whitespace_simd(l_cur, l_end);
                uint64_t val = 0;
                auto [ptr, ec] = std::from_chars(l_cur, l_end, val);
                if (ec == std::errc() && val > out_sample.drm_vram_kib) {
                    out_sample.drm_vram_kib = val;
                }
            }
        }
    }
    return true;
}

} // namespace wattcurb::proc
