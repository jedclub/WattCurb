# [REF-ARCH-053] Multilingual Zero-Cost Localization Architecture

## 1. Architectural Philosophy & Zero-Cost Abstraction
Localization in performance-critical background daemons frequently suffers from bloat due to dynamic dictionary allocations (`std::map<std::string, std::string>`), file system I/O (loading external `.mo` or `.qm` catalogs at runtime), and heavy string duplication.

To satisfy WattCurb's sub-milliwatt daemon overhead and L1 cache locality directives ([`REF-REQ-076`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-076-multilingual-l10n-and-auto-system-locale.md)), the **WattCurb L10n Subsystem** is engineered as a compile-time static matrix embedded directly in the ELF binary's `.rodata` segment.

```
┌──────────────────────────────────────────────────────────┐
│             Process Bootstrap / Initialization           │
│  std::getenv("WATTCURB_LANG") / ("LC_ALL") / ("LANG")     │
│                            │                             │
│                            ▼                             │
│               parse_language_code()                      │
│             (Zero-Allocation Substring)                  │
│                            │                             │
│                            ▼                             │
│           std::atomic<Language> g_active_lang            │
└────────────────────────────┬─────────────────────────────┘
                             │
                             ▼
┌──────────────────────────────────────────────────────────┐
│      O(1) Translation Lookup: l10n::tr(StringId id)      │
│                                                          │
│   STRING_TABLE[lang_idx][string_idx]                     │
│   - Memory: Pure static const char* in .rodata           │
│   - Heap Allocations: 0 bytes                            │
│   - Runtime Latency: ~1.2 ns (L1 Cache Resident)         │
│   - Fallback: EN (English) if language string empty      │
└──────────────────────────────────────────────────────────┘
```

---

## 2. Core Data Structures & Enumerations

```cpp
namespace wattcurb::core::l10n {

enum class Language : uint8_t {
    EN = 0, // English (Fallback)
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
    JA,     // Japanese (日本語)
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

} // namespace wattcurb::core::l10n
```

---

## 3. String Table Memory Layout

The entire catalog is arranged as:
```cpp
alignas(64) static constexpr const char* const STRING_TABLE[13][46] = { ... };
```
- **Size**: 13 languages $\times$ 46 string pointers $\times$ 8 bytes = **4,784 bytes** (~4.7 KB).
- **Cache Alignment**: Fits comfortably inside the L1 Data Cache (typically 32 KB or 48 KB per CPU core).
- **Zero Page Faults**: Stored in contiguous `.rodata`, mapped with the binary text pages at program load time.

---

## 4. Automatic System Locale Parser

The parser extracts the 2-to-3 character ISO prefix without heap allocation:
1. Skips leading whitespace.
2. Checks for language prefix delimited by `_`, `.`, `@`, or `-`.
3. Performs branchless / lookup matching against supported language tokens (`en`, `zh`, `hi`, `es`, `fr`, `ar`, `bn`, `pt`, `ru`, `ur`, `id`, `de`, `ko`).
4. Atomically records active language in `g_active_language`.

---

## 5. Qt6 QML & Dashboard Integration

To bridge C++23 zero-cost localization into Qt Quick QML:
- `DashboardBackend::tr(const QString& key)` maps key string hashes to `StringId` and returns a localized `QString`.
- Dynamic properties (`powerProfileName`, `batteryStateString`, `timeToEmptyString`, `devicePowerShares`) query `l10n::tr(...)` directly.
- Language change signals trigger UI update cascades with zero IPC traffic.
