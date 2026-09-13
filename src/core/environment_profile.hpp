#pragma once

#include "core/cpu_features.hpp"
#include "core/posix_fs.hpp"
#include <concepts>
#include <cstdint>
#include <cstring>
#include <linux/perf_event.h>
#include <string_view>
#include <sys/syscall.h>
#include <type_traits>
#include <unistd.h>

namespace wattcurb::core {

// Implements REF-REQ-026 & REF-ARCH-016: C++23 Zero-Cost Environment Specialization

enum class CpuIsaTier : uint8_t {
    GenericV2    = 0, // SSE4.2 scalar baseline (Universal compatibility)
    Avx2Bmi2     = 1, // AVX2 + BMI1/BMI2 vectorization (Intel/AMD modern)
    ZenClzero    = 2, // AVX2 + BMI2 + AMD Zen CLZERO & RDPID
    Avx512       = 3  // AVX-512 foundation & vector extensions
};

enum class PlatformFormFactor : uint8_t {
    MobileLaptop        = 0, // Battery present, mobile power throttling
    DesktopWorkstation  = 1, // AC-only, no battery gas gauge
    VirtualHeadless     = 2  // Virtual machine or container, minimal sysfs sensors
};

enum class PrivilegeTier : uint8_t {
    KernelDirectPMU    = 0, // perf_event_open (syscall 298) and direct MSR access
    StandardSysfs      = 1, // Standard unprivileged user sysfs access
    RestrictedSandbox  = 2  // Read-only /proc only, sysfs blocked
};

// 1. C++23 Concepts for Compile-Time Interface Constraints

template <typename T>
concept CpuIsaPolicyConcept = requires(const char*& cur, const char* end, int count, void* ptr) {
    { T::skip_whitespace(cur, end) } noexcept -> std::same_as<void>;
    { T::find_whitespace(cur, end) } noexcept -> std::same_as<void>;
    { T::skip_tokens(cur, end, count) } noexcept -> std::same_as<void>;
    { T::clear_cacheline_64(ptr) } noexcept -> std::same_as<void>;
    { T::read_core_id() } noexcept -> std::same_as<uint32_t>;
    { T::read_tsc() } noexcept -> std::same_as<uint64_t>;
    { T::tier_name() } noexcept -> std::same_as<std::string_view>;
};

template <typename T>
concept PlatformPolicyConcept = requires {
    { T::has_battery() } noexcept -> std::same_as<bool>;
    { T::supports_zen_ccx() } noexcept -> std::same_as<bool>;
    { T::supports_pmu() } noexcept -> std::same_as<bool>;
    { T::form_factor_name() } noexcept -> std::same_as<std::string_view>;
};

// 2. Concrete ISA Policies

struct ScalarGenericIsaPolicy {
    static constexpr std::string_view tier_name() noexcept { return "Generic-x86_64-v2"; }

    static inline void skip_whitespace(const char*& cur, const char* end) noexcept {
        while (cur < end && static_cast<unsigned char>(*cur) <= ' ') ++cur;
    }

    static inline void find_whitespace(const char*& cur, const char* end) noexcept {
        while (cur < end && static_cast<unsigned char>(*cur) > ' ') ++cur;
    }

    static inline void skip_tokens(const char*& cur, const char* end, int count) noexcept {
        while (count > 0 && cur < end) {
            while (cur < end && static_cast<unsigned char>(*cur) > ' ') ++cur;
            while (cur < end && static_cast<unsigned char>(*cur) <= ' ') ++cur;
            --count;
        }
    }

    static inline void clear_cacheline_64(void* ptr) noexcept {
        std::memset(ptr, 0, 64);
    }

    static inline uint32_t read_core_id() noexcept {
        return static_cast<uint32_t>(::sched_getcpu());
    }

    static inline uint64_t read_tsc() noexcept {
        return hw_isa::read_tsc();
    }
};

struct Avx2Bmi2IsaPolicy {
    static constexpr std::string_view tier_name() noexcept { return "AVX2-BMI2-Vectorized"; }

    static inline void skip_whitespace(const char*& cur, const char* end) noexcept {
        simd::skip_whitespace_simd(cur, end);
    }

    static inline void find_whitespace(const char*& cur, const char* end) noexcept {
        simd::find_whitespace_simd(cur, end);
    }

    static inline void skip_tokens(const char*& cur, const char* end, int count) noexcept {
        simd::skip_tokens_simd(cur, end, count);
    }

    static inline void clear_cacheline_64(void* ptr) noexcept {
        std::memset(ptr, 0, 64);
    }

    static inline uint32_t read_core_id() noexcept {
        return hw_isa::read_core_id();
    }

    static inline uint64_t read_tsc() noexcept {
        return hw_isa::read_tsc();
    }
};

struct ZenSpecializedIsaPolicy {
    static constexpr std::string_view tier_name() noexcept { return "AMD-Zen-CLZERO-Specialized"; }

    static inline void skip_whitespace(const char*& cur, const char* end) noexcept {
        simd::skip_whitespace_simd(cur, end);
    }

    static inline void find_whitespace(const char*& cur, const char* end) noexcept {
        simd::find_whitespace_simd(cur, end);
    }

    static inline void skip_tokens(const char*& cur, const char* end, int count) noexcept {
        simd::skip_tokens_simd(cur, end, count);
    }

    static inline void clear_cacheline_64(void* ptr) noexcept {
#if defined(__CLZERO__)
        _mm_clzero(ptr);
#else
        asm volatile("clzero" : : "a"(ptr) : "memory");
#endif
    }

    static inline uint32_t read_core_id() noexcept {
        return hw_isa::read_core_id();
    }

    static inline uint64_t read_tsc() noexcept {
        return hw_isa::read_tsc();
    }
};

// 3. Concrete Platform Policies

struct MobileLaptopPolicy {
    static constexpr std::string_view form_factor_name() noexcept { return "Mobile-Laptop"; }
    static constexpr bool has_battery() noexcept { return true; }
    static constexpr bool supports_zen_ccx() noexcept { return true; }
    static constexpr bool supports_pmu() noexcept { return true; }
};

struct DesktopWorkstationPolicy {
    static constexpr std::string_view form_factor_name() noexcept { return "Desktop-Workstation"; }
    static constexpr bool has_battery() noexcept { return false; }
    static constexpr bool supports_zen_ccx() noexcept { return true; }
    static constexpr bool supports_pmu() noexcept { return true; }
};

struct VirtualHeadlessPolicy {
    static constexpr std::string_view form_factor_name() noexcept { return "Virtual-Headless"; }
    static constexpr bool has_battery() noexcept { return false; }
    static constexpr bool supports_zen_ccx() noexcept { return false; }
    static constexpr bool supports_pmu() noexcept { return false; }
};

// 4. Combined Environment Policy
template <CpuIsaPolicyConcept IsaPolicy, PlatformPolicyConcept PlatPolicy>
struct CombinedEnvironmentPolicy {
    using Isa = IsaPolicy;
    using Platform = PlatPolicy;

    static constexpr std::string_view name() noexcept {
        return "CombinedEnvironmentPolicy";
    }
};

// 5. Environment Profile Structure (Captured once at process startup)
struct EnvironmentProfile {
    CpuIsaTier isa_tier{CpuIsaTier::GenericV2};
    PlatformFormFactor form_factor{PlatformFormFactor::DesktopWorkstation};
    PrivilegeTier privilege{PrivilegeTier::StandardSysfs};
    bool is_amd_zen{false};
    bool is_battery_present{false};
    bool has_direct_pmu{false};

    static EnvironmentProfile detect_host() noexcept {
        EnvironmentProfile p;

        // 1. Detect ISA Tier
        uint64_t feats = runtime_features();
        if ((feats & CpuFeature::CLZERO) && (feats & CpuFeature::AVX2)) {
            p.isa_tier = CpuIsaTier::ZenClzero;
            p.is_amd_zen = true;
        } else if ((feats & CpuFeature::AVX2) && (feats & CpuFeature::BMI2)) {
            p.isa_tier = CpuIsaTier::Avx2Bmi2;
        } else {
            p.isa_tier = CpuIsaTier::GenericV2;
        }

        // 2. Detect Platform Form Factor (Check for BAT* power supplies via zero-allocation POSIX)
        bool found_battery = false;
        if (core::fs::file_exists("/sys/class/power_supply")) {
            core::fs::for_each_dir_entry("/sys/class/power_supply", [&](std::string_view name) {
                if (name.rfind("BAT", 0) == 0) {
                    found_battery = true;
                }
            });
        }
        p.is_battery_present = found_battery;

        // Detect Virtualization (DMI sys_vendor or hypervisor via zero-allocation POSIX)
        bool is_vm = false;
        char vendor_buf[128];
        if (core::fs::read_small_file("/sys/class/dmi/id/sys_vendor", vendor_buf, sizeof(vendor_buf))) {
            std::string_view vendor(vendor_buf);
            if (vendor.find("QEMU") != std::string_view::npos ||
                vendor.find("VMware") != std::string_view::npos ||
                vendor.find("KVM") != std::string_view::npos) {
                is_vm = true;
            }
        }

        if (is_vm) {
            p.form_factor = PlatformFormFactor::VirtualHeadless;
        } else if (found_battery) {
            p.form_factor = PlatformFormFactor::MobileLaptop;
        } else {
            p.form_factor = PlatformFormFactor::DesktopWorkstation;
        }

        // 3. Test PMU Capabilities (perf_event_open probe)
        struct perf_event_attr pe{};
        pe.type = PERF_TYPE_HARDWARE;
        pe.size = sizeof(struct perf_event_attr);
        pe.config = PERF_COUNT_HW_INSTRUCTIONS;
        pe.disabled = 1;
        pe.exclude_kernel = 1;
        pe.exclude_hv = 1;
        int pmu_fd = static_cast<int>(::syscall(SYS_perf_event_open, &pe, 0, -1, -1, 0));
        if (pmu_fd >= 0) {
            ::close(pmu_fd);
            p.privilege = PrivilegeTier::KernelDirectPMU;
            p.has_direct_pmu = true;
        } else {
            p.privilege = PrivilegeTier::StandardSysfs;
            p.has_direct_pmu = false;
        }

        return p;
    }
};

// 6. Zero-Cost Outer Loop Dispatcher
// Evaluates environmental switches ONCE at bootstrap and invokes the worker with concrete inlined Policy.
class EnvironmentDispatcher {
public:
    template <typename WorkerFunc>
    static void dispatch(const EnvironmentProfile& env, WorkerFunc&& worker) {
        if (env.isa_tier == CpuIsaTier::ZenClzero) {
            if (env.form_factor == PlatformFormFactor::MobileLaptop) {
                worker(CombinedEnvironmentPolicy<ZenSpecializedIsaPolicy, MobileLaptopPolicy>{});
            } else if (env.form_factor == PlatformFormFactor::DesktopWorkstation) {
                worker(CombinedEnvironmentPolicy<ZenSpecializedIsaPolicy, DesktopWorkstationPolicy>{});
            } else {
                worker(CombinedEnvironmentPolicy<ZenSpecializedIsaPolicy, VirtualHeadlessPolicy>{});
            }
        } else if (env.isa_tier == CpuIsaTier::Avx2Bmi2) {
            if (env.form_factor == PlatformFormFactor::MobileLaptop) {
                worker(CombinedEnvironmentPolicy<Avx2Bmi2IsaPolicy, MobileLaptopPolicy>{});
            } else {
                worker(CombinedEnvironmentPolicy<Avx2Bmi2IsaPolicy, DesktopWorkstationPolicy>{});
            }
        } else {
            if (env.form_factor == PlatformFormFactor::MobileLaptop) {
                worker(CombinedEnvironmentPolicy<ScalarGenericIsaPolicy, MobileLaptopPolicy>{});
            } else {
                worker(CombinedEnvironmentPolicy<ScalarGenericIsaPolicy, DesktopWorkstationPolicy>{});
            }
        }
    }
};

} // namespace wattcurb::core
