#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string_view>
#include "core/custom_containers.hpp"

namespace wattcurb {

// Implements REF-REQ-001, REF-REQ-010 & REF-ARCH-002
struct HardwareSample {
    std::chrono::steady_clock::time_point timestamp{};

    // 1. Power Supply & Battery Gas Gauge (REF-REQ-010 Sec 2.1, REF-REQ-022)
    std::optional<uint64_t> battery_power_uw;
    std::optional<uint64_t> battery_voltage_uv;
    std::optional<uint64_t> battery_voltage_min_design_uv;
    std::optional<int64_t> battery_current_ua;
    std::optional<uint64_t> battery_energy_now_uwh;
    std::optional<uint64_t> battery_energy_full_uwh;
    std::optional<uint64_t> battery_energy_full_design_uwh;
    std::optional<uint32_t> battery_cycle_count;
    std::optional<uint32_t> battery_capacity_percent;
    bool is_discharging{false};
    bool is_ac_online{false};

    // Deep Battery Chemistry & ThinkPad Hardware Thresholds (REF-REQ-022)
    core::FixedString<16> battery_capacity_level{};
    core::FixedString<16> battery_technology{};
    core::FixedString<32> battery_model_name{};
    core::FixedString<24> battery_manufacturer{};
    core::FixedString<24> battery_serial_number{};
    std::optional<uint32_t> battery_charge_start_threshold;
    std::optional<uint32_t> battery_charge_end_threshold;
    core::FixedString<32> battery_charge_behaviour{};

    // USB-C Power Delivery Input & Type
    std::optional<uint64_t> usbc_pd_voltage_uv;
    std::optional<uint64_t> usbc_pd_current_ua;
    std::optional<uint64_t> usbc_pd_voltage_max_uv;
    std::optional<uint64_t> usbc_pd_current_max_ua;
    core::FixedString<32> usbc_pd_type{};
    bool usbc_pd_online{false};

    // Connected Peripheral Batteries (Bluetooth/HID/Stylus) (REF-REQ-022)
    struct PeripheralBattery {
        core::FixedString<32> name{};
        uint32_t capacity_percent{0};
        bool is_charging{false};
    };
    core::FixedVector<PeripheralBattery, 4> peripheral_batteries{};

    // 2. CPU & Platform Subsystem (REF-REQ-010 Sec 2.2)
    std::optional<uint64_t> rapl_package_uj;
    std::optional<uint64_t> rapl_core_uj;
    std::optional<uint64_t> rapl_dram_uj;
    std::optional<int32_t> cpu_temp_mdeg; // k10temp / coretemp
    uint32_t cpu_freq_avg_khz{0};
    uint32_t cpu_freq_min_khz{0};
    uint32_t cpu_freq_max_khz{0};
    uint32_t cpu_cores_online{0};
    std::array<uint64_t, 4> cstate_time_us{}; // Aggregate POLL, C1, C2, C3
    std::array<char, 16> cpu_governor{};

    // 3. Graphics Processing Unit (AMDGPU / DRM) (REF-REQ-010 Sec 2.3, REF-REQ-051)
    std::optional<uint64_t> gpu_power_uw; // Raw power reading (Package Power Tracking or discrete board)
    bool gpu_is_apu_ppt{false};           // True if sensor is APU Package Power Tracking (PPT), not standalone dGPU
    std::array<char, 16> gpu_power_label{}; // Sensor label (e.g. "PPT", "edge", "VDDGFX")
    std::optional<uint32_t> gpu_busy_percent;
    std::optional<uint64_t> gpu_freq_hz;
    std::optional<int32_t> gpu_temp_mdeg;
    std::optional<uint32_t> gpu_vddgfx_mv;
    std::optional<uint32_t> gpu_vddsoc_mv;
    std::optional<uint64_t> gpu_vram_used_bytes;
    std::optional<uint64_t> gpu_vram_total_bytes;
    std::array<char, 32> gpu_pcie_link_speed{};
    std::optional<uint32_t> gpu_pcie_link_width;

    // 4. Storage Subsystem (NVMe SSD) (REF-REQ-010 Sec 2.4)
    bool nvme_active{false};
    std::optional<int32_t> nvme_temp_composite_mdeg;
    std::optional<int32_t> nvme_temp_sensor1_mdeg;
    uint64_t disk_read_sectors{0};
    uint64_t disk_write_sectors{0};
    uint64_t disk_io_ticks_ms{0};

    // 5. Thermal & Mechanical Chassis (ThinkPad EC) (REF-REQ-010 Sec 2.5)
    std::optional<uint32_t> fan_rpm;
    std::optional<uint32_t> fan_pwm;
    std::optional<int32_t> chassis_temp_mdeg;
    std::optional<uint32_t> kbdlight_level;
    std::optional<bool> bluetooth_enabled;

    // 6. Display Subsystem (REF-REQ-010 Sec 2.6)
    std::optional<uint32_t> backlight_brightness;
    std::optional<uint32_t> backlight_max_brightness;

    // 7. Bus & Wireless Peripherals (REF-REQ-010 Sec 2.7)
    bool wifi_active{false};
    std::optional<int32_t> wifi_temp_mdeg;
    std::array<char, 32> aspm_policy{};

    // 8. Syscall-Level Direct Hardware Telemetry (REF-REQ-015, REF-REQ-024)
    uint64_t pmu_instructions{0};
    uint64_t pmu_cycles{0};
    double pmu_ipc{0.0};
    uint64_t pmu_llc_misses{0};
    uint64_t pmu_branch_misses{0};
    double pmu_energy_proxy_index{0.0};  // EPI: Instructions * IPC + 200 * LLC_Miss + 30 * Branch_Miss (REF-REQ-024)
    double pmu_estimated_power_mw{0.0};  // P_est: Micro-power estimate in mW (REF-REQ-024)
    double pmu_energy_waste_ratio{0.0};  // EWR: Percentage of energy wasted on cache/branch stalls (0-100%) (REF-REQ-024)
    std::optional<uint32_t> cpu_core_vid_mv; // Silicon Core Voltage (mV) via MSR / hwmon
    uint8_t pcie_link_speed_gen{0};          // Binary Config Space: PCIe Gen (1-5)
    uint8_t pcie_link_width_lanes{0};        // Binary Config Space: Negotiated Width (1-16)
};

// Zero-Allocation Fixed-Capacity Process Comm (REF-REQ-009, REF-ARCH-005)
// TASK_COMM_LEN in Linux kernel is strictly 16 bytes.
// Making ProcessSample TriviallyCopyable unlocks SIMD vector memmove/sort.
struct ProcessComm {
    std::array<char, 16> data{};

    ProcessComm() noexcept = default;
    ProcessComm(std::string_view sv) noexcept {
        size_t len = std::min(sv.size(), size_t{15});
        std::memcpy(data.data(), sv.data(), len);
        data[len] = '\0';
    }
    ProcessComm(const char* s) noexcept {
        if (!s) return;
        size_t len = std::min(std::strlen(s), size_t{15});
        std::memcpy(data.data(), s, len);
        data[len] = '\0';
    }

    [[nodiscard]] const char* c_str() const noexcept { return data.data(); }
    [[nodiscard]] std::string_view view() const noexcept { return std::string_view(data.data()); }
    [[nodiscard]] size_t size() const noexcept { return std::strlen(data.data()); }
    [[nodiscard]] bool empty() const noexcept { return data[0] == '\0'; }

    [[nodiscard]] std::string_view substr(size_t pos = 0, size_t count = std::string_view::npos) const noexcept {
        return view().substr(pos, count);
    }

    bool operator==(std::string_view sv) const noexcept { return view() == sv; }
    bool operator==(const char* s) const noexcept { return std::strcmp(data.data(), s) == 0; }
    bool operator==(const ProcessComm& o) const noexcept { return data == o.data; }

    friend std::ostream& operator<<(std::ostream& os, const ProcessComm& pc) {
        return os << pc.data.data();
    }
};

// Implements REF-REQ-004, REF-REQ-011, REF-ARCH-002, REF-ARCH-007 & REF-RES-011
// Cache-Line Aligned Process Hot Chunk (64 bytes exact)
// Holds all high-probability metrics accessed in 100% of monitoring iterations.
struct alignas(64) ProcessHotChunk {
    int32_t pid{0};
    int32_t ppid{0};
    uint64_t utime_ticks{0};
    uint64_t stime_ticks{0};
    uint64_t voluntary_ctxt_switches{0};
    uint64_t nonvoluntary_ctxt_switches{0};
    uint32_t rss_kib{0};
    uint32_t pss_kib{0};
    uint32_t minflt{0};
    uint32_t majflt{0};

    // Bit-Packed Metadata Word (8 bytes / 64 bits):
    // Extreme 66% space reduction vs unpackaged scalar ints.
    int64_t cpu_core : 10 {-1};
    uint64_t num_threads : 16 {1};
    int64_t nice : 6 {0};
    // Linux /proc/<pid>/stat priority is 0..139 (nice=19 -> 139). A *signed*
    // 8-bit field truncates 139 to -117, corrupting every niced/RT process.
    // 8 unsigned bits hold the full range without changing the 64-bit layout.
    uint64_t priority : 8 {0};
    uint64_t open_sockets : 12 {0};
    uint64_t has_io_perm : 1 {1};
    uint64_t is_kthread : 1 {0};
    uint64_t cross_ccx_migrated : 1 {0};
    uint64_t reserved_flags : 9 {0};

    [[nodiscard]] inline uint64_t total_cpu_ticks() const noexcept {
        return utime_ticks + stime_ticks;
    }
    [[nodiscard]] inline uint64_t total_wakeups() const noexcept {
        return voluntary_ctxt_switches + nonvoluntary_ctxt_switches;
    }
};
static_assert(sizeof(ProcessHotChunk) == 64, "ProcessHotChunk must be exactly 64 bytes (1 cache line)");
static_assert(alignof(ProcessHotChunk) == 64, "ProcessHotChunk must be 64-byte cacheline aligned");
static_assert(std::is_trivially_copyable_v<ProcessHotChunk>, "ProcessHotChunk must be TriviallyCopyable");

// Ultra-Dense 32-byte Process Hot Record (REF-ARCH-007)
// Packs two complete processes into a single 64-byte hardware cache line.
// 500 active processes require ONLY 16 KB (fits in half of L1D cache).
struct CompactProcessHot {
    uint32_t pid : 22 {0};
    uint32_t is_kthread : 1 {0};
    uint32_t has_io_perm : 1 {1};
    uint32_t reserved_bits : 8 {0};

    uint32_t delta_cpu_ticks{0};
    uint32_t delta_wakeups{0};
    uint32_t rss_kib{0};
    uint32_t pss_kib{0};
    uint32_t minflt{0};
    uint16_t majflt{0};

    int16_t cpu_core{-1};
    uint16_t num_threads{1};
    int8_t nice{0};
    // priority is 0..139; int8_t truncates it. uint8_t covers the full range.
    uint8_t priority{0};
};
static_assert(sizeof(CompactProcessHot) == 32, "CompactProcessHot must be exactly 32 bytes (2 per cacheline)");
static_assert(std::is_trivially_copyable_v<CompactProcessHot>, "CompactProcessHot must be TriviallyCopyable");

// Implements REF-REQ-004, REF-REQ-011, REF-ARCH-002, REF-ARCH-007 & REF-RES-011
// TriviallyCopyable POD: 64-byte Hot/Cold cacheline aligned
struct alignas(64) ProcessSample {
    // =========================================================================
    // 1. HOT CACHE-LINE CHUNK (Bytes 0..63): 100% Sequential Access Frequency
    // Serves inner monitoring, two-pointer delta, and WDI attribution loops.
    // Exact 64-byte hardware footprint ensures ZERO L1D cache line crossing.
    // =========================================================================
    int32_t pid{0};
    int32_t ppid{0};
    uint64_t utime_ticks{0};
    uint64_t stime_ticks{0};
    uint64_t voluntary_ctxt_switches{0};
    uint64_t nonvoluntary_ctxt_switches{0};
    uint32_t rss_kib{0};
    uint32_t pss_kib{0};
    uint32_t minflt{0};
    uint32_t majflt{0};

    // Bit-Packed Metadata Word (8 bytes / 64 bits)
    int64_t cpu_core : 10 {-1};
    uint64_t num_threads : 16 {1};
    int64_t nice : 6 {0};
    // See ProcessHotChunk: priority is 0..139, so this must be unsigned.
    uint64_t priority : 8 {0};
    uint64_t open_sockets : 12 {0};
    uint64_t has_io_perm : 1 {1};
    uint64_t is_kthread : 1 {0};
    uint64_t cross_ccx_migrated : 1 {0};
    uint64_t reserved_flags : 9 {0};

    // =========================================================================
    // 2. WARM & COLD CACHE-LINE CHUNK (Bytes 64..191): Conditional / Rare Access
    // String names, DRM engine telemetry, IO bandwidth, and fd caches.
    // Separated from Hot Chunk so that steady-state loops never fetch these lines.
    // =========================================================================
    ProcessComm comm{};
    uint32_t uid{0};
    int32_t pinned_drm_fd{-1}; // REF-RES-006: Cached DRM render node fd
    uint64_t timerslack_ns{50000};
    uint64_t read_bytes{0};
    uint64_t write_bytes{0};
    uint64_t io_syscalls{0};
    uint64_t drm_engine_gfx_ns{0};
    uint64_t drm_engine_compute_ns{0};
    uint64_t drm_engine_dec_ns{0};
    uint64_t drm_engine_enc_ns{0};
    uint64_t drm_vram_kib{0};

    [[nodiscard]] inline const ProcessHotChunk& hot() const noexcept {
        return *reinterpret_cast<const ProcessHotChunk*>(this);
    }
    [[nodiscard]] inline ProcessHotChunk& hot() noexcept {
        return *reinterpret_cast<ProcessHotChunk*>(this);
    }
};

static_assert(std::is_trivially_copyable_v<ProcessSample>, "ProcessSample must be TriviallyCopyable for SIMD acceleration");
static_assert(alignof(ProcessSample) == 64, "ProcessSample must be 64-byte cacheline aligned");
static_assert(sizeof(ProcessSample) == 192, "ProcessSample must span exactly 3 cachelines (1 Hot + 2 Cold)");


// Implements REF-REQ-001, REF-REQ-010 & REF-RES-002
struct HardwarePowerBreakdown {
    // Hardware Domain Power (Watts)
    double total_system_watts{0.0};
    double cpu_package_watts{0.0};
    double gpu_watts{0.0};
    double display_watts{0.0};
    double fan_estimated_watts{0.0};
    double storage_estimated_watts{0.0};
    double uncore_and_platform_watts{0.0};

    // Operational States
    bool is_battery_discharging{false};
    bool is_ac_online{false};
    bool has_direct_rapl{false};

    // Battery Health, Chemistry & Degradation (REF-REQ-022)
    double battery_health_percent{0.0};
    double battery_degradation_percent{0.0};
    double battery_lost_capacity_wh{0.0};
    double battery_energy_now_wh{0.0};
    double battery_energy_full_wh{0.0};
    double battery_energy_design_wh{0.0};
    double battery_voltage_now_v{0.0};
    double battery_voltage_min_design_v{0.0};
    double battery_current_now_a{0.0};
    double battery_remaining_hours{0.0}; // Discharge: time to empty
    double battery_remaining_hours_to_empty{0.0};
    double battery_remaining_hours_to_threshold{0.0}; // Charge: time to threshold (e.g. 80%)
    double battery_remaining_hours_to_full{0.0};      // Charge: time to 100% full
    uint32_t battery_cycle_count{0};
    uint32_t battery_capacity_percent{0};
    bool is_conservation_mode_active{false};
    bool is_ac_passthrough{false};

    // Deep Identity & Thresholds
    core::FixedString<16> battery_technology{};
    core::FixedString<16> battery_capacity_level{};
    core::FixedString<32> battery_model_name{};
    core::FixedString<24> battery_manufacturer{};
    core::FixedString<24> battery_serial_number{};
    std::optional<uint32_t> battery_charge_start_threshold;
    std::optional<uint32_t> battery_charge_end_threshold;
    core::FixedString<32> battery_charge_behaviour{};

    // USB-PD Power & Type
    double usbc_input_watts{0.0};
    core::FixedString<32> usbc_pd_type{};
    bool usbc_online{false};

    // Connected Peripherals (Bluetooth mouse/keyboard/etc.)
    core::FixedVector<HardwareSample::PeripheralBattery, 4> peripheral_batteries{};

    // CPU & Platform Telemetry
    double cpu_temp_c{0.0};
    double cpu_freq_avg_mhz{0.0};
    double cpu_freq_min_mhz{0.0};
    double cpu_freq_max_mhz{0.0};
    core::FixedString<32> cpu_governor{};
    double cstate_c0_active_percent{0.0};
    double cstate_c1_percent{0.0};
    double cstate_c2_percent{0.0};
    double cstate_c3_deep_percent{0.0};

    // GPU Telemetry
    uint32_t gpu_busy_percent{0};
    double gpu_freq_mhz{0.0};
    double gpu_temp_c{0.0};
    double gpu_vddgfx_v{0.0};
    double gpu_vddsoc_v{0.0};
    double gpu_vram_used_mb{0.0};
    double gpu_vram_total_mb{0.0};
    core::FixedString<32> gpu_pcie_link{};

    // Storage Telemetry
    core::FixedString<32> nvme_status{};
    double nvme_temp_c{0.0};
    double disk_read_mb_per_sec{0.0};
    double disk_write_mb_per_sec{0.0};

    // Chassis & Cooling
    uint32_t fan_rpm{0};
    uint32_t kbdlight_level{0};
    bool bluetooth_enabled{false};

    // Display & Wireless
    double display_brightness_percent{0.0};
    core::FixedString<32> wifi_status{};
    double wifi_temp_c{0.0};
    core::FixedString<32> aspm_policy{};

    // Direct Syscall Hardware Telemetry (REF-REQ-015, REF-REQ-024)
    uint64_t pmu_instructions{0};
    uint64_t pmu_cycles{0};
    double pmu_ipc{0.0};
    uint64_t pmu_llc_misses{0};
    uint64_t pmu_branch_misses{0};
    double pmu_energy_proxy_index{0.0};
    double pmu_estimated_power_mw{0.0};
    double pmu_energy_waste_ratio{0.0};
    std::optional<uint32_t> cpu_core_vid_mv;
    uint8_t pcie_link_speed_gen{0};
    uint8_t pcie_link_width_lanes{0};
};

// Implements REF-REQ-004, REF-REQ-011 & REF-RES-003
struct ProcessAttributedPower {
    int32_t pid{0};
    ProcessComm comm{};
    uint32_t uid{0};
    double cpu_watts{0.0};
    double gpu_watts{0.0};
    double io_watts{0.0};
    double wakeup_tax_watts{0.0};
    double fan_attributed_watts{0.0};
    double total_attributed_watts{0.0};
    double wdi_score{0.0}; // WattCurb Drain Index
    uint64_t wakeups_per_sec{0};
    uint64_t vram_kib{0};
    double disk_io_mb_per_sec{0.0};
    bool is_runaway_candidate{false};

    // Deep Process Physical Telemetry (REF-REQ-013, REF-REQ-016)
    int32_t cpu_core{-1};
    uint32_t num_threads{1};
    int32_t nice{0};
    int32_t priority{0};
    bool cross_ccx_migration{false};
    uint64_t timerslack_ns{50000};
    uint64_t pss_kib{0};
    uint64_t minflt_per_sec{0};
    uint64_t majflt_per_sec{0};
    uint32_t open_sockets{0};
    double wifi_attributed_watts{0.0};
    double dram_attributed_watts{0.0};

    core::FixedString<4> cstate_affinity{"C3"}; // e.g. "C0", "C1", "C2", "C3" (REF-REQ-090, REF-ARCH-067)
    core::FixedString<32> primary_hw_domain;  // e.g. "GPU Silicon", "CPU C-State Wakeup", "CPU Compute", "NVMe Storage"
    core::FixedString<96> hardware_mechanism; // e.g. "AMDGPU GFX Engine (455MB VRAM, 98% GPU)"
    uint8_t safety_tier{5};                  // ProcessSafetyTier (Tier 0-5)
    uint8_t recommended_action{0};           // MitigationAction (0-5)
};

static_assert(std::is_trivially_copyable_v<ProcessAttributedPower>, "ProcessAttributedPower must be TriviallyCopyable for SIMD acceleration");

// Implements REF-REQ-011 (Hardware Domain Direct Attribution)
struct ProcessDomainShare {
    int32_t pid{0};
    ProcessComm comm{};
    double watts{0.0};
    double share_percent{0.0};
    core::FixedString<80> detail;
};

static_assert(std::is_trivially_copyable_v<ProcessDomainShare>, "ProcessDomainShare must be TriviallyCopyable");

struct DomainCulprit {
    core::FixedString<64> domain_name;
    double domain_total_watts{0.0};
    core::FixedVector<ProcessDomainShare, 8> top_culprits;
};

// Implements REF-REQ-031, REF-REQ-035 & REF-ARCH-021: 4-Stage Adaptive Power Profiles
enum class PowerProfileMode : uint8_t {
    Performance = 0,     // Full 4.1GHz boost, zero throttling, SMU 25W unlocked
    Balanced = 1,        // Dynamic clock, standard CFS, runaway mitigation only
    PowerSaver = 2,      // Smart Save: 1.7GHz cap, SCHED_IDLE on background workers
    UltraEndurance = 3   // Ultra Save: 1.4GHz, SCHED_IDLE graceful throttle, 48Hz panel, zero-kill non-halting safety (REF-REQ-044)
};

// REF-REQ-094: Per-owner record of which battery thresholds have already been
// crossed in the current discharge cycle. Each threshold demotes the profile at
// most once, so the daemon can never keep pulling a user off their choice.
struct ProfileDemotionLatch {
    bool crossed_30{false};
    bool crossed_20{false};
};

struct ActiveMitigationStatus {
    PowerProfileMode current_profile{PowerProfileMode::Balanced};
    size_t throttled_count{0};
    size_t frozen_count{0};
    uint64_t reclaimed_bytes{0};
    double estimated_savings_watts{0.0};
    core::FixedString<128> active_summary;
    std::array<core::FixedString<96>, 8> feature_summaries{};
    size_t feature_summary_count{0};
};

static_assert(std::is_trivially_copyable_v<ActiveMitigationStatus>, "ActiveMitigationStatus must be TriviallyCopyable");

// Implements REF-REQ-005, REF-REQ-011, REF-REQ-012 & REF-ARCH-002
struct AnalysisReportData {
    std::chrono::milliseconds sample_duration{0};
    HardwarePowerBreakdown hardware;
    core::FixedVector<ProcessAttributedPower, 64> top_processes;
    core::FixedVector<DomainCulprit, 16> domain_culprits;
    size_t total_monitored_processes{0};
    uint64_t total_system_wakeups_per_sec{0};
    size_t sample_count{1};
    double total_energy_joules{0.0};
    bool is_short_window{false};
    ActiveMitigationStatus mitigation_status;
};

// Zero-Allocation Cacheline-Aligned Process Snapshot & Ping-Pong Pool (REF-ARCH-006, REF-REQ-018)
// Generous 2048-entry headroom accommodates server bursts and container storms.
using ProcessSnapshot = core::FixedVector<ProcessSample, 2048>;
using ProcessPool = core::DoubleBufferedPool<ProcessSample, 2048>;

} // namespace wattcurb
