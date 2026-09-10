#pragma once

#include <string>
#include <string_view>

namespace wattcurb::core {

// Implements REF-REQ-002, REF-REQ-007, REF-ARCH-004
class SingletonLock {
public:
    explicit SingletonLock(std::string_view lock_name = "wattcurb.lock");
    ~SingletonLock();

    SingletonLock(const SingletonLock&) = delete;
    SingletonLock& operator=(const SingletonLock&) = delete;
    SingletonLock(SingletonLock&& other) noexcept;
    SingletonLock& operator=(SingletonLock&& other) noexcept;

    [[nodiscard]] bool is_locked() const noexcept { return socket_fd_ >= 0; }
    [[nodiscard]] int socket_fd() const noexcept { return socket_fd_; }

    void release() noexcept;

    // Client helper: check if another daemon instance is already active
    static bool is_daemon_running(std::string_view lock_name = "wattcurb.lock");

    // Client helper: query running daemon over abstract UNIX datagram socket
    static bool query_daemon(std::string_view command, std::string& out_response, std::string_view lock_name = "wattcurb.lock", int timeout_ms = 1500);

private:
    std::string lock_name_;
    int socket_fd_{-1};
};

} // namespace wattcurb::core
