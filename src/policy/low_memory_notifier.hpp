#pragma once

#include <cstdint>
#include <systemd/sd-bus.h>

namespace wattcurb::policy {

// Implements REF-REQ-130, REF-ARCH-077 (Phase 1):
// LowMemoryNotifier: Cooperative D-Bus Signal Engine
//
// Broadcasts `LowMemoryWarning(uint8_t level)` on the system D-Bus conforming to
// the FreeDesktop `org.freedesktop.LowMemoryMonitor` interface.
//
// In-app cooperative receivers (Chromium, Firefox, Electron, WebKitGTK, GTK4):
//   - Level 100 (Moderate): V8/JS Major GC sweep, discard image/font/decoded media caches.
//   - Level 255 (Critical): Aggressive discard of inactive tabs/caches, malloc_trim(0).
//   - Level 0 (Normal): Pressure cleared, resume standard caching policies.
//
// Characteristics:
//   - ZERO Disk I/O (pure CPU within recipient processes).
//   - Zero System Stall / Non-blocking.
class LowMemoryNotifier {
public:
    static constexpr uint8_t LEVEL_NORMAL   = 0;
    static constexpr uint8_t LEVEL_MODERATE = 100;
    static constexpr uint8_t LEVEL_CRITICAL = 255;

    // Minimum interval (in seconds) before re-emitting the exact same warning level.
    // Level escalations/de-escalations emit immediately.
    static constexpr uint64_t REPEAT_COOLDOWN_SEC = 15;

    LowMemoryNotifier() noexcept = default;
    ~LowMemoryNotifier() noexcept;

    LowMemoryNotifier(const LowMemoryNotifier&) = delete;
    LowMemoryNotifier& operator=(const LowMemoryNotifier&) = delete;

    bool initialize() noexcept;
    void shutdown() noexcept;

    // Emits LowMemoryWarning(level) over system bus.
    // Returns true if the signal was successfully dispatched.
    bool notify(uint8_t level, uint64_t now_sec = 0) noexcept;

    [[nodiscard]] bool is_connected() const noexcept { return m_bus != nullptr; }
    [[nodiscard]] uint8_t last_level() const noexcept { return m_last_level; }
    [[nodiscard]] uint64_t last_emit_timestamp() const noexcept { return m_last_emit_sec; }

private:
    sd_bus* m_bus{nullptr};
    uint8_t m_last_level{0};
    uint64_t m_last_emit_sec{0};
    bool m_service_name_claimed{false};
};

} // namespace wattcurb::policy
