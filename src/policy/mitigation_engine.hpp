#pragma once

#include "core/types.hpp"
#include "policy/process_classifier.hpp"
#include <string_view>
#include <cstdint>
#include <optional>

namespace wattcurb::policy {

// Implements REF-REQ-019, REF-REQ-031, REF-ARCH-008 & REF-ARCH-021
// Closed-Loop Adaptive Mitigation Engine with 3-Tier State Machine & Bidirectional Rollback
class MitigationEngine {
public:
    MitigationEngine() noexcept;

    // Evaluates current power report, determines profile via hysteresis, and applies closed-loop actuations
    ActiveMitigationStatus evaluate_and_actuate(
        AnalysisReportData& report,
        bool on_battery,
        double battery_pct
    ) noexcept;

    // Profile inspection and manual override
    [[nodiscard]] PowerProfileMode current_profile() const noexcept { return m_current_profile; }
    void set_profile_override(std::optional<PowerProfileMode> mode) noexcept { m_profile_override = mode; }
    [[nodiscard]] std::optional<PowerProfileMode> profile_override() const noexcept { return m_profile_override; }

    // Hysteresis-aware profile calculator (REF-REQ-031 Sec 2.2)
    [[nodiscard]] PowerProfileMode determine_profile(bool on_battery, double battery_pct) const noexcept;

    // Bidirectional Rollback (REF-REQ-031 Sec 3.2, REF-ARCH-021)
    void rollback_all() noexcept;
    void thaw_all_frozen() noexcept;
    [[nodiscard]] size_t tracked_count() const noexcept { return m_tracked.size(); }

    // Process-Level Actuation Primitives
    static bool apply_sched_idle(int32_t pid) noexcept;
    static bool restore_sched_normal(int32_t pid, int original_policy = 0, int original_nice = 0) noexcept;
    static bool apply_timer_slack(int32_t pid, uint64_t slack_ns) noexcept;
    static bool apply_memory_reclaim(int32_t pid, uint64_t bytes) noexcept;
    static bool apply_cgroup_freeze(int32_t pid, bool freeze) noexcept;
    static bool apply_cgroup_cpu_quota(int32_t pid, uint32_t max_quota_us = 200000, uint32_t period_us = 100000) noexcept;
    static bool restore_cgroup_cpu_quota(int32_t pid) noexcept;

    // Anti-Starvation & CPU Headroom Partitioning (REF-REQ-054, REF-ARCH-030, REF-REQ-057)
    static int32_t get_total_online_cpus() noexcept;
    static int32_t get_reserved_headroom_cores() noexcept;
    static cpu_set_t get_headroom_allowed_cpuset(PowerProfileMode mode = PowerProfileMode::Balanced) noexcept;
    static cpu_set_t get_all_cores_cpuset() noexcept;
    static bool apply_core_affinity_cap(int32_t pid, const cpu_set_t* allowed_set = nullptr) noexcept;
    static bool restore_core_affinity(int32_t pid, const cpu_set_t* target_affinity = nullptr) noexcept;
    static bool apply_sched_batch(int32_t pid, int nice_val = 10) noexcept;

    // Adaptive C1/C2 Dual-Cluster Spatial Load Dispersion & Interactive Latency Shield (REF-REQ-084, REF-ARCH-061)
    struct alignas(64) CpuClusterTopology {
        int32_t total_cpus{16};
        int32_t cluster_count{2};
        cpu_set_t c1_cpuset{};                 // Primary / Interactive Cluster (e.g. Cores 0..7)
        cpu_set_t c2_cpuset{};                 // Secondary / Compute Cluster (e.g. Cores 8..15)
        cpu_set_t interactive_shield_cpuset{}; // Clean headroom within C1 (e.g. Cores 0..3)
        cpu_set_t all_cores_cpuset{};
    };

    static const CpuClusterTopology& get_cluster_topology() noexcept;
    static cpu_set_t get_c1_cpuset() noexcept;
    static cpu_set_t get_c2_cpuset() noexcept;
    static cpu_set_t get_interactive_shield_cpuset() noexcept;
    static bool shield_interactive_process(int32_t pid, std::string_view comm) noexcept;
    static void shield_all_interactive_terminals() noexcept;
    static bool is_heavy_compute_candidate(const ProcessAttributedPower& proc) noexcept;

    // Hardware Baseline & Actuation Primitives (REF-REQ-055, REF-ARCH-031, REF-REQ-063)
    struct alignas(64) HardwareBaselineState {
        bool captured{false};
        char platform_profile[32]{"balanced"};
        char cpu_governor[32]{"schedutil"};
        int cpu_boost{1};
        char aspm_policy[32]{"default"};
        uint32_t scaling_max_freq_khz{0};
        uint32_t panel_power_savings{1};
        char gpu_dpm_level[32]{"auto"};
        char smt_control[16]{"on"};
        bool bluetooth_blocked{false};
        uint32_t backlight_brightness{0};
        uint32_t backlight_max{0};
        bool backlight_capped{false};
        bool kwin_blur_unloaded{false};
        bool drrs_applied{false};
        bool baloo_suspended{false};
        bool wifi_txpower_capped{false};
        uint32_t vm_dirty_writeback_centisecs{500};
        uint32_t vm_dirty_expire_centisecs{3000};
        uint32_t vm_laptop_mode{0};
        bool vm_writeback_modified{false};
    };

    static void capture_hardware_baseline() noexcept;
    static void restore_hardware_baseline() noexcept;
    [[nodiscard]] static const HardwareBaselineState& hardware_baseline() noexcept;

    static bool set_platform_profile(const char* profile) noexcept;
    static bool set_cpu_governor(const char* governor) noexcept;
    static bool set_cpu_boost(bool enable) noexcept;
    static bool set_cpu_scaling_max_freq(uint32_t khz) noexcept;
    static bool set_panel_power_savings(uint32_t level) noexcept;
    static bool set_pcie_aspm_policy(const char* policy) noexcept;
    static bool set_cpu_epp_policy(const char* policy) noexcept;
    static bool set_gpu_max_clock(uint32_t mhz) noexcept;
    static bool restore_gpu_max_clock() noexcept;
    static bool set_gpu_dpm_level(const char* level) noexcept;
    static bool set_smt_control(const char* state) noexcept;
    static bool set_bluetooth_blocked(bool block) noexcept;
    static bool cap_display_backlight(double max_pct) noexcept;
    static bool restore_display_backlight() noexcept;
    static bool set_display_refresh_rate(uint32_t hz) noexcept;
    static bool set_kwin_effects_suspended(bool suspend) noexcept;
    static bool set_baloo_suspended(bool suspend) noexcept;
    static bool set_wifi_txpower_limit(uint32_t mbm) noexcept;
    static bool restore_wifi_txpower() noexcept;
    static bool set_vm_dirty_writeback_centisecs(uint32_t centisecs) noexcept;
    static bool set_vm_dirty_expire_centisecs(uint32_t centisecs) noexcept;
    static bool set_vm_laptop_mode(uint32_t mode) noexcept;
    static bool restore_vm_writeback_baseline() noexcept;
    static bool apply_power_profile(PowerProfileMode mode) noexcept;

    // Process Immunity & Audio Protection (REF-REQ-049, REF-REQ-054)
    static bool is_immune_process(int32_t pid) noexcept;
    static void audit_and_heal_audio_stack() noexcept;

    // Fast resolution of cgroup v2 path for a given pid without heap allocations
    static bool resolve_cgroup_path(int32_t pid, char* out_buf, size_t out_cap) noexcept;

    // Internal tracking structure for rollback & idempotency (REF-REQ-055)
    static constexpr size_t MAX_TRACKED_MITIGATIONS = 128;
    struct alignas(64) TrackedMitigation {
        int32_t pid{0};
        ProcessSafetyTier tier{ProcessSafetyTier::BackgroundWorker};
        MitigationAction current_action{MitigationAction::None};
        uint64_t applied_timestamp_sec{0};
        
        // Exact Pre-Mitigation Baseline State Journal (REF-REQ-055)
        int original_nice{0};
        int original_sched_policy{0}; // SCHED_OTHER
        uint64_t original_timerslack_ns{50000};
        cpu_set_t original_affinity{};
        bool affinity_capped{false};
        bool sched_batch_applied{false};
        bool sched_idle_applied{false};
        bool c2_cluster_dispersed{false};
        uint32_t low_power_ticks{0};
    };

    [[nodiscard]] const core::FixedVector<TrackedMitigation, MAX_TRACKED_MITIGATIONS>& tracked_mitigations() const noexcept {
        return m_tracked;
    }

private:
    PowerProfileMode m_current_profile{PowerProfileMode::Balanced};
    std::optional<PowerProfileMode> m_profile_override{std::nullopt};
    bool m_aspm_modified{false};
    bool m_backlight_capped{false};
    uint32_t m_scan_counter{0};

    core::FixedVector<TrackedMitigation, MAX_TRACKED_MITIGATIONS> m_tracked{};
};

} // namespace wattcurb::policy
