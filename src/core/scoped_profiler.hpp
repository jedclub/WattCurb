#pragma once

#include "core/cpu_features.hpp"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <array>
#include <algorithm>
#include <iostream>
#include <iomanip>

namespace wattcurb::core {

#if defined(WATTCURB_DEV_PROFILE) || (!defined(NDEBUG) && !defined(WATTCURB_DISABLE_DEV_PROFILE))

struct ProfileEntry {
    const char* name{nullptr};
    uint64_t invocations{0};
    uint64_t total_ns{0};
    uint64_t total_tsc{0};
    uint64_t min_ns{UINT64_MAX};
    uint64_t max_ns{0};
};

class ScopedProfilerRegistry {
public:
    static constexpr size_t MAX_ENTRIES = 128;

    static ScopedProfilerRegistry& instance() noexcept {
        static ScopedProfilerRegistry reg;
        return reg;
    }

    void record(const char* name, uint64_t duration_ns, uint64_t delta_tsc) noexcept {
        for (size_t i = 0; i < count_; ++i) {
            if (entries_[i].name == name || std::strcmp(entries_[i].name, name) == 0) {
                entries_[i].invocations++;
                entries_[i].total_ns += duration_ns;
                entries_[i].total_tsc += delta_tsc;
                if (duration_ns < entries_[i].min_ns) entries_[i].min_ns = duration_ns;
                if (duration_ns > entries_[i].max_ns) entries_[i].max_ns = duration_ns;
                return;
            }
        }
        if (count_ < MAX_ENTRIES) {
            auto& e = entries_[count_++];
            e.name = name;
            e.invocations = 1;
            e.total_ns = duration_ns;
            e.total_tsc = delta_tsc;
            e.min_ns = duration_ns;
            e.max_ns = duration_ns;
        }
    }

    void reset() noexcept {
        for (size_t i = 0; i < count_; ++i) {
            entries_[i] = ProfileEntry{};
        }
        count_ = 0;
    }

    void print_summary(std::ostream& out = std::cout) const {
        if (count_ == 0) return;

        // Sort entries by total_ns descending
        std::array<ProfileEntry, MAX_ENTRIES> sorted = entries_;
        std::sort(sorted.begin(), sorted.begin() + count_, [](const ProfileEntry& a, const ProfileEntry& b) {
            return a.total_ns > b.total_ns;
        });

        uint64_t sum_all_ns = 0;
        uint64_t sum_all_tsc = 0;
        for (size_t i = 0; i < count_; ++i) {
            sum_all_ns += sorted[i].total_ns;
            sum_all_tsc += sorted[i].total_tsc;
        }

        out << "\n====================================================================================================\n";
        out << " [DEV PROFILER] Fine-Grained Subsystem Execution Cost Breakdown (REF-REQ-014)\n";
        out << "====================================================================================================\n";
        out << std::left << std::setw(28) << "Subsystem / Scope Name"
            << std::right << std::setw(8)  << "Calls"
            << std::setw(14) << "Total (ms)"
            << std::setw(12) << "Share (%)"
            << std::setw(14) << "Avg (us/op)"
            << std::setw(12) << "Min (us)"
            << std::setw(12) << "Max (us)"
            << "\n";
        out << "----------------------------------------------------------------------------------------------------\n";

        for (size_t i = 0; i < count_; ++i) {
            const auto& e = sorted[i];
            double total_ms = static_cast<double>(e.total_ns) / 1e6;
            double share_pct = (sum_all_ns > 0) ? (static_cast<double>(e.total_ns) * 100.0 / static_cast<double>(sum_all_ns)) : 0.0;
            double avg_us = (e.invocations > 0) ? (static_cast<double>(e.total_ns) / static_cast<double>(e.invocations) / 1e3) : 0.0;
            double min_us = (e.min_ns != UINT64_MAX) ? (static_cast<double>(e.min_ns) / 1e3) : 0.0;
            double max_us = static_cast<double>(e.max_ns) / 1e3;

            out << std::left  << std::setw(28) << e.name
                << std::right << std::setw(8)  << e.invocations
                << std::fixed << std::setprecision(3)
                << std::setw(14) << total_ms
                << std::setprecision(1)
                << std::setw(11) << share_pct << "%"
                << std::setprecision(2)
                << std::setw(14) << avg_us
                << std::setw(12) << min_us
                << std::setw(12) << max_us
                << "\n";
        }
        out << "----------------------------------------------------------------------------------------------------\n";
        out << " Cumulative Instrumented Time: " << std::fixed << std::setprecision(3) 
            << (static_cast<double>(sum_all_ns) / 1e6) << " ms | Total CPU Cycles: " << sum_all_tsc << "\n";
        out << "====================================================================================================\n\n";
    }

private:
    std::array<ProfileEntry, MAX_ENTRIES> entries_{};
    size_t count_{0};
};

class ScopedProfiler {
public:
    explicit ScopedProfiler(const char* name) noexcept
        : name_(name),
          start_time_(std::chrono::steady_clock::now()),
          start_tsc_(hw_isa::read_tsc()) {}

    ~ScopedProfiler() noexcept {
        auto end_time = std::chrono::steady_clock::now();
        uint64_t end_tsc = hw_isa::read_tsc();
        auto duration_ns = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time_).count());
        uint64_t delta_tsc = (end_tsc >= start_tsc_) ? (end_tsc - start_tsc_) : 0;
        ScopedProfilerRegistry::instance().record(name_, duration_ns, delta_tsc);
    }

    ScopedProfiler(const ScopedProfiler&) = delete;
    ScopedProfiler& operator=(const ScopedProfiler&) = delete;

private:
    const char* name_;
    std::chrono::steady_clock::time_point start_time_;
    uint64_t start_tsc_;
};

#define WATTCURB_PROFILE_SCOPE(name) ::wattcurb::core::ScopedProfiler _prof_scope_##__LINE__(name)

#else

// Zero-Cost Abstraction in Production Release Builds (REF-REQ-008 & REF-REQ-014)
class ScopedProfilerRegistry {
public:
    static ScopedProfilerRegistry& instance() noexcept {
        static ScopedProfilerRegistry reg;
        return reg;
    }
    void record(const char*, uint64_t, uint64_t) noexcept {}
    void reset() noexcept {}
    void print_summary(std::ostream& = std::cout) const noexcept {}
};

#define WATTCURB_PROFILE_SCOPE(name) ((void)0)

#endif

} // namespace wattcurb::core
