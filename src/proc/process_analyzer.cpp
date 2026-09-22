#include "proc/process_analyzer.hpp"
#include "core/cpu_features.hpp"
#include "core/scoped_profiler.hpp"

#include <array>
#include <cerrno>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <system_error>
#include <unistd.h>

namespace wattcurb::proc {

namespace {

// Linux 64-bit kernel dirent layout for direct SYS_getdents64 (REF-RES-006)
struct LinuxDirent64 {
    uint64_t        d_ino;
    int64_t         d_off;
    unsigned short  d_reclen;
    unsigned char   d_type;
    char            d_name[1];
};

// Fast, zero-allocation stack buffer reader (REF-REQ-007, REF-ARCH-005)
bool read_file_to_stack_buf(const char* path, char* buf, size_t max_len, size_t& out_bytes) noexcept {
    if (max_len == 0) return false;
    int fd = ::open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;

    // A single read() may return short even for a small procfs file. Loop until
    // EOF or the buffer is full so the parser never sees a half-written buffer.
    size_t total = 0;
    const size_t cap = max_len - 1;
    while (total < cap) {
        ssize_t n = ::read(fd, buf + total, cap - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            ::close(fd);
            return false;
        }
        if (n == 0) break;
        total += static_cast<size_t>(n);
    }
    ::close(fd);

    if (total == 0) return false;
    buf[total] = '\0';
    out_bytes = total;
    return true;
}

// Ultra-low overhead openat reader avoiding full VFS path walk from root (REF-REQ-009, REF-ARCH-005)
inline bool read_fileat_to_stack_buf(int dirfd, const char* rel_path, char* buf, size_t max_len, size_t& out_bytes) noexcept {
    if (max_len == 0) return false;
    int fd = ::openat(dirfd, rel_path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;

    size_t total = 0;
    const size_t cap = max_len - 1;
    while (total < cap) {
        ssize_t n = ::read(fd, buf + total, cap - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            ::close(fd);
            return false;
        }
        if (n == 0) break;
        total += static_cast<size_t>(n);
    }
    ::close(fd);

    if (total == 0) return false;
    buf[total] = '\0';
    out_bytes = total;
    return true;
}

// Inlined fast integer to ASCII subpath formatter (completely eliminates std::snprintf)
inline void format_pid_subpath(char* out, int32_t pid, const char* subpath, size_t subpath_len) noexcept {
    char* p = out;
    uint32_t u = static_cast<uint32_t>(pid);
    char digits[10];
    int ti = 0;
    do {
        digits[ti++] = static_cast<char>('0' + (u % 10));
        u /= 10;
    } while (u > 0);
    while (ti > 0) {
        *p++ = digits[--ti];
    }
    std::memcpy(p, subpath, subpath_len);
    p[subpath_len] = '\0';
}

inline void skip_token_fast(const char*& cur, const char* end) noexcept {
    while (cur < end && *cur != ' ') ++cur;
    while (cur < end && *cur == ' ') ++cur;
}

inline uint64_t parse_u64_fast(const char*& cur, const char* end) noexcept {
    uint64_t val = 0;
    while (cur < end && static_cast<unsigned char>(*cur - '0') <= 9) {
        val = val * 10 + static_cast<uint64_t>(*cur - '0');
        ++cur;
    }
    return val;
}

inline int32_t parse_i32_fast(const char*& cur, const char* end) noexcept {
    if (cur >= end) return 0;
    const int32_t is_neg = (*cur == '-');
    cur += is_neg;

    int32_t val = 0;
    while (cur < end && static_cast<unsigned char>(*cur - '0') <= 9) {
        val = val * 10 + static_cast<int32_t>(*cur - '0');
        ++cur;
    }
    const int32_t mask = -is_neg;
    return (val ^ mask) + is_neg;
}

} // namespace

template <typename OutputContainer, typename PrevContainer>
static void do_capture_active_processes(
    const ProcessAnalyzer& analyzer,
    OutputContainer& samples,
    const PrevContainer* prev_samples,
    std::string_view procfs_root,
    core::FixedVector<int32_t, 512>& kthread_pids,
    [[maybe_unused]] uint64_t pass_counter
) {
    WATTCURB_PROFILE_SCOPE("proc.capture_active_all");
    samples.clear();

    alignas(64) char read_buf[2048];
    char path_buf[128];

    char root_buf[128];
    size_t rlen = std::min(procfs_root.size(), sizeof(root_buf) - 1);
    std::memcpy(root_buf, procfs_root.data(), rlen);
    root_buf[rlen] = '\0';

    int proc_dfd = ::open(root_buf, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (proc_dfd < 0) return;

    alignas(64) char dentry_buf[16384];
    while (true) {
        long nread = 0;
        {
            WATTCURB_PROFILE_SCOPE("proc.root_getdents");
            nread = ::syscall(SYS_getdents64, proc_dfd, dentry_buf, sizeof(dentry_buf));
        }
        if (nread <= 0) break;

        for (long bpos = 0; bpos < nread;) {
            auto* proc_entry = reinterpret_cast<const LinuxDirent64*>(dentry_buf + bpos);
            bpos += proc_entry->d_reclen;

            if (proc_entry->d_type != DT_DIR && proc_entry->d_type != DT_UNKNOWN) continue;
            if (proc_entry->d_name[0] < '1' || proc_entry->d_name[0] > '9') continue;

            int32_t pid = 0;
            const char* p_cur = proc_entry->d_name;
            while (*p_cur >= '0' && *p_cur <= '9') {
                pid = pid * 10 + (*p_cur - '0');
                ++p_cur;
            }
            if (*p_cur != '\0') continue;

            {
                WATTCURB_PROFILE_SCOPE("proc.kthread_filter");
                if (std::binary_search(kthread_pids.begin(), kthread_pids.end(), pid)) {
                    continue;
                }
            }

            ProcessSample sample;
            sample.pid = pid;

            format_pid_subpath(path_buf, pid, "/stat", 5);
            size_t bytes = 0;
            {
                WATTCURB_PROFILE_SCOPE("proc.stat_read");
                if (!read_fileat_to_stack_buf(proc_dfd, path_buf, read_buf, sizeof(read_buf), bytes)) {
                    continue;
                }
            }

            {
                WATTCURB_PROFILE_SCOPE("proc.stat_parse");
                if (!ProcessAnalyzer::parse_proc_stat(std::string_view(read_buf, bytes), sample)) {
                    continue;
                }
            }

            if (sample.ppid == 2) {
                if (!std::binary_search(kthread_pids.begin(), kthread_pids.end(), pid)) {
                    if (kthread_pids.size() < kthread_pids.capacity()) {
                        kthread_pids.push_back(pid);
                        std::sort(kthread_pids.begin(), kthread_pids.end());
                    }
                }
                samples.push_back(std::move(sample));
                continue;
            }

            const ProcessSample* prev = nullptr;
            if (prev_samples != nullptr) {
                auto it = std::lower_bound(prev_samples->begin(), prev_samples->end(), sample.pid,
                    [](const ProcessSample& s, int32_t p) noexcept { return s.pid < p; });
                if (it != prev_samples->end() && it->pid == sample.pid) {
                    prev = &(*it);
                }
            }

            if (prev != nullptr &&
                sample.utime_ticks == prev->utime_ticks &&
                sample.stime_ticks == prev->stime_ticks) {
                WATTCURB_PROFILE_SCOPE("proc.lazy_deep_skip");
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
                sample.nice = prev->nice;
                sample.priority = prev->priority;
                sample.open_sockets = prev->open_sockets;
                sample.pinned_drm_fd = prev->pinned_drm_fd;
                sample.has_io_perm = prev->has_io_perm;
                samples.push_back(std::move(sample));
                continue;
            }

            {
                WATTCURB_PROFILE_SCOPE("proc.status_read_parse");
                format_pid_subpath(path_buf, pid, "/status", 7);
                if (read_fileat_to_stack_buf(proc_dfd, path_buf, read_buf, sizeof(read_buf), bytes)) {
                    ProcessAnalyzer::parse_proc_status(std::string_view(read_buf, bytes), sample);
                }
            }

            const uint64_t cur_ticks = sample.utime_ticks + sample.stime_ticks;
            const uint64_t prev_ticks = (prev != nullptr) ? (prev->utime_ticks + prev->stime_ticks) : 0;
            const uint64_t delta_ticks = (cur_ticks >= prev_ticks) ? (cur_ticks - prev_ticks) : 0;

            if (prev != nullptr && !prev->has_io_perm) {
                sample.has_io_perm = false;
            } else if (prev != nullptr && delta_ticks < 2 && (pass_counter % 2 == 1)) {
                // Interleaved Pacing: Reuse previous I/O counters for low-delta processes (REF-REQ-027)
                sample.read_bytes = prev->read_bytes;
                sample.write_bytes = prev->write_bytes;
                sample.io_syscalls = prev->io_syscalls;
                sample.has_io_perm = prev->has_io_perm;
            } else {
                WATTCURB_PROFILE_SCOPE("proc.io_read_parse");
                format_pid_subpath(path_buf, pid, "/io", 3);
                if (read_fileat_to_stack_buf(proc_dfd, path_buf, read_buf, sizeof(read_buf), bytes)) {
                    ProcessAnalyzer::parse_proc_io(std::string_view(read_buf, bytes), sample);
                } else {
                    sample.has_io_perm = false;
                }
            }

            if (prev != nullptr && delta_ticks < 2 && (pass_counter % 2 == 0)) {
                // Interleaved Pacing: Reuse memory footprint for low-delta processes (REF-REQ-027)
                sample.pss_kib = prev->pss_kib;
                sample.rss_kib = prev->rss_kib;
            } else {
                WATTCURB_PROFILE_SCOPE("proc.statm_read_parse");
                format_pid_subpath(path_buf, pid, "/statm", 6);
                if (read_fileat_to_stack_buf(proc_dfd, path_buf, read_buf, sizeof(read_buf), bytes)) {
                    ProcessAnalyzer::parse_proc_statm(std::string_view(read_buf, bytes), sample);
                }
            }

            if (prev != nullptr && prev->timerslack_ns > 0) {
                sample.timerslack_ns = prev->timerslack_ns;
            } else {
                WATTCURB_PROFILE_SCOPE("proc.timerslack_read");
                format_pid_subpath(path_buf, pid, "/timerslack_ns", 14);
                if (read_fileat_to_stack_buf(proc_dfd, path_buf, read_buf, sizeof(read_buf), bytes)) {
                    uint64_t slack = 50000;
                    auto [ptr, ec_slack] = std::from_chars(read_buf, read_buf + bytes, slack);
                    if (ec_slack == std::errc()) sample.timerslack_ns = slack;
                }
            }

            {
                WATTCURB_PROFILE_SCOPE("proc.fd_socket_scan");
                analyzer.inspect_pid_fds(pid, sample, prev, proc_dfd);
            }

            samples.push_back(std::move(sample));
        }
    }
    ::close(proc_dfd);

    {
        WATTCURB_PROFILE_SCOPE("proc.samples_sort");
        std::sort(samples.begin(), samples.end(), [](const ProcessSample& a, const ProcessSample& b) noexcept {
            return a.pid < b.pid;
        });
    }
}

ProcessAnalyzer::ProcessAnalyzer(std::string_view procfs_root)
    : procfs_root_(procfs_root) {}

std::vector<ProcessSample> ProcessAnalyzer::capture_active_processes(
    const std::vector<ProcessSample>* prev_samples) const {
    ++pass_counter_;
    std::vector<ProcessSample> samples;
    samples.reserve(448);
    do_capture_active_processes(*this, samples, prev_samples, procfs_root_.view(), kthread_pids_, pass_counter_);
    return samples;
}

void ProcessAnalyzer::capture_active_processes(
    ProcessSnapshot& out,
    const ProcessSnapshot* prev_samples) const {
    ++pass_counter_;
    do_capture_active_processes(*this, out, prev_samples, procfs_root_.view(), kthread_pids_, pass_counter_);
}

ProcessSnapshot ProcessAnalyzer::capture_snapshot(
    const ProcessSnapshot* prev_samples) const {
    ProcessSnapshot res;
    capture_active_processes(res, prev_samples);
    return res;
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

void ProcessAnalyzer::inspect_pid_fds(int32_t pid, ProcessSample& sample, const ProcessSample* prev, int proc_dfd) const {
    // Fast bypass for ephemeral and low-activity processes (REF-REQ-027)
    // GUI worker threads, compilers, and idle tools (< 20 total ticks & < 50 ctxt switches)
    // almost never hold network sockets or DRM nodes.
    if (prev == nullptr) {
        if ((sample.utime_ticks + sample.stime_ticks < 20 && sample.voluntary_ctxt_switches < 50) ||
            (sample.num_threads == 1 && sample.io_syscalls == 0 && sample.minflt == 0)) {
            sample.open_sockets = 0;
            sample.pinned_drm_fd = -1; // Unchecked ephemeral process
            return;
        }
    }

    // 1. Persistent DRM FD Pinning (REF-RES-006, REF-REQ-051):
    // If we previously located a DRM render node FD for this PID, query its fdinfo directly with ZERO readlinkat!
    bool drm_resolved = false;
    if (prev != nullptr) {
        if (prev->pinned_drm_fd >= 0) {
            char info_path[160];
            std::snprintf(info_path, sizeof(info_path), "%s/%d/fdinfo/%d", procfs_root_.c_str(), pid, prev->pinned_drm_fd);
            alignas(64) char fdinfo_buf[2048];
            size_t bytes = 0;
            {
                WATTCURB_PROFILE_SCOPE("proc.fd_drm_fdinfo");
                if (read_file_to_stack_buf(info_path, fdinfo_buf, sizeof(fdinfo_buf), bytes)) {
                    if (parse_drm_fdinfo(std::string_view(fdinfo_buf, bytes), sample)) {
                        sample.pinned_drm_fd = prev->pinned_drm_fd;
                        drm_resolved = true;
                    }
                }
            }
        } else if (prev->pinned_drm_fd == -2) {
            // Already verified in previous turns that this is a Non-GPU process!
            sample.pinned_drm_fd = -2;
            drm_resolved = true;
        } else {
            sample.pinned_drm_fd = -1;
        }
    }

    // 2. Fast Socket and FD Bypass (REF-RES-006, REF-RES-007, REF-REQ-027, REF-REQ-052):
    // If previous sample exists, preserve socket count.
    // WiFi CAM attribution strictly requires wakeups_per_sec > 10 (delta_sw >= 20 over 2s).
    // In steady state, established processes maintain static socket counts; eliminate VFS readlink storms!
    if (prev != nullptr) {
        sample.open_sockets = prev->open_sockets;
        if (drm_resolved || prev->pinned_drm_fd <= 0) {
            // If process had 0 sockets previously, rescan only periodically every 10 passes (~20s) (REF-REQ-052)
            if (prev->open_sockets == 0) {
                if (pass_counter_ % 10 != 0) {
                    return;
                }
            } else {
                // Established network processes: rescan every 8 passes (~16s) to eliminate VFS readlink storms (REF-REQ-027)
                if (pass_counter_ % 8 != 0) {
                    return;
                }
            }
        }
    }

    // 3. Direct SYS_getdents64 Directory Scan via openat (Zero-Heap Allocation) (REF-RES-006, REF-REQ-027)
    char rel_fd_path[32];
    format_pid_subpath(rel_fd_path, pid, "/fd", 3);

    int dfd = -1;
    {
        WATTCURB_PROFILE_SCOPE("proc.fd_opendir");
        if (proc_dfd >= 0) {
            dfd = ::openat(proc_dfd, rel_fd_path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        } else {
            char full_fd_path[128];
            std::snprintf(full_fd_path, sizeof(full_fd_path), "%s/%s", procfs_root_.c_str(), rel_fd_path);
            dfd = ::open(full_fd_path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        }
    }
    if (dfd < 0) return;

    char fdinfo_dir_path[128];
    std::snprintf(fdinfo_dir_path, sizeof(fdinfo_dir_path), "%s/%d/fdinfo", procfs_root_.c_str(), pid);

    alignas(64) char dentry_buf[2048];
    alignas(64) char symlink_buf[64]; // REF-RES-007: Single 64-byte cacheline buffer
    alignas(64) char fdinfo_buf[2048];

    // 64-bit constants for 1-cycle string matching (REF-ARCH-005)
    constexpr uint64_t SOCKET_PREFIX = 0x5b3a74656b636f73ULL; // "socket:["
    constexpr uint64_t DRI_PREFIX    = 0x6972642f7665642fULL; // "/dev/dri"

    uint32_t sockets_count = 0;
    {
        WATTCURB_PROFILE_SCOPE("proc.fd_readlink_loop");
        while (true) {
            long nread = ::syscall(SYS_getdents64, dfd, dentry_buf, sizeof(dentry_buf));
            if (nread <= 0) break;

            for (long bpos = 0; bpos < nread;) {
                auto* entry = reinterpret_cast<const LinuxDirent64*>(dentry_buf + bpos);
                bpos += entry->d_reclen;

                if (entry->d_name[0] < '0' || entry->d_name[0] > '9') continue;
                if (entry->d_type == DT_REG || entry->d_type == DT_DIR) continue;
                if (entry->d_type != DT_LNK && entry->d_type != DT_UNKNOWN) continue;

                ssize_t len = ::syscall(SYS_readlinkat, dfd, entry->d_name, symlink_buf, sizeof(symlink_buf) - 1);
                if (len <= 0) continue;
                symlink_buf[len] = '\0';

                // 1-cycle 64-bit register comparison
                if (len >= 8 && (*reinterpret_cast<const uint64_t*>(symlink_buf) == SOCKET_PREFIX)) {
                    ++sockets_count;
                    // Cap at 32 sockets: WiFi CAM attribution saturates at 0.65W (REF-REQ-011).
                    // Prevents scanning hundreds of file/pipe FDs in browsers and complex daemons!
                    if (drm_resolved && sockets_count >= 32) {
                        break;
                    }
                } else if (!drm_resolved && len >= 9 && (*reinterpret_cast<const uint64_t*>(symlink_buf) == DRI_PREFIX) && symlink_buf[8] == '/') {
                    char info_path[160];
                    std::snprintf(info_path, sizeof(info_path), "%s/%s", fdinfo_dir_path, entry->d_name);

                    size_t bytes = 0;
                    {
                        WATTCURB_PROFILE_SCOPE("proc.fd_drm_fdinfo");
                        if (read_file_to_stack_buf(info_path, fdinfo_buf, sizeof(fdinfo_buf), bytes)) {
                            if (parse_drm_fdinfo(std::string_view(fdinfo_buf, bytes), sample)) {
                                int32_t fd_val = -1;
                                if (std::from_chars(entry->d_name, entry->d_name + std::strlen(entry->d_name), fd_val).ec == std::errc()) {
                                    sample.pinned_drm_fd = fd_val;
                                    drm_resolved = true;
                                }
                            }
                        }
                    }
                }
            }
            if (drm_resolved && sockets_count >= 32) break;
        }
    }
    sample.open_sockets = sockets_count;
    if (!drm_resolved && sample.pinned_drm_fd < 0) {
        sample.pinned_drm_fd = -2; // Full scan verified that this process has NO DRM render node!
    }

    ::close(dfd);
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
    const char* start = content.data();
    const char* end = start + content.size();

    const char* open_paren = core::simd::find_char_fast(start, end, '(');
    if (open_paren == end) return false;

    // Fast parse PID before '('
    const char* p_cur = start;
    out_sample.pid = parse_i32_fast(p_cur, open_paren);

    // Fast find matching ')' from open_paren + 1 with AVX2
    const char* close_paren = core::simd::find_char_fast(open_paren + 1, end, ')');
    if (close_paren == end) return false;

    // Check for rare case of nested ')' in process comm
    const char* next_close = core::simd::find_char_fast(close_paren + 1, end, ')');
    while (next_close != end) {
        close_paren = next_close;
        next_close = core::simd::find_char_fast(close_paren + 1, end, ')');
    }

    // Parse comm into zero-allocation ProcessComm (REF-ARCH-005)
    out_sample.comm = ProcessComm(std::string_view(open_paren + 1, static_cast<size_t>(close_paren - open_paren - 1)));

    // Parse fields after ')'
    const char* cur = close_paren + 1;
    core::simd::skip_whitespace_simd(cur, end);

    // Field 3: state (single char)
    if (cur < end) ++cur;
    core::simd::skip_whitespace_simd(cur, end);

    // Field 4: ppid
    out_sample.ppid = parse_i32_fast(cur, end);
    core::simd::skip_whitespace_simd(cur, end);

    // Tokens 5..9: Skip 5 tokens rapidly with AVX2 & BMI2 PDEP bitmask to reach Token 10 (minflt)
    core::simd::skip_tokens_simd(cur, end, 5);

    // Token 10: minflt
    out_sample.minflt = parse_u64_fast(cur, end);
    core::simd::skip_whitespace_simd(cur, end);

    // Token 11: cminflt (skip 1 token)
    core::simd::skip_tokens_simd(cur, end, 1);

    // Token 12: majflt
    out_sample.majflt = parse_u64_fast(cur, end);
    core::simd::skip_whitespace_simd(cur, end);

    // Token 13: cmajflt (skip 1 token)
    core::simd::skip_tokens_simd(cur, end, 1);

    // Token 14: utime
    out_sample.utime_ticks = parse_u64_fast(cur, end);
    core::simd::skip_whitespace_simd(cur, end);

    // Token 15: stime
    out_sample.stime_ticks = parse_u64_fast(cur, end);
    core::simd::skip_whitespace_simd(cur, end);

    // Tokens 16..17: Skip cutime and cstime (2 tokens) with fast scalar/SIMD
    core::simd::skip_tokens_simd(cur, end, 2);

    // Token 18: priority
    out_sample.priority = parse_i32_fast(cur, end);
    core::simd::skip_whitespace_simd(cur, end);

    // Token 19: nice
    out_sample.nice = parse_i32_fast(cur, end);
    core::simd::skip_whitespace_simd(cur, end);

    // Token 20: num_threads
    out_sample.num_threads = static_cast<uint32_t>(parse_u64_fast(cur, end));
    core::simd::skip_whitespace_simd(cur, end);

    // Tokens 21..38: Skip 18 tokens rapidly with AVX2 & BMI2 PDEP vector bitmask to reach Token 39 (processor)
    core::simd::skip_tokens_simd(cur, end, 18);

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
