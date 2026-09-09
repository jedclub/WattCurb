#pragma once

#include "core/types.hpp"
#include <filesystem>
#include <string>

namespace wattcurb::hw {

// Implements REF-REQ-001, REF-REQ-007, REF-ARCH-004
class HardwareProbe {
public:
    explicit HardwareProbe(std::filesystem::path sysfs_root = "/sys");
    ~HardwareProbe();

    HardwareProbe(const HardwareProbe&) = delete;
    HardwareProbe& operator=(const HardwareProbe&) = delete;
    HardwareProbe(HardwareProbe&& other) noexcept;
    HardwareProbe& operator=(HardwareProbe&& other) noexcept;

    void refresh_device_paths();
    [[nodiscard]] HardwareSample capture_sample() const;

    [[nodiscard]] bool has_battery() const noexcept { return battery_power_fd_ >= 0 || !battery_path_.empty(); }
    [[nodiscard]] bool has_rapl() const noexcept { return rapl_pkg_fd_ >= 0 || !rapl_pkg_path_.empty(); }
    [[nodiscard]] bool has_gpu() const noexcept { return gpu_power_fd_ >= 0 || !gpu_power_path_.empty(); }
    [[nodiscard]] bool has_backlight() const noexcept { return backlight_cur_fd_ >= 0 || !backlight_path_.empty(); }

    // Direct pread helper exposed for testing (REF-TEST-004)
    static std::optional<uint64_t> read_uint64_fd(int fd);
    static std::optional<int64_t> read_int64_fd(int fd);
    static std::string read_string_fd(int fd);

private:
    std::filesystem::path sysfs_root_;
    std::filesystem::path battery_path_;
    std::filesystem::path rapl_pkg_path_;
    std::filesystem::path rapl_core_path_;
    std::filesystem::path gpu_power_path_;
    std::filesystem::path backlight_path_;

    // Persistent file descriptors for direct kernel access without pathname traversal
    int battery_power_fd_{-1};
    int battery_status_fd_{-1};
    int battery_voltage_fd_{-1};
    int battery_current_fd_{-1};
    int rapl_pkg_fd_{-1};
    int rapl_core_fd_{-1};
    int gpu_power_fd_{-1};
    int backlight_cur_fd_{-1};
    int backlight_max_fd_{-1};

    void open_persistent_fds();
    void close_fds() noexcept;
};

} // namespace wattcurb::hw
