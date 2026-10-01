#pragma once

#include <cstdint>
#include <cstddef>
#include <string_view>
#include "core/types.hpp"
#include "core/scoped_profiler.hpp"

namespace wattcurb::policy {

// Implements REF-REQ-136, REF-ARCH-083:
// SIMD-Vectorized Profile Fan Curve Engine & Failsafe Controller
//
// Generates monotone cubic Hermite splines for ThinkPad EC cooling fans across
// the [30°C, 70°C] domain, evaluating 41 integer temperature points with SIMD
// vectorization and guaranteeing a 100% full-speed safety ceiling at >= 70°C.
// Supports up to 10 control points per profile with L1D cache-line aligned SoA lookup.

struct FanControlPoint {
    float temp_c{30.0f};    // Clamped [30.0, 70.0]
    float speed_pct{0.0f};  // Clamped [0.0, 100.0]
};

struct alignas(64) ProfileFanCurve {
    uint8_t point_count{0};
    FanControlPoint points[10]{}; // Expanded up to 10 points (REF-REQ-136.2)
    // 41 integer lookup levels: Index 0 = 30°C, Index 40 = 70°C
    // Padded to 48 elements for 16B SIMD and 64B cache alignment
    alignas(64) uint8_t lookup_levels[48]{};
    alignas(64) float lookup_pcts[48]{};
    bool is_custom{false};
};

class FanCurveEngine {
public:
    static constexpr float MIN_TEMP_C = 30.0f;
    static constexpr float MAX_TEMP_C = 70.0f;
    static constexpr size_t LOOKUP_TABLE_SIZE = 41; // 70 - 30 + 1
    static constexpr size_t MAX_POINTS = 10;        // Expanded to 10 control points
    static constexpr size_t MIN_POINTS = 2;
    static constexpr uint8_t FAN_LEVEL_FULL_SPEED = 8; // Sentinel for "full-speed" keyword

    FanCurveEngine() noexcept;
    ~FanCurveEngine() noexcept = default;

    bool initialize() noexcept;

    // Resets all or specific profile curves to factory defaults
    void reset_to_defaults(int profile_idx = -1) noexcept;

    // Sets custom points for a specific profile (0=Perf, 1=Balanced, 2=Save, 3=Ultra)
    // auto_save is false by default to prevent synchronous I/O churn during interactive UI drags
    bool set_profile_curve(int profile_idx, const FanControlPoint* points, size_t count, bool auto_save = false) noexcept;

    // Evaluates curve using SIMD-vectorized Monotone Cubic Hermite Spline
    static bool compute_spline_lookup(const FanControlPoint* points, size_t count,
                                      uint8_t* out_levels, float* out_pcts,
                                      bool allow_fan_stop) noexcept;

    // Fast O(1) query for runtime monitoring cycle (L1D Cache hit optimized)
    [[nodiscard]] inline uint8_t get_fan_level_for_temp(int profile_idx, double temp_c) const noexcept {
        WATTCURB_PROFILE_SCOPE("FanCurve::GetLevel");
        if (profile_idx < 0 || profile_idx > 3) [[unlikely]] profile_idx = 1;
        if (temp_c <= 0.0) [[unlikely]] return 0;
        if (temp_c >= MAX_TEMP_C) [[unlikely]] return FAN_LEVEL_FULL_SPEED;
        if (temp_c <= MIN_TEMP_C) [[unlikely]] return m_fast_levels[profile_idx][0];

        int idx = static_cast<int>(temp_c) - static_cast<int>(MIN_TEMP_C);
        return m_fast_levels[profile_idx][idx];
    }

    [[nodiscard]] inline float get_fan_pct_for_temp(int profile_idx, double temp_c) const noexcept {
        WATTCURB_PROFILE_SCOPE("FanCurve::GetPct");
        if (profile_idx < 0 || profile_idx > 3) [[unlikely]] profile_idx = 1;
        if (temp_c <= 0.0) [[unlikely]] return 0.0f;
        if (temp_c >= MAX_TEMP_C) [[unlikely]] return 100.0f;
        if (temp_c <= MIN_TEMP_C) [[unlikely]] return m_fast_pcts[profile_idx][0];

        int idx = static_cast<int>(temp_c) - static_cast<int>(MIN_TEMP_C);
        return m_fast_pcts[profile_idx][idx];
    }

    [[nodiscard]] const ProfileFanCurve& get_profile_curve(int profile_idx) const noexcept;
    [[nodiscard]] bool is_profile_custom(int profile_idx) const noexcept;

    // Persistence
    bool load_from_file(const char* path = "/etc/wattcurb/fan_curves.conf") noexcept;
    bool save_to_file(const char* path = "/etc/wattcurb/fan_curves.conf") const noexcept;

    // Unit conversion
    [[nodiscard]] static uint8_t pct_to_thinkpad_level(float pct, bool allow_fan_stop) noexcept;

private:
    void sync_fast_lookup(int profile_idx) noexcept;

    // Hot-Path contiguous lookup cache:
    // 4 profiles x 48 bytes = 192 bytes total! Fits strictly in 3 cache lines (100% L1D residency)
    alignas(64) uint8_t m_fast_levels[4][48]{};
    alignas(64) float m_fast_pcts[4][48]{};

    // Cold-Path full curve configuration and control point data
    ProfileFanCurve m_curves[4]{};
};

} // namespace wattcurb::policy
