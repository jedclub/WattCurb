#pragma once

#include "core/cpu_features.hpp"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <array>
#include <vector>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <atomic>
#include <time.h>

namespace wattcurb::core {

// Implements REF-REQ-142 & REF-ARCH-089:
// Ultra-Low-Overhead Function-Level Scoped Profiling & 30-Minute Continuous Session
#if defined(WATTCURB_DEV_PROFILE) || (!defined(NDEBUG) && !defined(WATTCURB_DISABLE_DEV_PROFILE))

struct alignas(64) ProfileEntry {
    const char* name{nullptr};
    std::atomic<uint64_t> invocations{0};
    std::atomic<uint64_t> total_tsc{0};
    std::atomic<uint64_t> min_tsc{UINT64_MAX};
    std::atomic<uint64_t> max_tsc{0};

    ProfileEntry() = default;
    ProfileEntry(const ProfileEntry&) = delete;
    ProfileEntry& operator=(const ProfileEntry&) = delete;
};

struct ProfileSnapshot {
    const char* name{nullptr};
    uint64_t invocations{0};
    uint64_t total_tsc{0};
    uint64_t min_tsc{0};
    uint64_t max_tsc{0};
    uint64_t total_ns{0};
    uint64_t min_ns{0};
    uint64_t max_ns{0};
};

class ScopedProfilerRegistry {
public:
    static constexpr size_t MAX_ENTRIES = 256;

    static ScopedProfilerRegistry& instance() noexcept {
        static ScopedProfilerRegistry reg;
        return reg;
    }

    // Resolves entry slot O(1) statically or registers during initialization
    size_t register_entry(const char* name) noexcept {
        if (name == nullptr) return 0;
        size_t c = count_.load(std::memory_order_relaxed);
        for (size_t i = 0; i < c; ++i) {
            const char* entry_name = entries_[i].name;
            if (entry_name == name || (entry_name != nullptr && std::strcmp(entry_name, name) == 0)) {
                return i;
            }
        }
        // Thread-safe allocation of new slot
        size_t new_idx = count_.fetch_add(1, std::memory_order_relaxed);
        if (new_idx < MAX_ENTRIES) {
            entries_[new_idx].name = name;
            return new_idx;
        }
        return 0; // fallback slot if registry full
    }

    // Hot-path record: O(1) array index, zero allocation, relaxed atomics (< 10 ns)
    inline void record_fast(size_t slot_idx, uint64_t delta_tsc) noexcept {
        if (slot_idx >= MAX_ENTRIES) return;
        auto& e = entries_[slot_idx];
        e.invocations.fetch_add(1, std::memory_order_relaxed);
        e.total_tsc.fetch_add(delta_tsc, std::memory_order_relaxed);

        uint64_t cur_min = e.min_tsc.load(std::memory_order_relaxed);
        while (delta_tsc < cur_min && !e.min_tsc.compare_exchange_weak(cur_min, delta_tsc, std::memory_order_relaxed)) {}

        uint64_t cur_max = e.max_tsc.load(std::memory_order_relaxed);
        while (delta_tsc > cur_max && !e.max_tsc.compare_exchange_weak(cur_max, delta_tsc, std::memory_order_relaxed)) {}
    }

    // Backward compatible record
    void record(const char* name, uint64_t /*duration_ns*/, uint64_t delta_tsc) noexcept {
        size_t idx = register_entry(name);
        record_fast(idx, delta_tsc);
    }

    void reset() noexcept {
        size_t c = count_.load(std::memory_order_relaxed);
        for (size_t i = 0; i < c && i < MAX_ENTRIES; ++i) {
            entries_[i].invocations.store(0, std::memory_order_relaxed);
            entries_[i].total_tsc.store(0, std::memory_order_relaxed);
            entries_[i].min_tsc.store(UINT64_MAX, std::memory_order_relaxed);
            entries_[i].max_tsc.store(0, std::memory_order_relaxed);
        }
    }

    [[nodiscard]] size_t count() const noexcept {
        size_t c = count_.load(std::memory_order_relaxed);
        return (c > MAX_ENTRIES) ? MAX_ENTRIES : c;
    }

    static double tsc_ticks_per_ns() noexcept {
        static double rate = []() noexcept -> double {
            auto t0 = std::chrono::steady_clock::now();
            uint64_t tsc0 = hw_isa::read_tsc();
            struct timespec req { 0, 5000000 }; // 5 ms sleep calibration
            ::nanosleep(&req, nullptr);
            auto t1 = std::chrono::steady_clock::now();
            uint64_t tsc1 = hw_isa::read_tsc();
            int64_t delta_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
            if (delta_ns > 0 && tsc1 > tsc0) {
                return static_cast<double>(tsc1 - tsc0) / static_cast<double>(delta_ns);
            }
            return 2.5; // fallback: 2.5 GHz
        }();
        return rate;
    }

    std::string format_summary() const {
        size_t c = count();
        if (c == 0) return "No profiling data captured.\n";

        double tsc_rate = tsc_ticks_per_ns();
        if (tsc_rate <= 0.0) tsc_rate = 2.5;

        std::vector<ProfileSnapshot> snapshots;
        snapshots.reserve(c);

        uint64_t sum_all_tsc = 0;
        for (size_t i = 0; i < c; ++i) {
            const auto& e = entries_[i];
            uint64_t inv = e.invocations.load(std::memory_order_relaxed);
            if (inv == 0) continue;

            uint64_t tsc = e.total_tsc.load(std::memory_order_relaxed);
            uint64_t mn_tsc = e.min_tsc.load(std::memory_order_relaxed);
            uint64_t mx_tsc = e.max_tsc.load(std::memory_order_relaxed);

            ProfileSnapshot s{};
            s.name = (e.name != nullptr) ? e.name : "unknown";
            s.invocations = inv;
            s.total_tsc = tsc;
            s.min_tsc = (mn_tsc == UINT64_MAX) ? 0 : mn_tsc;
            s.max_tsc = mx_tsc;
            s.total_ns = static_cast<uint64_t>(static_cast<double>(tsc) / tsc_rate);
            s.min_ns = static_cast<uint64_t>(static_cast<double>(s.min_tsc) / tsc_rate);
            s.max_ns = static_cast<uint64_t>(static_cast<double>(s.max_tsc) / tsc_rate);

            sum_all_tsc += tsc;
            snapshots.push_back(s);
        }

        std::sort(snapshots.begin(), snapshots.end(), [](const ProfileSnapshot& a, const ProfileSnapshot& b) {
            return a.total_tsc > b.total_tsc;
        });

        std::ostringstream ss;
        ss << "\n========================================================================================================================\n";
        ss << " [WTCURB PROFILER] Subsystem & Function Execution Breakdown (REF-REQ-142, REF-ARCH-089)\n";
        ss << " Measured TSC Clock Rate: " << std::fixed << std::setprecision(3) << tsc_rate << " ticks/ns ("
           << std::setprecision(2) << (tsc_rate * 1000.0) << " MHz)\n";
        ss << "========================================================================================================================\n";
        ss << std::left  << std::setw(34) << "Subsystem / Function Scope"
           << std::right << std::setw(10) << "Calls"
           << std::setw(14) << "Total (ms)"
           << std::setw(11) << "Share (%)"
           << std::setw(14) << "Avg (us/op)"
           << std::setw(14) << "Avg Cycles"
           << std::setw(12) << "Min (us)"
           << std::setw(12) << "Max (us)"
           << "\n";
        ss << "------------------------------------------------------------------------------------------------------------------------\n";

        for (const auto& s : snapshots) {
            double total_ms = static_cast<double>(s.total_ns) / 1e6;
            double share_pct = (sum_all_tsc > 0) ? (static_cast<double>(s.total_tsc) * 100.0 / static_cast<double>(sum_all_tsc)) : 0.0;
            double avg_us = (s.invocations > 0) ? (static_cast<double>(s.total_ns) / static_cast<double>(s.invocations) / 1e3) : 0.0;
            double avg_cycles = (s.invocations > 0) ? (static_cast<double>(s.total_tsc) / static_cast<double>(s.invocations)) : 0.0;
            double min_us = static_cast<double>(s.min_ns) / 1e3;
            double max_us = static_cast<double>(s.max_ns) / 1e3;

            ss << std::left  << std::setw(34) << s.name
               << std::right << std::setw(10) << s.invocations
               << std::fixed << std::setprecision(3)
               << std::setw(14) << total_ms
               << std::setprecision(1)
               << std::setw(10) << share_pct << "%"
               << std::setprecision(2)
               << std::setw(14) << avg_us
               << std::setprecision(0)
               << std::setw(14) << avg_cycles
               << std::setprecision(2)
               << std::setw(12) << min_us
               << std::setw(12) << max_us
               << "\n";
        }

        double sum_all_ms = (static_cast<double>(sum_all_tsc) / tsc_rate) / 1e6;
        ss << "------------------------------------------------------------------------------------------------------------------------\n";
        ss << " Total Instrumented Active Time: " << std::fixed << std::setprecision(3) << sum_all_ms 
           << " ms | Total CPU Cycles: " << sum_all_tsc << "\n";
        ss << "========================================================================================================================\n\n";

        return ss.str();
    }

    void print_summary(std::ostream& out = std::cout) const {
        out << format_summary() << std::flush;
    }

    bool dump_to_file(const char* filepath) const {
        if (filepath == nullptr || *filepath == '\0') return false;
        std::ofstream ofs(filepath, std::ios::out | std::ios::trunc);
        if (!ofs.is_open()) return false;
        ofs << format_summary();
        ofs.close();
        return true;
    }

private:
    std::array<ProfileEntry, MAX_ENTRIES> entries_{};
    std::atomic<size_t> count_{0};
};

class ScopedProfiler {
public:
    explicit ScopedProfiler(size_t slot_idx) noexcept
        : slot_idx_(slot_idx),
          start_tsc_(hw_isa::read_tsc()) {}

    explicit ScopedProfiler(const char* name) noexcept
        : slot_idx_(ScopedProfilerRegistry::instance().register_entry(name)),
          start_tsc_(hw_isa::read_tsc()) {}

    ~ScopedProfiler() noexcept {
        uint64_t end_tsc = hw_isa::read_tsc();
        uint64_t delta = (end_tsc >= start_tsc_) ? (end_tsc - start_tsc_) : 0;
        ScopedProfilerRegistry::instance().record_fast(slot_idx_, delta);
    }

    ScopedProfiler(const ScopedProfiler&) = delete;
    ScopedProfiler& operator=(const ScopedProfiler&) = delete;

private:
    size_t slot_idx_;
    uint64_t start_tsc_;
};

#define WATTCURB_PROFILE_SCOPE(name) \
    static const size_t _prof_slot_##__LINE__ = ::wattcurb::core::ScopedProfilerRegistry::instance().register_entry(name); \
    ::wattcurb::core::ScopedProfiler _prof_scope_##__LINE__(_prof_slot_##__LINE__)

#else

// Zero-Cost Abstraction in Production Release Builds (REF-REQ-008, REF-REQ-014, REF-REQ-142)
class ScopedProfilerRegistry {
public:
    static ScopedProfilerRegistry& instance() noexcept {
        static ScopedProfilerRegistry reg;
        return reg;
    }
    size_t register_entry(const char*) noexcept { return 0; }
    void record_fast(size_t, uint64_t) noexcept {}
    void record(const char*, uint64_t, uint64_t) noexcept {}
    void reset() noexcept {}
    size_t count() const noexcept { return 0; }
    void print_summary(std::ostream& = std::cout) const noexcept {}
    std::string format_summary() const { return {}; }
    bool dump_to_file(const char*) const noexcept { return true; }
};

class ScopedProfiler {
public:
    explicit ScopedProfiler(size_t) noexcept {}
    explicit ScopedProfiler(const char*) noexcept {}
    ~ScopedProfiler() noexcept = default;
};

#define WATTCURB_PROFILE_SCOPE(name) ((void)0)

#endif

} // namespace wattcurb::core
