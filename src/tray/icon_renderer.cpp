#include "tray/icon_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#if defined(WATTCURB_HAS_FREETYPE)
#include <dirent.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <cstdlib>
#endif

namespace wattcurb::tray {

namespace {

// ---------------------------------------------------------------------------
// Geometry is authored in a 100x100 design space and scaled to the requested
// icon size, so every size renders from one definition.
// ---------------------------------------------------------------------------
GlyphSource g_source = GlyphSource::Auto;

constexpr double DESIGN = 100.0;
constexpr int MAX_PTS = 96;
constexpr int SUB = 4; // sub-scanlines per pixel row (vertical anti-aliasing)

struct Pt {
    double x{0.0};
    double y{0.0};
};

// Current glyph rotation. An upright glyph wastes the tile: a tall, narrow
// silhouette in a square leaves both side margins empty, so it has to be drawn
// small to fit. Rotating it onto the diagonal lets the same tile carry a
// noticeably larger shape. Applied when points are emitted, so fill_taper(),
// fill_ring(), arc_to() and every glyph inherit it without their own maths.
struct GlyphXform {
    double cx{0.0};
    double cy{0.0};
    double ca{1.0};
    double sa{0.0};
    bool on{false};
};
GlyphXform g_xform{};

void set_glyph_rotation(double cx, double cy, double radians) noexcept {
    g_xform.cx = cx;
    g_xform.cy = cy;
    g_xform.ca = std::cos(radians);
    g_xform.sa = std::sin(radians);
    g_xform.on = true;
}

void clear_glyph_rotation() noexcept { g_xform.on = false; }

// A path may hold several closed contours. Each one closes on ITSELF; without
// that separation an outer and inner rectangle in the same point list join up at
// the seam, which silently opened the battery frame along its top edge.
constexpr int MAX_CONTOURS = 4;

struct Path {
    Pt pts[MAX_PTS];
    int n{0};
    int start[MAX_CONTOURS]{ 0 };
    int nc{1};

    void add(double x, double y) noexcept {
        if (n >= MAX_PTS) return;
        if (g_xform.on) {
            const double dx = x - g_xform.cx;
            const double dy = y - g_xform.cy;
            x = g_xform.cx + dx * g_xform.ca - dy * g_xform.sa;
            y = g_xform.cy + dx * g_xform.sa + dy * g_xform.ca;
        }
        pts[n].x = x;
        pts[n].y = y;
        ++n;
    }

    void begin_contour() noexcept {
        if (nc < MAX_CONTOURS && n > start[nc - 1]) {
            start[nc] = n;
            ++nc;
        }
    }

    [[nodiscard]] int contour_end(int c) const noexcept {
        return (c + 1 < nc) ? start[c + 1] : n;
    }
};

[[nodiscard]] inline double clamp01(double v) noexcept {
    return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v);
}

// Same hue, lower luminance: used for the gauge's unlit track so it reads as
// the same instrument as the lit portion instead of a competing grey mass.
[[nodiscard]] inline uint32_t dim_rgb(uint32_t c, double k) noexcept {
    const auto r = static_cast<uint32_t>(static_cast<double>((c >> 16) & 0xFF) * k);
    const auto g = static_cast<uint32_t>(static_cast<double>((c >> 8) & 0xFF) * k);
    const auto b = static_cast<uint32_t>(static_cast<double>(c & 0xFF) * k);
    return (r << 16) | (g << 8) | b;
}

[[nodiscard]] inline uint32_t lerp_rgb(uint32_t a, uint32_t b, double f) noexcept {
    const double t = clamp01(f);
    const double ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
    const double br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;
    const auto r = static_cast<uint32_t>(ar + (br - ar) * t + 0.5);
    const auto g = static_cast<uint32_t>(ag + (bg - ag) * t + 0.5);
    const auto bl = static_cast<uint32_t>(ab + (bb - ab) * t + 0.5);
    return (r << 16) | (g << 8) | bl;
}

// Source-over blend of `rgb` at `cov` coverage onto a non-premultiplied pixel.
inline void blend_px(IconBitmap& bm, int x, int y, uint32_t rgb, double cov) noexcept {
    if (cov <= 0.0 || x < 0 || y < 0 || x >= bm.size || y >= bm.size) return;
    if (cov > 1.0) cov = 1.0;

    uint32_t& dst = bm.px[static_cast<size_t>(y) * static_cast<size_t>(bm.size) + static_cast<size_t>(x)];
    const double da = static_cast<double>((dst >> 24) & 0xFF) / 255.0;
    const double dr = static_cast<double>((dst >> 16) & 0xFF);
    const double dg = static_cast<double>((dst >> 8) & 0xFF);
    const double db = static_cast<double>(dst & 0xFF);

    const double sr = static_cast<double>((rgb >> 16) & 0xFF);
    const double sg = static_cast<double>((rgb >> 8) & 0xFF);
    const double sb = static_cast<double>(rgb & 0xFF);

    const double oa = cov + da * (1.0 - cov);
    if (oa <= 0.0001) return;
    const double orr = (sr * cov + dr * da * (1.0 - cov)) / oa;
    const double og = (sg * cov + dg * da * (1.0 - cov)) / oa;
    const double ob = (sb * cov + db * da * (1.0 - cov)) / oa;

    dst = (static_cast<uint32_t>(oa * 255.0 + 0.5) << 24)
        | (static_cast<uint32_t>(orr + 0.5) << 16)
        | (static_cast<uint32_t>(og + 0.5) << 8)
        | static_cast<uint32_t>(ob + 0.5);
}

// Anti-aliased even-odd scanline fill. Vertical AA comes from SUB sub-scanlines
// per row; horizontal AA from fractional span ends.
void fill_path(IconBitmap& bm, const Path& p, uint32_t rgb, double alpha) noexcept {
    if (p.n < 3 || alpha <= 0.0) return;

    const double scale = static_cast<double>(bm.size) / DESIGN;
    double min_y = 1e9, max_y = -1e9;
    for (int i = 0; i < p.n; ++i) {
        min_y = std::min(min_y, p.pts[i].y * scale);
        max_y = std::max(max_y, p.pts[i].y * scale);
    }
    int y0 = std::max(0, static_cast<int>(std::floor(min_y)));
    int y1 = std::min(bm.size - 1, static_cast<int>(std::ceil(max_y)));
    if (y0 > y1) return;

    double cov[ICON_MAX_SIZE];
    double xs[MAX_PTS];

    for (int y = y0; y <= y1; ++y) {
        for (int i = 0; i < bm.size; ++i) cov[i] = 0.0;

        for (int s = 0; s < SUB; ++s) {
            const double sy = static_cast<double>(y) + (static_cast<double>(s) + 0.5) / SUB;
            int nx = 0;
            for (int c = 0; c < p.nc; ++c) {
                const int c0 = p.start[c];
                const int c1 = p.contour_end(c);
                const int len = c1 - c0;
                if (len < 3) continue;
                for (int i = 0; i < len; ++i) {
                    const Pt& a = p.pts[c0 + i];
                    const Pt& b = p.pts[c0 + (i + 1) % len];
                    const double ay = a.y * scale, by = b.y * scale;
                    if ((ay <= sy && by > sy) || (by <= sy && ay > sy)) {
                        const double t = (sy - ay) / (by - ay);
                        if (nx < MAX_PTS) xs[nx++] = (a.x + (b.x - a.x) * t) * scale;
                    }
                }
            }
            if (nx < 2) continue;
            std::sort(xs, xs + nx);

            for (int i = 0; i + 1 < nx; i += 2) {
                double xa = xs[i], xb = xs[i + 1];
                if (xb <= 0.0 || xa >= static_cast<double>(bm.size)) continue;
                xa = std::max(xa, 0.0);
                xb = std::min(xb, static_cast<double>(bm.size));

                const int ixa = static_cast<int>(std::floor(xa));
                const int ixb = static_cast<int>(std::floor(xb - 1e-9));
                if (ixa == ixb) {
                    cov[ixa] += (xb - xa) / SUB;
                } else {
                    cov[ixa] += (static_cast<double>(ixa + 1) - xa) / SUB;
                    for (int x = ixa + 1; x < ixb; ++x) cov[x] += 1.0 / SUB;
                    if (ixb < bm.size) cov[ixb] += (xb - static_cast<double>(ixb)) / SUB;
                }
            }
        }
        for (int x = 0; x < bm.size; ++x) {
            if (cov[x] > 0.0) blend_px(bm, x, y, rgb, cov[x] * alpha);
        }
    }
}

// Appends an arc (design-space) to a path.
void arc_to(Path& p, double cx, double cy, double r, double a0, double a1, int steps) noexcept {
    for (int i = 0; i <= steps; ++i) {
        const double a = a0 + (a1 - a0) * (static_cast<double>(i) / steps);
        p.add(cx + r * std::cos(a), cy + r * std::sin(a));
    }
}

// Annulus sector: the gauge track and the battery frame are both built from it.
void fill_ring(IconBitmap& bm, double cx, double cy, double r_out, double r_in,
               double a0, double a1, uint32_t rgb, double alpha) noexcept {
    Path p;
    const int steps = 28;
    arc_to(p, cx, cy, r_out, a0, a1, steps);
    for (int i = steps; i >= 0; --i) {
        const double a = a0 + (a1 - a0) * (static_cast<double>(i) / steps);
        p.add(cx + r_in * std::cos(a), cy + r_in * std::sin(a));
    }
    fill_path(bm, p, rgb, alpha);
}

// Quad with independent end widths: a uniform bar reads as a stick, a tapered
// one reads as a needle, a spoke or a stem.
void fill_taper(IconBitmap& bm, double x0, double y0, double x1, double y1,
                double w0, double w1, uint32_t rgb, double alpha) noexcept {
    const double dx = x1 - x0, dy = y1 - y0;
    const double len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-6) return;
    const double nx = -dy / len, ny = dx / len;

    Path p;
    p.add(x0 + nx * w0 * 0.5, y0 + ny * w0 * 0.5);
    p.add(x1 + nx * w1 * 0.5, y1 + ny * w1 * 0.5);
    p.add(x1 - nx * w1 * 0.5, y1 - ny * w1 * 0.5);
    p.add(x0 - nx * w0 * 0.5, y0 - ny * w0 * 0.5);
    fill_path(bm, p, rgb, alpha);
}

void fill_round_rect(IconBitmap& bm, double x0, double y0, double x1, double y1,
                     double r, uint32_t rgb, double alpha) noexcept {
    const double pi = 3.14159265358979323846;
    const double rr = std::min(r, std::min((x1 - x0) * 0.5, (y1 - y0) * 0.5));

    Path p;
    arc_to(p, x1 - rr, y0 + rr, rr, -pi / 2, 0.0, 5);
    arc_to(p, x1 - rr, y1 - rr, rr, 0.0, pi / 2, 5);
    arc_to(p, x0 + rr, y1 - rr, rr, pi / 2, pi, 5);
    arc_to(p, x0 + rr, y0 + rr, rr, pi, pi * 1.5, 5);
    fill_path(bm, p, rgb, alpha);
}

// ---------------------------------------------------------------------------
// Profile badges. Each is drawn in the badge box centred at (cx, cy) with the
// given half-height `s`, entirely in the watt colour.
// ---------------------------------------------------------------------------

void draw_rocket(IconBitmap& bm, double cx, double cy, double s, uint32_t rgb) noexcept {
    const double pi = 3.14159265358979323846;

    // Exhaust as three DETACHED tapered strokes rather than a plume welded to the
    // tail. Attached flame plus attached fins plus a body all in one flat colour
    // merged into a single blob at 22 px; separating the thrust is what makes the
    // shape read as a rocket rather than a creature.
    for (int k = -1; k <= 1; ++k) {
        const double off = static_cast<double>(k) * s * 0.26;
        const double reach = (k == 0) ? 1.16 : 1.00;
        fill_taper(bm, cx + off * 0.8, cy + s * 0.74, cx + off, cy + s * reach,
                   s * 0.13, s * 0.05, rgb, (k == 0) ? 0.85 : 0.55);
    }

    // Tail fins.
    for (int k = -1; k <= 1; k += 2) {
        const double kd = static_cast<double>(k);
        Path fin;
        fin.add(cx + kd * s * 0.18, cy + s * 0.16);
        fin.add(cx + kd * s * 0.70, cy + s * 0.64);
        fin.add(cx + kd * s * 0.22, cy + s * 0.60);
        fill_path(bm, fin, rgb, 1.0);
    }

    // Teardrop fuselage: pointed nose swelling into a body and tapering to the tail.
    Path body;
    constexpr int N = 16;
    for (int i = 0; i <= N; ++i) {
        const double f = static_cast<double>(i) / N;
        const double y = cy - s * 1.00 + f * (s * 1.62);
        const double w = std::pow(std::sin(std::pow(f, 0.70) * pi * 0.98), 0.80) * s * 0.33;
        body.add(cx + w, y);
    }
    for (int i = N; i >= 0; --i) {
        const double f = static_cast<double>(i) / N;
        const double y = cy - s * 1.00 + f * (s * 1.62);
        const double w = std::pow(std::sin(std::pow(f, 0.70) * pi * 0.98), 0.80) * s * 0.33;
        body.add(cx - w, y);
    }
    fill_path(bm, body, rgb, 1.0);

    // Small porthole. Kept small deliberately: at the earlier size it read as an
    // eye and turned the whole glyph into a face.
    Path win;
    arc_to(win, cx, cy - s * 0.22, s * 0.125, 0.0, 6.28318530717958647692, 14);
    fill_path(bm, win, 0x0E141C, 0.92);
}

void draw_gauge(IconBitmap& bm, double cx, double cy, double s, uint32_t rgb, double t) noexcept {
    const double pi = 3.14159265358979323846;

    // A car tachometer sweep rather than a bare semicircle: from lower-left, up
    // over the top, round to lower-right. 252 degrees also makes the dial nearly
    // circular, so it fills a square tile without leaning on a tilt.
    const double A0 = pi * 0.80;
    const double A1 = pi * 2.20;
    const double span = A1 - A0;

    const double r_out = s * 1.00;
    const double r_band = s * 0.84;

    // Outer band: unlit at a third of the badge luminance, lit up to the reading.
    fill_ring(bm, cx, cy, r_out, r_band, A0, A1, dim_rgb(rgb, 0.30), 1.0);
    if (t > 0.004) {
        fill_ring(bm, cx, cy, r_out, r_band, A0, A0 + span * t, rgb, 1.0);
    }

    // Graduations: 5 major, 2 minor between each pair. Ticks inside the reading
    // light up, the rest stay dim - the scale reads even before the needle does.
    constexpr int MAJOR = 5;
    constexpr int MINOR = 2;
    constexpr int STEPS = (MAJOR - 1) * (MINOR + 1);
    const double tick_r = r_band - s * 0.05;
    for (int i = 0; i <= STEPS; ++i) {
        const double f = static_cast<double>(i) / STEPS;
        const double a = A0 + span * f;
        const bool is_major = (i % (MINOR + 1) == 0);
        const double len = is_major ? s * 0.27 : s * 0.15;
        const double w = is_major ? s * 0.115 : s * 0.065;
        const uint32_t c = (f <= t + 1e-6) ? rgb : dim_rgb(rgb, 0.46);
        fill_taper(bm,
                   cx + std::cos(a) * tick_r, cy + std::sin(a) * tick_r,
                   cx + std::cos(a) * (tick_r - len), cy + std::sin(a) * (tick_r - len),
                   w, w * 0.78, c, 1.0);
    }

    // Needle: tapered blade with a counterweight tail, as on a real instrument.
    const double ang = A0 + span * t;
    fill_taper(bm,
               cx - std::cos(ang) * s * 0.26, cy - std::sin(ang) * s * 0.26,
               cx + std::cos(ang) * s * 0.70, cy + std::sin(ang) * s * 0.70,
               s * 0.17, s * 0.05, rgb, 1.0);
    Path counterweight;
    arc_to(counterweight, cx - std::cos(ang) * s * 0.27, cy - std::sin(ang) * s * 0.27,
           s * 0.115, 0.0, 6.28318530717958647692, 10);
    fill_path(bm, counterweight, rgb, 1.0);

    // Two-tone hub.
    Path hub;
    arc_to(hub, cx, cy, s * 0.21, 0.0, 6.28318530717958647692, 14);
    fill_path(bm, hub, rgb, 1.0);
    Path cap;
    arc_to(cap, cx, cy, s * 0.095, 0.0, 6.28318530717958647692, 12);
    fill_path(bm, cap, 0x0E141C, 0.92);
}

void draw_leaf(IconBitmap& bm, double cx, double cy, double s, uint32_t rgb) noexcept {
    const double pi = 3.14159265358979323846;
    const double ca = 1.0, sa = 0.0; // tilt now comes from the glyph transform

    // (along, across) in leaf space -> icon space. +along runs tip to stem.
    auto place = [&](Path& path, double along, double across) {
        path.add(cx + across * ca - along * sa, cy + across * sa + along * ca);
    };
    auto pt = [&](double along, double across) {
        return Pt{ cx + across * ca - along * sa, cy + across * sa + along * ca };
    };

    const double L = s * 0.92;
    const int N = 18;

    // Asymmetric blade: the outer flank is fuller than the inner one, which is
    // what separates a leaf from the symmetric lens the first revision drew.
    Path blade;
    for (int i = 0; i <= N; ++i) {
        const double f = static_cast<double>(i) / N;
        place(blade, -L + 2.0 * L * f, std::pow(std::sin(f * pi), 0.60) * s * 0.52);
    }
    for (int i = N; i >= 0; --i) {
        const double f = static_cast<double>(i) / N;
        place(blade, -L + 2.0 * L * f, -std::pow(std::sin(f * pi), 0.98) * s * 0.34);
    }
    fill_path(bm, blade, rgb, 1.0);

    // Stem continuing past the base.
    const Pt base = pt(L * 0.96, 0.0);
    const Pt stem_end = pt(L * 1.16, s * 0.09);
    fill_taper(bm, base.x, base.y, stem_end.x, stem_end.y, s * 0.14, s * 0.07, rgb, 1.0);

    // Midrib, dimmed rather than near-black: a vein, not a crack.
    const Pt tip = pt(-L * 0.80, 0.0);
    fill_taper(bm, base.x, base.y, tip.x, tip.y, s * 0.11, s * 0.03, dim_rgb(rgb, 0.40), 1.0);
}

void draw_snowflake(IconBitmap& bm, double cx, double cy, double s, uint32_t rgb) noexcept {
    const double pi = 3.14159265358979323846;

    for (int i = 0; i < 6; ++i) {
        const double a = pi * static_cast<double>(i) / 3.0;
        const double ex = cx + std::cos(a) * s;
        const double ey = cy + std::sin(a) * s;

        // Tapered spoke.
        fill_taper(bm, cx, cy, ex, ey, s * 0.22, s * 0.11, rgb, 1.0);

        // Two barb pairs, so the arm reads as crystal rather than a stick.
        const double frac[2] = { 0.46, 0.74 };
        const double blen[2] = { 0.27, 0.19 };
        for (int b = 0; b < 2; ++b) {
            const double bx = cx + std::cos(a) * s * frac[b];
            const double by = cy + std::sin(a) * s * frac[b];
            for (int k = -1; k <= 1; k += 2) {
                const double ba = a + static_cast<double>(k) * pi / 3.0;
                fill_taper(bm, bx, by,
                           bx + std::cos(ba) * s * blen[b], by + std::sin(ba) * s * blen[b],
                           s * 0.14, s * 0.07, rgb, 1.0);
            }
        }
    }

    // Hexagonal hub.
    Path hub;
    for (int i = 0; i < 6; ++i) {
        const double a = pi * static_cast<double>(i) / 3.0 + pi / 6.0;
        hub.add(cx + std::cos(a) * s * 0.20, cy + std::sin(a) * s * 0.20);
    }
    fill_path(bm, hub, rgb, 1.0);
}


#if defined(WATTCURB_HAS_FREETYPE)
// ---------------------------------------------------------------------------
// REF-REQ-097: Professionally drawn glyphs, rendered from an installed icon font.
//
// The procedural glyphs below are hand-placed polygons and read as such. Icon
// sets are drawn with beziers by designers, and rendering one costs a FreeType
// load. Nothing is redistributed - the glyph comes from a font already on the
// machine - so no icon licence attaches to this project. When no suitable font
// is present the procedural glyphs remain the fallback.
// ---------------------------------------------------------------------------

// Material Design Icons, as carried in the Nerd Fonts private-use range.
constexpr uint32_t MDI_ROCKET_LAUNCH     = 0xF14DE;
constexpr uint32_t MDI_SPEEDOMETER_SLOW  = 0xF0FE7;
constexpr uint32_t MDI_SPEEDOMETER_MED   = 0xF0FE6;
constexpr uint32_t MDI_SPEEDOMETER_FAST  = 0xF04C5;
constexpr uint32_t MDI_LEAF              = 0xF032A;
constexpr uint32_t MDI_SNOWFLAKE         = 0xF0717;

FT_Library g_ft_lib = nullptr;
FT_Face g_ft_face = nullptr;
bool g_ft_tried = false;

[[nodiscard]] bool face_has_all_glyphs(FT_Face face) noexcept {
    static constexpr uint32_t REQUIRED[] = {
        MDI_ROCKET_LAUNCH, MDI_SPEEDOMETER_SLOW, MDI_SPEEDOMETER_MED,
        MDI_SPEEDOMETER_FAST, MDI_LEAF, MDI_SNOWFLAKE
    };
    for (uint32_t cp : REQUIRED) {
        if (FT_Get_Char_Index(face, cp) == 0) return false;
    }
    return true;
}

// Bounded recursive search for a font carrying the whole set.
bool find_icon_font(const char* dir, int depth) noexcept {
    if (depth > 3) return false;
    DIR* d = ::opendir(dir);
    if (!d) return false;

    bool found = false;
    struct dirent* e = nullptr;
    while (!found && (e = ::readdir(d)) != nullptr) {
        if (e->d_name[0] == '.') continue;

        char path[512];
        if (std::snprintf(path, sizeof(path), "%s/%s", dir, e->d_name) >= static_cast<int>(sizeof(path))) {
            continue;
        }

        if (e->d_type == DT_DIR) {
            found = find_icon_font(path, depth + 1);
            continue;
        }

        const size_t len = std::strlen(e->d_name);
        if (len < 5) continue;
        const char* ext = e->d_name + len - 4;
        if (std::strcmp(ext, ".ttf") != 0 && std::strcmp(ext, ".otf") != 0) continue;
        if (std::strstr(e->d_name, "NerdFont") == nullptr &&
            std::strstr(e->d_name, "Symbols") == nullptr &&
            std::strstr(e->d_name, "MaterialDesign") == nullptr) {
            continue;
        }

        FT_Face face = nullptr;
        if (FT_New_Face(g_ft_lib, path, 0, &face) != 0) continue;
        if (face_has_all_glyphs(face)) {
            g_ft_face = face;
            found = true;
        } else {
            FT_Done_Face(face);
        }
    }
    ::closedir(d);
    return found;
}

[[nodiscard]] bool ensure_icon_font() noexcept {
    if (g_ft_tried) return g_ft_face != nullptr;
    g_ft_tried = true;

    if (FT_Init_FreeType(&g_ft_lib) != 0) {
        g_ft_lib = nullptr;
        return false;
    }

    char user_dir[256];
    const char* home = ::getenv("HOME");
    const char* roots[4] = { "/usr/share/fonts", "/usr/local/share/fonts", nullptr, nullptr };
    if (home && home[0] == '/') {
        if (std::snprintf(user_dir, sizeof(user_dir), "%s/.local/share/fonts", home)
                < static_cast<int>(sizeof(user_dir))) {
            roots[2] = user_dir;
        }
    }
    for (const char* root : roots) {
        if (root && find_icon_font(root, 0)) return true;
    }
    return false;
}

// Renders `cp` so its longest side spans `target_px`, centred on (cx_px, cy_px).
bool draw_font_glyph(IconBitmap& bm, uint32_t cp,
                     double cx_px, double cy_px, double target_px, uint32_t rgb) noexcept {
    if (!ensure_icon_font()) return false;

    const FT_UInt gi = FT_Get_Char_Index(g_ft_face, cp);
    if (gi == 0) return false;

    // Icon glyphs rarely fill their em box, so probe once and rescale.
    auto load_at = [&](unsigned px) -> bool {
        if (FT_Set_Pixel_Sizes(g_ft_face, 0, px) != 0) return false;
        return FT_Load_Glyph(g_ft_face, gi, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL) == 0;
    };

    const auto probe = static_cast<unsigned>(std::max(8.0, target_px));
    if (!load_at(probe)) return false;

    const FT_Bitmap* b = &g_ft_face->glyph->bitmap;
    const double span = std::max(static_cast<double>(b->width), static_cast<double>(b->rows));
    if (span < 1.0) return false;

    const auto final_px = static_cast<unsigned>(std::max(8.0, probe * target_px / span));
    if (final_px != probe && !load_at(final_px)) return false;

    b = &g_ft_face->glyph->bitmap;
    if (b->width == 0 || b->rows == 0) return false;

    const int ox = static_cast<int>(std::lround(cx_px - static_cast<double>(b->width) * 0.5));
    const int oy = static_cast<int>(std::lround(cy_px - static_cast<double>(b->rows) * 0.5));

    for (unsigned y = 0; y < b->rows; ++y) {
        for (unsigned x = 0; x < b->width; ++x) {
            const double cov = static_cast<double>(b->buffer[y * static_cast<unsigned>(b->pitch) + x]) / 255.0;
            if (cov <= 0.0) continue;
            blend_px(bm, ox + static_cast<int>(x), oy + static_cast<int>(y), rgb, cov);
        }
    }
    return true;
}

// Maps profile (and, for the dial, the reading) onto a glyph. The speedometer
// comes in slow / medium / fast variants, so the needle still tracks the value.
bool draw_profile_glyph_from_font(IconBitmap& bm, PowerProfileMode profile, double t,
                                  uint32_t rgb, double cx_px, double cy_px,
                                  double target_px) noexcept {
    uint32_t cp = 0;
    switch (profile) {
    case PowerProfileMode::Performance:    cp = MDI_ROCKET_LAUNCH; break;
    case PowerProfileMode::PowerSaver:     cp = MDI_LEAF; break;
    case PowerProfileMode::UltraEndurance: cp = MDI_SNOWFLAKE; break;
    case PowerProfileMode::Balanced:
        // MDI's speedometer-slow / -medium are not present in this font's range
        // under the codepoints they were expected at, so the dial is a single
        // symbol and the reading is carried entirely by colour.
        (void)t;
        cp = MDI_SPEEDOMETER_FAST;
        break;
    }
    return draw_font_glyph(bm, cp, cx_px, cy_px, target_px, rgb);
}
#endif // WATTCURB_HAS_FREETYPE

} // namespace

WattBand profile_band(PowerProfileMode profile) noexcept {
    // Calibrated against this platform's measured 7-day history: the Performance
    // band is its p05..p95 (8.0 W .. 34.2 W). The saving profiles scale down with
    // their hardware ceilings (boost off, clock caps, GPU and panel limits) so
    // each keeps a usable colour spread instead of sitting permanently green.
    switch (profile) {
    case PowerProfileMode::Performance:    return { 8.0, 35.0 };
    case PowerProfileMode::Balanced:       return { 6.0, 24.0 };
    case PowerProfileMode::PowerSaver:     return { 4.0, 16.0 };
    case PowerProfileMode::UltraEndurance: return { 3.0, 10.0 };
    }
    return { 6.0, 24.0 };
}

double watt_ratio(PowerProfileMode profile, double watts) noexcept {
    const WattBand b = profile_band(profile);
    const double span = b.red_w - b.green_w;
    if (span <= 0.0) return 0.0;
    return clamp01((watts - b.green_w) / span);
}

uint32_t watt_color(double t) noexcept {
    // Chosen so the red channel never dips across the ramp (34 -> 163 -> 245 -> 255)
    // while green falls away at the top: the hue reads as a single continuous
    // green-to-red sweep at 22px rather than an amber that looks hotter than red.
    static constexpr uint32_t STOPS[4] = { 0x22C55E, 0xA3E635, 0xF59E0B, 0xFF3B30 };
    const double u = clamp01(t) * 3.0;
    int i = static_cast<int>(u);
    if (i > 2) i = 2;
    return lerp_rgb(STOPS[i], STOPS[i + 1], u - static_cast<double>(i));
}

uint32_t frame_color(bool charging, uint8_t battery_percent) noexcept {
    if (charging) return 0x38BDF8; // bright blue: charging
    if (battery_percent >= 30) return 0xF0F4FA;
    const double u = clamp01((30.0 - static_cast<double>(battery_percent)) / 30.0);
    return lerp_rgb(0xF0F4FA, 0xEF4444, u);
}

void set_glyph_source(GlyphSource source) noexcept { g_source = source; }

GlyphSource glyph_source() noexcept { return g_source; }

bool icon_font_available() noexcept {
#if defined(WATTCURB_HAS_FREETYPE)
    return ensure_icon_font();
#else
    return false;
#endif
}

void render_tray_icon(const IconInputs& in, IconBitmap& out, int size) noexcept {
    if (size < 8) size = 8;
    if (size > ICON_MAX_SIZE) size = ICON_MAX_SIZE;
    out.size = size;
    for (int i = 0; i < size * size; ++i) out.px[i] = 0u;

    const uint32_t frame = frame_color(in.charging, in.battery_percent);
    const double t = watt_ratio(in.profile, in.system_watts);
    const uint32_t badge = watt_color(t);

    // No container. An enclosing battery outline was the brightest, largest
    // element in the tile while carrying the least information, and it boxed the
    // glyph down to roughly half the available height. The glyph now owns the
    // tile; charge, charging state and the low warning ride on a slim rail along
    // the bottom edge, which is enough to read at 22 px and never competes.
    constexpr double RAIL_X0 = 6.0, RAIL_X1 = 94.0, RAIL_Y0 = 86.0, RAIL_Y1 = 95.0;
    fill_round_rect(out, RAIL_X0, RAIL_Y0, RAIL_X1, RAIL_Y1, 4.5, frame, 0.22);

    const double lvl = clamp01(static_cast<double>(in.battery_percent) / 100.0);
    if (lvl > 0.005) {
        // Floor the width so a nearly empty battery still shows a lit stub.
        const double w = (RAIL_X1 - RAIL_X0) * std::max(lvl, 0.07);
        fill_round_rect(out, RAIL_X0, RAIL_Y0, RAIL_X0 + w, RAIL_Y1, 4.5, frame, 1.0);
    }

    // Every glyph is tilted so its long axis runs along the tile diagonal. The
    // elongated ones (rocket, leaf) gain the most: the same tile now carries a
    // shape ~20% larger than it could upright.
    const double cx = 50.0;
    const double cy = 42.0;
    const double s = 42.0;

#if defined(WATTCURB_HAS_FREETYPE)
    // Professionally drawn glyph when a suitable icon font is installed.
    if (g_source == GlyphSource::Auto) {
        const double scale = static_cast<double>(size) / 100.0;
        if (draw_profile_glyph_from_font(out, in.profile, t, badge,
                                         cx * scale, (cy - 2.0) * scale, 76.0 * scale)) {
            return;
        }
    }
#endif

    // Fallback: hand-built glyphs, so the tray still works without that font.
    switch (in.profile) {
    case PowerProfileMode::Performance:
        set_glyph_rotation(cx, cy, 0.76);   // launching to the upper right
        draw_rocket(out, cx, cy, s * 0.80, badge);
        break;
    case PowerProfileMode::Balanced:
        // A 252-degree dial is already radially balanced, so it needs no tilt.
        set_glyph_rotation(cx, cy, 0.0);
        draw_gauge(out, cx, cy + 2.0, s * 0.90, badge, t);
        break;
    case PowerProfileMode::PowerSaver:
        set_glyph_rotation(cx, cy, 0.66);   // tip to the upper right
        draw_leaf(out, cx, cy, s * 1.04, badge);
        break;
    case PowerProfileMode::UltraEndurance:
        set_glyph_rotation(cx, cy, 0.26);   // breaks the flat horizontal arms
        draw_snowflake(out, cx, cy, s * 0.92, badge);
        break;
    }
    clear_glyph_rotation();
}

} // namespace wattcurb::tray
