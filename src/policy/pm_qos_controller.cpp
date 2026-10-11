#include "policy/pm_qos_controller.hpp"
#include <cerrno>
#include <cstring>
#include <cstdio>

namespace wattcurb::policy {

const char* volatile PmQosController::s_device_path = "/dev/cpu_dma_latency";

void PmQosController::set_device_path_for_testing(const char* path) noexcept {
    s_device_path = path;
}

void PmQosController::reset_device_path() noexcept {
    s_device_path = "/dev/cpu_dma_latency";
}

const char* PmQosController::device_path() noexcept {
    return const_cast<const char*>(s_device_path);
}

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
        ::dprintf(2, "[PM_QOS DEBUG] open(%s) failed: errno=%d (%s)\n",
                  s_device_path ? s_device_path : "null", errno, std::strerror(errno));
        return false;
    }

    int32_t val = max_latency_us;
    ssize_t bytes = ::write(fd, &val, sizeof(val));
    if (bytes != sizeof(val)) {
        ::dprintf(2, "[PM_QOS DEBUG] write(%s, 4B) returned %ld, errno=%d (%s)\n",
                  s_device_path ? s_device_path : "null", (long)bytes, errno, std::strerror(errno));
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
