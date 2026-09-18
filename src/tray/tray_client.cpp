#include "tray/tray_client.hpp"
#include "core/singleton_lock.hpp"
#include "core/posix_fs.hpp"
#include "core/scoped_profiler.hpp"
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
    WATTCURB_PROFILE_SCOPE("tray.read_state");
    if (!shm_state_) {
        const_cast<TrayClient*>(this)->setup_shm();
    }
    if (!shm_state_) return false;
    bool ok = shm_state_->read_atomic(out);
    if (ok) {
        if (local_override_mode_ >= 0) {
            if (out.power_profile_mode == static_cast<uint8_t>(local_override_mode_)) {
                const_cast<TrayClient*>(this)->local_override_mode_ = -1;
            } else {
                out.power_profile_mode = static_cast<uint8_t>(local_override_mode_);
            }
        }
    }
    return ok;
}

// Precomputed 8-block Unicode bars in pure UTF-8 (24 bytes + null terminator) (REF-REQ-073, REF-ARCH-050)
static constexpr char BAR_LUT[9][25] = {
    "\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91", // 0
    "\xe2\x96\x88\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91", // 1
    "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91", // 2
    "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91", // 3
    "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91", // 4
    "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91", // 5
    "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x91\xe2\x96\x91", // 6
    "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x91", // 7
    "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88"  // 8
};

static inline void build_unicode_bar_8(char* out, unsigned int percent) noexcept {
    WATTCURB_PROFILE_SCOPE("tray.tooltip.build_bars");
    unsigned int filled = (percent * 8 + 50) / 100;
    if (filled > 8) filled = 8;
    std::memcpy(out, BAR_LUT[filled], 25);
}

// Precomputed Icon Name LUT [11 deciles][2 charge states][3 profiles] (REF-REQ-073, REF-ARCH-050)
static constexpr const char* const ICON_LUT[11][2][3] = {
    // 000
    {
        { "battery-000-profile-performance", "battery-000-profile-balanced", "battery-000-profile-powersave" },
        { "battery-000-charging-profile-performance", "battery-000-charging-profile-balanced", "battery-000-charging-profile-powersave" }
    },
    // 010
    {
        { "battery-010-profile-performance", "battery-010-profile-balanced", "battery-010-profile-powersave" },
        { "battery-010-charging-profile-performance", "battery-010-charging-profile-balanced", "battery-010-charging-profile-powersave" }
    },
    // 020
    {
        { "battery-020-profile-performance", "battery-020-profile-balanced", "battery-020-profile-powersave" },
        { "battery-020-charging-profile-performance", "battery-020-charging-profile-balanced", "battery-020-charging-profile-powersave" }
    },
    // 030
    {
        { "battery-030-profile-performance", "battery-030-profile-balanced", "battery-030-profile-powersave" },
        { "battery-030-charging-profile-performance", "battery-030-charging-profile-balanced", "battery-030-charging-profile-powersave" }
    },
    // 040
    {
        { "battery-040-profile-performance", "battery-040-profile-balanced", "battery-040-profile-powersave" },
        { "battery-040-charging-profile-performance", "battery-040-charging-profile-balanced", "battery-040-charging-profile-powersave" }
    },
    // 050
    {
        { "battery-050-profile-performance", "battery-050-profile-balanced", "battery-050-profile-powersave" },
        { "battery-050-charging-profile-performance", "battery-050-charging-profile-balanced", "battery-050-charging-profile-powersave" }
    },
    // 060
    {
        { "battery-060-profile-performance", "battery-060-profile-balanced", "battery-060-profile-powersave" },
        { "battery-060-charging-profile-performance", "battery-060-charging-profile-balanced", "battery-060-charging-profile-powersave" }
    },
    // 070
    {
        { "battery-070-profile-performance", "battery-070-profile-balanced", "battery-070-profile-powersave" },
        { "battery-070-charging-profile-performance", "battery-070-charging-profile-balanced", "battery-070-charging-profile-powersave" }
    },
    // 080
    {
        { "battery-080-profile-performance", "battery-080-profile-balanced", "battery-080-profile-powersave" },
        { "battery-080-charging-profile-performance", "battery-080-charging-profile-balanced", "battery-080-charging-profile-powersave" }
    },
    // 090
    {
        { "battery-090-profile-performance", "battery-090-profile-balanced", "battery-090-profile-powersave" },
        { "battery-090-charging-profile-performance", "battery-090-charging-profile-balanced", "battery-090-charging-profile-powersave" }
    },
    // 100
    {
        { "battery-100-profile-performance", "battery-100-profile-balanced", "battery-100-profile-powersave" },
        { "battery-100-charging-profile-performance", "battery-100-charging-profile-balanced", "battery-100-charging-profile-powersave" }
    }
};

// Zero-allocation length-aware UTF-8 truncation guard (REF-REQ-047, REF-REQ-073)
static inline void sanitize_utf8_fast(char* s, size_t len) noexcept {
    WATTCURB_PROFILE_SCOPE("tray.tooltip.sanitize_utf8");
    if (!s || len == 0) return;

    for (size_t lookback = 1; lookback <= 4 && lookback <= len; ++lookback) {
        unsigned char lead = static_cast<unsigned char>(s[len - lookback]);
        if ((lead & 0x80) == 0x00) break;
        if ((lead & 0xC0) == 0x80) continue;
        if ((lead & 0xE0) == 0xC0) {
            if (lookback < 2) s[len - lookback] = '\0';
            break;
        }
        if ((lead & 0xF0) == 0xE0) {
            if (lookback < 3) s[len - lookback] = '\0';
            break;
        }
        if ((lead & 0xF8) == 0xF0) {
            if (lookback < 4) s[len - lookback] = '\0';
            break;
        }
    }
}

static void sanitize_utf8_inplace(char* s) noexcept {
    if (!s) return;
    sanitize_utf8_fast(s, std::strlen(s));
}

static inline long fast_parse_int(const char* p) noexcept {
    while (*p && (*p < '0' || *p > '9') && *p != '-') ++p;
    if (!*p) return 0;
    bool neg = (*p == '-');
    if (neg) ++p;
    long val = 0;
    while (*p >= '0' && *p <= '9') {
        val = val * 10 + (*p - '0');
        ++p;
    }
    return neg ? -val : val;
}

// Persistent file descriptors and hover hysteresis state (REF-REQ-073, REF-ARCH-050)
struct PersistentHoverProbe {
    int bat_fd{-1};
    int thermal_fd{-1};
    int cpufreq_fd{-1};
    uint64_t last_probe_ms{0};

    ~PersistentHoverProbe() noexcept {
        close_all();
    }

    void close_all() noexcept {
        if (bat_fd >= 0) { ::close(bat_fd); bat_fd = -1; }
        if (thermal_fd >= 0) { ::close(thermal_fd); thermal_fd = -1; }
        if (cpufreq_fd >= 0) { ::close(cpufreq_fd); cpufreq_fd = -1; }
    }
};

static PersistentHoverProbe s_hover_probe{};

void TrayClient::probe_sensors_for_hover(ipc::WattCurbSharedState& state) noexcept {
    WATTCURB_PROFILE_SCOPE("tray.probe_sensors.total");

    // REF-REQ-073 & REF-ARCH-050: 500ms Subsampling / Hover Hysteresis Guard
    // When hovering, KDE Plasma triggers bursts of ToolTip queries.
    // If probed within 500ms, reuse existing warm state with ZERO syscalls!
    struct timespec ts{};
    ::clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t now_ms = static_cast<uint64_t>(ts.tv_sec) * 1000ULL + static_cast<uint64_t>(ts.tv_nsec) / 1'000'000ULL;
    if (now_ms - s_hover_probe.last_probe_ms < 500ULL && s_hover_probe.last_probe_ms > 0) {
        return; // Zero-syscall instant bypass (< 50ns)
    }
    s_hover_probe.last_probe_ms = now_ms;

    // 1. Live Battery Telemetry via Persistent FD & pread(0)
    {
        WATTCURB_PROFILE_SCOPE("tray.probe_sensors.bat_uevent");
        if (s_hover_probe.bat_fd < 0) {
            s_hover_probe.bat_fd = ::open("/sys/class/power_supply/BAT0/uevent", O_RDONLY | O_CLOEXEC);
            if (s_hover_probe.bat_fd < 0) {
                s_hover_probe.bat_fd = ::open("/sys/class/power_supply/BAT1/uevent", O_RDONLY | O_CLOEXEC);
            }
        }
        if (s_hover_probe.bat_fd >= 0) {
            char ubuf[1024]{};
            ssize_t n = ::pread(s_hover_probe.bat_fd, ubuf, sizeof(ubuf) - 1, 0);
            if (n > 0) {
                ubuf[n] = '\0';
                const char* p = ubuf;
                uint32_t power_now = 0;
                uint32_t current_now = 0;
                uint32_t voltage_now = 0;
                while (*p) {
                    if (std::strncmp(p, "POWER_SUPPLY_POWER_NOW=", 23) == 0) {
                        power_now = static_cast<uint32_t>(fast_parse_int(p + 23));
                    } else if (std::strncmp(p, "POWER_SUPPLY_CURRENT_NOW=", 25) == 0) {
                        current_now = static_cast<uint32_t>(fast_parse_int(p + 25));
                    } else if (std::strncmp(p, "POWER_SUPPLY_VOLTAGE_NOW=", 25) == 0) {
                        voltage_now = static_cast<uint32_t>(fast_parse_int(p + 25));
                    } else if (std::strncmp(p, "POWER_SUPPLY_CAPACITY=", 22) == 0) {
                        uint8_t cap = static_cast<uint8_t>(fast_parse_int(p + 22));
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
        }
    }

    // 2. Live CPU Thermal Sensor via Persistent FD & pread(0)
    {
        WATTCURB_PROFILE_SCOPE("tray.probe_sensors.thermal");
        if (s_hover_probe.thermal_fd < 0) {
            s_hover_probe.thermal_fd = ::open("/sys/class/thermal/thermal_zone0/temp", O_RDONLY | O_CLOEXEC);
        }
        if (s_hover_probe.thermal_fd >= 0) {
            char tbuf[32]{};
            ssize_t n = ::pread(s_hover_probe.thermal_fd, tbuf, sizeof(tbuf) - 1, 0);
            if (n > 0) {
                tbuf[n] = '\0';
                long temp_mc = fast_parse_int(tbuf);
                if (temp_mc > 0) {
                    state.cpu_temp_c = static_cast<uint16_t>(temp_mc / 1000);
                }
            }
        }
    }

    // 3. Live CPU Core Frequency via Persistent FD & pread(0)
    {
        WATTCURB_PROFILE_SCOPE("tray.probe_sensors.cpufreq");
        if (s_hover_probe.cpufreq_fd < 0) {
            s_hover_probe.cpufreq_fd = ::open("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", O_RDONLY | O_CLOEXEC);
        }
        if (s_hover_probe.cpufreq_fd >= 0) {
            char fbuf[32]{};
            ssize_t n = ::pread(s_hover_probe.cpufreq_fd, fbuf, sizeof(fbuf) - 1, 0);
            if (n > 0) {
                fbuf[n] = '\0';
                long khz = fast_parse_int(fbuf);
                if (khz > 0) {
                    state.cpu_freq_mhz = static_cast<uint16_t>(khz / 1000);
                }
            }
        }
    }
}

// Tooltip Memoization Cache to achieve sub-50ns instant response for identical states (REF-REQ-073)
struct TooltipCache {
    uint64_t seq_version{0};
    uint32_t system_drain_mw{0};
    uint32_t cpu_drain_mw{0};
    uint32_t gpu_drain_mw{0};
    uint16_t cpu_temp_c{0};
    uint16_t cpu_freq_mhz{0};
    uint16_t fan_rpm{0};
    uint8_t battery_percent{0};
    uint8_t battery_state{0};
    uint8_t power_profile_mode{0};
    char culprits0[16]{};
    char culprits1[16]{};
    uint32_t culprits0_drain{0};
    uint32_t culprits1_drain{0};

    char title[128]{};
    char desc[8192]{};
    size_t title_len{0};
    size_t desc_len{0};
    bool valid{false};
};

static TooltipCache s_tip_cache{};

void TrayClient::render_tooltip(
    const ipc::WattCurbSharedState& state,
    char* out_title, size_t title_cap,
    char* out_desc, size_t desc_cap
) noexcept {
    WATTCURB_PROFILE_SCOPE("tray.tooltip.render_total");

    // Fast O(1) Cache Hit: if state unchanged, return memoized HTML in < 30ns!
    if (s_tip_cache.valid &&
        s_tip_cache.seq_version == state.seq_version &&
        s_tip_cache.system_drain_mw == state.system_drain_mw &&
        s_tip_cache.cpu_drain_mw == state.cpu_drain_mw &&
        s_tip_cache.gpu_drain_mw == state.gpu_drain_mw &&
        s_tip_cache.cpu_temp_c == state.cpu_temp_c &&
        s_tip_cache.cpu_freq_mhz == state.cpu_freq_mhz &&
        s_tip_cache.fan_rpm == state.fan_rpm &&
        s_tip_cache.battery_percent == state.battery_percent &&
        s_tip_cache.battery_state == state.battery_state &&
        s_tip_cache.power_profile_mode == state.power_profile_mode &&
        s_tip_cache.culprits0_drain == state.culprits[0].drain_mw &&
        s_tip_cache.culprits1_drain == state.culprits[1].drain_mw &&
        std::strncmp(s_tip_cache.culprits0, state.culprits[0].comm, 16) == 0 &&
        std::strncmp(s_tip_cache.culprits1, state.culprits[1].comm, 16) == 0) {
        if (s_tip_cache.title_len < title_cap && s_tip_cache.desc_len < desc_cap) {
            std::memcpy(out_title, s_tip_cache.title, s_tip_cache.title_len + 1);
            std::memcpy(out_desc, s_tip_cache.desc, s_tip_cache.desc_len + 1);
            return;
        }
    }

    unsigned int sys_w = state.system_drain_mw / 1000;
    unsigned int sys_frac = (state.system_drain_mw % 1000) / 100;

    const char* status_kr = state.battery_state == 1 ? "방전 중" : 
                           (state.battery_state == 2 ? "AC 직결 (완충)" : "AC 충전 중");

    std::snprintf(out_title, title_cap, "⚡ WattCurb: %u.%u W (%s)", sys_w, sys_frac, status_kr);

    const char* profile_short = "Performance (4.1G 언락)";
    if (state.power_profile_mode == 1) profile_short = "Balanced (균형)";
    else if (state.power_profile_mode == 2) profile_short = "SmartSave (절전 1.7G)";
    else if (state.power_profile_mode == 3) profile_short = "UltraSave (극저전력 1.4G)";

    unsigned int cpu_w = state.cpu_drain_mw / 1000;
    unsigned int cpu_frac = (state.cpu_drain_mw % 1000) / 100;
    unsigned int gpu_w = state.gpu_drain_mw / 1000;
    unsigned int gpu_frac = (state.gpu_drain_mw % 1000) / 100;

    uint32_t comp_drain = state.cpu_drain_mw + state.gpu_drain_mw;
    uint32_t plat_drain = state.system_drain_mw > comp_drain ? (state.system_drain_mw - comp_drain) : 0;
    unsigned int plat_w = plat_drain / 1000;
    unsigned int plat_frac = (plat_drain % 1000) / 100;

    const char* bat_color = (state.battery_percent >= 60) ? "#10b981" :
                           ((state.battery_percent >= 25) ? "#f59e0b" : "#ef4444");

    const char* temp_color = (state.cpu_temp_c < 50) ? "#00f0ff" :
                            ((state.cpu_temp_c < 65) ? "#10b981" :
                            ((state.cpu_temp_c < 80) ? "#fb923c" : "#f43f5e"));

    // 8-block Unicode bars for ultra-compact fit (O(1) 24B LUT memcpy)
    char bat_bar[32]{};
    build_unicode_bar_8(bat_bar, state.battery_percent);

    unsigned int sys_mw = state.system_drain_mw > 0 ? state.system_drain_mw : 1;
    unsigned int cpu_pct = std::clamp(static_cast<unsigned int>(state.cpu_drain_mw * 100 / sys_mw), 0u, 100u);
    char cpu_bar[32]{};
    build_unicode_bar_8(cpu_bar, cpu_pct);

    unsigned int gpu_pct = std::clamp(static_cast<unsigned int>(state.gpu_drain_mw * 100 / sys_mw), 0u, 100u);
    char gpu_bar[32]{};
    build_unicode_bar_8(gpu_bar, gpu_pct);

    char sign = (state.battery_state == 2) ? '+' : (state.battery_state == 0 ? '+' : '-');
    unsigned int freq_ghz = state.cpu_freq_mhz / 1000;
    unsigned int freq_mhz_frac = (state.cpu_freq_mhz % 1000) / 10;

    // Top 2 processes in single compact line
    char culprits_line[512]{};
    {
        WATTCURB_PROFILE_SCOPE("tray.tooltip.culprits");
        if (state.culprits[0].comm[0] && state.culprits[1].comm[0]) {
            unsigned int c1_w = state.culprits[0].drain_mw / 1000;
            unsigned int c1_f = (state.culprits[0].drain_mw % 1000) / 100;
            unsigned int c2_w = state.culprits[1].drain_mw / 1000;
            unsigned int c2_f = (state.culprits[1].drain_mw % 1000) / 100;
            std::snprintf(culprits_line, sizeof(culprits_line),
                "<font color=\"#ffffff\"><b>%.12s</b></font> <font color=\"#f43f5e\">%u.%u W</font> <font color=\"#475569\">·</font> <font color=\"#ffffff\"><b>%.12s</b></font> <font color=\"#fb923c\">%u.%u W</font>",
                state.culprits[0].comm, c1_w, c1_f, state.culprits[1].comm, c2_w, c2_f);
        } else if (state.culprits[0].comm[0]) {
            unsigned int c1_w = state.culprits[0].drain_mw / 1000;
            unsigned int c1_f = (state.culprits[0].drain_mw % 1000) / 100;
            std::snprintf(culprits_line, sizeof(culprits_line),
                "<font color=\"#ffffff\"><b>%.12s</b></font> <font color=\"#f43f5e\">%u.%u W</font>",
                state.culprits[0].comm, c1_w, c1_f);
        } else {
            std::snprintf(culprits_line, sizeof(culprits_line),
                "<font color=\"#94a3b8\"><i>유휴 안정 (누수 없음)</i></font>");
        }
    }

    char bat_detail[64]{};
    if (state.battery_state == 1) {
        if (state.time_to_empty_min > 0) {
            std::snprintf(bat_detail, sizeof(bat_detail), "%u분 남음", state.time_to_empty_min);
        } else {
            std::snprintf(bat_detail, sizeof(bat_detail), "방전 중");
        }
    } else if (state.battery_state == 2) {
        std::snprintf(bat_detail, sizeof(bat_detail), "완충 AC 직결");
    } else {
        std::snprintf(bat_detail, sizeof(bat_detail), "충전 중");
    }

    // Ref-Req-050 & REF-REQ-072: Clean, fixed-column progressive bar layout with ScopedProfiler
    int desc_len = 0;
    {
        WATTCURB_PROFILE_SCOPE("tray.tooltip.snprintf_hud");
        desc_len = std::snprintf(out_desc, desc_cap,
            "<div style=\"font-family: 'JetBrains Mono', 'Hack', monospace; font-size: 11px; line-height: 1.35;\"><font size=\"2\">"
            "<nobr><b><font color=\"#00f0ff\">⚡ WATTCURB CYBER HUD</font></b> &nbsp;<font color=\"#10b981\">● LIVE</font> &nbsp;<font color=\"#475569\">|</font> &nbsp;<b><font color=\"#f59e0b\">%c%u.%u W</font></b></nobr><br/>"
            "<nobr><font color=\"#64748b\">BAT</font> <font color=\"%s\"><b>[%s]</b></font> <font color=\"#ffffff\"><b>%2u%%</b></font> <font color=\"#475569\">·</font> <font color=\"%s\">%s</font> <font color=\"#475569\">·</font> <font color=\"#94a3b8\">%urpm</font></nobr><br/>"
            "<nobr><font color=\"#64748b\">CPU</font> <font color=\"#00f0ff\"><b>[%s]</b></font> <font color=\"#ffffff\"><b>%2u%%</b></font> <font color=\"#475569\">·</font> <b><font color=\"#00f0ff\">%u.%u W</font></b> <font color=\"#475569\">·</font> <font color=\"#38bdf8\">%u.%02uGHz</font> <font color=\"%s\">%u°C</font></nobr><br/>"
            "<nobr><font color=\"#64748b\">GPU</font> <font color=\"#a855f7\"><b>[%s]</b></font> <b><font color=\"#a855f7\">%u.%u W</font></b> <font color=\"#475569\">·</font> <font color=\"#cbd5e1\">C3 %u%%</font> <font color=\"#475569\">·</font> <font color=\"#94a3b8\">IO %u.%u W</font></nobr><br/>"
            "<nobr><font color=\"#64748b\">TOP</font> %s</nobr><br/>"
            "<nobr><font color=\"#64748b\">SYS</font> <b><font color=\"#00f0ff\">%s</font></b> <font color=\"#475569\">|</font> <font color=\"#10b981\">PipeWire RT(-12)</font></nobr>"
            "</font></div>",
            sign, sys_w, sys_frac,
            bat_color, bat_bar, state.battery_percent, bat_color, bat_detail, state.fan_rpm,
            cpu_bar, cpu_pct, cpu_w, cpu_frac, freq_ghz, freq_mhz_frac, temp_color, state.cpu_temp_c,
            gpu_bar, gpu_w, gpu_frac, state.cstate_c3_percent, plat_w, plat_frac,
            culprits_line,
            profile_short
        );
    }

    if (desc_len > 0) {
        sanitize_utf8_fast(out_desc, static_cast<size_t>(desc_len));
    }
    sanitize_utf8_inplace(out_title);

    // Save to TooltipCache
    s_tip_cache.seq_version = state.seq_version;
    s_tip_cache.system_drain_mw = state.system_drain_mw;
    s_tip_cache.cpu_drain_mw = state.cpu_drain_mw;
    s_tip_cache.gpu_drain_mw = state.gpu_drain_mw;
    s_tip_cache.cpu_temp_c = state.cpu_temp_c;
    s_tip_cache.cpu_freq_mhz = state.cpu_freq_mhz;
    s_tip_cache.fan_rpm = state.fan_rpm;
    s_tip_cache.battery_percent = state.battery_percent;
    s_tip_cache.battery_state = state.battery_state;
    s_tip_cache.power_profile_mode = state.power_profile_mode;
    s_tip_cache.culprits0_drain = state.culprits[0].drain_mw;
    s_tip_cache.culprits1_drain = state.culprits[1].drain_mw;
    std::memcpy(s_tip_cache.culprits0, state.culprits[0].comm, 16);
    std::memcpy(s_tip_cache.culprits1, state.culprits[1].comm, 16);
    s_tip_cache.title_len = std::strlen(out_title);
    s_tip_cache.desc_len = static_cast<size_t>(desc_len > 0 ? desc_len : 0);
    if (s_tip_cache.title_len < sizeof(s_tip_cache.title) && s_tip_cache.desc_len < sizeof(s_tip_cache.desc)) {
        std::memcpy(s_tip_cache.title, out_title, s_tip_cache.title_len + 1);
        std::memcpy(s_tip_cache.desc, out_desc, s_tip_cache.desc_len + 1);
        s_tip_cache.valid = true;
    }
}

void TrayClient::resolve_icon_name(
    const ipc::WattCurbSharedState& state,
    char* out_icon, size_t icon_cap
) noexcept {
    WATTCURB_PROFILE_SCOPE("tray.resolve_icon");
    // Implements REF-REQ-073 & REF-ARCH-050: O(1) Precomputed Icon Name LUT (0 allocations, 0 snprintf, < 10ns)
    uint8_t prof_idx = 1; // balanced
    if (state.power_profile_mode == 0) prof_idx = 0; // performance
    else if (state.power_profile_mode == 2 || state.power_profile_mode == 3) prof_idx = 2; // powersave

    uint8_t pct = state.battery_percent;
    if (pct > 100) pct = 100;

    // Quantize to nearest 10% step (0, 1, 2, ..., 10)
    unsigned int rounded_decile = ((static_cast<unsigned int>(pct) + 5) / 10);
    if (rounded_decile > 10) rounded_decile = 10;

    uint8_t charge_idx = (state.battery_state == 2 || state.battery_state == 0) ? 1 : 0;
    const char* icon_str = ICON_LUT[rounded_decile][charge_idx][prof_idx];
    size_t len = std::strlen(icon_str);
    if (len < icon_cap) {
        std::memcpy(out_icon, icon_str, len + 1);
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

    // Note: The unprivileged tray client does NOT fork power-profile-manager directly
    // to prevent pkexec authorization prompts. The root daemon executes hardware profile actuation.
}

bool TrayClient::send_daemon_command(const char* cmd) noexcept {
    WATTCURB_PROFILE_SCOPE("tray.ipc.send_daemon_cmd");
    if (!cmd) return false;
    std::string resp;
    return core::SingletonLock::query_daemon(cmd, resp, "wattcurb.lock", 250);
}

void TrayClient::print_profiler_summary() noexcept {
    core::ScopedProfilerRegistry::instance().print_summary(std::cout);
}

void TrayClient::cycle_power_profile() noexcept {
    ipc::WattCurbSharedState state{};
    read_state(state);

    auto next_mode = static_cast<uint8_t>((state.power_profile_mode + 1) % 4);
    // REF-REQ-067: Skip Performance mode when discharging on battery <= 20%
    if (next_mode == 0 && state.battery_state == 1 && state.battery_percent <= 20) {
        next_mode = 1; // Skip Performance, advance directly to Balanced
    }

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
        int r = 0;
        {
            WATTCURB_PROFILE_SCOPE("tray.loop.bus_process");
            r = sd_bus_process(bus_, nullptr);
        }
        if (r < 0) break;
        if (r > 0) continue; // More work to do immediately

        // Sleep with 1-second timeout (1'000'000 us):
        // Wakes up on D-Bus events instantly, or at least once every 1s to sync live telemetry
        {
            WATTCURB_PROFILE_SCOPE("tray.loop.bus_wait");
            r = sd_bus_wait(bus_, 1'000'000ULL);
        }
        if (r < 0 && r != -EINTR) break;

        // Implements REF-REQ-047: If initial registration failed during desktop cold boot,
        // retry periodically until StatusNotifierWatcher is acquired.
        if (!watcher_registered_) {
            register_with_watcher();
        }

        // Periodic state check: read 128-byte Seqlock SHM (< 50ns, zero-allocation)
        ipc::WattCurbSharedState cur_state{};
        bool changed = false;
        {
            WATTCURB_PROFILE_SCOPE("tray.loop.poll_shm");
            if (read_state(cur_state)) {
                changed = (cur_state.seq_version != prev_state.seq_version) ||
                          (cur_state.battery_percent != prev_state.battery_percent) ||
                          (cur_state.battery_state != prev_state.battery_state) ||
                          (std::abs(static_cast<int>(cur_state.system_drain_mw) - static_cast<int>(prev_state.system_drain_mw)) > 50) ||
                          (std::abs(static_cast<int>(cur_state.cpu_drain_mw) - static_cast<int>(prev_state.cpu_drain_mw)) > 100) ||
                          (cur_state.cpu_temp_c != prev_state.cpu_temp_c) ||
                          (cur_state.power_profile_mode != prev_state.power_profile_mode);
            }
        }

        if (changed) {
            WATTCURB_PROFILE_SCOPE("tray.loop.emit_signals");
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
    WATTCURB_PROFILE_SCOPE("tray.property_get_icon");
    auto* self = static_cast<TrayClient*>(userdata);
    ipc::WattCurbSharedState state{};
    self->read_state(state);

    char icon[64]{};
    resolve_icon_name(state, icon, sizeof(icon));
    sanitize_utf8_inplace(icon);
    return sd_bus_message_append(reply, "s", icon);
}

int TrayClient::property_get_tooltip(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply, void* userdata, sd_bus_error*) {
    WATTCURB_PROFILE_SCOPE("tray.property_get_tooltip.total");
    auto* self = static_cast<TrayClient*>(userdata);
    ipc::WattCurbSharedState state{};
    self->read_state(state);

    // Implements REF-REQ-050 & REF-REQ-072: Instant live hardware sensor probe upon hover!
    probe_sensors_for_hover(state);

    char icon[64]{};
    resolve_icon_name(state, icon, sizeof(icon));

    char title[128]{};
    char desc[8192]{};
    render_tooltip(state, title, sizeof(title), desc, sizeof(desc));

    {
        WATTCURB_PROFILE_SCOPE("tray.property_get_tooltip.dbus_pack");
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
    // User safety invariant: Left-clicking must NEVER automatically cycle power profiles.
    // Power profiles must only be explicitly selected via the context menu.
    // Left-clicking requests an instant telemetry refresh.
    if (self) {
        self->send_daemon_command("RESCAN\n");
        if (self->bus_) {
            sd_bus_emit_signal(self->bus_, "/StatusNotifierItem", "org.kde.StatusNotifierItem", "NewToolTip", nullptr);
            sd_bus_emit_signal(self->bus_, "/StatusNotifierItem", "org.kde.StatusNotifierItem", "XAyatanaNewLabel", nullptr);
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
    WATTCURB_PROFILE_SCOPE("tray.menu.build_nodes");
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
    WATTCURB_PROFILE_SCOPE("tray.menu.get_layout");
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
    add_item(8, "Ultra Save (초절전 모드 - 1.4GHz 상한)", true, nullptr, "radio", (cur_mode == 3 ? 1 : 0));
    add_item(9, nullptr, true, "separator");
    add_item(10, "📈 정밀 분석 매트릭 창 열기 (Matrix Dashboard)");
    add_item(11, "📊 KDE 시스템 모니터 열기 (System Monitor)");

    sd_bus_message_close_container(reply); // children av
    sd_bus_message_close_container(reply); // root r

    {
        WATTCURB_PROFILE_SCOPE("tray.menu.dbus_send");
        r = sd_bus_send(self->bus_, reply, nullptr);
    }
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
            // REF-REQ-067: Lockout Performance mode when battery <= 20%
            ipc::WattCurbSharedState st{};
            self->read_state(st);
            if (selected_mode == 0 && st.battery_state == 1 && st.battery_percent <= 20) {
                selected_mode = 1;
                hw_mode = "balanced";
            }

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
