#include "policy/memory_hygiene_engine.hpp"

#include <sys/statvfs.h>
#include <sys/stat.h>
#include <sys/swap.h>
#include <sys/sysinfo.h>
#include <signal.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <array>

namespace wattcurb::policy {

namespace {

// Safe zero-alloc directory removal helper
bool safe_remove_directory_contents(const char* dir_path, size_t depth) noexcept {
    if (depth > MemoryHygieneEngine::MAX_SCAN_DEPTH) return false;
    DIR* dir = ::opendir(dir_path);
    if (!dir) return false;

    struct dirent* entry = nullptr;
    char subpath[512];

    while ((entry = ::readdir(dir)) != nullptr) {
        if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        int written = std::snprintf(subpath, sizeof(subpath), "%s/%s", dir_path, entry->d_name);
        if (written <= 0 || static_cast<size_t>(written) >= sizeof(subpath)) {
            continue;
        }

        struct stat st{};
        if (::lstat(subpath, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            safe_remove_directory_contents(subpath, depth + 1);
            ::rmdir(subpath);
        } else {
            ::unlink(subpath);
        }
    }
    ::closedir(dir);
    return true;
}

// Bounded directory size estimation (depth <= 2, entries <= 64)
uint64_t estimate_dir_bytes(const char* dir_path, size_t depth) noexcept {
    if (depth > MemoryHygieneEngine::MAX_SCAN_DEPTH) return 0;
    DIR* dir = ::opendir(dir_path);
    if (!dir) return 0;

    uint64_t total = 0;
    size_t inspected = 0;
    struct dirent* entry = nullptr;
    char subpath[512];

    while ((entry = ::readdir(dir)) != nullptr && inspected < MemoryHygieneEngine::MAX_INSPECTED_ENTRIES) {
        if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        ++inspected;

        int written = std::snprintf(subpath, sizeof(subpath), "%s/%s", dir_path, entry->d_name);
        if (written <= 0 || static_cast<size_t>(written) >= sizeof(subpath)) {
            continue;
        }

        struct stat st{};
        if (::lstat(subpath, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            total += static_cast<uint64_t>(st.st_size);
            total += estimate_dir_bytes(subpath, depth + 1);
        } else {
            total += static_cast<uint64_t>(st.st_size);
        }
    }
    ::closedir(dir);
    return total;
}

} // namespace

bool MemoryHygieneEngine::initialize() noexcept {
    m_tmpfs_cooldown = TMPFS_SCAN_COOLDOWN_BASE_SEC;
    m_swap_cooldown = SWAP_EVAL_COOLDOWN_BASE_SEC;
    m_last_tmpfs_scan_sec = 0;
    m_last_swap_eval_sec = 0;
    m_last_reclaim_sec = 0;
    m_total_evicted_bytes = 0;
    m_total_evicted_count = 0;
    return true;
}

bool MemoryHygieneEngine::check_tmpfs_fast_filter(
    const char* mount_point,
    uint64_t& out_total_kb,
    uint64_t& out_used_kb
) noexcept {
    if (!mount_point) return false;
    struct statvfs st{};
    if (::statvfs(mount_point, &st) != 0) return false;

    if (st.f_blocks == 0) return false;
    const uint64_t total_bytes = static_cast<uint64_t>(st.f_blocks) * st.f_frsize;
    const uint64_t avail_bytes = static_cast<uint64_t>(st.f_bavail) * st.f_frsize;
    const uint64_t used_bytes = (total_bytes > avail_bytes) ? (total_bytes - avail_bytes) : 0;

    out_total_kb = total_bytes / 1024;
    out_used_kb = used_bytes / 1024;

    const double used_pct = (static_cast<double>(used_bytes) / static_cast<double>(total_bytes)) * 100.0;
    return (used_pct >= TMPFS_TRIGGER_USED_PCT) || (used_bytes >= TMPFS_TRIGGER_USED_BYTES);
}

bool MemoryHygieneEngine::is_system_whitelisted(std::string_view path) noexcept {
    static constexpr std::string_view WHITELIST[] = {
        "/tmp/.X11-unix",
        "/tmp/.ICE-unix",
        "/tmp/wayland-",
        "/tmp/pulse-",
        "/tmp/pipewire-",
        "/tmp/systemd-private-",
        "/tmp/runtime-",
        "/tmp/.font-unix",
        "/tmp/.Test-unix",
        "/tmp/ssh-",
        "/tmp/dbus-",
    };

    for (const auto& w : WHITELIST) {
        if (path.starts_with(w)) {
            return true;
        }
    }
    return false;
}

bool MemoryHygieneEngine::is_file_type_immune(mode_t mode, std::string_view filename) noexcept {
    if (S_ISSOCK(mode) || S_ISFIFO(mode) || S_ISBLK(mode) || S_ISCHR(mode)) {
        return true;
    }
    if (filename.ends_with(".lock")) {
        return true;
    }
    return false;
}

bool MemoryHygieneEngine::is_process_dead(int32_t pid) noexcept {
    if (pid <= 1) return false;
    if (::kill(pid, 0) == -1 && errno == ESRCH) {
        return true;
    }
    return false;
}

bool MemoryHygieneEngine::assert_swap_margin(uint64_t mem_avail_kb, uint64_t swap_used_kb) noexcept {
    const uint64_t margin_kb = SWAP_HEADROOM_SAFETY_MARGIN_BYTES / 1024;
    const uint64_t required_kb = static_cast<uint64_t>(static_cast<double>(swap_used_kb) * SWAP_HEADROOM_SAFETY_FACTOR) + margin_kb;
    return mem_avail_kb >= required_kb;
}

uint64_t MemoryHygieneEngine::compute_backoff(uint64_t current_cooldown, bool actionable) noexcept {
    if (actionable) {
        return TMPFS_SCAN_COOLDOWN_BASE_SEC;
    }
    return std::min(current_cooldown * 2, MAX_COOLDOWN_BACKOFF_SEC);
}

HygieneStrategy MemoryHygieneEngine::select_strategy(const MemoryHygieneSample& sample) noexcept {
    // Strategy Alpha: Tmpfs Orphan Eviction has highest priority (frees swap slots with 0 RAM cost)
    if (sample.stale_orphan_bytes >= (1ULL * 1024 * 1024 * 1024)) {
        return HygieneStrategy::TmpfsOrphanEvict;
    }

    // Strategy Beta: Background Batch Soft-Clamp under memory pressure
    if (sample.runaway_batch_pid > 0 && sample.mem_available_kb < (2ULL * 1024 * 1024)) {
        return HygieneStrategy::BatchSoftClamp;
    }

    // Strategy Gamma: Safe Idle Deswap only when on AC, Idle, and strict margin satisfied
    if (sample.on_ac_power && sample.user_is_idle && (sample.disk_swap_used_kb * 1024 >= MIN_SWAP_RECLAIM_BYTES)) {
        const uint64_t total_swap_kb = sample.zram_used_kb + sample.disk_swap_used_kb;
        if (assert_swap_margin(sample.mem_available_kb, total_swap_kb)) {
            return HygieneStrategy::SafeIdleDeswap;
        }
        return HygieneStrategy::HoldAndProtect;
    }

    return HygieneStrategy::HoldAndProtect;
}

bool MemoryHygieneEngine::scan_tmpfs_orphans(
    core::FixedVector<OrphanDirCandidate, MAX_CANDIDATES>& out,
    uint64_t now_sec
) noexcept {
    out.clear();
    DIR* dir = ::opendir("/tmp");
    if (!dir) return false;

    struct dirent* entry = nullptr;
    size_t inspected = 0;
    char path_buf[512];

    while ((entry = ::readdir(dir)) != nullptr && inspected < MAX_INSPECTED_ENTRIES && out.size() < MAX_CANDIDATES) {
        if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        ++inspected;

        std::string_view name{entry->d_name};
        int written = std::snprintf(path_buf, sizeof(path_buf), "/tmp/%s", entry->d_name);
        if (written <= 0 || static_cast<size_t>(written) >= sizeof(path_buf)) {
            continue;
        }

        if (is_system_whitelisted(path_buf)) {
            continue;
        }

        struct stat st{};
        if (::lstat(path_buf, &st) != 0) continue;

        if (is_file_type_immune(st.st_mode, name)) {
            continue;
        }

        // Age Gate: must be older than ORPHAN_MIN_AGE_SEC (1 hour)
        if (now_sec > static_cast<uint64_t>(st.st_mtime)) {
            const uint64_t age = now_sec - static_cast<uint64_t>(st.st_mtime);
            if (age < ORPHAN_MIN_AGE_SEC) {
                continue;
            }
        } else {
            continue; // Clock skewed / mtime in future
        }

        // Match transient patterns: claude-*, CMake*, node-compile-cache, *.tmp, lazen-*
        const bool matches_transient =
            name.starts_with("claude-") ||
            name.starts_with("CMake") ||
            name.starts_with("node-compile-cache") ||
            name.starts_with("lazen-") ||
            name.starts_with(".tmp-") ||
            name.ends_with(".tmp") ||
            name.ends_with(".ppm");

        if (!matches_transient) continue;

        // Candidate qualifies
        OrphanDirCandidate candidate{};
        std::strncpy(candidate.path, path_buf, sizeof(candidate.path) - 1);
        candidate.mtime_sec = static_cast<uint64_t>(st.st_mtime);

        if (S_ISDIR(st.st_mode)) {
            candidate.bytes = estimate_dir_bytes(path_buf, 1);
        } else {
            candidate.bytes = static_cast<uint64_t>(st.st_size);
        }

        out.push_back(candidate);
    }

    ::closedir(dir);
    return !out.empty();
}

size_t MemoryHygieneEngine::evict_tmpfs_orphans(
    const core::FixedVector<OrphanDirCandidate, MAX_CANDIDATES>& candidates
) noexcept {
    size_t count = 0;
    for (const auto& c : candidates) {
        // Strict path safety verification
        std::string_view p{c.path};
        if (!p.starts_with("/tmp/") || p.contains("..")) continue;
        if (is_system_whitelisted(p)) continue;

        struct stat st{};
        if (::lstat(c.path, &st) != 0) continue;
        if (is_file_type_immune(st.st_mode, p)) continue;

        if (S_ISDIR(st.st_mode)) {
            safe_remove_directory_contents(c.path, 1);
            if (::rmdir(c.path) == 0) {
                ++count;
                m_total_evicted_bytes += c.bytes;
            }
        } else {
            if (::unlink(c.path) == 0) {
                ++count;
                m_total_evicted_bytes += c.bytes;
            }
        }
    }
    m_total_evicted_count += static_cast<uint32_t>(count);
    return count;
}

bool MemoryHygieneEngine::actuate_batch_soft_clamp(int32_t pid) noexcept {
    if (pid <= 1) return false;
    struct sched_param sp{};
    sp.sched_priority = 0;
    if (::sched_setscheduler(pid, SCHED_IDLE, &sp) != 0) {
        return false;
    }
    return true;
}

bool MemoryHygieneEngine::actuate_safe_deswap(uint64_t mem_avail_kb, uint64_t swap_used_kb) noexcept {
    if (!assert_swap_margin(mem_avail_kb, swap_used_kb)) {
        return false; // Strict Safety Gate Abort
    }

    // Step 1: Phased swapoff targeting secondary disk swapfile
    if (::swapoff("/swap/swapfile") != 0) {
        return false;
    }

    // Step 2: Trigger ZRAM compaction to defragment in-memory pages
    int compact_fd = ::open("/sys/block/zram0/compact", O_WRONLY | O_CLOEXEC);
    if (compact_fd >= 0) {
        (void)::write(compact_fd, "1\n", 2);
        ::close(compact_fd);
    }

    // Step 3: Re-arm disk swap with lowest priority (-1) for overflow safety
    (void)::swapon("/swap/swapfile", -1);
    return true;
}

HygieneStrategy MemoryHygieneEngine::evaluate_and_actuate(
    const AnalysisReportData& report,
    bool on_ac_power,
    bool user_is_idle,
    uint64_t now_sec
) noexcept {
    // 0. Quench Lockout Check: If reclamation recently ran, hold to avoid oscillation
    if (m_last_reclaim_sec > 0 && (now_sec - m_last_reclaim_sec) < POST_RECLAIM_QUENCH_SEC) {
        m_last_strategy = HygieneStrategy::HoldAndProtect;
        return m_last_strategy;
    }

    MemoryHygieneSample sample{};
    sample.on_ac_power = on_ac_power;
    sample.user_is_idle = user_is_idle;
    sample.runaway_batch_pid = 0;

    struct sysinfo si{};
    if (::sysinfo(&si) == 0) {
        sample.mem_available_kb = (static_cast<uint64_t>(si.freeram) * static_cast<uint64_t>(si.mem_unit)) / 1024;
        const uint64_t swap_total_bytes = static_cast<uint64_t>(si.totalswap) * static_cast<uint64_t>(si.mem_unit);
        const uint64_t swap_free_bytes = static_cast<uint64_t>(si.freeswap) * static_cast<uint64_t>(si.mem_unit);
        sample.disk_swap_used_kb = (swap_total_bytes > swap_free_bytes) ? ((swap_total_bytes - swap_free_bytes) / 1024) : 0;
    }

    // 1. Level 1 Fast Filter for Tmpfs
    uint64_t tmpfs_total_kb = 0;
    uint64_t tmpfs_used_kb = 0;
    const bool tmpfs_inflated = check_tmpfs_fast_filter("/tmp", tmpfs_total_kb, tmpfs_used_kb);
    sample.tmpfs_total_kb = tmpfs_total_kb;
    sample.tmpfs_used_kb = tmpfs_used_kb;

    // 2. Level 2 Conditional Deep Scan with Cooldown
    core::FixedVector<OrphanDirCandidate, MAX_CANDIDATES> candidates{};
    if (tmpfs_inflated && (now_sec - m_last_tmpfs_scan_sec >= m_tmpfs_cooldown)) {
        m_last_tmpfs_scan_sec = now_sec;
        scan_tmpfs_orphans(candidates, now_sec);

        for (const auto& c : candidates) {
            sample.stale_orphan_bytes += c.bytes;
        }
        sample.stale_orphan_dirs_count = static_cast<uint32_t>(candidates.size());

        // Exponential backoff if no actionable candidates
        m_tmpfs_cooldown = compute_backoff(m_tmpfs_cooldown, sample.stale_orphan_bytes > 0);
    }

    // 3. Batch Worker Identification from report top_processes
    for (const auto& p : report.top_processes) {
        if (p.pid > 1 && (p.total_attributed_watts > 3.0 || p.wdi_score > 50.0)) {
            sample.runaway_batch_pid = p.pid;
            break;
        }
    }

    // 4. Strategy Selection
    m_last_strategy = select_strategy(sample);
    m_last_sample = sample;

    // 5. Safe Reclamation Actuation
    switch (m_last_strategy) {
    case HygieneStrategy::TmpfsOrphanEvict:
        if (!candidates.empty()) {
            evict_tmpfs_orphans(candidates);
            m_last_reclaim_sec = now_sec;
            m_tmpfs_cooldown = TMPFS_SCAN_COOLDOWN_BASE_SEC; // reset backoff
        }
        break;
    case HygieneStrategy::BatchSoftClamp:
        if (sample.runaway_batch_pid > 0) {
            actuate_batch_soft_clamp(sample.runaway_batch_pid);
        }
        break;
    case HygieneStrategy::SafeIdleDeswap:
        if (now_sec - m_last_swap_eval_sec >= m_swap_cooldown) {
            m_last_swap_eval_sec = now_sec;
            if (actuate_safe_deswap(sample.mem_available_kb, sample.disk_swap_used_kb)) {
                m_last_reclaim_sec = now_sec;
            } else {
                m_swap_cooldown = compute_backoff(m_swap_cooldown, false);
            }
        }
        break;
    case HygieneStrategy::HoldAndProtect:
    default:
        break;
    }

    return m_last_strategy;
}

} // namespace wattcurb::policy
