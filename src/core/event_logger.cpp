#include "core/event_logger.hpp"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <unistd.h>

namespace wattcurb::core {

namespace {

const char* profile_name(PowerProfileMode mode) noexcept {
    switch (mode) {
        case PowerProfileMode::Performance: return "Performance";
        case PowerProfileMode::Balanced: return "Balanced";
        case PowerProfileMode::PowerSaver: return "PowerSaver";
        case PowerProfileMode::UltraEndurance: return "UltraEndurance";
        default: return "Unknown";
    }
}

} // namespace

void EventLogger::initialize(const char* audit_path) noexcept {
    if (s_audit_fd >= 0) {
        ::close(s_audit_fd);
        s_audit_fd = -1;
    }

    if (audit_path != nullptr && audit_path[0] != '\0') {
        s_audit_fd = ::open(audit_path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    }
}

void EventLogger::shutdown() noexcept {
    if (s_audit_fd >= 0) {
        ::close(s_audit_fd);
        s_audit_fd = -1;
    }
}

size_t EventLogger::format_entry(char* out_buf, size_t max_len, const char* tag, const char* message) noexcept {
    if (!out_buf || max_len == 0) return 0;

    std::time_t now = ::time(nullptr);
    struct std::tm tm_buf{};
    ::localtime_r(&now, &tm_buf);

    int written = std::snprintf(
        out_buf, max_len,
        "[%04d-%02d-%02d %02d:%02d:%02d] [WATTCURB][%s] %s\n",
        tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
        tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
        tag ? tag : "INFO",
        message ? message : ""
    );

    if (written < 0) return 0;
    return static_cast<size_t>(std::min(written, static_cast<int>(max_len - 1)));
}

void EventLogger::write_entry(const char* tag, const char* message) noexcept {
    char buf[512];
    size_t len = format_entry(buf, sizeof(buf), tag, message);
    if (len == 0) return;

    // 1. Emit to stdout (captured by systemd journald)
    (void)::write(STDOUT_FILENO, buf, len);

    // 2. Emit to dedicated audit file if open
    if (s_audit_fd >= 0) {
        (void)::write(s_audit_fd, buf, len);
    }
}

void EventLogger::log_profile_change(PowerProfileMode old_mode, PowerProfileMode new_mode,
                                    const char* trigger, const char* details) noexcept {
    char msg[384];
    if (details && details[0] != '\0') {
        std::snprintf(msg, sizeof(msg),
                      "Profile Transition: %s -> %s | Trigger: %s | Limits: %s",
                      profile_name(old_mode), profile_name(new_mode),
                      trigger ? trigger : "unspecified", details);
    } else {
        std::snprintf(msg, sizeof(msg),
                      "Profile Transition: %s -> %s | Trigger: %s",
                      profile_name(old_mode), profile_name(new_mode),
                      trigger ? trigger : "unspecified");
    }
    write_entry("PROFILE", msg);
}

void EventLogger::log_mitigation(int32_t pid, const char* comm, const char* action,
                                const char* details) noexcept {
    char msg[384];
    std::snprintf(msg, sizeof(msg),
                  "Mitigation Actuated: PID %d (%s) | Action: %s | Details: %s",
                  pid, comm ? comm : "unknown", action ? action : "capping",
                  details ? details : "none");
    write_entry("MITIGATION", msg);
}

void EventLogger::log_rollback(int32_t pid, const char* comm, const char* details) noexcept {
    char msg[384];
    std::snprintf(msg, sizeof(msg),
                  "Mitigation Rollback: PID %d (%s) | Details: %s",
                  pid, comm ? comm : "unknown", details ? details : "restored to baseline");
    write_entry("ROLLBACK", msg);
}

void EventLogger::log_alert(const char* category, const char* message) noexcept {
    char tag_buf[32];
    std::snprintf(tag_buf, sizeof(tag_buf), "ALERT:%s", category ? category : "SYS");
    write_entry(tag_buf, message);
}

void EventLogger::log_raw(const char* tag, const char* message) noexcept {
    write_entry(tag, message);
}

} // namespace wattcurb::core
