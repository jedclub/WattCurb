#include "core/singleton_lock.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
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

} // namespace wattcurb::core
