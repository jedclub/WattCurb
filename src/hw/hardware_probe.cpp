#include "hw/hardware_probe.hpp"

#include <charconv>
#include <fcntl.h>
#include <system_error>
#include <unistd.h>
#include <array>
#include <utility>

namespace wattcurb::hw {

namespace {

int open_ro_cloexec(const std::filesystem::path& path) {
    if (path.empty()) return -1;
    return ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
}

void safe_close(int& fd) noexcept {
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}

} // namespace

HardwareProbe::HardwareProbe(std::filesystem::path sysfs_root)
    : sysfs_root_(std::move(sysfs_root)) {
    refresh_device_paths();
}

HardwareProbe::~HardwareProbe() {
    close_fds();
}

HardwareProbe::HardwareProbe(HardwareProbe&& other) noexcept
    : sysfs_root_(std::move(other.sysfs_root_)),
      battery_path_(std::move(other.battery_path_)),
      rapl_pkg_path_(std::move(other.rapl_pkg_path_)),
      rapl_core_path_(std::move(other.rapl_core_path_)),
      gpu_power_path_(std::move(other.gpu_power_path_)),
      backlight_path_(std::move(other.backlight_path_)),
      battery_power_fd_(std::exchange(other.battery_power_fd_, -1)),
      battery_status_fd_(std::exchange(other.battery_status_fd_, -1)),
      battery_voltage_fd_(std::exchange(other.battery_voltage_fd_, -1)),
      battery_current_fd_(std::exchange(other.battery_current_fd_, -1)),
      rapl_pkg_fd_(std::exchange(other.rapl_pkg_fd_, -1)),
      rapl_core_fd_(std::exchange(other.rapl_core_fd_, -1)),
      gpu_power_fd_(std::exchange(other.gpu_power_fd_, -1)),
      backlight_cur_fd_(std::exchange(other.backlight_cur_fd_, -1)),
      backlight_max_fd_(std::exchange(other.backlight_max_fd_, -1)) {}

HardwareProbe& HardwareProbe::operator=(HardwareProbe&& other) noexcept {
    if (this != &other) {
        close_fds();
        sysfs_root_ = std::move(other.sysfs_root_);
        battery_path_ = std::move(other.battery_path_);
        rapl_pkg_path_ = std::move(other.rapl_pkg_path_);
        rapl_core_path_ = std::move(other.rapl_core_path_);
        gpu_power_path_ = std::move(other.gpu_power_path_);
        backlight_path_ = std::move(other.backlight_path_);

        battery_power_fd_ = std::exchange(other.battery_power_fd_, -1);
        battery_status_fd_ = std::exchange(other.battery_status_fd_, -1);
        battery_voltage_fd_ = std::exchange(other.battery_voltage_fd_, -1);
        battery_current_fd_ = std::exchange(other.battery_current_fd_, -1);
        rapl_pkg_fd_ = std::exchange(other.rapl_pkg_fd_, -1);
        rapl_core_fd_ = std::exchange(other.rapl_core_fd_, -1);
        gpu_power_fd_ = std::exchange(other.gpu_power_fd_, -1);
        backlight_cur_fd_ = std::exchange(other.backlight_cur_fd_, -1);
        backlight_max_fd_ = std::exchange(other.backlight_max_fd_, -1);
    }
    return *this;
}

void HardwareProbe::close_fds() noexcept {
    safe_close(battery_power_fd_);
    safe_close(battery_status_fd_);
    safe_close(battery_voltage_fd_);
    safe_close(battery_current_fd_);
    safe_close(rapl_pkg_fd_);
    safe_close(rapl_core_fd_);
    safe_close(gpu_power_fd_);
    safe_close(backlight_cur_fd_);
    safe_close(backlight_max_fd_);
}

void HardwareProbe::open_persistent_fds() {
    close_fds();

    if (!battery_path_.empty()) {
        battery_power_fd_ = open_ro_cloexec(battery_path_ / "power_now");
        battery_status_fd_ = open_ro_cloexec(battery_path_ / "status");
        battery_voltage_fd_ = open_ro_cloexec(battery_path_ / "voltage_now");
        battery_current_fd_ = open_ro_cloexec(battery_path_ / "current_now");
    }
    if (!rapl_pkg_path_.empty()) {
        rapl_pkg_fd_ = open_ro_cloexec(rapl_pkg_path_);
    }
    if (!rapl_core_path_.empty()) {
        rapl_core_fd_ = open_ro_cloexec(rapl_core_path_);
    }
    if (!gpu_power_path_.empty()) {
        gpu_power_fd_ = open_ro_cloexec(gpu_power_path_);
    }
    if (!backlight_path_.empty()) {
        backlight_cur_fd_ = open_ro_cloexec(backlight_path_ / "brightness");
        backlight_max_fd_ = open_ro_cloexec(backlight_path_ / "max_brightness");
    }
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
                break;
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

    open_persistent_fds();
}

HardwareSample HardwareProbe::capture_sample() const {
    HardwareSample sample;
    sample.timestamp = std::chrono::steady_clock::now();

    // 1. Read Battery via persistent pread
    if (battery_status_fd_ >= 0) {
        auto status_str = read_string_fd(battery_status_fd_);
        sample.is_discharging = (status_str.rfind("Discharging", 0) == 0);
    }

    if (battery_power_fd_ >= 0) {
        sample.battery_power_uw = read_uint64_fd(battery_power_fd_);
    } else if (battery_voltage_fd_ >= 0 && battery_current_fd_ >= 0) {
        auto v = read_uint64_fd(battery_voltage_fd_);
        auto i = read_int64_fd(battery_current_fd_);
        if (v.has_value() && i.has_value()) {
            sample.battery_voltage_uv = v;
            sample.battery_current_ua = i;
            int64_t abs_curr = *i < 0 ? -*i : *i;
            sample.battery_power_uw = static_cast<uint64_t>((*v * static_cast<uint64_t>(abs_curr)) / 1'000'000ULL);
        }
    }

    // 2. Read RAPL via persistent pread
    if (rapl_pkg_fd_ >= 0) {
        sample.rapl_package_uj = read_uint64_fd(rapl_pkg_fd_);
    }
    if (rapl_core_fd_ >= 0) {
        sample.rapl_core_uj = read_uint64_fd(rapl_core_fd_);
    }

    // 3. Read GPU via persistent pread
    if (gpu_power_fd_ >= 0) {
        sample.gpu_power_uw = read_uint64_fd(gpu_power_fd_);
    }

    // 4. Read Backlight via persistent pread
    if (backlight_cur_fd_ >= 0) {
        auto cur_b = read_uint64_fd(backlight_cur_fd_);
        if (cur_b.has_value()) sample.backlight_brightness = static_cast<uint32_t>(*cur_b);
    }
    if (backlight_max_fd_ >= 0) {
        auto max_b = read_uint64_fd(backlight_max_fd_);
        if (max_b.has_value()) sample.backlight_max_brightness = static_cast<uint32_t>(*max_b);
    }

    return sample;
}

std::optional<uint64_t> HardwareProbe::read_uint64_fd(int fd) {
    if (fd < 0) return std::nullopt;

    std::array<char, 64> buffer{};
    ssize_t bytes_read = ::pread(fd, buffer.data(), buffer.size() - 1, 0);
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

std::optional<int64_t> HardwareProbe::read_int64_fd(int fd) {
    if (fd < 0) return std::nullopt;

    std::array<char, 64> buffer{};
    ssize_t bytes_read = ::pread(fd, buffer.data(), buffer.size() - 1, 0);
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

std::string HardwareProbe::read_string_fd(int fd) {
    if (fd < 0) return {};

    std::array<char, 64> buffer{};
    ssize_t bytes_read = ::pread(fd, buffer.data(), buffer.size() - 1, 0);
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
