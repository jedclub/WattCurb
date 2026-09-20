#include "policy/pm_qos_controller.hpp"
#include <cerrno>

namespace wattcurb::policy {

// Implements REF-REQ-085, REF-ARCH-062, REF-RES-021:
// PM QoS Hardware Latency Pinning Actuation
bool PmQosController::pin_c0_latency(int32_t max_latency_us) noexcept {
    if (m_fd >= 0) {
        int32_t val = max_latency_us;
        ssize_t bytes = ::write(m_fd, &val, sizeof(val));
        return (bytes == sizeof(val));
    }

    int fd = ::open(s_device_path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }

    int32_t val = max_latency_us;
    ssize_t bytes = ::write(fd, &val, sizeof(val));
    if (bytes != sizeof(val)) {
        ::close(fd);
        return false;
    }

    m_fd = fd;
    return true;
}

void PmQosController::release_latency_pin() noexcept {
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

} // namespace wattcurb::policy
