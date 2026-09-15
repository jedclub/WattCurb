#include "tray/tray_client.hpp"
#include "core/posix_fs.hpp"
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
    if (match_slot_) {
        sd_bus_slot_unref(match_slot_);
        match_slot_ = nullptr;
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

static void build_unicode_bar(char* out, size_t out_cap, unsigned int percent, unsigned int total_blocks) noexcept {
    if (out_cap < 1) return;
    unsigned int filled = (percent * total_blocks + 50) / 100;
    if (filled > total_blocks) filled = total_blocks;
    unsigned int empty = total_blocks - filled;

    size_t pos = 0;
    // '█' (U+2588, UTF-8: E2 96 88)
    for (unsigned int i = 0; i < filled && pos + 3 < out_cap; ++i) {
        out[pos++] = '\xe2';
        out[pos++] = '\x96';
        out[pos++] = '\x88';
    }
    // '░' (U+2591, UTF-8: E2 96 91)
    for (unsigned int i = 0; i < empty && pos + 3 < out_cap; ++i) {
        out[pos++] = '\xe2';
        out[pos++] = '\x96';
        out[pos++] = '\x91';
    }
    out[pos] = '\0';
}

// Zero-allocation UTF-8 truncation guard (REF-REQ-047)
// Ensures that if snprintf truncates in the middle of a multibyte UTF-8 character,
// the incomplete byte sequence is cleanly terminated, preventing D-Bus -EINVAL rejection.
static void sanitize_utf8_inplace(char* s) noexcept {
    if (!s) return;
    size_t len = std::strlen(s);
    if (len == 0) return;

    for (size_t lookback = 1; lookback <= 4 && lookback <= len; ++lookback) {
        unsigned char lead = static_cast<unsigned char>(s[len - lookback]);
        if ((lead & 0x80) == 0x00) {
            // Pure ASCII boundary
            break;
        }
        if ((lead & 0xC0) == 0x80) {
            // Continuation byte, keep searching backward for lead byte
            continue;
        }
        if ((lead & 0xE0) == 0xC0) {
            // 2-byte sequence expecting 2 bytes total (lookback == 2)
            if (lookback < 2) s[len - lookback] = '\0';
            break;
        }
        if ((lead & 0xF0) == 0xE0) {
            // 3-byte sequence expecting 3 bytes total (lookback == 3)
            if (lookback < 3) s[len - lookback] = '\0';
            break;
        }
        if ((lead & 0xF8) == 0xF0) {
            // 4-byte sequence expecting 4 bytes total (lookback == 4)
            if (lookback < 4) s[len - lookback] = '\0';
            break;
        }
    }
}

static void probe_live_sensors_on_hover(ipc::WattCurbSharedState& state) noexcept {
    // Implements REF-REQ-050: Sub-5us on-demand hardware telemetry probe upon mouse hover
    // 1. Live Battery Telemetry: /sys/class/power_supply/BAT0/uevent
    char ubuf[1024]{};
    ssize_t n = core::fs::read_small_file("/sys/class/power_supply/BAT0/uevent", ubuf, sizeof(ubuf) - 1);
    if (n <= 0) {
        n = core::fs::read_small_file("/sys/class/power_supply/BAT1/uevent", ubuf, sizeof(ubuf) - 1);
    }
    if (n > 0) {
        const char* p = ubuf;
        uint32_t power_now = 0;
        uint32_t current_now = 0;
        uint32_t voltage_now = 0;
        while (*p) {
            if (std::strncmp(p, "POWER_SUPPLY_POWER_NOW=", 23) == 0) {
                power_now = static_cast<uint32_t>(std::strtoul(p + 23, nullptr, 10));
            } else if (std::strncmp(p, "POWER_SUPPLY_CURRENT_NOW=", 25) == 0) {
                current_now = static_cast<uint32_t>(std::strtoul(p + 25, nullptr, 10));
            } else if (std::strncmp(p, "POWER_SUPPLY_VOLTAGE_NOW=", 25) == 0) {
                voltage_now = static_cast<uint32_t>(std::strtoul(p + 25, nullptr, 10));
            } else if (std::strncmp(p, "POWER_SUPPLY_CAPACITY=", 22) == 0) {
                uint8_t cap = static_cast<uint8_t>(std::strtoul(p + 22, nullptr, 10));
                if (cap > 0 && cap <= 100) state.battery_percent = cap;
            } else if (std::strncmp(p, "POWER_SUPPLY_STATUS=Discharging", 31) == 0) {
                state.battery_state = 1;
            } else if (std::strncmp(p, "POWER_SUPPLY_STATUS=Charging", 28) == 0) {
                state.battery_state = 0;
            } else if (std::strncmp(p, "POWER_SUPPLY_STATUS=Full", 24) == 0 ||
                       std::strncmp(p, "POWER_SUPPLY_STATUS=Not charging", 32) == 0) {
                state.battery_state = 2;
            }
            while (*p && *p != '\n') ++p;
            if (*p == '\n') ++p;
        }
        if (power_now > 0) {
            state.system_drain_mw = power_now / 1000;
        } else if (current_now > 0 && voltage_now > 0) {
            state.system_drain_mw = static_cast<uint32_t>((static_cast<uint64_t>(current_now) * voltage_now) / 1'000'000'000ULL);
        }
    }

    // 2. Live CPU Thermal Sensor: /sys/class/thermal/thermal_zone0/temp
    char tbuf[32]{};
    if (core::fs::read_small_file("/sys/class/thermal/thermal_zone0/temp", tbuf, sizeof(tbuf) - 1) > 0) {
        long temp_mc = std::strtol(tbuf, nullptr, 10);
        if (temp_mc > 0) {
            state.cpu_temp_c = static_cast<uint16_t>(temp_mc / 1000);
        }
    }

    // 3. Live CPU Core Frequency: scaling_cur_freq
    char fbuf[32]{};
    if (core::fs::read_small_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", fbuf, sizeof(fbuf) - 1) > 0) {
        long khz = std::strtol(fbuf, nullptr, 10);
        if (khz > 0) {
            state.cpu_freq_mhz = static_cast<uint16_t>(khz / 1000);
        }
    }
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
                           (state.battery_state == 2 ? "AC 직결 (완충)" : "AC 충전 중");

    std::snprintf(out_title, title_cap, "⚡ WattCurb: %u.%u W (%s)", sys_w, sys_frac, status_kr);

    const char* profile_name = "Performance (4.1GHz Boost 풀파워)";
    if (state.power_profile_mode == 1) profile_name = "Balanced (지능형 균형)";
    else if (state.power_profile_mode == 2) profile_name = "SmartSave (스마트 절전 1.7GHz)";
    else if (state.power_profile_mode == 3) profile_name = "UltraSave (극저전력 1.4GHz)";

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
    char time_buf[128]{};
    if (state.battery_state == 1) {
        if (state.time_to_empty_min >= 60) {
            std::snprintf(time_buf, sizeof(time_buf), "%u시간 %02u분 남음", state.time_to_empty_min / 60, state.time_to_empty_min % 60);
        } else if (state.time_to_empty_min > 0) {
            std::snprintf(time_buf, sizeof(time_buf), "%u분 남음", state.time_to_empty_min);
        } else {
            std::snprintf(time_buf, sizeof(time_buf), "측정 중...");
        }
    } else if (state.battery_state == 2) {
        std::snprintf(time_buf, sizeof(time_buf), "완충 AC 직결 (배터리 수명보호)");
    } else {
        std::snprintf(time_buf, sizeof(time_buf), "충전 중 (완충 시 자동보호)");
    }

    const char* bat_color = (state.battery_percent >= 60) ? "#10b981" :
                           ((state.battery_percent >= 25) ? "#f59e0b" : "#ef4444");

    const char* temp_color = (state.cpu_temp_c < 50) ? "#00f0ff" :
                            ((state.cpu_temp_c < 65) ? "#10b981" :
                            ((state.cpu_temp_c < 80) ? "#fb923c" : "#f43f5e"));

    const char* gpu_status = (state.gpu_drain_mw > 1000) ? "3D 렌더링 가속" : 
                            ((state.gpu_drain_mw > 200) ? "2D GUI 가속" : "D3Cold 초절전");

    const char* c3_status = (state.cstate_c3_percent >= 80) ? "최적 심층 수면" :
                           ((state.cstate_c3_percent >= 40) ? "정상 유휴 슬립" : "실리콘 각성 부하");

    // Progressive Unicode Visual Gauges (10 bars)
    char bat_bar[64]{};
    build_unicode_bar(bat_bar, sizeof(bat_bar), state.battery_percent, 10);

    unsigned int sys_mw = state.system_drain_mw > 0 ? state.system_drain_mw : 1;
    unsigned int cpu_pct = std::clamp(static_cast<unsigned int>(state.cpu_drain_mw * 100 / sys_mw), 0u, 100u);
    unsigned int gpu_pct = std::clamp(static_cast<unsigned int>(state.gpu_drain_mw * 100 / sys_mw), 0u, 100u);
    unsigned int plat_pct = std::clamp(static_cast<unsigned int>(plat_drain * 100 / sys_mw), 0u, 100u);

    char cpu_bar[48]{};
    char gpu_bar[48]{};
    char plat_bar[48]{};
    char c3_bar[48]{};

    build_unicode_bar(cpu_bar, sizeof(cpu_bar), cpu_pct, 10);
    build_unicode_bar(gpu_bar, sizeof(gpu_bar), gpu_pct, 10);
    build_unicode_bar(plat_bar, sizeof(plat_bar), plat_pct, 10);
    build_unicode_bar(c3_bar, sizeof(c3_bar), state.cstate_c3_percent, 10);

    auto get_tier_name = [](uint8_t t) noexcept -> const char* {
        switch (t) {
            case 0: return "커널/오디오(면역)";
            case 1: return "컴포지터/디스플레이";
            case 2: return "데스크탑 환경";
            case 3: return "사용자 앱";
            case 4: return "백그라운드";
            case 5: return "폭주 후보";
            default: return "일반 작업";
        }
    };

    // Progressive Top process format strings
    char top1_buf[512]{};
    char top2_buf[512]{};
    if (state.culprits[0].comm[0]) {
        unsigned int c1_w = state.culprits[0].drain_mw / 1000;
        unsigned int c1_f = (state.culprits[0].drain_mw % 1000) / 100;
        unsigned int c1_pct = std::clamp(static_cast<unsigned int>(state.culprits[0].drain_mw * 100 / sys_mw), 0u, 100u);
        char c1_bar[48]{};
        build_unicode_bar(c1_bar, sizeof(c1_bar), c1_pct, 8);
        std::snprintf(top1_buf, sizeof(top1_buf),
            "&nbsp;• #1 <font color=\"#ffffff\"><b>%-12s</b></font> <font color=\"#64748b\">(PID %d · %s)</font><br/>"
            "&nbsp;&nbsp;&nbsp;&nbsp;<b><font color=\"#f43f5e\">%u.%u W</font></b> <font color=\"#94a3b8\">(%u%%)</font> &nbsp;<font color=\"#f43f5e\"><b>[%s]</b></font><br/>",
            state.culprits[0].comm, state.culprits[0].pid, get_tier_name(state.culprits[0].tier),
            c1_w, c1_f, c1_pct, c1_bar);
    } else {
        std::snprintf(top1_buf, sizeof(top1_buf),
            "&nbsp;• <font color=\"#94a3b8\"><i>시스템 안정 유휴 (과다 누수 프로세스 없음)</i></font><br/>");
    }

    if (state.culprits[1].comm[0]) {
        unsigned int c2_w = state.culprits[1].drain_mw / 1000;
        unsigned int c2_f = (state.culprits[1].drain_mw % 1000) / 100;
        unsigned int c2_pct = std::clamp(static_cast<unsigned int>(state.culprits[1].drain_mw * 100 / sys_mw), 0u, 100u);
        char c2_bar[48]{};
        build_unicode_bar(c2_bar, sizeof(c2_bar), c2_pct, 8);
        std::snprintf(top2_buf, sizeof(top2_buf),
            "&nbsp;• #2 <font color=\"#ffffff\"><b>%-12s</b></font> <font color=\"#64748b\">(PID %d · %s)</font><br/>"
            "&nbsp;&nbsp;&nbsp;&nbsp;<b><font color=\"#fb923c\">%u.%u W</font></b> <font color=\"#94a3b8\">(%u%%)</font> &nbsp;<font color=\"#fb923c\"><b>[%s]</b></font><br/>",
            state.culprits[1].comm, state.culprits[1].pid, get_tier_name(state.culprits[1].tier),
            c2_w, c2_f, c2_pct, c2_bar);
    }

    char mitig_buf[128]{};
    if (state.power_profile_mode == 0) {
        std::snprintf(mitig_buf, sizeof(mitig_buf), "풀파워 언락 (CPU 4.1GHz Boost & Zero-Throttling)");
    } else if (state.active_mitigations > 0) {
        std::snprintf(mitig_buf, sizeof(mitig_buf), "실시간 감속 가동 중 (%u개 제어)", state.active_mitigations);
    } else {
        std::snprintf(mitig_buf, sizeof(mitig_buf), "Zero-Wakeup 슬립 유지 (스케줄러 대기)");
    }

    char sign = (state.battery_state == 2) ? '+' : (state.battery_state == 0 ? '+' : '-');
    unsigned int freq_ghz = state.cpu_freq_mhz / 1000;
    unsigned int freq_mhz_frac = (state.cpu_freq_mhz % 1000) / 10;

    // Compact Typography with monospace font and high-density visual alignment
    std::snprintf(out_desc, desc_cap,
        "<div style=\"font-family: 'JetBrains Mono', 'Hack', 'Fira Code', monospace, sans-serif; font-size: 11px; line-height: 1.3;\"><font size=\"2\">"
        "<b><font color=\"#00f0ff\">⚡ WATTCURB CYBER HUD</font></b> &nbsp;<font color=\"#10b981\"><b>● LIVE</b></font> &nbsp;<font color=\"#64748b\">|</font>&nbsp; <b><font color=\"#f59e0b\">%c%u.%u W</font></b><br/>"
        "<font color=\"#334155\">──────────────────────────────────────</font><br/>"
        "<font color=\"#38bdf8\"><b>[배터리 & 전력 동태 (Power Flow)]</b></font><br/>"
        "&nbsp;• 충전율 : <font color=\"%s\"><b>%u%%</b></font> <font color=\"%s\"><b>[%s]</b></font> <font color=\"#94a3b8\">(수명 %u%% · %s)</font><br/>"
        "&nbsp;• 상태값 : <font color=\"#38bdf8\"><b>%s</b></font> &nbsp;<font color=\"#64748b\">|</font>&nbsp; 웨이크업: <font color=\"#f43f5e\"><b>%u/s</b></font> &nbsp;<font color=\"#64748b\">|</font>&nbsp; 팬: <font color=\"#cbd5e1\"><b>%u RPM</b></font><br/>"
        "<font color=\"#334155\">──────────────────────────────────────</font><br/>"
        "<font color=\"#a855f7\"><b>[실리콘 하드웨어 도메인 (Progressive HW)]</b></font><br/>"
        "&nbsp;• CPU 연산 &nbsp;: <b><font color=\"#00f0ff\">%2u.%u W</font></b> <font color=\"#64748b\">(%2u%%)</font> <font color=\"#00f0ff\"><b>[%s]</b></font> <b><font color=\"#38bdf8\">%u.%02u GHz</font></b> <font color=\"%s\"><b>%u°C</b></font><br/>"
        "&nbsp;• GPU 그래픽: <b><font color=\"#10b981\">%2u.%u W</font></b> <font color=\"#64748b\">(%2u%%)</font> <font color=\"#10b981\"><b>[%s]</b></font> <font color=\"#38bdf8\">%s</font><br/>"
        "&nbsp;• 플랫폼/IO &nbsp;: <b><font color=\"#e2e8f0\">%2u.%u W</font></b> <font color=\"#64748b\">(%2u%%)</font> <font color=\"#e2e8f0\"><b>[%s]</b></font> <font color=\"#64748b\">LPDDR5X/APST</font><br/>"
        "&nbsp;• C3 심층수면: <b><font color=\"#a855f7\">%2u%% C3</font></b> <font color=\"#64748b\">Sleep</font> <font color=\"#a855f7\"><b>[%s]</b></font> <font color=\"#10b981\">%s</font><br/>"
        "<font color=\"#334155\">──────────────────────────────────────</font><br/>"
        "<font color=\"#f43f5e\"><b>[실시간 최다 전력 누수 프로세스 (Top Culprits)]</b></font><br/>"
        "%s"
        "%s"
        "<font color=\"#334155\">──────────────────────────────────────</font><br/>"
        "<font color=\"#64748b\">프로파일: </font><b><font color=\"#00f0ff\">%s</font></b><br/>"
        "<font color=\"#64748b\">오디오면역: </font><b><font color=\"#10b981\">PipeWire/Pulse Realtime TS (-12)</font></b><br/>"
        "<font color=\"#64748b\">정책엔진: </font><b><font color=\"#38bdf8\">%s</font></b>"
        "</font></div>",
        sign, sys_w, sys_frac,
        bat_color, state.battery_percent, bat_color, bat_bar, state.battery_health_percent, time_buf,
        status_kr, state.wakeups_per_sec, state.fan_rpm,
        cpu_w, cpu_frac, cpu_pct, cpu_bar, freq_ghz, freq_mhz_frac, temp_color, state.cpu_temp_c,
        gpu_w, gpu_frac, gpu_pct, gpu_bar, gpu_status,
        plat_w, plat_frac, plat_pct, plat_bar,
        state.cstate_c3_percent, c3_bar, c3_status,
        top1_buf, top2_buf,
        profile_name, mitig_buf
    );

    sanitize_utf8_inplace(out_desc);
    sanitize_utf8_inplace(out_title);
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

    // Implements REF-REQ-047: Watch for org.kde.StatusNotifierWatcher appearance on D-Bus
    // When plasmashell/kded6 boots or restarts, automatically re-register immediately.
    r = sd_bus_match_signal(
        bus_,
        &match_slot_,
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "NameOwnerChanged",
        on_name_owner_changed,
        this
    );

    return true;
}

int TrayClient::on_name_owner_changed(sd_bus_message* msg, void* userdata, sd_bus_error*) {
    auto* self = static_cast<TrayClient*>(userdata);
    if (!self || !msg) return 0;

    const char* name = nullptr;
    const char* old_owner = nullptr;
    const char* new_owner = nullptr;
    int r = sd_bus_message_read(msg, "sss", &name, &old_owner, &new_owner);
    if (r >= 0 && name && std::strcmp(name, "org.kde.StatusNotifierWatcher") == 0) {
        if (new_owner && new_owner[0] != '\0') {
            // Watcher service appeared or restarted on bus
            self->register_with_watcher();
        } else {
            // Watcher disappeared
            self->watcher_registered_ = false;
        }
    }
    return 0;
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
        watcher_registered_ = true;
        // Emit initial change signals to trigger plasmashell to query properties immediately
        sd_bus_emit_signal(bus_, "/StatusNotifierItem", "org.kde.StatusNotifierItem", "NewIcon", nullptr);
        sd_bus_emit_signal(bus_, "/StatusNotifierItem", "org.kde.StatusNotifierItem", "NewStatus", "s", "Active");
        sd_bus_emit_signal(bus_, "/StatusNotifierItem", "org.kde.StatusNotifierItem", "NewToolTip", nullptr);
        sd_bus_flush(bus_);
        return true;
    }
    watcher_registered_ = false;
    return false;
}

bool TrayClient::initialize() noexcept {
    setup_shm(); // Non-fatal: if daemon is not yet running, local defaults apply
    if (!setup_dbus()) return false;
    register_with_watcher(); // Non-fatal if watcher is still starting up
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

        // Sleep with 1-second timeout (1'000'000 us):
        // Wakes up on D-Bus events instantly, or at least once every 1s to sync live telemetry
        r = sd_bus_wait(bus_, 1'000'000ULL);
        if (r < 0 && r != -EINTR) break;

        // Implements REF-REQ-047: If initial registration failed during desktop cold boot,
        // retry periodically until StatusNotifierWatcher is acquired.
        if (!watcher_registered_) {
            register_with_watcher();
        }

        // Periodic state check: read 128-byte Seqlock SHM (< 50ns, zero-allocation)
        ipc::WattCurbSharedState cur_state{};
        if (read_state(cur_state)) {
            bool changed = (cur_state.seq_version != prev_state.seq_version) ||
                           (cur_state.battery_percent != prev_state.battery_percent) ||
                           (cur_state.battery_state != prev_state.battery_state) ||
                           (std::abs(static_cast<int>(cur_state.system_drain_mw) - static_cast<int>(prev_state.system_drain_mw)) > 50) ||
                           (std::abs(static_cast<int>(cur_state.cpu_drain_mw) - static_cast<int>(prev_state.cpu_drain_mw)) > 100) ||
                           (cur_state.cpu_temp_c != prev_state.cpu_temp_c) ||
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
    sanitize_utf8_inplace(icon);
    return sd_bus_message_append(reply, "s", icon);
}

int TrayClient::property_get_tooltip(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply, void* userdata, sd_bus_error*) {
    auto* self = static_cast<TrayClient*>(userdata);
    ipc::WattCurbSharedState state{};
    self->read_state(state);

    // Implements REF-REQ-050: Instant live hardware sensor probe upon hover!
    probe_live_sensors_on_hover(state);

    char icon[64]{};
    resolve_icon_name(state, icon, sizeof(icon));
    sanitize_utf8_inplace(icon);

    char title[128]{};
    char desc[8192]{};
    render_tooltip(state, title, sizeof(title), desc, sizeof(desc));
    sanitize_utf8_inplace(title);
    sanitize_utf8_inplace(desc);

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
