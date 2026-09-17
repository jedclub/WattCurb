#pragma once

#include "core/types.hpp"
#include <cstdint>
#include <string_view>

namespace wattcurb::core {

// Implements REF-REQ-059 & REF-ARCH-035: Zero-Allocation Event-Driven Audit Logger
class EventLogger {
public:
    static void initialize(const char* audit_path = "/var/log/wattcurb/audit.log") noexcept;
    static void shutdown() noexcept;

    static void log_profile_change(PowerProfileMode old_mode, PowerProfileMode new_mode,
                                   const char* trigger, const char* details = nullptr) noexcept;

    static void log_mitigation(int32_t pid, const char* comm, const char* action,
                               const char* details) noexcept;

    static void log_rollback(int32_t pid, const char* comm, const char* details) noexcept;

    static void log_alert(const char* category, const char* message) noexcept;

    static void log_raw(const char* tag, const char* message) noexcept;

    // Helper for formatted buffer test validation
    static size_t format_entry(char* out_buf, size_t max_len, const char* tag, const char* message) noexcept;

private:
    static inline int s_audit_fd{-1};
    static void write_entry(const char* tag, const char* message) noexcept;
};

} // namespace wattcurb::core
