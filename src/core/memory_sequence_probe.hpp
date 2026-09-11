#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace wattcurb::core {

// Implements REF-ARCH-007 & REF-RES-011: Deep Memory Sequence Analyzer Probe
// Analyzes field-by-field memory access order, boundary crossings, and cache-line locality.
// In production release (-DNDEBUG), zero overhead is guaranteed via empty inline stubs.

enum class ProcFieldId : uint32_t {
    Pid = 0,
    Ppid = 1,
    UtimeTicks = 2,
    StimeTicks = 3,
    VoluntaryCtxt = 4,
    NonvoluntaryCtxt = 5,
    RssKib = 6,
    PssKib = 7,
    Minflt = 8,
    Majflt = 9,
    CpuCore = 10,
    NumThreads = 11,
    Nice = 12,
    Priority = 13,
    OpenSockets = 14,
    HasIoPerm = 15,
    Comm = 16,
    Uid = 17,
    PinnedDrmFd = 18,
    TimerslackNs = 19,
    ReadBytes = 20,
    WriteBytes = 21,
    IoSyscalls = 22,
    DrmEngineGfx = 23,
    DrmEngineCompute = 24,
    DrmEngineDec = 25,
    DrmEngineEnc = 26,
    DrmVramKib = 27,
    Count = 28
};

#if defined(WATTCURB_MEMORY_PROBE) || !defined(NDEBUG)

struct MemoryAccessRecord {
    uint32_t step{0};
    uint32_t field_id{0};
    const char* field_name{""};
    size_t offset{0};
    size_t size{0};
    size_t cache_line{0};
    bool is_crossing{false};
};

struct FieldStats {
    const char* name{""};
    size_t offset{0};
    size_t size{0};
    uint64_t access_count{0};
    uint64_t line_crossings{0};
};

class MemorySequenceProbe {
public:
    static constexpr size_t MAX_FIELDS = 32;
    static constexpr size_t MAX_RECORDS = 256;
    static constexpr size_t CACHELINE_SIZE = 64;

    static MemorySequenceProbe& instance() noexcept {
        static MemorySequenceProbe s_instance;
        return s_instance;
    }

    void reset() noexcept {
        pass_count_ = 0;
        record_count_ = 0;
        total_accesses_ = 0;
        total_crossings_ = 0;
        prev_cache_line_ = 0;
        prev_field_id_ = 0;
        has_prev_ = false;
        for (auto& f : fields_) f = FieldStats{};
        for (size_t i = 0; i < MAX_FIELDS; ++i) {
            for (size_t j = 0; j < MAX_FIELDS; ++j) {
                transitions_[i][j] = 0;
            }
        }
    }

    void begin_pass() noexcept {
        ++pass_count_;
        has_prev_ = false;
    }

    void end_pass() noexcept {}

    void record_access(uint32_t field_id, const char* name, size_t offset, size_t size) noexcept {
        if (field_id >= MAX_FIELDS) return;
        ++total_accesses_;

        size_t cline = offset / CACHELINE_SIZE;
        bool crossing = has_prev_ && (cline != prev_cache_line_);
        if (crossing) {
            ++total_crossings_;
            fields_[field_id].line_crossings++;
        }

        if (has_prev_ && prev_field_id_ < MAX_FIELDS) {
            transitions_[prev_field_id_][field_id]++;
        }

        prev_cache_line_ = cline;
        prev_field_id_ = field_id;
        has_prev_ = true;

        fields_[field_id].name = name;
        fields_[field_id].offset = offset;
        fields_[field_id].size = size;
        fields_[field_id].access_count++;

        if (record_count_ < MAX_RECORDS) {
            records_[record_count_++] = MemoryAccessRecord{
                .step = static_cast<uint32_t>(record_count_),
                .field_id = field_id,
                .field_name = name,
                .offset = offset,
                .size = size,
                .cache_line = cline,
                .is_crossing = crossing
            };
        }
    }

    [[nodiscard]] std::string generate_report() const {
        std::ostringstream ss;
        ss << "=========================================================================\n";
        ss << "   WattCurb Deep Memory Access Sequence & Cache-Line Analysis Report     \n";
        ss << "=========================================================================\n";
        ss << "Total Passes Profiled   : " << pass_count_ << "\n";
        ss << "Total Memory Accesses   : " << total_accesses_ << "\n";
        ss << "Cache-Line Crossings    : " << total_crossings_ << "\n";
        double crossing_rate = total_accesses_ > 0 ? (100.0 * static_cast<double>(total_crossings_) / static_cast<double>(total_accesses_)) : 0.0;
        ss << "Boundary Crossing Rate  : " << std::fixed << std::setprecision(2) << crossing_rate << "%\n\n";

        ss << "[Field Access Frequency & Hot/Cold Classification]\n";
        ss << "  ID | Offset | Size | Line | Access Count | Probability | Category | Name\n";
        ss << " ----+--------+------+------+--------------+-------------+----------+---------------------\n";

        for (size_t i = 0; i < MAX_FIELDS; ++i) {
            if (fields_[i].access_count == 0) continue;
            double prob = pass_count_ > 0 ? (100.0 * static_cast<double>(fields_[i].access_count) / static_cast<double>(pass_count_)) : 0.0;
            const char* cat = prob >= 90.0 ? "HOT (L1D)" : (prob >= 20.0 ? "WARM" : "COLD");
            ss << "  " << std::setw(2) << i << " | "
               << std::setw(6) << fields_[i].offset << " | "
               << std::setw(4) << fields_[i].size << " | "
               << std::setw(4) << (fields_[i].offset / CACHELINE_SIZE) << " | "
               << std::setw(12) << fields_[i].access_count << " | "
               << std::setw(10) << std::fixed << std::setprecision(1) << prob << "% | "
               << std::setw(8) << cat << " | "
               << fields_[i].name << "\n";
        }

        ss << "\n[Chronological Access Sequence Timeline (Sample Window)]\n";
        for (size_t i = 0; i < std::min(record_count_, size_t{24}); ++i) {
            const auto& r = records_[i];
            ss << "  Step " << std::setw(2) << r.step << ": ["
               << r.field_name << "] @ Offset " << r.offset << " (Line " << r.cache_line << ")";
            if (r.is_crossing) ss << " <-- [CACHE-LINE CROSSING HAZARD!]";
            ss << "\n";
        }

        return ss.str();
    }

    [[nodiscard]] size_t total_accesses() const noexcept { return total_accesses_; }
    [[nodiscard]] size_t total_crossings() const noexcept { return total_crossings_; }
    [[nodiscard]] size_t pass_count() const noexcept { return pass_count_; }
    [[nodiscard]] const FieldStats& field_stat(size_t idx) const noexcept { return fields_[idx]; }

private:
    MemorySequenceProbe() = default;
    size_t pass_count_{0};
    size_t record_count_{0};
    size_t total_accesses_{0};
    size_t total_crossings_{0};
    size_t prev_cache_line_{0};
    uint32_t prev_field_id_{0};
    bool has_prev_{false};

    std::array<MemoryAccessRecord, MAX_RECORDS> records_{};
    std::array<FieldStats, MAX_FIELDS> fields_{};
    std::array<std::array<uint32_t, MAX_FIELDS>, MAX_FIELDS> transitions_{};
};

#define WATTCURB_RECORD_MEM_ACCESS(field_id, name, offset, size) \
    ::wattcurb::core::MemorySequenceProbe::instance().record_access(static_cast<uint32_t>(field_id), name, offset, size)

#else

// Zero-overhead production release stub
class MemorySequenceProbe {
public:
    static MemorySequenceProbe& instance() noexcept {
        static MemorySequenceProbe s_instance;
        return s_instance;
    }
    inline void reset() noexcept {}
    inline void begin_pass() noexcept {}
    inline void end_pass() noexcept {}
    inline void record_access(uint32_t, const char*, size_t, size_t) noexcept {}
    [[nodiscard]] inline std::string generate_report() const { return "Memory probe disabled in release build."; }
    [[nodiscard]] inline size_t total_accesses() const noexcept { return 0; }
    [[nodiscard]] inline size_t total_crossings() const noexcept { return 0; }
    [[nodiscard]] inline size_t pass_count() const noexcept { return 0; }
};

#define WATTCURB_RECORD_MEM_ACCESS(field_id, name, offset, size) ((void)0)

#endif

} // namespace wattcurb::core
