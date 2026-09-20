#pragma once

#include <cstdint>
#include <string_view>
#include <optional>

// REF-REQ-076 & REF-ARCH-053: C++23 Zero-Cost Multilingual Localization Subsystem
// Covers the top 12 world languages by population + Korean (Native Dev/Desktop Language)
namespace wattcurb::core::l10n {

enum class Language : uint8_t {
    EN = 0, // English (Global Default / Fallback)
    ZH,     // Mandarin Chinese (简体中文)
    HI,     // Hindi (हिन्दी)
    ES,     // Spanish (Español)
    FR,     // French (Français)
    AR,     // Modern Standard Arabic (العربية)
    BN,     // Bengali (বাংলা)
    PT,     // Portuguese (Português)
    RU,     // Russian (Русский)
    UR,     // Urdu (اردو)
    ID,     // Indonesian (Bahasa Indonesia)
    DE,     // German (Deutsch)
    KO,     // Korean (한국어)
    COUNT
};

enum class StringId : uint16_t {
    STATUS_DISCHARGING = 0,
    STATUS_AC_PASSTHROUGH,
    STATUS_AC_CHARGING,
    STATUS_AC_CONNECTED,
    STATUS_ON_BATTERY,
    BATTERY_TIME_LEFT,
    BATTERY_TIME_CALCULATING,
    BATTERY_TIME_UNLIMITED,
    BATTERY_TIME_HOURS_MINS,
    BATTERY_TIME_MINS,
    PROFILE_PERFORMANCE_SHORT,
    PROFILE_BALANCED_SHORT,
    PROFILE_SMARTSAVE_SHORT,
    PROFILE_ULTRASAVE_SHORT,
    PROFILE_PERFORMANCE_LONG,
    PROFILE_BALANCED_LONG,
    PROFILE_SMARTSAVE_LONG,
    PROFILE_ULTRASAVE_LONG,
    HUD_IDLE_STABLE,
    ACTION_OPEN_DASHBOARD,
    ACTION_OPEN_BATTERY_REPORT,
    ACTION_OPEN_SYSMONITOR,
    DEV_CPU_PKG,
    DEV_GPU_SILICON,
    DEV_DISPLAY,
    DEV_STORAGE,
    DEV_COOLING_FAN,
    DEV_PLATFORM,
    CLI_STATUS_HEADER,
    CLI_TOTAL_DRAIN,
    CLI_CPU_DRAIN,
    CLI_GPU_DRAIN,
    CLI_BATTERY_LEVEL,
    CLI_WAKEUPS,
    CLI_COOLING_FAN,
    CLI_ACTIVE_MITIGATIONS,
    CLI_TOP_CULPRIT,
    DASH_TOTAL_DRAIN,
    DASH_CPU_MEM_SUBSYSTEM,
    DASH_BATTERY_POWER_SUPPLY,
    DASH_GPU_SILICON_LOAD,
    DASH_DISPLAY_BACKLIGHT,
    DASH_STORAGE_NVME,
    DASH_POWER_SHARE_HW,
    DASH_POWER_SHARE_PROC,
    DASH_TIMELINE,
    DASH_DETAILS,
    COUNT
};

// Global active language getter and setter (Atomic, thread-safe, 0 allocations)
Language get_active_language() noexcept;
void set_language(Language lang) noexcept;

// Auto-detect language from system environment (WATTCURB_LANG -> LC_ALL -> LC_MESSAGES -> LANG -> Default EN)
Language detect_system_language() noexcept;
void init_from_system() noexcept;

// Language metadata
const char* get_language_code(Language lang) noexcept;
const char* get_language_name(Language lang) noexcept;

// O(1) Translation lookup (Returns static const char* in .rodata, 0 heap allocations)
const char* tr(StringId id) noexcept;
const char* tr(StringId id, Language lang) noexcept;

// Parsing helpers
std::optional<Language> parse_language_code(std::string_view code) noexcept;
std::optional<StringId> parse_string_key(std::string_view key) noexcept;

} // namespace wattcurb::core::l10n
