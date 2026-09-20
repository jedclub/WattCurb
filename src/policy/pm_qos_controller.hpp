#pragma once

#include <cstdint>
#include <fcntl.h>
#include <unistd.h>
#include <string_view>

namespace wattcurb::policy {

// Implements REF-REQ-085, REF-ARCH-062, REF-RES-021:
// Linux PM QoS Interface via /dev/cpu_dma_latency
// Clamps CPU idle governors (menu, teo) to maximum exit latency (e.g. 0µs)
// to prevent deep power-gated C-state sleep (C2/C3/C6) during active user interaction.
// When the open file descriptor is closed, the kernel automatically removes the constraint.
class PmQosController {
public:
    PmQosController() noexcept = default;
    ~PmQosController() noexcept { release_latency_pin(); }

    PmQosController(const PmQosController&) = delete;
    PmQosController& operator=(const PmQosController&) = delete;

    PmQosController(PmQosController&& other) noexcept : m_fd(other.m_fd) {
        other.m_fd = -1;
    }
    PmQosController& operator=(PmQosController&& other) noexcept {
        if (this != &other) {
            release_latency_pin();
            m_fd = other.m_fd;
            other.m_fd = -1;
        }
        return *this;
    }

    // Opens /dev/cpu_dma_latency and writes max_latency_us (default 0 for C0/C1)
    bool pin_c0_latency(int32_t max_latency_us = 0) noexcept;

    // Closes /dev/cpu_dma_latency FD, restoring deep C-states instantly
    void release_latency_pin() noexcept;

    [[nodiscard]] bool is_pinned() const noexcept { return m_fd >= 0; }
    [[nodiscard]] int raw_fd() const noexcept { return m_fd; }

    // Test support: allow setting custom device path
    static void set_device_path_for_testing(const char* path) noexcept {
        s_device_path = path;
    }
    static void reset_device_path() noexcept {
        s_device_path = "/dev/cpu_dma_latency";
    }
    static const char* device_path() noexcept {
        return s_device_path;
    }

private:
    int m_fd{-1};
    inline static const char* s_device_path = "/dev/cpu_dma_latency";
};

} // namespace wattcurb::policy
