#include "policy/swap_tier_manager.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>

namespace wattcurb::policy {

namespace {

[[nodiscard]] const char* skip_whitespace(const char* p, const char* end) noexcept {
    while (p < end && (*p == ' ' || *p == '\t')) ++p;
    return p;
}

[[nodiscard]] const char* skip_non_whitespace(const char* p, const char* end) noexcept {
    while (p < end && *p != ' ' && *p != '\t' && *p != '\n') ++p;
    return p;
}

[[nodiscard]] bool parse_u64(const char*& p, const char* end, uint64_t& out) noexcept {
    p = skip_whitespace(p, end);
    if (p >= end || *p < '0' || *p > '9') return false;
    out = 0;
    while (p < end && *p >= '0' && *p <= '9') {
        out = out * 10 + static_cast<uint64_t>(*p - '0');
        ++p;
    }
    return true;
}

[[nodiscard]] bool parse_i32(const char*& p, const char* end, int32_t& out) noexcept {
    p = skip_whitespace(p, end);
    if (p >= end) return false;
    bool neg = false;
    if (*p == '-') {
        neg = true;
        ++p;
    } else if (*p == '+') {
        ++p;
    }
    if (p >= end || *p < '0' || *p > '9') return false;
    int64_t val = 0;
    while (p < end && *p >= '0' && *p <= '9') {
        val = val * 10 + (*p - '0');
        ++p;
    }
    out = neg ? static_cast<int32_t>(-val) : static_cast<int32_t>(val);
    return true;
}

} // namespace

bool SwapTierManager::parse_swaps_buffer(
    const char* buf,
    size_t len,
    core::FixedVector<SwapDeviceEntry, MAX_SWAP_DEVICES>& out
) noexcept {
    out.clear();
    if (buf == nullptr || len == 0) return false;

    const char* p = buf;
    const char* const end = buf + len;

    // Skip header line (e.g. "Filename\tType\tSize\tUsed\tPriority")
    while (p < end && *p != '\n') ++p;
    if (p < end && *p == '\n') ++p;

    while (p < end) {
        p = skip_whitespace(p, end);
        if (p >= end) break;
        if (*p == '\n') {
            ++p;
            continue;
        }

        const char* const line_end = static_cast<const char*>(std::memchr(p, '\n', static_cast<size_t>(end - p)));
        const char* const curr_line_end = (line_end != nullptr) ? line_end : end;

        // 1. Filename
        const char* const name_start = p;
        const char* const name_end = skip_non_whitespace(p, curr_line_end);
        const size_t name_len = static_cast<size_t>(name_end - name_start);
        if (name_len == 0 || out.size() >= MAX_SWAP_DEVICES) break;

        SwapDeviceEntry entry{};
        const size_t copy_len = (name_len < sizeof(entry.name) - 1) ? name_len : (sizeof(entry.name) - 1);
        std::memcpy(entry.name, name_start, copy_len);
        entry.name[copy_len] = '\0';

        entry.is_zram = (std::strstr(entry.name, "zram") != nullptr);

        p = name_end;

        // 2. Type (skip)
        p = skip_whitespace(p, curr_line_end);
        p = skip_non_whitespace(p, curr_line_end);

        // 3. Size (KiB)
        if (!parse_u64(p, curr_line_end, entry.size_kb)) break;

        // 4. Used (KiB)
        if (!parse_u64(p, curr_line_end, entry.used_kb)) break;

        // 5. Priority
        if (!parse_i32(p, curr_line_end, entry.priority)) break;

        out.push_back(entry);

        p = (line_end != nullptr) ? line_end + 1 : end;
    }

    return !out.empty();
}

bool SwapTierManager::refresh() noexcept {
    int fd = ::open("/proc/swaps", O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;

    char buf[2048];
    const ssize_t bytes_read = ::read(fd, buf, sizeof(buf) - 1);
    ::close(fd);

    if (bytes_read <= 0) return false;
    buf[bytes_read] = '\0';

    if (!parse_swaps_buffer(buf, static_cast<size_t>(bytes_read), m_devices)) {
        return false;
    }

    m_zram_total_kb = 0;
    m_zram_used_kb = 0;
    m_disk_total_kb = 0;
    m_disk_used_kb = 0;
    m_zram_priority = -1000;
    m_max_disk_priority = -1000;

    for (size_t i = 0; i < m_devices.size(); ++i) {
        const auto& dev = m_devices[i];
        if (dev.is_zram) {
            m_zram_total_kb += dev.size_kb;
            m_zram_used_kb += dev.used_kb;
            if (dev.priority > m_zram_priority) {
                m_zram_priority = dev.priority;
            }
        } else {
            m_disk_total_kb += dev.size_kb;
            m_disk_used_kb += dev.used_kb;
            if (dev.priority > m_max_disk_priority) {
                m_max_disk_priority = dev.priority;
            }
        }
    }

    return true;
}

double SwapTierManager::zram_usage_pct() const noexcept {
    if (m_zram_total_kb == 0) return 0.0;
    return (static_cast<double>(m_zram_used_kb) * 100.0) / static_cast<double>(m_zram_total_kb);
}

double SwapTierManager::disk_swap_usage_pct() const noexcept {
    if (m_disk_total_kb == 0) return 0.0;
    return (static_cast<double>(m_disk_used_kb) * 100.0) / static_cast<double>(m_disk_total_kb);
}

bool SwapTierManager::is_zram_prioritized() const noexcept {
    if (m_zram_total_kb == 0) return false;
    if (m_disk_total_kb == 0) return true; // Only ZRAM exists
    return m_zram_priority > m_max_disk_priority;
}

double SwapTierManager::read_zram_compression_ratio(const char* zram_name) noexcept {
    char path[128];
    std::snprintf(path, sizeof(path), "/sys/block/%s/orig_data_size", zram_name);
    int fd = ::open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return 1.0;

    char buf[64];
    ssize_t n = ::read(fd, buf, sizeof(buf) - 1);
    ::close(fd);
    if (n <= 0) return 1.0;
    buf[n] = '\0';
    const uint64_t orig = static_cast<uint64_t>(std::strtoull(buf, nullptr, 10));

    std::snprintf(path, sizeof(path), "/sys/block/%s/compr_data_size", zram_name);
    fd = ::open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return 1.0;
    n = ::read(fd, buf, sizeof(buf) - 1);
    ::close(fd);
    if (n <= 0) return 1.0;
    buf[n] = '\0';
    const uint64_t compr = static_cast<uint64_t>(std::strtoull(buf, nullptr, 10));

    if (compr == 0) return 1.0;
    return static_cast<double>(orig) / static_cast<double>(compr);
}

} // namespace wattcurb::policy
