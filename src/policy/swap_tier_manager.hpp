#pragma once

#include "core/custom_containers.hpp"
#include <cstdint>
#include <cstddef>

namespace wattcurb::policy {

// Implements REF-REQ-130, REF-ARCH-077 (Phase 3):
// SwapTierManager: ZRAM vs Disk Swap Prioritization Engine
//
// Continuously inspects /proc/swaps to verify tiered swap hierarchy:
//   - Tier 1: In-Memory /dev/zram0 (High Priority, e.g. 100, zstd compressed)
//   - Tier 2: Storage Backed /swap/swapfile or dynamic files (Lower Priority, e.g. -1, -2)
//
// Enforces that disk swap is treated as the LAST RESORT:
//   - Disk swap is only engaged when ZRAM capacity exceeds 85%.
//   - Before 85%, all memory reclamation is strictly in-RAM (Zero Disk I/O).
struct SwapDeviceEntry {
    char name[64]{};
    uint64_t size_kb{0};
    uint64_t used_kb{0};
    int32_t priority{0};
    bool is_zram{false};
};

class SwapTierManager {
public:
    static constexpr size_t MAX_SWAP_DEVICES = 8;
    static constexpr double ZRAM_SATURATION_THRESHOLD_PCT = 85.0;

    SwapTierManager() noexcept = default;
    ~SwapTierManager() noexcept = default;

    // Parses /proc/swaps into fixed internal structure without heap allocations
    bool refresh() noexcept;

    // Pure parser for unit testing and validation against raw /proc/swaps text
    static bool parse_swaps_buffer(
        const char* buf,
        size_t len,
        core::FixedVector<SwapDeviceEntry, MAX_SWAP_DEVICES>& out
    ) noexcept;

    [[nodiscard]] bool has_zram() const noexcept { return m_zram_total_kb > 0; }
    [[nodiscard]] uint64_t zram_total_kb() const noexcept { return m_zram_total_kb; }
    [[nodiscard]] uint64_t zram_used_kb() const noexcept { return m_zram_used_kb; }
    [[nodiscard]] double zram_usage_pct() const noexcept;
    [[nodiscard]] bool is_zram_saturated() const noexcept {
        return zram_usage_pct() >= ZRAM_SATURATION_THRESHOLD_PCT;
    }

    [[nodiscard]] uint64_t disk_swap_total_kb() const noexcept { return m_disk_total_kb; }
    [[nodiscard]] uint64_t disk_swap_used_kb() const noexcept { return m_disk_used_kb; }
    [[nodiscard]] double disk_swap_usage_pct() const noexcept;

    // Verifies whether ZRAM has higher kernel swap priority than disk devices
    [[nodiscard]] bool is_zram_prioritized() const noexcept;

    // Reads sysfs ZRAM compression ratio (orig_data_size / compr_data_size)
    [[nodiscard]] static double read_zram_compression_ratio(const char* zram_name = "zram0") noexcept;

    [[nodiscard]] const core::FixedVector<SwapDeviceEntry, MAX_SWAP_DEVICES>& devices() const noexcept {
        return m_devices;
    }

private:
    core::FixedVector<SwapDeviceEntry, MAX_SWAP_DEVICES> m_devices{};
    uint64_t m_zram_total_kb{0};
    uint64_t m_zram_used_kb{0};
    uint64_t m_disk_total_kb{0};
    uint64_t m_disk_used_kb{0};
    int32_t m_zram_priority{-1000};
    int32_t m_max_disk_priority{-1000};
};

} // namespace wattcurb::policy
