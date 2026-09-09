#pragma once

#include <cstdint>
#include <string_view>
#include <immintrin.h>

namespace wattcurb::core {

// Implements REF-REQ-009: CPU Feature Detection & SIMD Optimization
struct CpuFeatures {
    bool has_avx2{false};
    bool has_bmi1{false};
    bool has_bmi2{false};
    bool has_popcnt{false};
    bool has_avx512f{false};

    static const CpuFeatures& instance() noexcept {
        static const CpuFeatures feats = detect();
        return feats;
    }

private:
    static CpuFeatures detect() noexcept {
        CpuFeatures f;
#if defined(__x86_64__) || defined(_M_X64)
        __builtin_cpu_init();
        f.has_avx2 = __builtin_cpu_supports("avx2");
        f.has_bmi1 = __builtin_cpu_supports("bmi");
        f.has_bmi2 = __builtin_cpu_supports("bmi2");
        f.has_popcnt = __builtin_cpu_supports("popcnt");
        f.has_avx512f = __builtin_cpu_supports("avx512f");
#endif
        return f;
    }
};

// Fast SIMD-accelerated string scanning routines
namespace simd {

// AVX2 32-byte chunk scanner for target character
inline const char* find_char_avx2(const char* start, const char* end, char target) noexcept {
    const __m256i target_vec = _mm256_set1_epi8(target);
    const char* ptr = start;

    while (ptr + 32 <= end) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr));
        __m256i cmp = _mm256_cmpeq_epi8(chunk, target_vec);
        uint32_t mask = static_cast<uint32_t>(_mm256_movemask_epi8(cmp));

        if (mask != 0) {
            uint32_t index = static_cast<uint32_t>(_tzcnt_u32(mask));
            return ptr + index;
        }
        ptr += 32;
    }

    // Scalar fallback for trailing bytes
    while (ptr < end) {
        if (*ptr == target) return ptr;
        ++ptr;
    }
    return end;
}

// Scalar fallback for CPUs without AVX2
inline const char* find_char_scalar(const char* start, const char* end, char target) noexcept {
    const char* ptr = start;
    while (ptr < end) {
        if (*ptr == target) return ptr;
        ++ptr;
    }
    return end;
}

// Zero-overhead function pointer resolved at startup
using FindCharFn = const char* (*)(const char*, const char*, char) noexcept;

inline FindCharFn resolve_find_char() noexcept {
    if (CpuFeatures::instance().has_avx2) {
        return &find_char_avx2;
    }
    return &find_char_scalar;
}

inline const char* find_char_fast(const char* start, const char* end, char target) noexcept {
#if defined(__AVX2__)
    // Direct compile-time specialization when built with -march=native / AVX2
    return find_char_avx2(start, end, target);
#else
    // Dynamic runtime CPUID dispatch
    static const FindCharFn fn = resolve_find_char();
    return fn(start, end, target);
#endif
}

} // namespace simd

} // namespace wattcurb::core
