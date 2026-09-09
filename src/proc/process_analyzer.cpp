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

static const ProcessSample* find_prev_sample(const std::vector<ProcessSample>& prev, int32_t pid) noexcept {
    auto it = std::lower_bound(prev.begin(), prev.end(), pid, [](const ProcessSample& s, int32_t p) noexcept {
        return s.pid < p;
    });
    if (it != prev.end() && it->pid == pid) {
        return &(*it);
    }
    return nullptr;
}

inline void skip_token_fast(const char*& cur, const char* end) noexcept {
    while (cur < end && *cur != ' ') ++cur;
    while (cur < end && *cur == ' ') ++cur;
}

inline uint64_t parse_u64_fast(const char*& cur, const char* end) noexcept {
    uint64_t val = 0;
    while (cur < end && *cur >= '0' && *cur <= '9') {
        val = val * 10 + static_cast<uint64_t>(*cur - '0');
        ++cur;
    }
    return val;
}

inline int32_t parse_i32_fast(const char*& cur, const char* end) noexcept {
    bool neg = false;
    if (cur < end && *cur == '-') {
        neg = true;
        ++cur;
    }
    int32_t val = 0;
    while (cur < end && *cur >= '0' && *cur <= '9') {
        val = val * 10 + (*cur - '0');
        ++cur;
    }
    return neg ? -val : val;
}

} // namespace

ProcessAnalyzer::ProcessAnalyzer(std::filesystem::path procfs_root)
    : procfs_root_(std::move(procfs_root)) {}

std::vector<ProcessSample> ProcessAnalyzer::capture_active_processes(
    const std::vector<ProcessSample>* prev_samples) const {
    std::vector<ProcessSample> samples;
    samples.reserve(448);

    alignas(64) char read_buf[2048];
    char path_buf[128];

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(procfs_root_, ec)) {
        if (!entry.is_directory(ec)) continue;

        const auto filename = entry.path().filename().string();
        int32_t pid = 0;
        auto [ptr, conv_ec] = std::from_chars(filename.data(), filename.data() + filename.size(), pid);
        if (conv_ec != std::errc() || pid <= 0) continue;

        ProcessSample sample;
        sample.pid = pid;

        // 1. Read /proc/[pid]/stat
        std::snprintf(path_buf, sizeof(path_buf), "%s/%d/stat", procfs_root_.c_str(), pid);
        size_t bytes = 0;
        if (!read_file_to_stack_buf(path_buf, read_buf, sizeof(read_buf), bytes)) {
            continue;
        }

        if (!parse_proc_stat(std::string_view(read_buf, bytes), sample)) {
            continue;
        }

        // Lazy Deep Inspection: Skip kernel threads (ppid == 2)
        if (sample.ppid == 2) {
            samples.push_back(std::move(sample));
            continue;
        }

        // Lazy Deep Inspection: If previous sample exists and CPU ticks did not change,
        // the process was completely sleeping! Skip reading /proc/[pid]/status, io, and fd/!
        if (prev_samples != nullptr) {
            const auto* prev = find_prev_sample(*prev_samples, sample.pid);
            if (prev != nullptr &&
                sample.utime_ticks == prev->utime_ticks &&
                sample.stime_ticks == prev->stime_ticks) {
                sample.uid = prev->uid;
                sample.voluntary_ctxt_switches = prev->voluntary_ctxt_switches;
                sample.nonvoluntary_ctxt_switches = prev->nonvoluntary_ctxt_switches;
                sample.read_bytes = prev->read_bytes;
                sample.write_bytes = prev->write_bytes;
                sample.io_syscalls = prev->io_syscalls;
                sample.drm_engine_gfx_ns = prev->drm_engine_gfx_ns;
                sample.drm_engine_compute_ns = prev->drm_engine_compute_ns;
                sample.drm_engine_dec_ns = prev->drm_engine_dec_ns;
                sample.drm_engine_enc_ns = prev->drm_engine_enc_ns;
                sample.drm_vram_kib = prev->drm_vram_kib;
                sample.timerslack_ns = prev->timerslack_ns;
                sample.pss_kib = prev->pss_kib;
                sample.rss_kib = prev->rss_kib;
                sample.open_sockets = prev->open_sockets;
                samples.push_back(std::move(sample));
                continue;
            }
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

        // 4. Read /proc/[pid]/statm (DRAM PSS & RSS - REF-REQ-013)
        std::snprintf(path_buf, sizeof(path_buf), "%s/%d/statm", procfs_root_.c_str(), pid);
        if (read_file_to_stack_buf(path_buf, read_buf, sizeof(read_buf), bytes)) {
            parse_proc_statm(std::string_view(read_buf, bytes), sample);
        }

        // 5. Read /proc/[pid]/timerslack_ns (Kernel Timer Coalescing - REF-REQ-013)
        std::snprintf(path_buf, sizeof(path_buf), "%s/%d/timerslack_ns", procfs_root_.c_str(), pid);
        if (read_file_to_stack_buf(path_buf, read_buf, sizeof(read_buf), bytes)) {
            uint64_t slack = 50000;
            auto [ptr, ec_slack] = std::from_chars(read_buf, read_buf + bytes, slack);
            if (ec_slack == std::errc()) sample.timerslack_ns = slack;
        }

        // 6. Inspect /proc/[pid]/fd (DRM & Sockets - REF-REQ-013)
        inspect_pid_fds(pid, sample);

        samples.push_back(std::move(sample));
    }

    // Always return sorted by PID for O(log N) binary search lookup in next cycle
    std::sort(samples.begin(), samples.end(), [](const ProcessSample& a, const ProcessSample& b) noexcept {
        return a.pid < b.pid;
    });

    return samples;
}

bool ProcessAnalyzer::read_pid_details(int32_t pid, ProcessSample& sample) const {
    char path_buf[128];
    alignas(64) char read_buf[2048];
    size_t bytes = 0;

    std::snprintf(path_buf, sizeof(path_buf), "%s/%d/stat", procfs_root_.c_str(), pid);
    if (!read_file_to_stack_buf(path_buf, read_buf, sizeof(read_buf), bytes)) {
        return false;
    }

    if (!parse_proc_stat(std::string_view(read_buf, bytes), sample)) {
        return false;
    }

    if (sample.ppid == 2) {
        return true;
    }

    std::snprintf(path_buf, sizeof(path_buf), "%s/%d/status", procfs_root_.c_str(), pid);
    if (read_file_to_stack_buf(path_buf, read_buf, sizeof(read_buf), bytes)) {
        parse_proc_status(std::string_view(read_buf, bytes), sample);
    }

    std::snprintf(path_buf, sizeof(path_buf), "%s/%d/io", procfs_root_.c_str(), pid);
    if (read_file_to_stack_buf(path_buf, read_buf, sizeof(read_buf), bytes)) {
        parse_proc_io(std::string_view(read_buf, bytes), sample);
    }

    std::snprintf(path_buf, sizeof(path_buf), "%s/%d/statm", procfs_root_.c_str(), pid);
    if (read_file_to_stack_buf(path_buf, read_buf, sizeof(read_buf), bytes)) {
        parse_proc_statm(std::string_view(read_buf, bytes), sample);
    }

    return true;
}

void ProcessAnalyzer::inspect_pid_fds(int32_t pid, ProcessSample& sample) const {
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
        if (target_str.rfind("socket:[", 0) == 0) {
            ++sample.open_sockets;
        } else if (target_str.find("/dev/dri/") != std::string::npos) {
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

bool ProcessAnalyzer::parse_proc_statm(std::string_view content, ProcessSample& out_sample) {
    // Format: size resident shared text lib data dirty (in pages)
    const char* cur = content.data();
    const char* end = cur + content.size();

    // Skip size
    core::simd::skip_whitespace_simd(cur, end);
    core::simd::find_whitespace_simd(cur, end);

    // Read resident
    core::simd::skip_whitespace_simd(cur, end);
    uint64_t res_pages = 0;
    auto [r_ptr, r_ec] = std::from_chars(cur, end, res_pages);
    if (r_ec != std::errc()) return false;
    cur = r_ptr;

    // Read shared
    core::simd::skip_whitespace_simd(cur, end);
    uint64_t sh_pages = 0;
    auto [s_ptr, s_ec] = std::from_chars(cur, end, sh_pages);
    if (s_ec != std::errc()) return false;

    out_sample.rss_kib = res_pages * 4;
    out_sample.pss_kib = (res_pages >= sh_pages ? (res_pages - sh_pages) + (sh_pages / 2) : res_pages) * 4;
    return true;
}


bool ProcessAnalyzer::parse_proc_stat(std::string_view content, ProcessSample& out_sample) {
    // Format: pid (comm) state ppid ...
    auto open_paren = content.find('(');
    auto close_paren = content.rfind(')');
    if (open_paren == std::string_view::npos || close_paren == std::string_view::npos || close_paren <= open_paren) {
        return false;
    }

    // Parse PID
    const char* p_cur = content.data();
    out_sample.pid = parse_i32_fast(p_cur, content.data() + open_paren);

    // Parse comm
    out_sample.comm = std::string(content.substr(open_paren + 1, close_paren - open_paren - 1));

    // Parse fields after ')'
    const char* cur = content.data() + close_paren + 1;
    const char* end = content.data() + content.size();

    // Skip whitespace after ')'
    while (cur < end && *cur == ' ') ++cur;

    // Field 3: state (single char)
    if (cur < end) ++cur;
    while (cur < end && *cur == ' ') ++cur;

    // Field 4: ppid
    out_sample.ppid = parse_i32_fast(cur, end);
    while (cur < end && *cur == ' ') ++cur;

    // Tokens 5..9: Skip 5 tokens to reach Token 10 (minflt)
    for (int i = 5; i <= 9 && cur < end; ++i) {
        skip_token_fast(cur, end);
    }

    // Token 10: minflt
    out_sample.minflt = parse_u64_fast(cur, end);
    while (cur < end && *cur == ' ') ++cur;

    // Token 11: cminflt (skip 1 token)
    skip_token_fast(cur, end);

    // Token 12: majflt
    out_sample.majflt = parse_u64_fast(cur, end);
    while (cur < end && *cur == ' ') ++cur;

    // Token 13: cmajflt (skip 1 token)
    skip_token_fast(cur, end);

    // Token 14: utime
    out_sample.utime_ticks = parse_u64_fast(cur, end);
    while (cur < end && *cur == ' ') ++cur;

    // Token 15: stime
    out_sample.stime_ticks = parse_u64_fast(cur, end);
    while (cur < end && *cur == ' ') ++cur;

    // Tokens 16..19: Skip 4 tokens to reach Token 20 (num_threads)
    for (int i = 16; i <= 19 && cur < end; ++i) {
        skip_token_fast(cur, end);
    }

    // Token 20: num_threads
    out_sample.num_threads = static_cast<uint32_t>(parse_u64_fast(cur, end));
    while (cur < end && *cur == ' ') ++cur;

    // Tokens 21..38: Skip 18 tokens to reach Token 39 (processor)
    for (int i = 21; i <= 38 && cur < end; ++i) {
        skip_token_fast(cur, end);
    }

    // Token 39: processor (Core ID)
    if (cur < end) {
        out_sample.cpu_core = parse_i32_fast(cur, end);
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
        } else if (line.rfind("syscr:", 0) == 0 || line.rfind("syscw:", 0) == 0) {
            const char* colon = core::simd::find_char_fast(line.data(), line.data() + line.size(), ':');
            if (colon != line.data() + line.size()) {
                const char* l_cur = colon + 1;
                const char* l_end = line.data() + line.size();
                core::simd::skip_whitespace_simd(l_cur, l_end);
                uint64_t val = 0;
                auto [ptr, ec] = std::from_chars(l_cur, l_end, val);
                if (ec == std::errc()) out_sample.io_syscalls += val;
            }
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
        } else if (line.rfind("drm-engine-dec:", 0) == 0) {
            const char* l_cur = line.data() + 15;
            const char* l_end = line.data() + line.size();
            core::simd::skip_whitespace_simd(l_cur, l_end);
            uint64_t val = 0;
            auto [ptr, ec] = std::from_chars(l_cur, l_end, val);
            if (ec == std::errc()) out_sample.drm_engine_dec_ns += val;
        } else if (line.rfind("drm-engine-enc:", 0) == 0) {
            const char* l_cur = line.data() + 15;
            const char* l_end = line.data() + line.size();
            core::simd::skip_whitespace_simd(l_cur, l_end);
            uint64_t val = 0;
            auto [ptr, ec] = std::from_chars(l_cur, l_end, val);
            if (ec == std::errc()) out_sample.drm_engine_enc_ns += val;
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
