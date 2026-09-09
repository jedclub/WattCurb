#pragma once

#include <cstdint>
#include <string_view>
#include <immintrin.h>
#include <cpuid.h>
#include <cstring>
#include <sched.h>

namespace wattcurb::core {

// Implements REF-REQ-009 & REF-ARCH-005: C++23 Zero-Cost CPUID & Constant-Branch Specialization
enum class CpuFeature : uint64_t {
    NONE    = 0,
    AVX     = 1ULL << 0,
    AVX2    = 1ULL << 1,
    FMA     = 1ULL << 2,
    BMI1    = 1ULL << 3,
    BMI2    = 1ULL << 4,
    POPCNT  = 1ULL << 5,
    CLZERO  = 1ULL << 6,  // AMD Zen cache line zeroing (64-byte L1D)
    RDPID   = 1ULL << 7,  // Read processor ID in 1 cycle
    RDTSCP  = 1ULL << 8,  // Cycle-accurate timestamp counter
    ERMS    = 1ULL << 9,  // Enhanced REP MOVSB string microcode
    AVX512F = 1ULL << 10,
    MOVBE   = 1ULL << 11
};

constexpr uint64_t operator|(CpuFeature a, CpuFeature b) noexcept {
    return static_cast<uint64_t>(a) | static_cast<uint64_t>(b);
}

constexpr uint64_t operator&(uint64_t mask, CpuFeature b) noexcept {
    return mask & static_cast<uint64_t>(b);
}

// 1. Compile-Time Feature Resolution (consteval - ZERO Runtime Overhead)
consteval uint64_t get_compile_time_features() noexcept {
    uint64_t mask = 0;
#if defined(__AVX__)
    mask |= static_cast<uint64_t>(CpuFeature::AVX);
#endif
#if defined(__AVX2__)
    mask |= static_cast<uint64_t>(CpuFeature::AVX2);
#endif
#if defined(__FMA__)
    mask |= static_cast<uint64_t>(CpuFeature::FMA);
#endif
#if defined(__BMI__)
    mask |= static_cast<uint64_t>(CpuFeature::BMI1);
#endif
#if defined(__BMI2__)
    mask |= static_cast<uint64_t>(CpuFeature::BMI2);
#endif
#if defined(__POPCNT__)
    mask |= static_cast<uint64_t>(CpuFeature::POPCNT);
#endif
#if defined(__CLZERO__)
    mask |= static_cast<uint64_t>(CpuFeature::CLZERO);
#endif
#if defined(__RDPID__)
    mask |= static_cast<uint64_t>(CpuFeature::RDPID);
#endif
#if defined(__RDTSCP__) || defined(__x86_64__)
    mask |= static_cast<uint64_t>(CpuFeature::RDTSCP);
#endif
#if defined(__AVX512F__)
    mask |= static_cast<uint64_t>(CpuFeature::AVX512F);
#endif
#if defined(__MOVBE__)
    mask |= static_cast<uint64_t>(CpuFeature::MOVBE);
#endif
    return mask;
}

// C++23 Zero-Cost Compile-Time Query: evaluates to constant true/false at compile time
template <CpuFeature Feat>
consteval bool has_compile_time() noexcept {
    return (get_compile_time_features() & Feat) != 0;
}

// 2. Runtime CPUID Interrogation (Executed once during startup)
inline uint64_t detect_runtime_cpuid() noexcept {
    uint64_t mask = 0;
#if defined(__x86_64__) || defined(_M_X64)
    unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        if (ecx & (1u << 28)) mask |= static_cast<uint64_t>(CpuFeature::AVX);
        if (ecx & (1u << 12)) mask |= static_cast<uint64_t>(CpuFeature::FMA);
        if (ecx & (1u << 23)) mask |= static_cast<uint64_t>(CpuFeature::POPCNT);
        if (ecx & (1u << 22)) mask |= static_cast<uint64_t>(CpuFeature::MOVBE);
        if (edx & (1u << 27)) mask |= static_cast<uint64_t>(CpuFeature::RDTSCP);
    }
    if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
        if (ebx & (1u << 5)) mask |= static_cast<uint64_t>(CpuFeature::AVX2);
        if (ebx & (1u << 3)) mask |= static_cast<uint64_t>(CpuFeature::BMI1);
        if (ebx & (1u << 8)) mask |= static_cast<uint64_t>(CpuFeature::BMI2);
        if (ebx & (1u << 9)) mask |= static_cast<uint64_t>(CpuFeature::ERMS);
        if (ebx & (1u << 16)) mask |= static_cast<uint64_t>(CpuFeature::AVX512F);
        if (ecx & (1u << 22)) mask |= static_cast<uint64_t>(CpuFeature::RDPID);
    }
    if (__get_cpuid(0x80000008, &eax, &ebx, &ecx, &edx)) {
        if (ebx & (1u << 0)) mask |= static_cast<uint64_t>(CpuFeature::CLZERO);
    }
#endif
    return mask;
}

// Global cached runtime feature mask (initialized once at process load)
inline uint64_t runtime_features() noexcept {
    static const uint64_t cached_mask = detect_runtime_cpuid();
    return cached_mask;
}

inline bool has_runtime(CpuFeature feat) noexcept {
    return (runtime_features() & feat) != 0;
}

// Backwards-compatible struct for legacy callers
struct CpuFeatures {
    bool has_avx2{false};
    bool has_bmi1{false};
    bool has_bmi2{false};
    bool has_popcnt{false};
    bool has_clzero{false};
    bool has_rdpid{false};
    bool has_avx512f{false};

    static const CpuFeatures& instance() noexcept {
        static const CpuFeatures feats = [] {
            CpuFeatures f;
            uint64_t m = runtime_features();
            f.has_avx2 = (m & CpuFeature::AVX2) != 0;
            f.has_bmi1 = (m & CpuFeature::BMI1) != 0;
            f.has_bmi2 = (m & CpuFeature::BMI2) != 0;
            f.has_popcnt = (m & CpuFeature::POPCNT) != 0;
            f.has_clzero = (m & CpuFeature::CLZERO) != 0;
            f.has_rdpid = (m & CpuFeature::RDPID) != 0;
            f.has_avx512f = (m & CpuFeature::AVX512F) != 0;
            return f;
        }();
        return feats;
    }
};

// 3. Hardware Primitives Specialization
namespace hw_isa {

// Clear an aligned 64-byte cache line in a single CPU operation without bus pollution
inline void clear_cacheline_64(void* ptr) noexcept {
    if constexpr (has_compile_time<CpuFeature::CLZERO>()) {
        _mm_clzero(ptr);
    } else {
        if (has_runtime(CpuFeature::CLZERO)) {
            asm volatile("clzero" : : "a"(ptr) : "memory");
        } else {
            std::memset(ptr, 0, 64);
        }
    }
}

// Clear arbitrary size (must be 64-byte aligned and multiple of 64)
inline void clear_aligned_buffer_64(void* ptr, size_t bytes) noexcept {
    char* cur = static_cast<char*>(ptr);
    char* end = cur + bytes;
    while (cur < end) {
        clear_cacheline_64(cur);
        cur += 64;
    }
}

// Read execution core ID in a single cycle (Zero kernel syscall overhead)
inline uint32_t read_core_id() noexcept {
    if constexpr (has_compile_time<CpuFeature::RDPID>()) {
        return _rdpid_u32();
    } else {
        if (has_runtime(CpuFeature::RDPID)) {
            uint32_t id = 0;
            asm volatile("rdpid %0" : "=r"(id));
            return id;
        }
        return static_cast<uint32_t>(::sched_getcpu());
    }
}

// Read cycle timestamp without pipeline serializing penalty
inline uint64_t read_tsc(uint32_t* aux = nullptr) noexcept {
    unsigned int id = 0;
    uint64_t tsc = __rdtscp(&id);
    if (aux) *aux = id;
    return tsc;
}

} // namespace hw_isa

// 4. Ultra-Fast SIMD String & Delimiter Scanning
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

    while (ptr < end) {
        if (*ptr == target) return ptr;
        ++ptr;
    }
    return end;
}

inline const char* find_char_scalar(const char* start, const char* end, char target) noexcept {
    const char* ptr = start;
    while (ptr < end) {
        if (*ptr == target) return ptr;
        ++ptr;
    }
    return end;
}

// Zero-overhead dispatch: evaluated at compile-time if native build, else cached function pointer
inline const char* find_char_fast(const char* start, const char* end, char target) noexcept {
    if constexpr (has_compile_time<CpuFeature::AVX2>()) {
        return find_char_avx2(start, end, target);
    } else {
        if (has_runtime(CpuFeature::AVX2)) {
            return find_char_avx2(start, end, target);
        }
        return find_char_scalar(start, end, target);
    }
}

// Skip spaces and tabs in 32-byte SIMD chunks
inline void skip_whitespace_simd(const char*& cur, const char* end) noexcept {
    if constexpr (has_compile_time<CpuFeature::AVX2>()) {
        const __m256i space_vec = _mm256_set1_epi8(' ');
        const __m256i tab_vec = _mm256_set1_epi8('\t');

        while (cur + 32 <= end) {
            __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(cur));
            __m256i cmp_sp = _mm256_cmpeq_epi8(chunk, space_vec);
            __m256i cmp_tb = _mm256_cmpeq_epi8(chunk, tab_vec);
            __m256i is_ws = _mm256_or_si256(cmp_sp, cmp_tb);

            uint32_t mask = static_cast<uint32_t>(_mm256_movemask_epi8(is_ws));
            if (mask != 0xFFFFFFFFu) {
                // Invert to find the first non-whitespace character
                uint32_t non_ws_mask = ~mask;
                uint32_t offset = static_cast<uint32_t>(_tzcnt_u32(non_ws_mask));
                cur += offset;
                return;
            }
            cur += 32;
        }
    }

    while (cur < end && (*cur == ' ' || *cur == '\t')) {
        ++cur;
    }
}

// Find next whitespace (space or tab) in 32-byte SIMD chunks
inline void find_whitespace_simd(const char*& cur, const char* end) noexcept {
    if constexpr (has_compile_time<CpuFeature::AVX2>()) {
        const __m256i space_vec = _mm256_set1_epi8(' ');
        const __m256i tab_vec = _mm256_set1_epi8('\t');

        while (cur + 32 <= end) {
            __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(cur));
            __m256i cmp_sp = _mm256_cmpeq_epi8(chunk, space_vec);
            __m256i cmp_tb = _mm256_cmpeq_epi8(chunk, tab_vec);
            __m256i is_ws = _mm256_or_si256(cmp_sp, cmp_tb);

            uint32_t mask = static_cast<uint32_t>(_mm256_movemask_epi8(is_ws));
            if (mask != 0) {
                uint32_t offset = static_cast<uint32_t>(_tzcnt_u32(mask));
                cur += offset;
                return;
            }
            cur += 32;
        }
    }

    while (cur < end && *cur != ' ' && *cur != '\t') {
        ++cur;
    }
}

} // namespace simd

} // namespace wattcurb::core
