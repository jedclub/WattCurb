#pragma once

#include "core/types.hpp"
#include <cstddef>
#include <cstdint>

namespace wattcurb::tray {

// Implements REF-REQ-095 & REF-ARCH-071: Watt-Reactive Procedural Tray Icon
//
// The tray previously referenced static freedesktop names
// ("battery-070-profile-performance"), so its badge colour was whatever the
// system theme happened to ship. Continuous watt-driven colour needs the icon
// to be drawn, so it is rasterised here and served over SNI IconPixmap.

inline constexpr int ICON_MAX_SIZE = 64;

struct IconBitmap {
    int size{0};
    uint32_t px[ICON_MAX_SIZE * ICON_MAX_SIZE]{}; // 0xAARRGGBB, host byte order
};

struct IconInputs {
    PowerProfileMode profile{PowerProfileMode::Balanced};
    double system_watts{0.0};
    uint8_t battery_percent{0};
    bool charging{false};
};

// Per-profile watt band. `t` = 0.0 at green_w and below, 1.0 at red_w and above,
// so each profile expresses colour across its own predictable range rather than
// one global scale on which the saving profiles would never leave green.
struct WattBand {
    double green_w;
    double red_w;
};

[[nodiscard]] WattBand profile_band(PowerProfileMode profile) noexcept;

// Position of `watts` inside the profile's band, clamped to [0, 1].
[[nodiscard]] double watt_ratio(PowerProfileMode profile, double watts) noexcept;

// Four-stop ramp: green -> lime -> amber -> red. Returns 0xRRGGBB.
[[nodiscard]] uint32_t watt_color(double t) noexcept;

// Battery frame colour: bright blue while charging, otherwise white fading to
// red as the charge falls from 30% to 0%.
[[nodiscard]] uint32_t frame_color(bool charging, uint8_t battery_percent) noexcept;

// Where glyph artwork comes from. Auto prefers an installed icon font and falls
// back to the built-in procedural glyphs; Procedural forces the fallback, which
// is what the dial-fill Oracle Gate exercises since a font glyph is static.
enum class GlyphSource : uint8_t { Auto = 0, Procedural = 1 };

void set_glyph_source(GlyphSource source) noexcept;
[[nodiscard]] GlyphSource glyph_source() noexcept;

// True when an icon font carrying the whole Material set was found.
[[nodiscard]] bool icon_font_available() noexcept;

void render_tray_icon(const IconInputs& in, IconBitmap& out, int size) noexcept;

} // namespace wattcurb::tray
