# [REF-REQ-076] Multilingual Localization (l10n) and Automatic System Locale Selection

## 1. Overview & Business Rationale
WattCurb is an ultra-low-overhead Linux power daemon and KDE Plasma 6 desktop power monitoring suite. To serve users across global Linux distributions and diverse regions, WattCurb must provide comprehensive native localization (l10n) covering the **top 12 languages in the world by population** (plus Korean as the native development and user interaction language), while adhering strictly to the **Zero-Allocation and Zero-Wakeup** design principles.

Language selection must be fully automatic based on the user's host environment (`LANG`, `LC_MESSAGES`, `LC_ALL`), with immediate zero-overhead fallback to English for unsupported or malformed locales, and explicit override capabilities via `WATTCURB_LANG`.

---

## 2. Supported Languages (World Population Ranking)

| Rank | Language | ISO 639-1 | Native Name | Est. Global Speakers |
| :---: | :--- | :---: | :--- | :---: |
| 1 | **English** | `en` | English (Fallback Default) | ~1.5 Billion |
| 2 | **Mandarin Chinese** | `zh` | 简体中文 (Simplified Chinese) | ~1.1 Billion |
| 3 | **Hindi** | `hi` | हिन्दी | ~610 Million |
| 4 | **Spanish** | `es` | Español | ~560 Million |
| 5 | **French** | `fr` | Français | ~310 Million |
| 6 | **Modern Standard Arabic** | `ar` | العربية | ~274 Million |
| 7 | **Bengali** | `bn` | বাংলা | ~273 Million |
| 8 | **Portuguese** | `pt` | Português | ~264 Million |
| 9 | **Russian** | `ru` | Русский | ~255 Million |
| 10 | **Urdu** | `ur` | اردو | ~232 Million |
| 11 | **Indonesian** | `id` | Bahasa Indonesia | ~200 Million |
| 12 | **German** | `de` | Deutsch | ~135 Million |
| **+** | **Korean** | `ko` | 한국어 (Native Dev / Desktop Language) | ~82 Million |

---

## 3. Detailed Functional Requirements

### 3.1 Zero-Heap String Table & O(1) Translation (`REF-REQ-076.1`)
- All localized strings must reside in a static compile-time `.rodata` matrix: `const char* const STRING_TABLE[LANGUAGE_COUNT][STRING_ID_COUNT]`.
- String lookup via `wattcurb::core::l10n::tr(StringId id)` must execute in $O(1)$ time with **0 heap allocations** (`malloc`, `new`, `std::string`) and no runtime dictionary lookups.
- Execution latency must remain below **5 nanoseconds** per translation call, preserving L1 Data Cache locality.

### 3.2 Automatic System Locale Detection & Parsing (`REF-REQ-076.2`)
- At process bootstrap, WattCurb components (`wattcurb`, `wattcurb-tray`, `wattcurb-dashboard`) must interrogate the host locale in priority order:
  1. `WATTCURB_LANG` environment variable (manual user override).
  2. `LC_ALL` environment variable.
  3. `LC_MESSAGES` environment variable.
  4. `LANG` environment variable.
- The parser must support standard POSIX locale strings (e.g., `ko_KR.UTF-8`, `zh_CN.UTF-8`, `es_ES@euro`, `de_DE.utf8`, `fr_FR`) as well as ISO-639-1 and ISO-639-2 codes (`en`, `eng`, `zh`, `zho`, `hi`, `hin`, etc.).
- If an unsupported locale or empty environment is encountered, the system must fallback gracefully to `Language::EN` without errors or warnings.

### 3.3 Universal Component Coverage (`REF-REQ-076.3`)
Localization must be applied across all user-facing interfaces:
1. **Desktop Tray Client (`wattcurb-tray`)**:
   - Status titles and tray tooltip Cyber HUD.
   - Power profile names (Performance, Balanced, Smart Save, Ultra Save).
   - Battery state descriptions ("Discharging", "%u min left", "AC Passthrough", "AC Charging").
   - DBusMenu context actions ("Open Matrix Dashboard", "Open KDE System Monitor").
2. **Matrix Dashboard (`wattcurb-dashboard`)**:
   - QML header badges, battery status, and profile indicators.
   - Hardware power share category labels (CPU Package, GPU Silicon, Display & Backlight, NVMe SSD, Cooling Fan, Platform/Motherboard).
   - Real-time time-to-empty string formatters.
   - QML helper `backend.tr("KEY")` for dynamic UI text rendering.
3. **CLI & Terminal Reporting (`wattcurb --status`)**:
   - Resident daemon status output headers and metric labels.

---

## 4. Non-Functional & Verification Constraints
- **Zero-Wakeup Invariant**: Language detection and translation lookups must not spawn threads, perform socket I/O, or create disk reads during steady-state monitoring loops.
- **Oracle Gate Testing**: Automated unit tests must verify:
  - 100% translation coverage across all 13 languages for every `StringId`.
  - Deterministic locale resolution from diverse POSIX strings (`es_MX.UTF-8`, `zh_TW`, `pt_BR`, `ko_KR`).
  - Fallback correctness for invalid or empty inputs.
