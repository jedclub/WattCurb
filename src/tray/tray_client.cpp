#include "tray/tray_client.hpp"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace wattcurb::tray {

namespace {

// Standard freedesktop SNI ToolTip D-Bus signature: (sa(iiay)ss)
// icon_name (s), icon_data (a(iiay)), title (s), description (s)

static const sd_bus_vtable sni_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("Category", "s", TrayClient::property_get_category, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Id", "s", TrayClient::property_get_id, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Title", "s", TrayClient::property_get_title, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Status", "s", TrayClient::property_get_status, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("IconName", "s", TrayClient::property_get_icon_name, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("IconThemePath", "s", TrayClient::property_get_icon_theme_path, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("ItemIsMenu", "b", TrayClient::property_get_item_is_menu, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("ToolTip", "(sa(iiay)ss)", TrayClient::property_get_tooltip, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_METHOD("Activate", "ii", nullptr, TrayClient::method_activate, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("ContextMenu", "ii", nullptr, TrayClient::method_context_menu, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("SecondaryActivate", "ii", nullptr, TrayClient::method_noop, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Scroll", "is", nullptr, TrayClient::method_noop, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("NewTitle", nullptr, 0),
    SD_BUS_SIGNAL("NewIcon", nullptr, 0),
    SD_BUS_SIGNAL("NewToolTip", nullptr, 0),
    SD_BUS_SIGNAL("NewStatus", "s", 0),
    SD_BUS_VTABLE_END
};

} // anonymous namespace

TrayClient::~TrayClient() noexcept {
    cleanup();
}

void TrayClient::cleanup() noexcept {
    if (slot_) {
        sd_bus_slot_unref(slot_);
        slot_ = nullptr;
    }
    if (bus_) {
        sd_bus_flush_close_unref(bus_);
        bus_ = nullptr;
    }
    if (shm_state_ && shm_state_ != MAP_FAILED) {
        ::munmap(const_cast<ipc::WattCurbSharedState*>(shm_state_), sizeof(ipc::WattCurbSharedState));
        shm_state_ = nullptr;
    }
    if (shm_fd_ >= 0) {
        ::close(shm_fd_);
        shm_fd_ = -1;
    }
}

bool TrayClient::setup_shm() noexcept {
    shm_fd_ = ::open(ipc::SHARED_STATE_SHM_PATH, O_RDONLY | O_CLOEXEC);
    if (shm_fd_ < 0) return false;

    void* ptr = ::mmap(nullptr, sizeof(ipc::WattCurbSharedState), PROT_READ, MAP_SHARED, shm_fd_, 0);
    if (ptr == MAP_FAILED) {
        ::close(shm_fd_);
        shm_fd_ = -1;
        return false;
    }
    shm_state_ = static_cast<const ipc::WattCurbSharedState*>(ptr);
    return true;
}

bool TrayClient::read_state(ipc::WattCurbSharedState& out) const noexcept {
    if (!shm_state_) return false;
    return shm_state_->read_atomic(out);
}

void TrayClient::render_tooltip(
    const ipc::WattCurbSharedState& state,
    char* out_title, size_t title_cap,
    char* out_desc, size_t desc_cap
) noexcept {
    // Ultra-fast integer fixed-point formatting: eliminates heavy float snprintf overhead
    unsigned int sys_w = state.system_drain_mw / 1000;
    unsigned int sys_frac = (state.system_drain_mw % 1000) / 100;

    const char* status_str = state.battery_state == 1 ? "Discharging" : 
                            (state.battery_state == 2 ? "AC Passthrough" : "AC / Charging");

    std::snprintf(out_title, title_cap, "WattCurb: %u.%u W (%s)", sys_w, sys_frac, status_str);

    const char* profile_name = "Balanced";
    if (state.power_profile_mode == 1) profile_name = "PowerSaver";
    else if (state.power_profile_mode == 2) profile_name = "UltraEndurance";

    unsigned int cpu_w = state.cpu_drain_mw / 1000;
    unsigned int cpu_frac = (state.cpu_drain_mw % 1000) / 100;
    unsigned int gpu_w = state.gpu_drain_mw / 1000;
    unsigned int gpu_frac = (state.gpu_drain_mw % 1000) / 100;

    std::snprintf(out_desc, desc_cap,
        "Battery: %u%% (Health: %u%%) | Est: %u min\n"
        "CPU: %u.%u W (%u°C, Fan %u RPM) | GPU: %u.%u W\n"
        "Top 1: %s (%u mW, PID %d)\n"
        "Top 2: %s (%u mW, PID %d)\n"
        "Profile: %s | Active Gates: %u",
        state.battery_percent, state.battery_health_percent, state.time_to_empty_min,
        cpu_w, cpu_frac, state.cpu_temp_c, state.fan_rpm,
        gpu_w, gpu_frac,
        state.culprits[0].comm[0] ? state.culprits[0].comm : "none",
        state.culprits[0].drain_mw, state.culprits[0].pid,
        state.culprits[1].comm[0] ? state.culprits[1].comm : "none",
        state.culprits[1].drain_mw, state.culprits[1].pid,
        profile_name,
        state.active_mitigations
    );
}

void TrayClient::resolve_icon_name(
    const ipc::WattCurbSharedState& state,
    char* out_icon, size_t icon_cap
) noexcept {
    if (state.battery_state == 0) {
        std::snprintf(out_icon, icon_cap, "ac-adapter");
    } else if (state.battery_state == 2) {
        std::snprintf(out_icon, icon_cap, "battery-charging");
    } else {
        if (state.battery_percent >= 80) {
            std::snprintf(out_icon, icon_cap, "battery-good");
        } else if (state.battery_percent >= 40) {
            std::snprintf(out_icon, icon_cap, "battery-medium");
        } else if (state.battery_percent >= 20) {
            std::snprintf(out_icon, icon_cap, "battery-low");
        } else {
            std::snprintf(out_icon, icon_cap, "battery-caution");
        }
    }
}

bool TrayClient::send_daemon_command(const char* cmd) noexcept {
    if (!cmd) return false;
    int fd = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return false;

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, "/tmp/wattcurb_lock.sock", sizeof(addr.sun_path) - 1);

    size_t len = std::strlen(cmd);
    ssize_t sent = ::sendto(fd, cmd, len, 0, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    ::close(fd);
    return (sent > 0);
}

void TrayClient::cycle_power_profile() noexcept {
    ipc::WattCurbSharedState state{};
    read_state(state);

    auto next_mode = static_cast<uint8_t>((state.power_profile_mode + 1) % 3);
    const char* cmd = "PROFILE 0\n";
    if (next_mode == 1) cmd = "PROFILE 1\n";
    else if (next_mode == 2) cmd = "PROFILE 2\n";

    send_daemon_command(cmd);
}

bool TrayClient::setup_dbus() noexcept {
    int r = sd_bus_default_user(&bus_);
    if (r < 0) return false;

    std::snprintf(service_name_, sizeof(service_name_), "org.kde.StatusNotifierItem-%d-1", ::getpid());
    r = sd_bus_request_name(bus_, service_name_, 0);
    if (r < 0) return false;

    r = sd_bus_add_object_vtable(bus_, &slot_, "/StatusNotifierItem", "org.kde.StatusNotifierItem", sni_vtable, this);
    if (r < 0) return false;

    return true;
}

bool TrayClient::register_with_watcher() noexcept {
    if (!bus_) return false;
    // RegisterStatusNotifierItem(QString service)
    int r = sd_bus_call_method(
        bus_,
        "org.kde.StatusNotifierWatcher",
        "/StatusNotifierWatcher",
        "org.kde.StatusNotifierWatcher",
        "RegisterStatusNotifierItem",
        nullptr,
        nullptr,
        "s",
        service_name_
    );
    return (r >= 0);
}

bool TrayClient::initialize() noexcept {
    setup_shm(); // Non-fatal: if daemon is not yet running, local defaults apply
    if (!setup_dbus()) return false;
    register_with_watcher(); // Non-fatal if watcher is starting up
    return true;
}

int TrayClient::run() noexcept {
    running_ = true;
    while (running_) {
        int r = sd_bus_process(bus_, nullptr);
        if (r < 0) break;
        if (r > 0) continue; // More work to do immediately

        // Pure event-driven sleep in epoll: 0% CPU consumption
        r = sd_bus_wait(bus_, static_cast<uint64_t>(-1));
        if (r < 0 && r != -EINTR) break;
    }
    return 0;
}

void TrayClient::stop() noexcept {
    running_ = false;
    if (bus_) {
        sd_bus_close(bus_);
    }
}

// D-Bus Property & Method Callbacks
int TrayClient::property_get_category(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply, void*, sd_bus_error*) {
    return sd_bus_message_append(reply, "s", "Hardware");
}

int TrayClient::property_get_id(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply, void*, sd_bus_error*) {
    return sd_bus_message_append(reply, "s", "wattcurb");
}

int TrayClient::property_get_title(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply, void*, sd_bus_error*) {
    return sd_bus_message_append(reply, "s", "WattCurb Power Monitor");
}

int TrayClient::property_get_status(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply, void*, sd_bus_error*) {
    return sd_bus_message_append(reply, "s", "Active");
}

int TrayClient::property_get_item_is_menu(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply, void*, sd_bus_error*) {
    return sd_bus_message_append(reply, "b", 0);
}

int TrayClient::property_get_icon_name(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply, void* userdata, sd_bus_error*) {
    auto* self = static_cast<TrayClient*>(userdata);
    ipc::WattCurbSharedState state{};
    self->read_state(state);

    char icon[32]{};
    resolve_icon_name(state, icon, sizeof(icon));
    return sd_bus_message_append(reply, "s", icon);
}

int TrayClient::property_get_tooltip(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply, void* userdata, sd_bus_error*) {
    auto* self = static_cast<TrayClient*>(userdata);
    ipc::WattCurbSharedState state{};
    self->read_state(state);

    char icon[32]{};
    resolve_icon_name(state, icon, sizeof(icon));

    char title[64]{};
    char desc[512]{};
    render_tooltip(state, title, sizeof(title), desc, sizeof(desc));

    // Open structure (sa(iiay)ss)
    int r = sd_bus_message_open_container(reply, 'r', "sa(iiay)ss");
    if (r < 0) return r;

    // icon_name (s)
    r = sd_bus_message_append(reply, "s", icon);
    if (r < 0) return r;

    // icon_data a(iiay) -> empty array
    r = sd_bus_message_open_container(reply, 'a', "(iiay)");
    if (r < 0) return r;
    r = sd_bus_message_close_container(reply);
    if (r < 0) return r;

    // title (s)
    r = sd_bus_message_append(reply, "s", title);
    if (r < 0) return r;

    // description (s)
    r = sd_bus_message_append(reply, "s", desc);
    if (r < 0) return r;

    return sd_bus_message_close_container(reply);
}

int TrayClient::property_get_icon_theme_path(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply, void*, sd_bus_error*) {
    return sd_bus_message_append(reply, "s", "");
}

int TrayClient::method_activate(sd_bus_message*, void* userdata, sd_bus_error*) {
    auto* self = static_cast<TrayClient*>(userdata);
    self->cycle_power_profile();
    return 0;
}

int TrayClient::method_context_menu(sd_bus_message*, void* userdata, sd_bus_error*) {
    auto* self = static_cast<TrayClient*>(userdata);
    (void)self;
    // Request instant on-demand observation from daemon
    send_daemon_command("RESCAN\n");
    return 0;
}

int TrayClient::method_noop(sd_bus_message*, void*, sd_bus_error*) {
    return 0;
}

} // namespace wattcurb::tray
