#include "hw/hardware_probe.hpp"

#include <charconv>
#include <fcntl.h>
#include <system_error>
#include <unistd.h>
#include <array>
#include <cstring>

namespace wattcurb::hw {

HardwareProbe::HardwareProbe(std::filesystem::path sysfs_root)
    : sysfs_root_(std::move(sysfs_root)) {
    refresh_device_paths();
}

void HardwareProbe::refresh_device_paths() {
    std::error_code ec;

    // 1. Battery Discovery (/sys/class/power_supply/BAT*)
    auto power_supply_dir = sysfs_root_ / "class/power_supply";
    if (std::filesystem::exists(power_supply_dir, ec)) {
        for (const auto& entry : std::filesystem::directory_iterator(power_supply_dir, ec)) {
            const auto filename = entry.path().filename().string();
            if (filename.rfind("BAT", 0) == 0) {
                battery_path_ = entry.path();
                break; // Use primary battery
            }
        }
    }

    // 2. RAPL Discovery (/sys/class/powercap/intel-rapl/intel-rapl:0)
    auto rapl_dir = sysfs_root_ / "class/powercap/intel-rapl";
    if (std::filesystem::exists(rapl_dir, ec)) {
        auto pkg_file = rapl_dir / "intel-rapl:0/energy_uj";
        if (std::filesystem::exists(pkg_file, ec)) {
            rapl_pkg_path_ = pkg_file;
            auto core_file = rapl_dir / "intel-rapl:0/intel-rapl:0:0/energy_uj";
            if (std::filesystem::exists(core_file, ec)) {
                rapl_core_path_ = core_file;
            }
        }
    }

    // 3. GPU Power Discovery (/sys/class/drm/card*/device/hwmon/hwmon*/power1_input)
    auto drm_dir = sysfs_root_ / "class/drm";
    if (std::filesystem::exists(drm_dir, ec)) {
        for (const auto& card_entry : std::filesystem::directory_iterator(drm_dir, ec)) {
            const auto card_name = card_entry.path().filename().string();
            if (card_name.rfind("card", 0) == 0 && card_name.find('-') == std::string::npos) {
                auto hwmon_base = card_entry.path() / "device/hwmon";
                if (std::filesystem::exists(hwmon_base, ec)) {
                    for (const auto& hwmon_entry : std::filesystem::directory_iterator(hwmon_base, ec)) {
                        auto p1_in = hwmon_entry.path() / "power1_input";
                        auto p1_avg = hwmon_entry.path() / "power1_average";
                        if (std::filesystem::exists(p1_in, ec)) {
                            gpu_power_path_ = p1_in;
                            break;
                        } else if (std::filesystem::exists(p1_avg, ec)) {
                            gpu_power_path_ = p1_avg;
                            break;
                        }
                    }
                }
                if (!gpu_power_path_.empty()) break;
            }
        }
    }

    // 4. Backlight Discovery (/sys/class/backlight/*)
    auto backlight_dir = sysfs_root_ / "class/backlight";
    if (std::filesystem::exists(backlight_dir, ec)) {
        for (const auto& bl_entry : std::filesystem::directory_iterator(backlight_dir, ec)) {
            auto brightness_file = bl_entry.path() / "brightness";
            if (std::filesystem::exists(brightness_file, ec)) {
                backlight_path_ = bl_entry.path();
                break;
            }
        }
    }
}

HardwareSample HardwareProbe::capture_sample() const {
    HardwareSample sample;
    sample.timestamp = std::chrono::steady_clock::now();

    // 1. Read Battery
    if (!battery_path_.empty()) {
        auto status_str = read_string_file(battery_path_ / "status");
        sample.is_discharging = (status_str.rfind("Discharging", 0) == 0);

        auto power = read_uint64_file(battery_path_ / "power_now");
        if (power.has_value()) {
            sample.battery_power_uw = power;
        } else {
            // Fallback to V x I
            auto v = read_uint64_file(battery_path_ / "voltage_now");
            auto i = read_int64_file(battery_path_ / "current_now");
            if (v.has_value() && i.has_value()) {
                sample.battery_voltage_uv = v;
                sample.battery_current_ua = i;
                int64_t abs_curr = *i < 0 ? -*i : *i;
                sample.battery_power_uw = static_cast<uint64_t>((*v * static_cast<uint64_t>(abs_curr)) / 1'000'000ULL);
            }
        }
    }

    // 2. Read RAPL (may fail if unprivileged)
    if (!rapl_pkg_path_.empty()) {
        sample.rapl_package_uj = read_uint64_file(rapl_pkg_path_);
    }
    if (!rapl_core_path_.empty()) {
        sample.rapl_core_uj = read_uint64_file(rapl_core_path_);
    }

    // 3. Read GPU
    if (!gpu_power_path_.empty()) {
        sample.gpu_power_uw = read_uint64_file(gpu_power_path_);
    }

    // 4. Read Backlight
    if (!backlight_path_.empty()) {
        auto cur_b = read_uint64_file(backlight_path_ / "brightness");
        auto max_b = read_uint64_file(backlight_path_ / "max_brightness");
        if (cur_b.has_value()) sample.backlight_brightness = static_cast<uint32_t>(*cur_b);
        if (max_b.has_value()) sample.backlight_max_brightness = static_cast<uint32_t>(*max_b);
    }

    return sample;
}

std::optional<uint64_t> HardwareProbe::read_uint64_file(const std::filesystem::path& path) {
    int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return std::nullopt;

    std::array<char, 64> buffer{};
    ssize_t bytes_read = ::read(fd, buffer.data(), buffer.size() - 1);
    ::close(fd);

    if (bytes_read <= 0) return std::nullopt;
    buffer[static_cast<size_t>(bytes_read)] = '\0';

    uint64_t value = 0;
    const char* start = buffer.data();
    while (*start == ' ' || *start == '\t' || *start == '\n') ++start;
    const char* end = buffer.data() + bytes_read;

    auto [ptr, ec] = std::from_chars(start, end, value);
    if (ec == std::errc()) {
        return value;
    }
    return std::nullopt;
}

std::optional<int64_t> HardwareProbe::read_int64_file(const std::filesystem::path& path) {
    int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return std::nullopt;

    std::array<char, 64> buffer{};
    ssize_t bytes_read = ::read(fd, buffer.data(), buffer.size() - 1);
    ::close(fd);

    if (bytes_read <= 0) return std::nullopt;
    buffer[static_cast<size_t>(bytes_read)] = '\0';

    int64_t value = 0;
    const char* start = buffer.data();
    while (*start == ' ' || *start == '\t' || *start == '\n') ++start;
    const char* end = buffer.data() + bytes_read;

    auto [ptr, ec] = std::from_chars(start, end, value);
    if (ec == std::errc()) {
        return value;
    }
    return std::nullopt;
}

std::string HardwareProbe::read_string_file(const std::filesystem::path& path) {
    int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return {};

    std::array<char, 64> buffer{};
    ssize_t bytes_read = ::read(fd, buffer.data(), buffer.size() - 1);
    ::close(fd);

    if (bytes_read <= 0) return {};
    while (bytes_read > 0 && (buffer[static_cast<size_t>(bytes_read - 1)] == '\n' ||
                              buffer[static_cast<size_t>(bytes_read - 1)] == '\r' ||
                              buffer[static_cast<size_t>(bytes_read - 1)] == ' ')) {
        --bytes_read;
    }
    buffer[static_cast<size_t>(bytes_read)] = '\0';
    return std::string(buffer.data(), static_cast<size_t>(bytes_read));
}

} // namespace wattcurb::hw
