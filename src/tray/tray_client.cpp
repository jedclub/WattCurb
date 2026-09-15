#include "tray/tray_client.hpp"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
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
    SD_BUS_PROPERTY("IconPixmap", "a(iiay)", TrayClient::property_get_icon_pixmap, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("IconThemePath", "s", TrayClient::property_get_icon_theme_path, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Menu", "o", TrayClient::property_get_menu, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("ItemIsMenu", "b", TrayClient::property_get_item_is_menu, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("WindowId", "i", TrayClient::property_get_window_id, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("ToolTip", "(sa(iiay)ss)", TrayClient::property_get_tooltip, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("XAyatanaLabel", "s", TrayClient::property_get_xayatana_label, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("XAyatanaLabelGuide", "s", TrayClient::property_get_xayatana_label_guide, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("XAyatanaOrderingIndex", "u", nullptr, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_METHOD("Activate", "ii", nullptr, TrayClient::method_activate, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("ContextMenu", "ii", nullptr, TrayClient::method_context_menu, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("SecondaryActivate", "ii", nullptr, TrayClient::method_noop, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Scroll", "is", nullptr, TrayClient::method_noop, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("NewTitle", nullptr, 0),
    SD_BUS_SIGNAL("NewIcon", nullptr, 0),
    SD_BUS_SIGNAL("NewToolTip", nullptr, 0),
    SD_BUS_SIGNAL("NewStatus", "s", 0),
    SD_BUS_SIGNAL("XAyatanaNewLabel", "ss", 0),
    SD_BUS_VTABLE_END
};

static const sd_bus_vtable dbusmenu_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("Version", "u", TrayClient::dbusmenu_property_get_version, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Status", "s", TrayClient::dbusmenu_property_get_status, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_METHOD("GetLayout", "iias", "u(ia{sv}av)", TrayClient::dbusmenu_method_get_layout, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Event", "isvu", nullptr, TrayClient::dbusmenu_method_event, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("AboutToShow", "i", "b", TrayClient::dbusmenu_method_about_to_show, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("LayoutUpdated", "ui", 0),
    SD_BUS_VTABLE_END
};

} // anonymous namespace

TrayClient::~TrayClient() noexcept {
    cleanup();
}

void TrayClient::cleanup() noexcept {
    if (menu_slot_) {
        sd_bus_slot_unref(menu_slot_);
        menu_slot_ = nullptr;
    }
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
    bool ok = shm_state_->read_atomic(out);
    if (ok && local_override_mode_ >= 0) {
        out.power_profile_mode = static_cast<uint8_t>(local_override_mode_);
    }
    return ok;
}

void TrayClient::render_tooltip(
    const ipc::WattCurbSharedState& state,
    char* out_title, size_t title_cap,
    char* out_desc, size_t desc_cap
) noexcept {
    // Ultra-fast integer fixed-point formatting: eliminates heavy float snprintf overhead
    unsigned int sys_w = state.system_drain_mw / 1000;
    unsigned int sys_frac = (state.system_drain_mw % 1000) / 100;

    const char* status_kr = state.battery_state == 1 ? "방전 중" : 
                           (state.battery_state == 2 ? "AC 패스스루" : "AC 충전 중");

    std::snprintf(out_title, title_cap, "⚡ WattCurb: %u.%u W (%s)", sys_w, sys_frac, status_kr);

    const char* profile_name = "Performance (고성능 4.1G)";
    if (state.power_profile_mode == 1) profile_name = "Balanced (기본 균형)";
    else if (state.power_profile_mode == 2) profile_name = "SmartSave (스마트 절전 1.7G)";
    else if (state.power_profile_mode == 3) profile_name = "UltraSave (극저전력 1.4G)";

    unsigned int cpu_w = state.cpu_drain_mw / 1000;
    unsigned int cpu_frac = (state.cpu_drain_mw % 1000) / 100;
    unsigned int gpu_w = state.gpu_drain_mw / 1000;
    unsigned int gpu_frac = (state.gpu_drain_mw % 1000) / 100;

    // Platform & DRAM / I/O derived power
    uint32_t comp_drain = state.cpu_drain_mw + state.gpu_drain_mw;
    uint32_t plat_drain = state.system_drain_mw > comp_drain ? (state.system_drain_mw - comp_drain) : 0;
    unsigned int plat_w = plat_drain / 1000;
    unsigned int plat_frac = (plat_drain % 1000) / 100;

    // Remaining time format
    char time_buf[32]{};
    if (state.battery_state == 1) {
        if (state.time_to_empty_min >= 60) {
            std::snprintf(time_buf, sizeof(time_buf), "%u시간 %02u분 남음", state.time_to_empty_min / 60, state.time_to_empty_min % 60);
        } else if (state.time_to_empty_min > 0) {
            std::snprintf(time_buf, sizeof(time_buf), "%u분 남음", state.time_to_empty_min);
        } else {
            std::snprintf(time_buf, sizeof(time_buf), "측정 중...");
        }
    } else if (state.battery_state == 2) {
        std::snprintf(time_buf, sizeof(time_buf), "마모 방지 0%%");
    } else {
        std::snprintf(time_buf, sizeof(time_buf), "완충 시 자동보호");
    }

    // Dynamic SVG Battery Gauge
    unsigned int bat_w = std::clamp(static_cast<unsigned int>(state.battery_percent) * 260 / 100, 0u, 260u);
    const char* bat_color = (state.battery_percent >= 60) ? "#10b981" :
                           ((state.battery_percent >= 25) ? "#f59e0b" : "#ef4444");
    const char* bat_svg_color = (state.battery_percent >= 60) ? "%2310b981" :
                               ((state.battery_percent >= 25) ? "%23f59e0b" : "%23ef4444");
    char bat_svg[512]{};
    std::snprintf(bat_svg, sizeof(bat_svg),
        "<img src=\"data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='260' height='10'><rect width='260' height='10' fill='%%230f172a' rx='2'/><rect width='%u' height='10' fill='%s' rx='2'/></svg>\"/>",
        bat_w, bat_svg_color);

    const char* temp_color = (state.cpu_temp_c < 65) ? "#00f0ff" :
                            ((state.cpu_temp_c < 80) ? "#fb923c" : "#f43f5e");

    const char* gpu_status = (state.gpu_drain_mw > 500) ? "활성 렌더링" : "절전 대기 (D3Cold)";

    // Top process format strings with SVG share bars
    char top1_buf[1024]{};
    char top2_buf[1024]{};
    if (state.culprits[0].comm[0]) {
        unsigned int c1_w = state.culprits[0].drain_mw / 1000;
        unsigned int c1_f = (state.culprits[0].drain_mw % 1000) / 100;
        unsigned int c1_pct = (state.system_drain_mw > 0) ? (state.culprits[0].drain_mw * 100 / state.system_drain_mw) : 0;
        unsigned int c1_bar_w = std::clamp(c1_pct * 140 / 100, 2u, 140u);
        std::snprintf(top1_buf, sizeof(top1_buf),
            "<tr><td width=\"36%%\"><b style=\"color: #ffffff; font-size: 11px;\">1. %s</b> <span style=\"color: #64748b; font-size: 9px;\">PID %d</span></td>"
            "<td width=\"38%%\"><img src=\"data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='140' height='7'><rect width='140' height='7' fill='%%232d1233' rx='2'/><rect width='%u' height='7' fill='%%23f43f5e' rx='2'/></svg>\"/></td>"
            "<td align=\"right\" width=\"26%%\"><b style=\"color: #f43f5e; font-size: 11px;\">%u.%u W</b> <span style=\"color: #cbd5e1; font-size: 10px;\">(%u%%)</span></td></tr>",
            state.culprits[0].comm, state.culprits[0].pid, c1_bar_w, c1_w, c1_f, c1_pct);
    } else {
        std::snprintf(top1_buf, sizeof(top1_buf),
            "<tr><td colspan=\"3\"><span style=\"color: #94a3b8; font-size: 10px;\"><i>시스템 안정 상태 (과다 점유 프로세스 없음)</i></span></td></tr>");
    }

    if (state.culprits[1].comm[0]) {
        unsigned int c2_w = state.culprits[1].drain_mw / 1000;
        unsigned int c2_f = (state.culprits[1].drain_mw % 1000) / 100;
        unsigned int c2_pct = (state.system_drain_mw > 0) ? (state.culprits[1].drain_mw * 100 / state.system_drain_mw) : 0;
        unsigned int c2_bar_w = std::clamp(c2_pct * 140 / 100, 2u, 140u);
        std::snprintf(top2_buf, sizeof(top2_buf),
            "<tr><td><b style=\"color: #ffffff; font-size: 11px;\">2. %s</b> <span style=\"color: #64748b; font-size: 9px;\">PID %d</span></td>"
            "<td><img src=\"data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='140' height='7'><rect width='140' height='7' fill='%%232d1233' rx='2'/><rect width='%u' height='7' fill='%%23fb923c' rx='2'/></svg>\"/></td>"
            "<td align=\"right\"><b style=\"color: #fb923c; font-size: 11px;\">%u.%u W</b> <span style=\"color: #cbd5e1; font-size: 10px;\">(%u%%)</span></td></tr>",
            state.culprits[1].comm, state.culprits[1].pid, c2_bar_w, c2_w, c2_f, c2_pct);
    }

    char mitig_buf[64]{};
    if (state.power_profile_mode == 0) {
        std::snprintf(mitig_buf, sizeof(mitig_buf), "전면 개방 (4.1G 풀파워 언락)");
    } else if (state.active_mitigations > 0) {
        std::snprintf(mitig_buf, sizeof(mitig_buf), "실시간 가동 중 (%u개 제어)", state.active_mitigations);
    } else {
        std::snprintf(mitig_buf, sizeof(mitig_buf), "Zero-Wakeup ACTIVE");
    }

    std::snprintf(out_desc, desc_cap,
        "<table width=\"100%%\" cellspacing=\"0\" cellpadding=\"0\" style=\"border-collapse: collapse; font-family: 'Segoe UI', Noto Sans, sans-serif;\">"
        "<tr><td style=\"padding-bottom: 4px;\">"
        "<table width=\"100%%\" cellspacing=\"0\" cellpadding=\"0\"><tr>"
        "<td><span style=\"color: #00f0ff; font-weight: 900; font-size: 13px; letter-spacing: 1px;\">⚡ WATTCURB // CYBER HUD</span>"
        "&nbsp;<span style=\"color: #10b981; font-weight: bold; font-size: 9px; background: #064e3b; padding: 2px 5px; border-radius: 3px;\">● REALTIME</span></td>"
        "<td align=\"right\"><span style=\"color: #64748b; font-size: 10px; font-weight: bold;\">TOTAL: </span>"
        "<span style=\"color: #f59e0b; font-size: 15px; font-weight: 900;\">%u.%u W</span></td>"
        "</tr></table>"
        "</td></tr>"
        "<tr><td style=\"background: #111a2e; padding: 5px 8px; border: 1px solid #1e3a5f; border-radius: 5px;\">"
        "<table width=\"100%%\" cellspacing=\"0\" cellpadding=\"0\"><tr>"
        "<td width=\"65%%\">"
        "<span style=\"color: #64748b; font-size: 9px; font-weight: bold;\">⚡ WattCurb Power Intelligence &nbsp;•&nbsp; 배터리</span><br/>"
        "%s"
        "</td>"
        "<td align=\"right\" valign=\"center\">"
        "<b style=\"color: %s; font-size: 13px;\">%u%%</b>"
        "<span style=\"color: #38bdf8; font-size: 10px; font-weight: bold;\"> &nbsp;%s</span><br/>"
        "<span style=\"color: #94a3b8; font-size: 9px;\">건강도 %u%% · %s</span>"
        "</td>"
        "</tr></table>"
        "</td></tr>"
        "<tr><td height=\"4\"></td></tr>"
        "<tr><td>"
        "<table width=\"100%%\" cellspacing=\"2\" cellpadding=\"3\" style=\"border-collapse: separate;\">"
        "<tr>"
        "<td width=\"50%%\" style=\"background: #0d1527; border: 1px solid #1e293b; border-left: 3px solid #00f0ff; border-radius: 4px;\">"
        "<span style=\"color: #64748b; font-size: 9px; font-weight: bold;\">CPU 연산 도메인</span><br/>"
        "<b style=\"color: #00f0ff; font-size: 13px;\">%u.%u W</b>"
        "<span style=\"color: %s; font-size: 10px; font-weight: bold;\"> &nbsp;%u°C</span>"
        "<span style=\"color: #64748b; font-size: 10px;\"> · %u RPM</span>"
        "</td>"
        "<td width=\"50%%\" style=\"background: #0d1527; border: 1px solid #1e293b; border-left: 3px solid #10b981; border-radius: 4px;\">"
        "<span style=\"color: #64748b; font-size: 9px; font-weight: bold;\">GPU 실리콘 도메인</span><br/>"
        "<b style=\"color: #10b981; font-size: 13px;\">%u.%u W</b>"
        "<span style=\"color: #38bdf8; font-size: 10px; font-weight: bold;\"> &nbsp;%s</span>"
        "</td>"
        "</tr>"
        "<tr>"
        "<td width=\"50%%\" style=\"background: #0d1527; border: 1px solid #1e293b; border-left: 3px solid #a855f7; border-radius: 4px;\">"
        "<span style=\"color: #64748b; font-size: 9px; font-weight: bold;\">플랫폼 & DRAM / IO</span><br/>"
        "<b style=\"color: #e2e8f0; font-size: 13px;\">%u.%u W</b>"
        "<span style=\"color: #64748b; font-size: 10px;\"> &nbsp;LPDDR5X / APST</span>"
        "</td>"
        "<td width=\"50%%\" style=\"background: #0d1527; border: 1px solid #1e293b; border-left: 3px solid #6366f1; border-radius: 4px;\">"
        "<span style=\"color: #64748b; font-size: 9px; font-weight: bold;\">심층 수면 & 웨이크업</span><br/>"
        "<b style=\"color: #a855f7; font-size: 13px;\">%u%% C3</b>"
        "<span style=\"color: #64748b; font-size: 10px;\"> &nbsp;%u/s · RAPL</span>"
        "</td>"
        "</tr>"
        "</table>"
        "</td></tr>"
        "<tr><td height=\"4\"></td></tr>"
        "<tr><td style=\"padding: 5px 8px; background: #160d24; border: 1px solid #4a1d48; border-radius: 5px;\">"
        "<span style=\"color: #f43f5e; font-size: 9px; font-weight: 900; letter-spacing: 0.5px;\">⚡ ENERGY DRAIN CULPRITS (실시간 최다 소비)</span>"
        "<table width=\"100%%\" cellspacing=\"0\" cellpadding=\"1\" style=\"margin-top: 2px;\">"
        "%s"
        "%s"
        "</table>"
        "</td></tr>"
        "<tr><td height=\"4\"></td></tr>"
        "<tr><td style=\"padding: 2px 4px;\">"
        "<table width=\"100%%\" cellspacing=\"0\" cellpadding=\"0\"><tr>"
        "<td><span style=\"color: #64748b; font-size: 9px; font-weight: bold;\">PROFILE: </span>"
        "<b style=\"color: #00f0ff; font-size: 10px;\">%s</b></td>"
        "<td align=\"right\"><span style=\"color: #64748b; font-size: 9px; font-weight: bold;\">ENGINE: </span>"
        "<b style=\"color: #10b981; font-size: 10px;\">%s</b></td>"
        "</tr></table>"
        "</td></tr>"
        "</table>",
        sys_w, sys_frac,
        bat_svg,
        bat_color, state.battery_percent, status_kr, state.battery_health_percent, time_buf,
        cpu_w, cpu_frac, temp_color, state.cpu_temp_c, state.fan_rpm,
        gpu_w, gpu_frac, gpu_status,
        plat_w, plat_frac,
        state.cstate_c3_percent, state.wakeups_per_sec,
        top1_buf, top2_buf,
        profile_name, mitig_buf
    );
}

void TrayClient::resolve_icon_name(
    const ipc::WattCurbSharedState& state,
    char* out_icon, size_t icon_cap
) noexcept {
    const char* prof = "balanced";
    if (state.power_profile_mode == 0) {
        prof = "performance";
    } else if (state.power_profile_mode == 2 || state.power_profile_mode == 3) {
        prof = "powersave";
    }

    uint8_t pct = state.battery_percent;
    if (pct > 100) pct = 100;

    // Quantize to nearest 10% step (000, 010, 020, ..., 100) matching KDE Breeze SVG assets
    unsigned int rounded = ((static_cast<unsigned int>(pct) + 5) / 10) * 10;
    if (rounded > 100) rounded = 100;

    if (state.battery_state == 2 || state.battery_state == 0) {
        // Charging or AC direct
        std::snprintf(out_icon, icon_cap, "battery-%03u-charging-profile-%s", rounded, prof);
    } else {
        // Discharging on battery
        std::snprintf(out_icon, icon_cap, "battery-%03u-profile-%s", rounded, prof);
    }
}

static void apply_hardware_profile(const char* mode) noexcept {
    if (!mode) return;
    const char* home = ::getenv("HOME");
    if (home) {
        char path[256];
        std::snprintf(path, sizeof(path), "%s/.cache/power_profile_mode", home);
        int fd = ::open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd >= 0) {
            ::write(fd, mode, std::strlen(mode));
            ::write(fd, "\n", 1);
            ::close(fd);
        }
    }

    if (::access("/home/jedclub/.local/bin/power-profile-manager", X_OK) == 0) {
        pid_t pid = ::fork();
        if (pid == 0) {
            ::execl("/home/jedclub/.local/bin/power-profile-manager", "power-profile-manager", mode, "--internal", nullptr);
            ::_exit(0);
        }
    }
}

bool TrayClient::send_daemon_command(const char* cmd) noexcept {
    if (!cmd) return false;
    int fd = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return false;

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    addr.sun_path[0] = '\0';
    const char lock_name[] = "wattcurb.lock";
    std::memcpy(addr.sun_path + 1, lock_name, sizeof(lock_name) - 1);
    socklen_t addr_len = static_cast<socklen_t>(sizeof(sa_family_t) + 1 + sizeof(lock_name) - 1);

    size_t len = std::strlen(cmd);
    ssize_t sent = ::sendto(fd, cmd, len, 0, reinterpret_cast<struct sockaddr*>(&addr), addr_len);
    ::close(fd);
    return (sent > 0);
}

void TrayClient::cycle_power_profile() noexcept {
    ipc::WattCurbSharedState state{};
    read_state(state);

    auto next_mode = static_cast<uint8_t>((state.power_profile_mode + 1) % 4);
    char cmd[16];
    std::snprintf(cmd, sizeof(cmd), "PROFILE %u\n", next_mode);
    send_daemon_command(cmd);

    const char* hw_mode = "performance";
    if (next_mode == 1) hw_mode = "balanced";
    else if (next_mode == 2) hw_mode = "save";
    else if (next_mode == 3) hw_mode = "ultra";
    apply_hardware_profile(hw_mode);
}

bool TrayClient::setup_dbus() noexcept {
    int r = sd_bus_default_user(&bus_);
    if (r < 0) return false;

    std::snprintf(service_name_, sizeof(service_name_), "org.kde.StatusNotifierItem-%d-1", ::getpid());
    r = sd_bus_request_name(bus_, service_name_, 0);
    if (r < 0) return false;

    r = sd_bus_add_object_vtable(bus_, &slot_, "/StatusNotifierItem", "org.kde.StatusNotifierItem", sni_vtable, this);
    if (r < 0) return false;

    r = sd_bus_add_object_vtable(bus_, &menu_slot_, "/MenuBar", "com.canonical.dbusmenu", dbusmenu_vtable, this);
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
    if (r >= 0) {
        // Emit initial change signals to trigger plasmashell to query properties immediately
        sd_bus_emit_signal(bus_, "/StatusNotifierItem", "org.kde.StatusNotifierItem", "NewIcon", nullptr);
        sd_bus_emit_signal(bus_, "/StatusNotifierItem", "org.kde.StatusNotifierItem", "NewStatus", "s", "Active");
        sd_bus_emit_signal(bus_, "/StatusNotifierItem", "org.kde.StatusNotifierItem", "NewToolTip", nullptr);
        sd_bus_flush(bus_);
    }
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
    ipc::WattCurbSharedState prev_state{};
    read_state(prev_state);

    while (running_) {
        int r = sd_bus_process(bus_, nullptr);
        if (r < 0) break;
        if (r > 0) continue; // More work to do immediately

        // Sleep with 3-second timeout (3'000'000 us):
        // Wakes up on D-Bus events instantly, or at least once every 3s to sync live telemetry
        r = sd_bus_wait(bus_, 3'000'000ULL);
        if (r < 0 && r != -EINTR) break;

        // Periodic state check: read 128-byte Seqlock SHM (< 50ns, zero-allocation)
        ipc::WattCurbSharedState cur_state{};
        if (read_state(cur_state)) {
            bool changed = (cur_state.battery_percent != prev_state.battery_percent) ||
                           (cur_state.battery_state != prev_state.battery_state) ||
                           (std::abs(static_cast<int>(cur_state.system_drain_mw) - static_cast<int>(prev_state.system_drain_mw)) > 200) ||
                           (cur_state.power_profile_mode != prev_state.power_profile_mode);

            if (changed) {
                prev_state = cur_state;
                sd_bus_emit_signal(bus_, "/StatusNotifierItem", "org.kde.StatusNotifierItem", "NewIcon", nullptr);
                sd_bus_emit_signal(bus_, "/StatusNotifierItem", "org.kde.StatusNotifierItem", "NewToolTip", nullptr);

                char label[32]{};
                unsigned int sys_w = cur_state.system_drain_mw / 1000;
                unsigned int sys_frac = (cur_state.system_drain_mw % 1000) / 100;
                char sign = (cur_state.battery_state == 2) ? '+' : '-';
                std::snprintf(label, sizeof(label), "%u%% (%c%u.%uW)", cur_state.battery_percent, sign, sys_w, sys_frac);
                sd_bus_emit_signal(bus_, "/StatusNotifierItem", "org.kde.StatusNotifierItem", "XAyatanaNewLabel", "ss", label, "");
                sd_bus_emit_signal(bus_, "/MenuBar", "com.canonical.dbusmenu", "LayoutUpdated", "ui", ++menu_revision_, 0);
            }
        }
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
    return sd_bus_message_append(reply, "s", "ApplicationStatus");
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

    char icon[64]{};
    resolve_icon_name(state, icon, sizeof(icon));
    return sd_bus_message_append(reply, "s", icon);
}

int TrayClient::property_get_tooltip(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply, void* userdata, sd_bus_error*) {
    auto* self = static_cast<TrayClient*>(userdata);
    ipc::WattCurbSharedState state{};
    self->read_state(state);

    char icon[64]{};
    resolve_icon_name(state, icon, sizeof(icon));

    char title[128]{};
    char desc[8192]{};
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

int TrayClient::property_get_icon_pixmap(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply, void*, sd_bus_error*) {
    int r = sd_bus_message_open_container(reply, 'a', "(iiay)");
    if (r < 0) return r;
    return sd_bus_message_close_container(reply);
}

int TrayClient::property_get_menu(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply, void*, sd_bus_error*) {
    return sd_bus_message_append(reply, "o", "/MenuBar");
}

int TrayClient::property_get_window_id(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply, void*, sd_bus_error*) {
    return sd_bus_message_append(reply, "i", 0);
}

int TrayClient::property_get_xayatana_label(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply, void* userdata, sd_bus_error*) {
    auto* self = static_cast<TrayClient*>(userdata);
    ipc::WattCurbSharedState state{};
    self->read_state(state);

    char label[32]{};
    unsigned int sys_w = state.system_drain_mw / 1000;
    unsigned int sys_frac = (state.system_drain_mw % 1000) / 100;
    char sign = (state.battery_state == 2) ? '+' : '-';
    std::snprintf(label, sizeof(label), "%u%% (%c%u.%uW)", state.battery_percent, sign, sys_w, sys_frac);
    return sd_bus_message_append(reply, "s", label);
}

int TrayClient::property_get_xayatana_label_guide(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply, void*, sd_bus_error*) {
    return sd_bus_message_append(reply, "s", " 100% (+00.0W)");
}

int TrayClient::method_activate(sd_bus_message*, void* userdata, sd_bus_error*) {
    auto* self = static_cast<TrayClient*>(userdata);
    // Implements REF-REQ-045: Explicit Menu Activation for Matrix Dashboard
    // The intensive Qt Quick Matrix Dashboard must NEVER be triggered by simple left-clicking.
    // Left-clicking cycles power profiles cleanly as originally specified in REF-REQ-035.
    if (self) {
        self->cycle_power_profile();
        if (self->bus_) {
            sd_bus_emit_signal(self->bus_, "/StatusNotifierItem", "org.kde.StatusNotifierItem", "NewIcon", nullptr);
            sd_bus_emit_signal(self->bus_, "/StatusNotifierItem", "org.kde.StatusNotifierItem", "NewToolTip", nullptr);
            sd_bus_emit_signal(self->bus_, "/StatusNotifierItem", "org.kde.StatusNotifierItem", "XAyatanaNewLabel", nullptr);
            sd_bus_emit_signal(self->bus_, "/MenuBar", "com.canonical.dbusmenu", "LayoutUpdated", "ui", ++self->menu_revision_, 0);
        }
    }
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

// com.canonical.dbusmenu VTable Implementation
int TrayClient::dbusmenu_property_get_version(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply, void*, sd_bus_error*) {
    return sd_bus_message_append(reply, "u", 3);
}

int TrayClient::dbusmenu_property_get_status(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply, void*, sd_bus_error*) {
    return sd_bus_message_append(reply, "s", "normal");
}

int TrayClient::dbusmenu_method_about_to_show(sd_bus_message* msg, void*, sd_bus_error*) {
    // Return true (1) so KDE Plasma always queries the latest GetLayout
    return sd_bus_reply_method_return(msg, "b", 1);
}

static void append_menu_node(
    sd_bus_message* reply,
    int id,
    const char* label,
    bool enabled,
    const char* type,
    const char* toggle_type,
    int toggle_state
) {
    sd_bus_message_open_container(reply, 'r', "ia{sv}av");
    sd_bus_message_append(reply, "i", id);

    sd_bus_message_open_container(reply, 'a', "{sv}");
    if (label) {
        sd_bus_message_open_container(reply, 'e', "sv");
        sd_bus_message_append(reply, "s", "label");
        sd_bus_message_open_container(reply, 'v', "s");
        sd_bus_message_append(reply, "s", label);
        sd_bus_message_close_container(reply);
        sd_bus_message_close_container(reply);
    }
    if (!enabled) {
        sd_bus_message_open_container(reply, 'e', "sv");
        sd_bus_message_append(reply, "s", "enabled");
        sd_bus_message_open_container(reply, 'v', "b");
        sd_bus_message_append(reply, "b", 0);
        sd_bus_message_close_container(reply);
        sd_bus_message_close_container(reply);
    }
    if (type) {
        sd_bus_message_open_container(reply, 'e', "sv");
        sd_bus_message_append(reply, "s", "type");
        sd_bus_message_open_container(reply, 'v', "s");
        sd_bus_message_append(reply, "s", type);
        sd_bus_message_close_container(reply);
        sd_bus_message_close_container(reply);
    }
    if (toggle_type) {
        sd_bus_message_open_container(reply, 'e', "sv");
        sd_bus_message_append(reply, "s", "toggle-type");
        sd_bus_message_open_container(reply, 'v', "s");
        sd_bus_message_append(reply, "s", toggle_type);
        sd_bus_message_close_container(reply);
        sd_bus_message_close_container(reply);

        sd_bus_message_open_container(reply, 'e', "sv");
        sd_bus_message_append(reply, "s", "toggle-state");
        sd_bus_message_open_container(reply, 'v', "i");
        sd_bus_message_append(reply, "i", toggle_state);
        sd_bus_message_close_container(reply);
        sd_bus_message_close_container(reply);
    }
    sd_bus_message_close_container(reply); // a{sv}

    sd_bus_message_open_container(reply, 'a', "v"); // empty children
    sd_bus_message_close_container(reply);

    sd_bus_message_close_container(reply); // r
}

int TrayClient::dbusmenu_method_get_layout(sd_bus_message* msg, void* userdata, sd_bus_error*) {
    auto* self = static_cast<TrayClient*>(userdata);
    ipc::WattCurbSharedState state{};
    self->read_state(state);

    sd_bus_message* reply = nullptr;
    int r = sd_bus_message_new_method_return(msg, &reply);
    if (r < 0) return r;

    // uint revision
    sd_bus_message_append(reply, "u", self->menu_revision_);

    // root node: (ia{sv}av)
    sd_bus_message_open_container(reply, 'r', "ia{sv}av");
    sd_bus_message_append(reply, "i", 0); // id=0

    sd_bus_message_open_container(reply, 'a', "{sv}");
    sd_bus_message_close_container(reply); // empty root props

    sd_bus_message_open_container(reply, 'a', "v"); // children

    char h1[128]{}, h2[128]{}, h3[128]{};
    unsigned int sys_w = state.system_drain_mw / 1000;
    unsigned int sys_frac = (state.system_drain_mw % 1000) / 100;
    const char* status_str = state.battery_state == 1 ? "On Battery" : (state.battery_state == 2 ? "Charging" : "AC Passthrough");
    char sign = (state.battery_state == 2) ? '+' : '-';

    std::snprintf(h1, sizeof(h1), "⚡ %u%% (Est: %u min) | %c%u.%u W (%s)",
        state.battery_percent, state.time_to_empty_min, sign, sys_w, sys_frac, status_str);

    std::snprintf(h2, sizeof(h2), "🔋 Health: %u%% | CPU: %u.%u W (%u°C, Fan %u RPM)",
        state.battery_health_percent, state.cpu_drain_mw / 1000, (state.cpu_drain_mw % 1000) / 100,
        state.cpu_temp_c, state.fan_rpm);

    std::snprintf(h3, sizeof(h3), "🔥 Top: %s (%u mW) | %s (%u mW)",
        state.culprits[0].comm[0] ? state.culprits[0].comm : "none", state.culprits[0].drain_mw,
        state.culprits[1].comm[0] ? state.culprits[1].comm : "none", state.culprits[1].drain_mw);

    auto add_item = [&](int id, const char* label, bool enabled = true, const char* type = nullptr, const char* toggle_type = nullptr, int toggle_state = 0) {
        sd_bus_message_open_container(reply, 'v', "(ia{sv}av)");
        append_menu_node(reply, id, label, enabled, type, toggle_type, toggle_state);
        sd_bus_message_close_container(reply);
    };

    uint8_t cur_mode = state.power_profile_mode;

    add_item(1, h1, false);
    add_item(2, h2, false);
    add_item(3, h3, false);
    add_item(4, nullptr, true, "separator");
    add_item(5, "Performance (고성능 모드 - 4.1GHz Boost)", true, nullptr, "radio", (cur_mode == 0 ? 1 : 0));
    add_item(6, "Balanced (균형 모드 - 기본 권장)", true, nullptr, "radio", (cur_mode == 1 ? 1 : 0));
    add_item(7, "Smart Save (스마트 절전 모드 - 1.7GHz)", true, nullptr, "radio", (cur_mode == 2 ? 1 : 0));
    add_item(8, "Ultra Save (초절전 모드 - 1.4GHz, 48Hz)", true, nullptr, "radio", (cur_mode == 3 ? 1 : 0));
    add_item(9, nullptr, true, "separator");
    add_item(10, "📈 정밀 분석 매트릭 창 열기 (Matrix Dashboard)");
    add_item(11, "📊 KDE 시스템 모니터 열기 (System Monitor)");

    sd_bus_message_close_container(reply); // children av
    sd_bus_message_close_container(reply); // root r

    r = sd_bus_send(self->bus_, reply, nullptr);
    sd_bus_message_unref(reply);
    return r;
}

int TrayClient::dbusmenu_method_event(sd_bus_message* msg, void* userdata, sd_bus_error*) {
    auto* self = static_cast<TrayClient*>(userdata);
    int id = 0;
    const char* event_id = nullptr;
    int r = sd_bus_message_read(msg, "is", &id, &event_id);
    if (r < 0) return r;

    if (event_id && std::strcmp(event_id, "clicked") == 0) {
        int selected_mode = -1;
        const char* hw_mode = nullptr;

        if (id == 5) {
            selected_mode = 0;
            hw_mode = "performance";
        } else if (id == 6) {
            selected_mode = 1;
            hw_mode = "balanced";
        } else if (id == 7) {
            selected_mode = 2;
            hw_mode = "save";
        } else if (id == 8) {
            selected_mode = 3;
            hw_mode = "ultra";
        } else if (id == 10) {
            // Non-blocking asynchronous daemon rescan trigger
            send_daemon_command("RESCAN\n");

            // Instant zero-delay popup of native KDE Plasma 6 Dashboard (REF-REQ-036)
            pid_t pid = ::fork();
            if (pid == 0) {
                ::setsid();
                ::system("pkill -f wattcurb-dashboard 2>/dev/null");
                const char* dash_bin = "/home/jedclub/.local/bin/wattcurb-dashboard";
                if (::access(dash_bin, X_OK) == 0) {
                    ::execl(dash_bin, "wattcurb-dashboard", nullptr);
                } else {
                    ::execlp("wattcurb-dashboard", "wattcurb-dashboard", nullptr);
                }
                ::_exit(0);
            }

            // Also emit signals immediately to update tray
            sd_bus_emit_signal(self->bus_, "/StatusNotifierItem", "org.kde.StatusNotifierItem", "NewIcon", nullptr);
            sd_bus_emit_signal(self->bus_, "/StatusNotifierItem", "org.kde.StatusNotifierItem", "NewToolTip", nullptr);
            sd_bus_emit_signal(self->bus_, "/MenuBar", "com.canonical.dbusmenu", "LayoutUpdated", "ui", ++self->menu_revision_, 0);
        } else if (id == 11) {
            if (::fork() == 0) {
                ::execlp("plasma-systemmonitor", "plasma-systemmonitor", nullptr);
                ::_exit(0);
            }
        }

        if (selected_mode >= 0) {
            self->local_override_mode_ = selected_mode;
            char cmd[16];
            std::snprintf(cmd, sizeof(cmd), "PROFILE %d\n", selected_mode);
            send_daemon_command(cmd);
            apply_hardware_profile(hw_mode);

            // Notify KDE Plasma that layout changed immediately to update radio buttons and icon
            ++self->menu_revision_;
            sd_bus_emit_signal(self->bus_, "/MenuBar", "com.canonical.dbusmenu", "LayoutUpdated", "ui", self->menu_revision_, 0);
            sd_bus_emit_signal(self->bus_, "/StatusNotifierItem", "org.kde.StatusNotifierItem", "NewIcon", nullptr);
            sd_bus_emit_signal(self->bus_, "/StatusNotifierItem", "org.kde.StatusNotifierItem", "NewToolTip", nullptr);
        }
    }
    return sd_bus_reply_method_return(msg, "");
}

} // namespace wattcurb::tray
