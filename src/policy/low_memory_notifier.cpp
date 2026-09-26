#include "policy/low_memory_notifier.hpp"
#include "core/event_logger.hpp"

#include <ctime>
#include <cstdio>

namespace wattcurb::policy {

namespace {

[[nodiscard]] inline uint64_t get_monotonic_sec() noexcept {
    struct timespec ts{};
    if (::clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return static_cast<uint64_t>(ts.tv_sec);
    }
    return 0;
}

} // namespace

LowMemoryNotifier::~LowMemoryNotifier() noexcept {
    shutdown();
}

bool LowMemoryNotifier::initialize() noexcept {
    if (m_bus != nullptr) return true;

    // Connect to system D-Bus with low latency
    int r = ::sd_bus_open_system(&m_bus);
    if (r < 0 || m_bus == nullptr) {
        m_bus = nullptr;
        return false;
    }

    // Try claiming the well-known service name if low-memory-monitor is absent.
    // If another service already owns it, we still proceed as a valid signal broadcaster.
    r = ::sd_bus_request_name(m_bus, "org.freedesktop.LowMemoryMonitor", SD_BUS_NAME_REPLACE_EXISTING);
    m_service_name_claimed = (r >= 0);

    return true;
}

void LowMemoryNotifier::shutdown() noexcept {
    if (m_bus != nullptr) {
        if (m_last_level != LEVEL_NORMAL) {
            // Send normal level before terminating
            (void)notify(LEVEL_NORMAL, get_monotonic_sec());
        }
        ::sd_bus_flush_close_unref(m_bus);
        m_bus = nullptr;
    }
    m_last_level = LEVEL_NORMAL;
    m_last_emit_sec = 0;
    m_service_name_claimed = false;
}

bool LowMemoryNotifier::notify(uint8_t level, uint64_t now_sec) noexcept {
    if (m_bus == nullptr) {
        if (!initialize()) return false;
    }

    if (now_sec == 0) {
        now_sec = get_monotonic_sec();
    }

    // Rate-limit identical signals to avoid spamming recipient process main loops
    if (level == m_last_level && (now_sec - m_last_emit_sec) < REPEAT_COOLDOWN_SEC) {
        return false;
    }

    int r = ::sd_bus_emit_signal(
        m_bus,
        "/org/freedesktop/LowMemoryMonitor",
        "org.freedesktop.LowMemoryMonitor",
        "LowMemoryWarning",
        "y",
        level
    );

    if (r >= 0) {
        ::sd_bus_flush(m_bus);
        m_last_level = level;
        m_last_emit_sec = now_sec;

        char detail[128];
        const char* desc = (level == LEVEL_CRITICAL) ? "Critical (255)" :
                           (level == LEVEL_MODERATE) ? "Moderate (100)" : "Normal (0)";
        std::snprintf(detail, sizeof(detail), "Emitted D-Bus LowMemoryWarning(%s) (REF-REQ-130)", desc);
        core::EventLogger::log_alert("MEMORY_GC", detail);
        return true;
    }

    return false;
}

} // namespace wattcurb::policy
