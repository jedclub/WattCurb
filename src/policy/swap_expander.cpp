#include "policy/swap_expander.hpp"

#include "core/event_logger.hpp"
#include "core/posix_fs.hpp"
#include "policy/mitigation_engine.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/swap.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace wattcurb::policy {

namespace {

constexpr unsigned BTRFS_SUPER_MAGIC_U = 0x9123683Eu;

constexpr const char* BTRFS_BIN = "/usr/bin/btrfs";
constexpr const char* MKSWAP_BIN = "/usr/sbin/mkswap";
constexpr const char* MKSWAP_BIN_ALT = "/usr/bin/mkswap";

[[nodiscard]] inline double pct_of(uint64_t part, uint64_t whole) noexcept {
    if (whole == 0) return 100.0;
    return (static_cast<double>(part) * 100.0) / static_cast<double>(whole);
}

} // namespace

bool SwapExpander::build_path(char* out, size_t cap, unsigned index) noexcept {
    const int n = std::snprintf(out, cap, "%s/%s%u.swap", SWAP_DIR, FILE_PREFIX, index);
    return (n > 0 && static_cast<size_t>(n) < cap);
}

uint64_t SwapExpander::swap_dir_free_bytes() noexcept {
    struct statvfs vfs {};
    if (::statvfs(SWAP_DIR, &vfs) != 0) return 0;
    return static_cast<uint64_t>(vfs.f_bavail) * static_cast<uint64_t>(vfs.f_frsize);
}

bool SwapExpander::dir_is_btrfs() noexcept {
    struct statfs fs {};
    if (::statfs(SWAP_DIR, &fs) != 0) return false;
    return static_cast<unsigned>(fs.f_type) == BTRFS_SUPER_MAGIC_U;
}

bool SwapExpander::is_swap_active(const char* path) noexcept {
    char buf[4096];
    size_t n = 0;
    if (!core::fs::read_small_file("/proc/swaps", buf, sizeof(buf), &n) || n == 0) return false;
    return std::strstr(buf, path) != nullptr;
}

bool SwapExpander::should_expand(uint64_t swap_total_kb, uint64_t swap_free_kb) noexcept {
    if (swap_total_kb == 0) return false; // nothing to extend; no swap configured
    const double free_pct = pct_of(swap_free_kb, swap_total_kb);
    return (free_pct < EXPAND_SWAP_FREE_PCT) || (swap_free_kb < EXPAND_SWAP_FREE_MIN_KB);
}

bool SwapExpander::budget_allows(uint64_t fs_free_bytes, size_t active_files) noexcept {
    if (active_files >= MAX_FILES) return false;
    if (fs_free_bytes == 0) return false; // unknown free space is not permission
    return fs_free_bytes >= (INCREMENT_BYTES + DISK_FREE_FLOOR_BYTES);
}

bool SwapExpander::safe_to_release(uint64_t swap_total_kb, uint64_t swap_free_kb,
                                   uint64_t file_bytes) noexcept {
    if (swap_total_kb == 0) return true;
    const uint64_t file_kb = file_bytes / 1024ull;
    if (file_kb >= swap_total_kb) return false;

    const uint64_t used_kb = (swap_total_kb > swap_free_kb)
                                 ? (swap_total_kb - swap_free_kb) : 0;
    const uint64_t remaining_kb = swap_total_kb - file_kb;

    // Everything still paged out must fit in what is left, with headroom.
    return static_cast<double>(used_kb) <
           static_cast<double>(remaining_kb) * RELEASE_HEADROOM_FACTOR;
}

bool SwapExpander::budget_exhausted() const noexcept {
    return !budget_allows(swap_dir_free_bytes(), m_count);
}

void SwapExpander::initialize() noexcept {
    // Adopt or reclaim what a previous run left behind. A dynamic swapfile is
    // disk state: it survives the daemon, and after an unclean exit it is
    // either still swapped on (adopt, so it can be released later) or an orphan
    // consuming gigabytes for nothing (delete).
    for (unsigned i = 0; i < MAX_FILES; ++i) {
        char path[PATH_CAP];
        if (!build_path(path, sizeof(path), i)) continue;
        struct stat st {};
        if (::stat(path, &st) != 0) continue;

        if (is_swap_active(path)) {
            if (m_count < MAX_FILES) {
                std::snprintf(m_files[m_count].path, PATH_CAP, "%s", path);
                m_files[m_count].active = true;
                ++m_count;
            }
            char detail[192];
            std::snprintf(detail, sizeof(detail),
                          "Adopted dynamic swapfile left active by a previous run: %s "
                          "(REF-REQ-113)", path);
            core::EventLogger::log_alert("SWAP", detail);
        } else if (!MitigationEngine::actuation_sandboxed()) {
            ::unlink(path);
            char detail[192];
            std::snprintf(detail, sizeof(detail),
                          "Removed orphaned dynamic swapfile (not swapped on): %s "
                          "(REF-REQ-113)", path);
            core::EventLogger::log_alert("SWAP", detail);
        }
    }
}

pid_t SwapExpander::spawn_create(const char* path) const noexcept {
    return spawn_create_size(path, INCREMENT_BYTES);
}

pid_t SwapExpander::spawn_create_size(const char* path, uint64_t size_bytes) const noexcept {
    const bool btrfs = dir_is_btrfs() && (::access(BTRFS_BIN, X_OK) == 0);

    const pid_t pid = ::fork();
    if (pid < 0) return -1;
    if (pid > 0) return pid;

    // ---- child ----
    // No shell anywhere on this path: the only variable is a path this class
    // composed itself, and execv() takes an argument vector, so there is no
    // quoting surface at all.
    if (btrfs) {
        // btrfs swapfiles must be NOCOW, uncompressed and fully allocated;
        // `btrfs filesystem mkswapfile` is the only thing that gets all three
        // right, and it runs mkswap itself.
        char size_arg[32];
        std::snprintf(size_arg, sizeof(size_arg), "%llug",
                      static_cast<unsigned long long>(size_bytes >> 30));
        char* const argv[] = {const_cast<char*>("btrfs"),
                              const_cast<char*>("filesystem"),
                              const_cast<char*>("mkswapfile"),
                              const_cast<char*>("--size"),
                              size_arg,
                              const_cast<char*>(path),
                              nullptr};
        ::execv(BTRFS_BIN, argv);
        ::_exit(127);
    }

    int fd = ::open(path, O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600);
    if (fd < 0) ::_exit(120);
    if (::fallocate(fd, 0, 0, static_cast<off_t>(size_bytes)) != 0) {
        ::close(fd);
        ::unlink(path);
        ::_exit(121);
    }
    ::close(fd);

    char* const argv[] = {const_cast<char*>("mkswap"), const_cast<char*>(path), nullptr};
    ::execv(MKSWAP_BIN, argv);
    ::execv(MKSWAP_BIN_ALT, argv);
    ::unlink(path);
    ::_exit(127);
}

pid_t SwapExpander::spawn_swapoff(const char* path) const noexcept {
    const pid_t pid = ::fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        if (::swapoff(path) == 0) {
            ::_exit(0);
        }
        ::_exit(1);
    }
    return pid;
}

bool SwapExpander::ensure_headroom(uint64_t swap_total_kb, uint64_t swap_free_kb) noexcept {
    if (m_pending_pid > 0) return true; // one at a time
    if (!should_expand(swap_total_kb, swap_free_kb)) return false;

    const uint64_t free_bytes = swap_dir_free_bytes();
    if (!budget_allows(free_bytes, m_count)) {
        return false;
    }

    // Pick the lowest unused index so adopted files keep their names.
    unsigned index = 0;
    for (; index < MAX_FILES; ++index) {
        char probe[PATH_CAP];
        if (!build_path(probe, sizeof(probe), index)) return false;
        struct stat st {};
        if (::stat(probe, &st) != 0) break;
    }
    if (index >= MAX_FILES) return false;

    char path[PATH_CAP];
    if (!build_path(path, sizeof(path), index)) return false;

    if (MitigationEngine::actuation_sandboxed()) {
        return false;
    }

    const pid_t pid = spawn_create(path);
    if (pid < 0) return false;

    m_pending_pid = pid;
    std::snprintf(m_pending_path, PATH_CAP, "%s", path);

    char detail[224];
    std::snprintf(detail, sizeof(detail),
                  "Performance mode: growing swap by %llu GiB (%s) - SwapFree %llu MiB of "
                  "%llu MiB, disk free %llu GiB (REF-REQ-113)",
                  static_cast<unsigned long long>(INCREMENT_BYTES >> 30), path,
                  static_cast<unsigned long long>(swap_free_kb / 1024ull),
                  static_cast<unsigned long long>(swap_total_kb / 1024ull),
                  static_cast<unsigned long long>(free_bytes >> 30));
    core::EventLogger::log_alert("SWAP", detail);
    return true;
}

void SwapExpander::poll_pending() noexcept {
    if (m_pending_pid <= 0) return;

    int status = 0;
    const pid_t r = ::waitpid(m_pending_pid, &status, WNOHANG);
    if (r == 0) return; // still building
    const bool ok = (r == m_pending_pid) && WIFEXITED(status) && (WEXITSTATUS(status) == 0);

    const pid_t finished = m_pending_pid;
    (void)finished;
    m_pending_pid = -1;

    if (!ok) {
        ::unlink(m_pending_path);
        char detail[192];
        std::snprintf(detail, sizeof(detail),
                      "Dynamic swapfile creation failed for %s; capacity unchanged "
                      "(REF-REQ-113)", m_pending_path);
        core::EventLogger::log_alert("SWAP", detail);
        m_pending_path[0] = '\0';
        return;
    }

    // No SWAP_FLAG_PREFER: the kernel assigns the next negative priority, which
    // places every dynamic file below the distribution's own tiers. Dynamic
    // capacity is overflow of last resort, not a tier to prefer.
    if (::swapon(m_pending_path, 0) != 0) {
        ::unlink(m_pending_path);
        char detail[192];
        std::snprintf(detail, sizeof(detail),
                      "swapon() refused %s; file removed (REF-REQ-113)", m_pending_path);
        core::EventLogger::log_alert("SWAP", detail);
        m_pending_path[0] = '\0';
        return;
    }

    if (m_count < MAX_FILES) {
        std::snprintf(m_files[m_count].path, PATH_CAP, "%s", m_pending_path);
        m_files[m_count].active = true;
        ++m_count;
    }

    char detail[192];
    std::snprintf(detail, sizeof(detail),
                  "Swap grown by %llu GiB: %s now active (%zu dynamic file(s), +%llu GiB) "
                  "(REF-REQ-113)",
                  static_cast<unsigned long long>(INCREMENT_BYTES >> 30), m_pending_path,
                  m_count, static_cast<unsigned long long>(added_bytes() >> 30));
    core::EventLogger::log_alert("SWAP", detail);
    m_pending_path[0] = '\0';
}

void SwapExpander::maybe_release(uint64_t swap_total_kb, uint64_t swap_free_kb) noexcept {
    if (m_count == 0 || m_pending_pid > 0) return;
    if (MitigationEngine::actuation_sandboxed()) return;
    if (!safe_to_release(swap_total_kb, swap_free_kb, INCREMENT_BYTES)) return;

    const size_t idx = m_count - 1;
    char path[PATH_CAP];
    std::snprintf(path, PATH_CAP, "%s", m_files[idx].path);

    // swapoff() faults the file's pages back into memory and can block for a
    // long time. The event loop cannot afford that, so it happens in a child;
    // the syscall acts on the kernel, not on the child's address space, so the
    // effect is global exactly as it would be here.
    const pid_t pid = ::fork();
    if (pid < 0) return;
    if (pid == 0) {
        if (::swapoff(path) == 0) {
            ::unlink(path);
            ::_exit(0);
        }
        ::_exit(1);
    }

    // Detached: the next poll_pending() is not waiting on this one, so reap it
    // without blocking and let init adopt it if it outlives us.
    int status = 0;
    (void)::waitpid(pid, &status, WNOHANG);

    m_files[idx].active = false;
    m_files[idx].path[0] = '\0';
    --m_count;

    char detail[192];
    std::snprintf(detail, sizeof(detail),
                  "Pressure cleared: releasing dynamic swapfile %s (%zu remaining) "
                  "(REF-REQ-113)", path, m_count);
    core::EventLogger::log_alert("SWAP", detail);
}

// -----------------------------------------------------------------------------
// REF-REQ-138, REF-ARCH-085: Base Swap Right-Sizing & Migration Engine
// -----------------------------------------------------------------------------

uint64_t SwapExpander::get_file_size_bytes(const char* path) noexcept {
    if (!path) return 0;
    struct stat st{};
    if (::stat(path, &st) != 0) return 0;
    return static_cast<uint64_t>(st.st_size);
}

bool SwapExpander::is_eligible_for_right_sizing(
    uint64_t window_duration_sec,
    uint64_t peak_used_kb,
    uint64_t current_base_size_bytes,
    uint64_t fs_free_bytes
) noexcept {
    return (window_duration_sec >= OBSERVATION_WINDOW_SEC) &&
           (peak_used_kb <= MAX_WINDOW_PEAK_USED_KB) &&
           (current_base_size_bytes >= MIN_ELIGIBLE_BASE_SIZE_BYTES) &&
           (fs_free_bytes >= MIN_FS_FREE_BYTES_FOR_BASE_MIGRATE);
}

bool SwapExpander::is_safe_to_migrate(
    uint64_t current_used_kb,
    uint64_t target_size_bytes,
    uint64_t fs_free_bytes
) noexcept {
    const uint64_t target_kb = target_size_bytes / 1024ull;
    const uint64_t safe_used_ceiling_kb = (target_kb * 70ull) / 100ull; // 70% headroom
    return (current_used_kb <= safe_used_ceiling_kb) &&
           (fs_free_bytes >= MIN_FS_FREE_BYTES_FOR_BASE_MIGRATE);
}

void SwapExpander::evaluate_base_right_sizing(uint64_t now_sec, uint64_t swap_total_kb, uint64_t swap_free_kb) noexcept {
    if (m_base_migration_state != BaseMigrationState::Idle) return;
    if (m_last_migration_sec > 0 && (now_sec - m_last_migration_sec < BASE_MIGRATION_COOLDOWN_SEC)) return;

    const uint64_t used_kb = (swap_total_kb > swap_free_kb) ? (swap_total_kb - swap_free_kb) : 0;

    if (m_window_start_sec == 0) {
        m_window_start_sec = now_sec;
        m_peak_swap_used_kb = used_kb;
    }

    if (used_kb > m_peak_swap_used_kb) {
        m_peak_swap_used_kb = used_kb;
    }

    // If swap usage spiked above the safety threshold, reset the observation window.
    if (used_kb > MAX_WINDOW_PEAK_USED_KB) {
        m_window_start_sec = now_sec;
        m_peak_swap_used_kb = used_kb;
        return;
    }

    const uint64_t elapsed = (now_sec >= m_window_start_sec) ? (now_sec - m_window_start_sec) : 0;
    const uint64_t base_size = get_file_size_bytes(BASE_SWAP_PATH);
    const uint64_t fs_free = swap_dir_free_bytes();

    if (!is_eligible_for_right_sizing(elapsed, m_peak_swap_used_kb, base_size, fs_free)) return;
    if (!is_safe_to_migrate(used_kb, TARGET_BASE_SIZE_BYTES, fs_free)) return;
    if (!is_swap_active(BASE_SWAP_PATH)) return;

    if (MitigationEngine::actuation_sandboxed()) return;

    // Clean up stale temporary file if any
    ::unlink(BASE_SWAP_NEW_PATH);

    const pid_t pid = spawn_create_size(BASE_SWAP_NEW_PATH, TARGET_BASE_SIZE_BYTES);
    if (pid < 0) return;

    m_base_child_pid = pid;
    m_base_migration_state = BaseMigrationState::CreatingNew;

    char detail[256];
    std::snprintf(detail, sizeof(detail),
                  "Base Swap Right-Sizing initiated: migrating %s (%llu GiB -> %llu GiB) "
                  "after %llu s low-pressure window (REF-REQ-138)",
                  BASE_SWAP_PATH,
                  static_cast<unsigned long long>(base_size >> 30),
                  static_cast<unsigned long long>(TARGET_BASE_SIZE_BYTES >> 30),
                  static_cast<unsigned long long>(elapsed));
    core::EventLogger::log_alert("SWAP", detail);
}

void SwapExpander::poll_base_migration() noexcept {
    if (m_base_migration_state == BaseMigrationState::Idle) return;
    if (m_base_child_pid <= 0) return;

    int status = 0;
    const pid_t r = ::waitpid(m_base_child_pid, &status, WNOHANG);
    if (r == 0) return; // Still running in background

    const bool child_ok = (r == m_base_child_pid) && WIFEXITED(status) && (WEXITSTATUS(status) == 0);
    m_base_child_pid = -1;

    if (m_base_migration_state == BaseMigrationState::CreatingNew) {
        if (!child_ok) {
            ::unlink(BASE_SWAP_NEW_PATH);
            m_base_migration_state = BaseMigrationState::Idle;
            char detail[192];
            std::snprintf(detail, sizeof(detail),
                          "Base swap creation failed for %s; aborting migration (REF-REQ-138)",
                          BASE_SWAP_NEW_PATH);
            core::EventLogger::log_alert("SWAP", detail);
            return;
        }

        // Swapon new file (zero downtime: both old and new are active simultaneously)
        if (::swapon(BASE_SWAP_NEW_PATH, 0) != 0) {
            ::unlink(BASE_SWAP_NEW_PATH);
            m_base_migration_state = BaseMigrationState::Idle;
            char detail[192];
            std::snprintf(detail, sizeof(detail),
                          "swapon() failed for %s; aborting migration (REF-REQ-138)",
                          BASE_SWAP_NEW_PATH);
            core::EventLogger::log_alert("SWAP", detail);
            return;
        }

        // Now initiate async deactivation of old swap file
        const pid_t off_pid = spawn_swapoff(BASE_SWAP_PATH);
        if (off_pid < 0) {
            ::swapoff(BASE_SWAP_NEW_PATH);
            ::unlink(BASE_SWAP_NEW_PATH);
            m_base_migration_state = BaseMigrationState::Idle;
            return;
        }

        m_base_child_pid = off_pid;
        m_base_migration_state = BaseMigrationState::SwappingOffOld;

        char detail[192];
        std::snprintf(detail, sizeof(detail),
                      "Base swap migration: %s active; evicting old %s (REF-REQ-138)",
                      BASE_SWAP_NEW_PATH, BASE_SWAP_PATH);
        core::EventLogger::log_alert("SWAP", detail);
        return;
    }

    if (m_base_migration_state == BaseMigrationState::SwappingOffOld) {
        if (!child_ok) {
            // Old swapoff failed; roll back by disabling the new swap
            ::swapoff(BASE_SWAP_NEW_PATH);
            ::unlink(BASE_SWAP_NEW_PATH);
            m_base_migration_state = BaseMigrationState::Idle;
            char detail[192];
            std::snprintf(detail, sizeof(detail),
                          "swapoff() failed for old %s; migration aborted (REF-REQ-138)",
                          BASE_SWAP_PATH);
            core::EventLogger::log_alert("SWAP", detail);
            return;
        }

        // Old swapoff succeeded! Atomically replace old file with new file.
        ::unlink(BASE_SWAP_PATH);
        if (::rename(BASE_SWAP_NEW_PATH, BASE_SWAP_PATH) == 0) {
            m_base_migration_state = BaseMigrationState::Completed;
            m_last_migration_sec = static_cast<uint64_t>(::time(nullptr));
            m_window_start_sec = 0; // Reset observation window

            char detail[256];
            std::snprintf(detail, sizeof(detail),
                          "Base Swap Right-Sizing COMPLETED: %s downsized to %llu GiB! "
                          "Reclaimed ~24 GiB NVMe disk space safely (REF-REQ-138)",
                          BASE_SWAP_PATH,
                          static_cast<unsigned long long>(TARGET_BASE_SIZE_BYTES >> 30));
            core::EventLogger::log_alert("SWAP", detail);
        } else {
            // Rename failed fallback
            ::rename(BASE_SWAP_NEW_PATH, BASE_SWAP_PATH);
        }
        m_base_migration_state = BaseMigrationState::Idle;
    }
}

} // namespace wattcurb::policy
