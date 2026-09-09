#pragma once

#include "core/types.hpp"
#include <filesystem>
#include <string>

namespace wattcurb::hw {

// Implements REF-REQ-001, REF-ARCH-002
class HardwareProbe {
public:
    explicit HardwareProbe(std::filesystem::path sysfs_root = "/sys");

    void refresh_device_paths();
    [[nodiscard]] HardwareSample capture_sample() const;

    [[nodiscard]] bool has_battery() const noexcept { return !battery_path_.empty(); }
    [[nodiscard]] bool has_rapl() const noexcept { return !rapl_pkg_path_.empty(); }
    [[nodiscard]] bool has_gpu() const noexcept { return !gpu_power_path_.empty(); }
    [[nodiscard]] bool has_backlight() const noexcept { return !backlight_path_.empty(); }

private:
    std::filesystem::path sysfs_root_;
    std::filesystem::path battery_path_;
    std::filesystem::path rapl_pkg_path_;
    std::filesystem::path rapl_core_path_;
    std::filesystem::path gpu_power_path_;
    std::filesystem::path backlight_path_;

    static std::optional<uint64_t> read_uint64_file(const std::filesystem::path& path);
    static std::optional<int64_t> read_int64_file(const std::filesystem::path& path);
    static std::string read_string_file(const std::filesystem::path& path);
};

} // namespace wattcurb::hw
