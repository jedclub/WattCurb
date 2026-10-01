#include "policy/disk_pressure_guard.hpp"
#include "core/event_logger.hpp"
#include "core/scoped_profiler.hpp"

#include <sys/statvfs.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <cstring>
#include <chrono>
#include <string>
#include <vector>
#include <algorithm>

namespace wattcurb::policy {

DiskPressureSample DiskPressureGuard::sample_storage(const char* mount_path) noexcept {
    WATTCURB_PROFILE_SCOPE("DiskPressure::SampleStorage");
    DiskPressureSample sample{};
    if (mount_path == nullptr) return sample;

    struct statvfs st{};
    if (::statvfs(mount_path, &st) != 0) {
        return sample;
    }

    sample.total_bytes = static_cast<uint64_t>(st.f_blocks) * static_cast<uint64_t>(st.f_frsize);
    sample.avail_bytes = static_cast<uint64_t>(st.f_bavail) * static_cast<uint64_t>(st.f_frsize);
    sample.used_bytes = (sample.total_bytes > sample.avail_bytes) ? (sample.total_bytes - sample.avail_bytes) : 0;

    if (sample.total_bytes > 0) {
        sample.used_pct = (static_cast<double>(sample.used_bytes) / static_cast<double>(sample.total_bytes)) * 100.0;
    }
    sample.is_pressure_satisfied = (sample.used_pct >= DISK_PRESSURE_THRESHOLD_PCT);

    return sample;
}

bool DiskPressureGuard::is_cooldown_expired(uint64_t now_sec) const noexcept {
    if (m_last_reclaim_time == 0) return true;
    return (now_sec >= m_last_reclaim_time + COOLDOWN_SEC);
}

void DiskPressureGuard::reset_metrics() noexcept {
    m_last_sample_time = 0;
    m_last_reclaim_time = 0;
    m_total_reclaimed_bytes = 0;
    m_reclaim_events = 0;
}

bool DiskPressureGuard::evaluate_and_actuate() noexcept {
    WATTCURB_PROFILE_SCOPE("DiskPressure::Evaluate");
    const auto now_tp = std::chrono::steady_clock::now().time_since_epoch();
    const uint64_t now_sec = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(now_tp).count());

    // 1. Relaxed cadence: evaluate at most once every 30 seconds
    if (now_sec < m_last_sample_time + 30ULL) {
        return false;
    }
    m_last_sample_time = now_sec;

    // 2. Check root storage pressure
    auto sample = sample_storage("/");
    if (!sample.is_pressure_satisfied) {
        return false;
    }

    // 3. Cooldown guard: prevent continuous I/O thrashing
    if (!is_cooldown_expired(now_sec)) {
        return false;
    }

    // 4. Actuate progressive tiered reclaim
    uint64_t reclaimed = reclaim_tiered_storage(MAX_BATCH_RECLAIM_BYTES);
    m_last_reclaim_time = now_sec;
    m_reclaim_events++;

    if (reclaimed > 0) {
        double reclaimed_mb = static_cast<double>(reclaimed) / (1024.0 * 1024.0);
        std::string msg = "Disk pressure >= 90.0%: progressively reclaimed " + std::to_string(reclaimed_mb) + " MB";
        core::EventLogger::log_alert("STORAGE", msg.c_str());
        return true;
    }

    return false;
}

uint64_t DiskPressureGuard::sweep_journal_and_dumps(uint64_t budget_bytes) noexcept {
    WATTCURB_PROFILE_SCOPE("DiskPressure::Tier1_JournalDumps");
    if (budget_bytes == 0) return 0;
    uint64_t reclaimed = 0;

    // Prune archived rotated journal files (*.journal~) in /var/log/journal
    auto sweep_dir_journals = [&](const char* base_path) {
        DIR* dir = ::opendir(base_path);
        if (!dir) return;

        struct dirent* entry = nullptr;
        while ((entry = ::readdir(dir)) != nullptr && reclaimed < budget_bytes) {
            if (entry->d_name[0] == '.') continue;

            std::string full_path = std::string(base_path) + "/" + entry->d_name;
            struct stat st{};
            if (::lstat(full_path.c_str(), &st) != 0) continue;

            if (S_ISDIR(st.st_mode)) {
                // Subdirectory (e.g. machine-id)
                DIR* subdir = ::opendir(full_path.c_str());
                if (subdir) {
                    struct dirent* sub_entry = nullptr;
                    while ((sub_entry = ::readdir(subdir)) != nullptr && reclaimed < budget_bytes) {
                        if (sub_entry->d_name[0] == '.') continue;
                        std::string sub_path = full_path + "/" + sub_entry->d_name;
                        // Target rotated archived journal files
                        if (sub_path.ends_with(".journal~")) {
                            struct stat sub_st{};
                            if (::lstat(sub_path.c_str(), &sub_st) == 0 && S_ISREG(sub_st.st_mode)) {
                                if (::unlink(sub_path.c_str()) == 0) {
                                    reclaimed += static_cast<uint64_t>(sub_st.st_size);
                                }
                            }
                        }
                    }
                    ::closedir(subdir);
                }
            } else if (S_ISREG(st.st_mode) && full_path.ends_with(".journal~")) {
                if (::unlink(full_path.c_str()) == 0) {
                    reclaimed += static_cast<uint64_t>(st.st_size);
                }
            }
        }
        ::closedir(dir);
    };

    sweep_dir_journals("/var/log/journal");

    // Prune old coredumps older than 3 days
    if (reclaimed < budget_bytes) {
        DIR* coredir = ::opendir("/var/lib/systemd/coredump");
        if (coredir) {
            const auto now_sys = std::chrono::system_clock::now();
            const auto now_sys_sec = std::chrono::duration_cast<std::chrono::seconds>(now_sys.time_since_epoch()).count();
            struct dirent* e = nullptr;
            while ((e = ::readdir(coredir)) != nullptr && reclaimed < budget_bytes) {
                if (e->d_name[0] == '.') continue;
                std::string p = std::string("/var/lib/systemd/coredump/") + e->d_name;
                struct stat st{};
                if (::lstat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
                    if (now_sys_sec > st.st_mtime && (now_sys_sec - st.st_mtime) >= (3 * 86400)) {
                        if (::unlink(p.c_str()) == 0) {
                            reclaimed += static_cast<uint64_t>(st.st_size);
                        }
                    }
                }
            }
            ::closedir(coredir);
        }
    }

    return reclaimed;
}

uint64_t DiskPressureGuard::sweep_package_caches(uint64_t budget_bytes) noexcept {
    WATTCURB_PROFILE_SCOPE("DiskPressure::Tier2_PackageCaches");
    if (budget_bytes == 0) return 0;
    uint64_t reclaimed = 0;

    auto sweep_cache_dir = [&](const char* dir_path, const char* ext) {
        DIR* dir = ::opendir(dir_path);
        if (!dir) return;

        struct dirent* e = nullptr;
        while ((e = ::readdir(dir)) != nullptr && reclaimed < budget_bytes) {
            if (e->d_name[0] == '.') continue;
            // Never touch lock or partial directories
            if (std::strcmp(e->d_name, "lock") == 0 || std::strcmp(e->d_name, "partial") == 0) continue;

            std::string p = std::string(dir_path) + "/" + e->d_name;
            if (p.ends_with(ext)) {
                struct stat st{};
                if (::lstat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
                    if (::unlink(p.c_str()) == 0) {
                        reclaimed += static_cast<uint64_t>(st.st_size);
                    }
                }
            }
        }
        ::closedir(dir);
    };

    // APT archives (.deb)
    sweep_cache_dir("/var/cache/apt/archives", ".deb");

    // Pacman cache (.pkg.tar.zst)
    if (reclaimed < budget_bytes) {
        sweep_cache_dir("/var/cache/pacman/pkg", ".pkg.tar.zst");
    }

    return reclaimed;
}

uint64_t DiskPressureGuard::sweep_old_tmp_files(const char* dir_path, uint64_t budget_bytes, uint64_t now_sec) noexcept {
    WATTCURB_PROFILE_SCOPE("DiskPressure::Tier3_OldTmpFiles");
    if (budget_bytes == 0 || dir_path == nullptr) return 0;
    uint64_t reclaimed = 0;

    DIR* dir = ::opendir(dir_path);
    if (!dir) return 0;

    struct dirent* entry = nullptr;
    while ((entry = ::readdir(dir)) != nullptr && reclaimed < budget_bytes) {
        if (entry->d_name[0] == '.') continue;

        // Strict Exclusion Invariants: Protect locks, sockets, and pids
        std::string_view name{entry->d_name};
        if (name.ends_with(".lock") || name.ends_with(".pid") || name.ends_with(".sock") ||
            name.starts_with(".X11-unix") || name.starts_with(".ICE-unix") || name.starts_with("systemd")) {
            continue;
        }

        std::string full_path = std::string(dir_path) + "/" + entry->d_name;
        struct stat st{};
        if (::lstat(full_path.c_str(), &st) != 0) continue;

        // Only sweep regular files; NEVER sweep sockets or fifos
        if (!S_ISREG(st.st_mode)) continue;

        // Must be older than 7 days
        if (now_sec > static_cast<uint64_t>(st.st_mtime) &&
            (now_sec - static_cast<uint64_t>(st.st_mtime)) >= TMP_FILE_MAX_AGE_SEC) {
            if (::unlink(full_path.c_str()) == 0) {
                reclaimed += static_cast<uint64_t>(st.st_size);
            }
        }
    }
    ::closedir(dir);

    return reclaimed;
}

uint64_t DiskPressureGuard::reclaim_tiered_storage(uint64_t max_bytes) noexcept {
    WATTCURB_PROFILE_SCOPE("DiskPressure::ReclaimTiered");
    uint64_t total_reclaimed = 0;

    const auto now_sys = std::chrono::system_clock::now();
    const uint64_t now_sec = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(now_sys.time_since_epoch()).count()
    );

    // Tier 1: System logs and coredump sweep
    if (total_reclaimed < max_bytes) {
        total_reclaimed += sweep_journal_and_dumps(max_bytes - total_reclaimed);
    }

    // Early exit check: did disk pressure clear?
    auto check1 = sample_storage("/");
    if (check1.used_pct < DISK_RELEASE_THRESHOLD_PCT) {
        m_total_reclaimed_bytes += total_reclaimed;
        return total_reclaimed;
    }

    // Tier 2: Package manager archive cache sweep
    if (total_reclaimed < max_bytes) {
        total_reclaimed += sweep_package_caches(max_bytes - total_reclaimed);
    }

    auto check2 = sample_storage("/");
    if (check2.used_pct < DISK_RELEASE_THRESHOLD_PCT) {
        m_total_reclaimed_bytes += total_reclaimed;
        return total_reclaimed;
    }

    // Tier 3: Old orphan tmp files (> 7 days) in /tmp and /var/tmp
    if (total_reclaimed < max_bytes) {
        total_reclaimed += sweep_old_tmp_files("/tmp", max_bytes - total_reclaimed, now_sec);
    }
    if (total_reclaimed < max_bytes) {
        total_reclaimed += sweep_old_tmp_files("/var/tmp", max_bytes - total_reclaimed, now_sec);
    }

    m_total_reclaimed_bytes += total_reclaimed;
    return total_reclaimed;
}

} // namespace wattcurb::policy
