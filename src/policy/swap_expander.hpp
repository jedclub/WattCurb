#pragma once

#include <cstdint>
#include <sys/types.h>

namespace wattcurb::policy {

// Implements REF-REQ-113, REF-ARCH-073: Dynamic Swap Expansion (Performance mode)
//
// Performance mode's contract is that nothing is held back, so the CPU throttle
// of REF-REQ-112 is the wrong answer there: it slows the exact workload the
// profile exists to run fast. The right answer in that profile is to enlarge
// the backing store and let the machine page, rather than to slow the
// allocator down.
//
// The daemon therefore adds swap capacity on demand while in Performance, in
// bounded increments, and gives it back once the pressure has passed. The CPU
// throttle remains as the last resort for the case where capacity cannot be
// added at all - the alternative there is a kernel SIGKILL, which REF-REQ-112
// exists to avoid.
class SwapExpander {
public:
    // Files are created beside the distribution's own swapfile. Using the same
    // filesystem keeps the free-space accounting honest: the floor below is
    // measured on the filesystem the files consume.
    static constexpr const char* SWAP_DIR = "/swap";
    static constexpr const char* FILE_PREFIX = "wattcurb-dyn-";

    static constexpr uint64_t INCREMENT_BYTES = 8ull << 30;          // 8 GiB per step
    static constexpr size_t   MAX_FILES = 4;                         // ceiling: +32 GiB
    static constexpr uint64_t DISK_FREE_FLOOR_BYTES = 20ull << 30;   // never dip below 20 GiB free

    // Grow while free swap is below EITHER bound. The percentage keeps a large
    // relative buffer on a big swap tier; the absolute minimum matters on a
    // small one, where 25% can still be a few hundred megabytes.
    static constexpr double   EXPAND_SWAP_FREE_PCT = 25.0;
    static constexpr uint64_t EXPAND_SWAP_FREE_MIN_KB = 6ull * 1024 * 1024; // 6 GiB

    // Give capacity back only when what is still swapped out would fit in the
    // remaining tiers with room to spare. swapoff() faults every page of the
    // file back in; doing that while memory is tight would itself cause the
    // exhaustion this class exists to prevent.
    static constexpr double   RELEASE_HEADROOM_FACTOR = 0.70;

    SwapExpander() = default;
    ~SwapExpander() = default;

    SwapExpander(const SwapExpander&) = delete;
    SwapExpander& operator=(const SwapExpander&) = delete;

    // Adopts files a previous run left behind: still-active ones are tracked so
    // they can be released later, inactive ones are deleted to reclaim the disk.
    // Same reasoning as REF-REQ-110 - this is state that outlives the daemon.
    void initialize() noexcept;

    // Non-blocking. Starts at most one expansion and returns true while an
    // expansion is in flight or was just started. Creating an 8 GiB swapfile
    // takes seconds, which the single-threaded event loop cannot spend, so the
    // work happens in a forked child and completes on a later tick.
    bool ensure_headroom(uint64_t swap_total_kb, uint64_t swap_free_kb) noexcept;

    // Reaps a finished child and runs swapon() on success. Called every tick.
    void poll_pending() noexcept;

    // Releases one file per call when it is safe to do so.
    void maybe_release(uint64_t swap_total_kb, uint64_t swap_free_kb) noexcept;

    [[nodiscard]] size_t active_count() const noexcept { return m_count; }
    [[nodiscard]] uint64_t added_bytes() const noexcept { return m_count * INCREMENT_BYTES; }
    [[nodiscard]] bool expansion_in_flight() const noexcept { return m_pending_pid > 0; }

    // True when no further capacity can be added - the file ceiling is reached
    // or the filesystem floor would be crossed. The guard falls back to the CPU
    // throttle in that case rather than letting the kernel kill something.
    [[nodiscard]] bool budget_exhausted() const noexcept;

    // ---- Pure decision functions (Oracle Gate) --------------------------
    [[nodiscard]] static bool should_expand(uint64_t swap_total_kb, uint64_t swap_free_kb) noexcept;
    [[nodiscard]] static bool budget_allows(uint64_t fs_free_bytes, size_t active_files) noexcept;
    [[nodiscard]] static bool safe_to_release(uint64_t swap_total_kb, uint64_t swap_free_kb,
                                              uint64_t file_bytes) noexcept;

    // Filesystem free bytes at SWAP_DIR, or 0 when it cannot be determined.
    [[nodiscard]] static uint64_t swap_dir_free_bytes() noexcept;

    // ---- REF-REQ-138, REF-ARCH-085: Base Swap Right-Sizing Constants ----
    static constexpr const char* BASE_SWAP_PATH = "/swap/swapfile";
    static constexpr const char* BASE_SWAP_NEW_PATH = "/swap/swapfile.new";
    static constexpr uint64_t TARGET_BASE_SIZE_BYTES = 8ull << 30;              // 8 GiB
    static constexpr uint64_t MIN_ELIGIBLE_BASE_SIZE_BYTES = 16ull << 30;       // >= 16 GiB candidate
    static constexpr uint64_t MAX_WINDOW_PEAK_USED_KB = (TARGET_BASE_SIZE_BYTES / 1024ull * 70ull) / 100ull; // 70% of 8 GiB (~5.6 GiB)
    static constexpr uint64_t OBSERVATION_WINDOW_SEC = 7200ull;                 // 2 hours
    static constexpr uint64_t MIN_FS_FREE_BYTES_FOR_BASE_MIGRATE = 16ull << 30; // 16 GiB
    static constexpr uint64_t BASE_MIGRATION_COOLDOWN_SEC = 86400ull;           // 24 hours

    enum class BaseMigrationState : uint8_t {
        Idle = 0,
        CreatingNew,
        SwappingOffOld,
        Completed
    };

    // REF-REQ-138: Track swap usage and evaluate base swap right-sizing.
    void evaluate_base_right_sizing(uint64_t now_sec, uint64_t swap_total_kb, uint64_t swap_free_kb) noexcept;

    // REF-REQ-138: Reaps child processes for base swap migration steps.
    void poll_base_migration() noexcept;

    [[nodiscard]] BaseMigrationState base_migration_state() const noexcept { return m_base_migration_state; }

    // Pure decision functions for Oracle Gate (REF-REQ-138)
    [[nodiscard]] static bool is_eligible_for_right_sizing(
        uint64_t window_duration_sec,
        uint64_t peak_used_kb,
        uint64_t current_base_size_bytes,
        uint64_t fs_free_bytes
    ) noexcept;

    [[nodiscard]] static bool is_safe_to_migrate(
        uint64_t current_used_kb,
        uint64_t target_size_bytes,
        uint64_t fs_free_bytes
    ) noexcept;

    [[nodiscard]] static uint64_t get_file_size_bytes(const char* path) noexcept;

private:
    static constexpr size_t PATH_CAP = 96;
    struct DynamicSwapFile {
        char path[PATH_CAP]{};
        bool active{false};
    };

    [[nodiscard]] static bool build_path(char* out, size_t cap, unsigned index) noexcept;
    [[nodiscard]] static bool is_swap_active(const char* path) noexcept;
    [[nodiscard]] static bool dir_is_btrfs() noexcept;
    [[nodiscard]] pid_t spawn_create(const char* path) const noexcept;
    [[nodiscard]] pid_t spawn_create_size(const char* path, uint64_t size_bytes) const noexcept;
    [[nodiscard]] pid_t spawn_swapoff(const char* path) const noexcept;

    DynamicSwapFile m_files[MAX_FILES]{};
    size_t m_count{0};

    pid_t m_pending_pid{-1};
    char m_pending_path[PATH_CAP]{};

    // REF-REQ-138 Base Swap Right-Sizing State
    BaseMigrationState m_base_migration_state{BaseMigrationState::Idle};
    pid_t m_base_child_pid{-1};
    uint64_t m_window_start_sec{0};
    uint64_t m_peak_swap_used_kb{0};
    uint64_t m_last_migration_sec{0};
};

} // namespace wattcurb::policy
