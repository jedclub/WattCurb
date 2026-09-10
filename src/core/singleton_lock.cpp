#include "core/singleton_lock.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <poll.h>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <utility>

namespace wattcurb::core {

namespace {

socklen_t prepare_abstract_addr(std::string_view name, struct sockaddr_un& addr) {
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    addr.sun_path[0] = '\0'; // Abstract namespace marker

    size_t copy_len = std::min(name.size(), sizeof(addr.sun_path) - 2);
    std::memcpy(addr.sun_path + 1, name.data(), copy_len);

    return static_cast<socklen_t>(sizeof(sa_family_t) + 1 + copy_len);
}

} // namespace

SingletonLock::SingletonLock(std::string_view lock_name)
    : lock_name_(lock_name) {
    int fd = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return;

    struct sockaddr_un addr{};
    socklen_t addr_len = prepare_abstract_addr(lock_name_, addr);

    if (::bind(fd, reinterpret_cast<struct sockaddr*>(&addr), addr_len) < 0) {
        // Another instance is holding the lock
        ::close(fd);
        socket_fd_ = -1;
        return;
    }

    socket_fd_ = fd;
}

SingletonLock::~SingletonLock() {
    release();
}

SingletonLock::SingletonLock(SingletonLock&& other) noexcept
    : lock_name_(std::move(other.lock_name_)),
      socket_fd_(std::exchange(other.socket_fd_, -1)) {}

SingletonLock& SingletonLock::operator=(SingletonLock&& other) noexcept {
    if (this != &other) {
        release();
        lock_name_ = std::move(other.lock_name_);
        socket_fd_ = std::exchange(other.socket_fd_, -1);
    }
    return *this;
}

void SingletonLock::release() noexcept {
    if (socket_fd_ >= 0) {
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
}

bool SingletonLock::is_daemon_running(std::string_view lock_name) {
    int fd = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return false;

    struct sockaddr_un addr{};
    socklen_t addr_len = prepare_abstract_addr(lock_name, addr);

    bool running = false;
    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), addr_len) == 0) {
        running = true;
    }
    ::close(fd);
    return running;
}

bool SingletonLock::query_daemon(std::string_view command, std::string& out_response, std::string_view lock_name, int timeout_ms) {
    int fd = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return false;

    // Bind client to a unique temporary abstract socket address
    char client_name[64];
    std::snprintf(client_name, sizeof(client_name), "wattcurb.client.%d.%lx", ::getpid(), static_cast<unsigned long>(::time(nullptr)));
    struct sockaddr_un client_addr{};
    socklen_t client_len = prepare_abstract_addr(client_name, client_addr);
    if (::bind(fd, reinterpret_cast<struct sockaddr*>(&client_addr), client_len) < 0) {
        ::close(fd);
        return false;
    }

    // Prepare target daemon abstract address
    struct sockaddr_un daemon_addr{};
    socklen_t daemon_len = prepare_abstract_addr(lock_name, daemon_addr);

    // Send command datagram
    ssize_t sent = ::sendto(fd, command.data(), command.size(), 0,
                            reinterpret_cast<struct sockaddr*>(&daemon_addr), daemon_len);
    if (sent < 0) {
        ::close(fd);
        return false;
    }

    // Wait for response with timeout using poll
    struct pollfd pfd{};
    pfd.fd = fd;
    pfd.events = POLLIN;
    int poll_ret = ::poll(&pfd, 1, timeout_ms > 0 ? timeout_ms : 1500);
    if (poll_ret <= 0 || !(pfd.revents & POLLIN)) {
        ::close(fd);
        return false;
    }

    // Receive response
    char recv_buf[65536];
    ssize_t bytes = ::recvfrom(fd, recv_buf, sizeof(recv_buf) - 1, 0, nullptr, nullptr);
    ::close(fd);

    if (bytes <= 0) return false;
    recv_buf[bytes] = '\0';
    out_response.assign(recv_buf, static_cast<size_t>(bytes));
    return true;
}

} // namespace wattcurb::core
