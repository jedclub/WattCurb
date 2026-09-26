#include "policy/process_pageout_actuator.hpp"
#include "policy/process_classifier.hpp"
#include "core/event_logger.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <cstring>
#include <cstdio>
#include <algorithm>

#ifndef SYS_pidfd_open
#define SYS_pidfd_open 434
#endif

#ifndef SYS_process_madvise
#define SYS_process_madvise 440
#endif

#ifndef MADV_PAGEOUT
#define MADV_PAGEOUT 21
#endif

namespace wattcurb::policy {

namespace {

// Parses hexadecimal uint64_t from string
[[nodiscard]] const char* parse_hex_u64(const char* p, const char* end, uint64_t& out) noexcept {
    out = 0;
    bool found = false;
    while (p < end && ((*p >= '0' && *p <= '9') ||
                       (*p >= 'a' && *p <= 'f') ||
                       (*p >= 'A' && *p <= 'F'))) {
        found = true;
        out <<= 4u;
        if (*p >= '0' && *p <= '9') {
            out |= static_cast<uint64_t>(*p - '0');
        } else if (*p >= 'a' && *p <= 'f') {
            out |= static_cast<uint64_t>(*p - 'a' + 10);
        } else {
            out |= static_cast<uint64_t>(*p - 'A' + 10);
        }
        ++p;
    }
    return found ? p : nullptr;
}

} // namespace

bool ProcessPageoutActuator::is_exempt(
    int32_t pid,
    int32_t protected_pid,
    uint8_t safety_tier,
    bool is_audio_active
) noexcept {
    if (pid <= 1) return true;
    if (protected_pid != 0 && pid == protected_pid) return true;
    if (is_audio_active) return true;

    const auto tier = static_cast<ProcessSafetyTier>(safety_tier);
    if (tier == ProcessSafetyTier::CriticalImmune ||
        tier == ProcessSafetyTier::DesktopCore ||
        tier == ProcessSafetyTier::DesktopShell) {
        return true;
    }

    return false;
}

PageoutResult ProcessPageoutActuator::pageout_process(
    int32_t pid,
    uint64_t max_bytes
) noexcept {
    PageoutResult result{};
    if (pid <= 1) return result;

    const int pidfd = static_cast<int>(::syscall(SYS_pidfd_open, pid, 0));
    if (pidfd < 0) return result;

    char maps_path[64];
    std::snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);
    const int maps_fd = ::open(maps_path, O_RDONLY | O_CLOEXEC);
    if (maps_fd < 0) {
        ::close(pidfd);
        return result;
    }

    struct iovec iov[MAX_IOV_BATCH];
    size_t iov_count = 0;
    uint64_t total_paged_out = 0;
    uint32_t total_vmas = 0;

    auto flush_batch = [&]() noexcept {
        if (iov_count == 0) return;
        const ssize_t ret = ::syscall(SYS_process_madvise, pidfd, iov, iov_count, MADV_PAGEOUT, 0);
        if (ret >= 0) {
            total_paged_out += static_cast<uint64_t>(ret);
        }
        iov_count = 0;
    };

    char buf[4096];
    size_t carry = 0;

    while (total_paged_out < max_bytes) {
        const ssize_t bytes_read = ::read(maps_fd, buf + carry, sizeof(buf) - carry - 1);
        if (bytes_read <= 0) break;

        const size_t total_buf = carry + static_cast<size_t>(bytes_read);
        buf[total_buf] = '\0';

        const char* line_start = buf;
        const char* const buf_end = buf + total_buf;

        while (line_start < buf_end && total_paged_out < max_bytes) {
            const char* const line_end = std::strchr(line_start, '\n');
            if (line_end == nullptr) {
                // Incomplete line at end of buffer, carry over
                break;
            }

            // Parse line: 7f4958000000-7f4958021000 rw-p 00000000 00:00 0 [heap]
            uint64_t start_addr = 0;
            uint64_t end_addr = 0;

            const char* p = parse_hex_u64(line_start, line_end, start_addr);
            if (p != nullptr && p < line_end && *p == '-') {
                p = parse_hex_u64(p + 1, line_end, end_addr);
            }

            if (p != nullptr && p + 5 < line_end && *p == ' ') {
                ++p; // Skip space
                const char r = p[0];
                const char w = p[1];
                const char s = p[3];

                // Target writable private anonymous memory (rw-p)
                if (r == 'r' && w == 'w' && s == 'p') {
                    // Check inode: skip offset and dev
                    // Seek to inode field
                    p += 4;
                    // Skip offset
                    while (p < line_end && *p == ' ') ++p;
                    while (p < line_end && *p != ' ') ++p;
                    // Skip dev
                    while (p < line_end && *p == ' ') ++p;
                    while (p < line_end && *p != ' ') ++p;
                    // Read inode
                    while (p < line_end && *p == ' ') ++p;
                    uint64_t inode = 0;
                    while (p < line_end && *p >= '0' && *p <= '9') {
                        inode = inode * 10 + static_cast<uint64_t>(*p - '0');
                        ++p;
                    }

                    // An anonymous mapping has inode == 0 and is not a file
                    if (inode == 0 && end_addr > start_addr) {
                        const uint64_t vma_size = end_addr - start_addr;
                        if (vma_size >= MIN_VMA_SIZE_BYTES) {
                            const uint64_t claim_size = std::min(vma_size, max_bytes - total_paged_out);
                            iov[iov_count].iov_base = reinterpret_cast<void*>(start_addr);
                            iov[iov_count].iov_len = static_cast<size_t>(claim_size);
                            ++iov_count;
                            ++total_vmas;

                            if (iov_count >= MAX_IOV_BATCH) {
                                flush_batch();
                            }
                        }
                    }
                }
            }

            line_start = line_end + 1;
        }

        // Shift remaining unparsed line to front
        carry = static_cast<size_t>(buf_end - line_start);
        if (carry > 0 && line_start != buf) {
            std::memmove(buf, line_start, carry);
        }
    }

    // Flush any remaining batched IOVs
    flush_batch();

    ::close(maps_fd);
    ::close(pidfd);

    result.success = (total_paged_out > 0);
    result.bytes_paged_out = total_paged_out;
    result.vma_count = total_vmas;
    return result;
}

uint64_t ProcessPageoutActuator::pageout_candidates(
    const AnalysisReportData& report,
    int32_t protected_pid,
    size_t max_candidates
) noexcept {
    uint64_t total_paged = 0;
    size_t actuated_count = 0;

    for (size_t i = 0; i < report.top_processes.size() && actuated_count < max_candidates; ++i) {
        const auto& p = report.top_processes[i];
        if (p.pid <= 1) continue;

        // Invariants: protected focused window and critical daemons are immune
        if (is_exempt(p.pid, protected_pid, p.safety_tier, /*is_audio_active=*/false)) {
            continue;
        }

        // Target background workers or runaway candidates with substantial PSS (> 64 MiB)
        const auto tier = static_cast<ProcessSafetyTier>(p.safety_tier);
        if (tier != ProcessSafetyTier::BackgroundWorker &&
            tier != ProcessSafetyTier::RunawayCandidate &&
            tier != ProcessSafetyTier::UserInteractive) {
            continue;
        }

        if (p.pss_kib < (64u * 1024u)) { // 64 MiB minimum
            continue;
        }

        const auto res = pageout_process(p.pid);
        if (res.success && res.bytes_paged_out > 0) {
            total_paged += res.bytes_paged_out;
            ++actuated_count;

            char detail[128];
            std::snprintf(detail, sizeof(detail),
                          "Paged out %llu MiB (%u VMAs) to ZRAM (REF-REQ-130)",
                          static_cast<unsigned long long>(res.bytes_paged_out / (1024ull * 1024ull)),
                          res.vma_count);
            core::EventLogger::log_mitigation(p.pid, p.comm.c_str(), "ProcessPageout", detail);
        }
    }

    return total_paged;
}

} // namespace wattcurb::policy
