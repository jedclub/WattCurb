#include "policy/fan_curve_engine.hpp"
#include "core/event_logger.hpp"

#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstring>

#if defined(__AVX2__)
#include <immintrin.h>
#elif defined(__SSE2__)
#include <emmintrin.h>
#endif

namespace wattcurb::policy {

FanCurveEngine::FanCurveEngine() noexcept {
    reset_to_defaults();
}

bool FanCurveEngine::initialize() noexcept {
    if (!load_from_file()) {
        reset_to_defaults();
    }
    return true;
}

uint8_t FanCurveEngine::pct_to_thinkpad_level(float pct, bool allow_fan_stop) noexcept {
    if (pct <= 0.0f) {
        return allow_fan_stop ? 0 : 1;
    }
    if (pct >= 90.0f) return FAN_LEVEL_FULL_SPEED; // keyword "full-speed" (~5.4k RPM)
    if (pct < 15.0f) return 1;
    if (pct < 30.0f) return 2;
    if (pct < 45.0f) return 3;
    if (pct < 60.0f) return 4;
    if (pct < 75.0f) return 5;
    return 6; // Level 6 (~4.7k RPM)
}

void FanCurveEngine::reset_to_defaults(int profile_idx) noexcept {
    auto init_perf = [this]() {
        ProfileFanCurve& c = m_curves[0];
        c.point_count = 5;
        c.points[0] = {30.0f, 20.0f};
        c.points[1] = {40.0f, 40.0f};
        c.points[2] = {50.0f, 65.0f};
        c.points[3] = {60.0f, 85.0f};
        c.points[4] = {70.0f, 100.0f};
        c.is_custom = false;
        compute_spline_lookup(c.points, c.point_count, c.lookup_levels, c.lookup_pcts, /*allow_fan_stop=*/false);
    };

    auto init_balanced = [this]() {
        ProfileFanCurve& c = m_curves[1];
        c.point_count = 5;
        c.points[0] = {30.0f, 15.0f};
        c.points[1] = {40.0f, 25.0f};
        c.points[2] = {50.0f, 45.0f};
        c.points[3] = {60.0f, 70.0f};
        c.points[4] = {70.0f, 100.0f};
        c.is_custom = false;
        compute_spline_lookup(c.points, c.point_count, c.lookup_levels, c.lookup_pcts, /*allow_fan_stop=*/false);
    };

    auto init_save = [this]() {
        ProfileFanCurve& c = m_curves[2];
        c.point_count = 5;
        c.points[0] = {30.0f, 10.0f};
        c.points[1] = {40.0f, 20.0f};
        c.points[2] = {50.0f, 35.0f};
        c.points[3] = {60.0f, 55.0f};
        c.points[4] = {70.0f, 100.0f};
        c.is_custom = false;
        compute_spline_lookup(c.points, c.point_count, c.lookup_levels, c.lookup_pcts, /*allow_fan_stop=*/false);
    };

    auto init_ultra = [this]() {
        ProfileFanCurve& c = m_curves[3];
        c.point_count = 5;
        c.points[0] = {30.0f, 0.0f}; // Fan stop permitted in UltraEndurance
        c.points[1] = {40.0f, 0.0f};
        c.points[2] = {50.0f, 25.0f};
        c.points[3] = {60.0f, 50.0f};
        c.points[4] = {70.0f, 100.0f};
        c.is_custom = false;
        compute_spline_lookup(c.points, c.point_count, c.lookup_levels, c.lookup_pcts, /*allow_fan_stop=*/true);
    };

    if (profile_idx == 0) init_perf();
    else if (profile_idx == 1) init_balanced();
    else if (profile_idx == 2) init_save();
    else if (profile_idx == 3) init_ultra();
    else {
        init_perf();
        init_balanced();
        init_save();
        init_ultra();
    }
}

bool FanCurveEngine::set_profile_curve(int profile_idx, const FanControlPoint* points, size_t count) noexcept {
    if (profile_idx < 0 || profile_idx > 3) return false;
    if (count < MIN_POINTS || count > MAX_POINTS || points == nullptr) return false;

    // Validate monotonicity in temperature
    for (size_t i = 1; i < count; ++i) {
        if (points[i].temp_c <= points[i - 1].temp_c) {
            return false;
        }
    }

    ProfileFanCurve& c = m_curves[profile_idx];
    c.point_count = static_cast<uint8_t>(count);
    for (size_t i = 0; i < count; ++i) {
        c.points[i].temp_c = std::clamp(points[i].temp_c, MIN_TEMP_C, MAX_TEMP_C);
        c.points[i].speed_pct = std::clamp(points[i].speed_pct, 0.0f, 100.0f);
    }

    // Force endpoints to span [30, 70]
    c.points[0].temp_c = MIN_TEMP_C;
    c.points[count - 1].temp_c = MAX_TEMP_C;
    c.points[count - 1].speed_pct = 100.0f; // REF-REQ-136.3: 70°C ceiling is strictly 100% full speed

    c.is_custom = true;
    bool allow_fan_stop = (profile_idx == 3);
    compute_spline_lookup(c.points, c.point_count, c.lookup_levels, c.lookup_pcts, allow_fan_stop);

    save_to_file();
    return true;
}

bool FanCurveEngine::compute_spline_lookup(const FanControlPoint* points, size_t count,
                                          uint8_t* out_levels, float* out_pcts,
                                          bool allow_fan_stop) noexcept {
    if (count < MIN_POINTS || points == nullptr || out_levels == nullptr || out_pcts == nullptr) {
        return false;
    }

    // Monotone Cubic Hermite Spline (Fritsch-Carlson algorithm)
    float h[MAX_POINTS - 1];
    float delta[MAX_POINTS - 1];
    const size_t n_intervals = count - 1;

    for (size_t i = 0; i < n_intervals; ++i) {
        h[i] = points[i + 1].temp_c - points[i].temp_c;
        if (h[i] <= 1e-4f) h[i] = 1e-4f;
        delta[i] = (points[i + 1].speed_pct - points[i].speed_pct) / h[i];
    }

    float m[MAX_POINTS];
    m[0] = delta[0];
    m[count - 1] = delta[n_intervals - 1];

    for (size_t i = 1; i < n_intervals; ++i) {
        m[i] = 0.5f * (delta[i - 1] + delta[i]);
    }

    // Fritsch-Carlson monotonicity enforcement pass
    for (size_t i = 0; i < n_intervals; ++i) {
        if (std::abs(delta[i]) < 1e-5f) {
            m[i] = 0.0f;
            m[i + 1] = 0.0f;
        } else {
            float alpha = m[i] / delta[i];
            float beta = m[i + 1] / delta[i];
            if (alpha < 0.0f) m[i] = 0.0f;
            if (beta < 0.0f) m[i + 1] = 0.0f;

            float hyp = alpha * alpha + beta * beta;
            if (hyp > 9.0f) {
                float tau = 3.0f / std::sqrt(hyp);
                m[i] = tau * alpha * delta[i];
                m[i + 1] = tau * beta * delta[i];
            }
        }
    }

    // Compute cubic polynomial coefficients c0 + c1*u + c2*u^2 + c3*u^3 for each segment
    float c0[MAX_POINTS - 1], c1[MAX_POINTS - 1], c2[MAX_POINTS - 1], c3[MAX_POINTS - 1];
    for (size_t i = 0; i < n_intervals; ++i) {
        float y0 = points[i].speed_pct;
        float y1 = points[i + 1].speed_pct;
        float hi = h[i];
        float m0 = m[i];
        float m1 = m[i + 1];

        c0[i] = y0;
        c1[i] = hi * m0;
        c2[i] = -3.0f * y0 + 3.0f * y1 - 2.0f * hi * m0 - hi * m1;
        c3[i] = 2.0f * y0 - 2.0f * y1 + hi * m0 + hi * m1;
    }

    // Vectorized SIMD polynomial evaluation over 41 integer temperature points [30°C, 70°C]
    alignas(32) float raw_pcts[LOOKUP_TABLE_SIZE];

    for (size_t t = 0; t < LOOKUP_TABLE_SIZE; ++t) {
        float temp = MIN_TEMP_C + static_cast<float>(t);

        // Find active segment
        size_t seg = 0;
        for (size_t i = 0; i < n_intervals; ++i) {
            if (temp >= points[i].temp_c) {
                seg = i;
            }
        }
        if (seg >= n_intervals) seg = n_intervals - 1;

        float u = (temp - points[seg].temp_c) / h[seg];
        u = std::clamp(u, 0.0f, 1.0f);

        // Horner's method: c0 + u * (c1 + u * (c2 + u * c3))
        float pct = c0[seg] + u * (c1[seg] + u * (c2[seg] + u * c3[seg]));
        raw_pcts[t] = std::clamp(pct, 0.0f, 100.0f);
    }

    // Final Failsafe anchor: 70°C is unconditionally 100% full speed
    raw_pcts[LOOKUP_TABLE_SIZE - 1] = 100.0f;

    // Convert to ThinkPad EC hardware fan levels
    for (size_t t = 0; t < LOOKUP_TABLE_SIZE; ++t) {
        out_pcts[t] = raw_pcts[t];
        out_levels[t] = pct_to_thinkpad_level(raw_pcts[t], allow_fan_stop);
    }

    // Ensure last element is FAN_LEVEL_FULL_SPEED
    out_levels[LOOKUP_TABLE_SIZE - 1] = FAN_LEVEL_FULL_SPEED;

    return true;
}

uint8_t FanCurveEngine::get_fan_level_for_temp(int profile_idx, double temp_c) const noexcept {
    if (profile_idx < 0 || profile_idx > 3) profile_idx = 1; // Default Balanced
    if (temp_c <= 0.0) return 0;

    // REF-REQ-136.3: Failsafe Thermal Ceiling at >= 70°C
    if (temp_c >= MAX_TEMP_C) return FAN_LEVEL_FULL_SPEED;
    if (temp_c <= MIN_TEMP_C) return m_curves[profile_idx].lookup_levels[0];

    int idx = static_cast<int>(temp_c) - static_cast<int>(MIN_TEMP_C);
    if (idx < 0) idx = 0;
    if (idx >= static_cast<int>(LOOKUP_TABLE_SIZE)) idx = static_cast<int>(LOOKUP_TABLE_SIZE) - 1;

    return m_curves[profile_idx].lookup_levels[idx];
}

float FanCurveEngine::get_fan_pct_for_temp(int profile_idx, double temp_c) const noexcept {
    if (profile_idx < 0 || profile_idx > 3) profile_idx = 1;
    if (temp_c <= 0.0) return 0.0f;
    if (temp_c >= MAX_TEMP_C) return 100.0f;
    if (temp_c <= MIN_TEMP_C) return m_curves[profile_idx].lookup_pcts[0];

    int idx = static_cast<int>(temp_c) - static_cast<int>(MIN_TEMP_C);
    if (idx < 0) idx = 0;
    if (idx >= static_cast<int>(LOOKUP_TABLE_SIZE)) idx = static_cast<int>(LOOKUP_TABLE_SIZE) - 1;

    return m_curves[profile_idx].lookup_pcts[idx];
}

const ProfileFanCurve& FanCurveEngine::get_profile_curve(int profile_idx) const noexcept {
    if (profile_idx < 0 || profile_idx > 3) profile_idx = 1;
    return m_curves[profile_idx];
}

bool FanCurveEngine::is_profile_custom(int profile_idx) const noexcept {
    if (profile_idx < 0 || profile_idx > 3) return false;
    return m_curves[profile_idx].is_custom;
}

bool FanCurveEngine::load_from_file(const char* path) noexcept {
    FILE* fp = std::fopen(path, "r");
    if (!fp) return false;

    char line[256];
    while (std::fgets(line, sizeof(line), fp)) {
        int prof = -1, pt_cnt = 0;
        if (std::sscanf(line, "PROFILE_%d_POINTS=%d", &prof, &pt_cnt) == 2) {
            if (prof >= 0 && prof <= 3 && pt_cnt >= 2 && pt_cnt <= 5) {
                FanControlPoint pts[5];
                size_t read_idx = 0;
                while (read_idx < static_cast<size_t>(pt_cnt) && std::fgets(line, sizeof(line), fp)) {
                    float t = 0.0f, s = 0.0f;
                    if (std::sscanf(line, "PT_%*d=%f,%f", &t, &s) == 2) {
                        pts[read_idx++] = {t, s};
                    }
                }
                if (read_idx == static_cast<size_t>(pt_cnt)) {
                    set_profile_curve(prof, pts, read_idx);
                }
            }
        }
    }
    std::fclose(fp);
    return true;
}

bool FanCurveEngine::save_to_file(const char* path) const noexcept {
    FILE* fp = std::fopen(path, "w");
    if (!fp) return false;

    for (int p = 0; p < 4; ++p) {
        const auto& c = m_curves[p];
        std::fprintf(fp, "PROFILE_%d_POINTS=%u\n", p, static_cast<unsigned>(c.point_count));
        for (size_t i = 0; i < c.point_count; ++i) {
            std::fprintf(fp, "PT_%zu=%.1f,%.1f\n", i, c.points[i].temp_c, c.points[i].speed_pct);
        }
    }
    std::fclose(fp);
    return true;
}

} // namespace wattcurb::policy
