#pragma once

#include "ipc/tray_shared_state.hpp"
#include <systemd/sd-bus.h>
#include <cstdint>
#include <cstddef>

namespace wattcurb::tray {

// Implements REF-REQ-035 & REF-ARCH-025:
// ThinkPower-Faithful Ultra-Low-Overhead SNI Desktop Tray Client
class TrayClient {
public:
    TrayClient() noexcept = default;
    ~TrayClient() noexcept;

    // Non-copyable, non-movable singleton client
    TrayClient(const TrayClient&) = delete;
    TrayClient& operator=(const TrayClient&) = delete;

    // Initializes shared memory mapping and registers SNI service over session D-Bus
    bool initialize() noexcept;

    // Event loop: sleeps in epoll on D-Bus descriptor, 0% CPU idle
    int run() noexcept;

    // Stops the event loop
    void stop() noexcept;

    // Static helper: Renders dense technical ThinkPower tooltip with ZERO heap allocations
    static void render_tooltip(
        const ipc::WattCurbSharedState& state,
        char* out_title, size_t title_cap,
        char* out_desc, size_t desc_cap
    ) noexcept;

    // Static helper: Resolves freedesktop battery icon based on power & charge status
    static void resolve_icon_name(
        const ipc::WattCurbSharedState& state,
        char* out_icon, size_t icon_cap
    ) noexcept;

    // Sends command to daemon via non-blocking Unix Domain Socket
    static bool send_daemon_command(const char* cmd) noexcept;

    // Cycles power profile (Left click handler)
    void cycle_power_profile() noexcept;

    // Read latest shared state directly from Seqlock SHM (Lock-free, < 50ns)
    bool read_state(ipc::WattCurbSharedState& out) const noexcept;

    // D-Bus VTable callbacks
    static int property_get_category(sd_bus* bus, const char* path, const char* interface, const char* property, sd_bus_message* reply, void* userdata, sd_bus_error* error);
    static int property_get_id(sd_bus* bus, const char* path, const char* interface, const char* property, sd_bus_message* reply, void* userdata, sd_bus_error* error);
    static int property_get_title(sd_bus* bus, const char* path, const char* interface, const char* property, sd_bus_message* reply, void* userdata, sd_bus_error* error);
    static int property_get_status(sd_bus* bus, const char* path, const char* interface, const char* property, sd_bus_message* reply, void* userdata, sd_bus_error* error);
    static int property_get_icon_name(sd_bus* bus, const char* path, const char* interface, const char* property, sd_bus_message* reply, void* userdata, sd_bus_error* error);
    static int property_get_tooltip(sd_bus* bus, const char* path, const char* interface, const char* property, sd_bus_message* reply, void* userdata, sd_bus_error* error);
    static int property_get_icon_theme_path(sd_bus* bus, const char* path, const char* interface, const char* property, sd_bus_message* reply, void* userdata, sd_bus_error* error);
    static int property_get_icon_pixmap(sd_bus* bus, const char* path, const char* interface, const char* property, sd_bus_message* reply, void* userdata, sd_bus_error* error);
    static int property_get_menu(sd_bus* bus, const char* path, const char* interface, const char* property, sd_bus_message* reply, void* userdata, sd_bus_error* error);
    static int property_get_item_is_menu(sd_bus* bus, const char* path, const char* interface, const char* property, sd_bus_message* reply, void* userdata, sd_bus_error* error);
    static int property_get_window_id(sd_bus* bus, const char* path, const char* interface, const char* property, sd_bus_message* reply, void* userdata, sd_bus_error* error);
    static int property_get_xayatana_label(sd_bus* bus, const char* path, const char* interface, const char* property, sd_bus_message* reply, void* userdata, sd_bus_error* error);
    static int property_get_xayatana_label_guide(sd_bus* bus, const char* path, const char* interface, const char* property, sd_bus_message* reply, void* userdata, sd_bus_error* error);
    static int method_activate(sd_bus_message* msg, void* userdata, sd_bus_error* error);
    static int method_context_menu(sd_bus_message* msg, void* userdata, sd_bus_error* error);
    static int method_noop(sd_bus_message* msg, void* userdata, sd_bus_error* error);

    // com.canonical.dbusmenu VTable callbacks
    static int dbusmenu_property_get_version(sd_bus* bus, const char* path, const char* interface, const char* property, sd_bus_message* reply, void* userdata, sd_bus_error* error);
    static int dbusmenu_property_get_status(sd_bus* bus, const char* path, const char* interface, const char* property, sd_bus_message* reply, void* userdata, sd_bus_error* error);
    static int dbusmenu_method_get_layout(sd_bus_message* msg, void* userdata, sd_bus_error* error);
    static int dbusmenu_method_event(sd_bus_message* msg, void* userdata, sd_bus_error* error);
    static int dbusmenu_method_about_to_show(sd_bus_message* msg, void* userdata, sd_bus_error* error);

private:
    bool setup_shm() noexcept;
    bool setup_dbus() noexcept;
    bool register_with_watcher() noexcept;
    void cleanup() noexcept;

    sd_bus* bus_{nullptr};
    sd_bus_slot* slot_{nullptr};
    sd_bus_slot* menu_slot_{nullptr};
    int shm_fd_{-1};
    const ipc::WattCurbSharedState* shm_state_{nullptr};
    bool running_{false};
    char service_name_[64]{0};
    uint32_t menu_revision_{1};
    int local_override_mode_{-1};
};

} // namespace wattcurb::tray
