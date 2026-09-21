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
    // ---------------------------------------------------------------------
    // REF-REQ-094: Single authority for automatic profile demotion.
    //
    // WattCurb must never change the user's chosen profile on its own except at
    // three battery thresholds, each firing AT MOST ONCE per discharge cycle:
    //     <= 30%   Performance -> Balanced   (only when currently Performance)
    //     <= 20%   any         -> PowerSaver
    //     <=  5%   any         -> UltraEndurance (enforced continuously)
    // Above 30%, on AC, and in every other situation the current profile stands.
    //
    // The latch pair is the caller's, so every policy site (MitigationEngine,
    // FeatureManager) shares one rule while keeping its own crossing state.
    // ---------------------------------------------------------------------
    [[nodiscard]] static PowerProfileMode resolve_profile(
        PowerProfileMode current, bool on_battery, double battery_pct,
        ProfileDemotionLatch& latch) noexcept;

    [[nodiscard]] PowerProfileMode determine_profile(bool on_battery, double battery_pct) noexcept;

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
        int audio_power_save{-1};
        char audio_power_save_controller[8]{"N"};
        bool audio_power_save_modified{false};
        bool pcie_runtime_pm_modified{false};
        bool usb_runtime_pm_modified{false};
        // Ultimate Performance Unleash Baseline & Tracking (REF-REQ-092, REF-ARCH-069)
        bool performance_pm_qos_active{false};
        int performance_pm_qos_fd{-1};
        int gpu_power_profile_mode_baseline{-1};
        bool gpu_power_profile_mode_modified{false};
        uint32_t nvme_apst_latency_baseline_us{100000};
        bool nvme_apst_modified{false};
        // Per-controller runtime PM baselines ("auto" / "on"), REF-REQ-092.4.
        // Captured separately from the nvme_core module parameter: the two are
        // independent knobs and restoring a constant into either one would
        // overwrite configuration WattCurb never set.
        static constexpr size_t MAX_NVME_CONTROLLERS = 4;
        char nvme_power_control_baseline[MAX_NVME_CONTROLLERS][8]{};
        bool nvme_power_control_modified{false};
        uint64_t sched_migration_cost_baseline_ns{500000};
        bool sched_migration_cost_modified{false};
        bool wifi_power_save_baseline{true};
        bool wifi_power_save_disabled{false};
        // True while the full-silicon unleash is the *last* actuation applied.
        // Transition-path ordering invariant for REF-TEST-056: a profile switch
        // into Performance must leave this engaged, i.e. rollback_all() must run
        // before apply_power_profile(), never after.
        bool performance_unleash_engaged{false};
        // REF-REQ-096: Audio continuity. Held independently of the Performance
        // clamp - the kernel takes the MINIMUM of all cpu_dma_latency holders, so
        // the two constraints compose without any interaction logic.
        int audio_pm_qos_fd{-1};
        int audio_codec_power_save_suspended{-1}; // value to restore, -1 = untouched
    };

    static void capture_hardware_baseline() noexcept;
    static void restore_hardware_baseline() noexcept;
    [[nodiscard]] static const HardwareBaselineState& hardware_baseline() noexcept;

    // REF-REQ-109: name of a competing power manager holding the same knobs, or
    // nullptr. Walks /proc, so callers cache the result.
    [[nodiscard]] static const char* competing_power_manager() noexcept;
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
    // Ultimate UltraEndurance Full-Spectrum Power Minimization (REF-REQ-088, REF-ARCH-065)
    static void trigger_3tier_vram_gc() noexcept;
    static void apply_pcie_runtime_pm_auto() noexcept;
    static void apply_usb_runtime_pm_auto() noexcept;
    static bool set_audio_codec_power_save(int seconds, bool controller = true) noexcept;
    static bool restore_audio_codec_baseline() noexcept;
    static bool apply_power_profile(PowerProfileMode mode) noexcept;

    // Hardware Actuation Sandbox (REF-REQ-092, REF-ARCH-069)
    // When engaged, every actuator that would mutate live kernel, sysfs or
    // process state performs no syscall and reports failure. Unit tests engage
    // it so the Oracle Gate never clamps the host CPU to C0, rewrites NVMe
    // APST, or renices the developer's audio daemon. Default is OFF.
    static void set_actuation_sandbox(bool enable) noexcept;
    [[nodiscard]] static bool actuation_sandboxed() noexcept;

    // Ultimate Performance Unleash Actuators (REF-REQ-092, REF-ARCH-069)
    static void set_performance_pm_qos(bool enable) noexcept;
    static bool set_gpu_power_profile_mode(int mode_id) noexcept;
    static bool restore_gpu_power_profile_mode_baseline() noexcept;
    static bool set_nvme_apst_max_latency(uint32_t max_latency_us) noexcept;
    static bool set_nvme_power_control(const char* value) noexcept;
    static bool restore_nvme_power_control_baseline() noexcept;
    static bool restore_nvme_apst_baseline() noexcept;
    static bool set_wifi_powersave(bool enable) noexcept;
    static bool restore_wifi_powersave_baseline() noexcept;
    static bool set_sched_migration_cost(uint64_t cost_ns) noexcept;
    static bool restore_sched_migration_cost_baseline() noexcept;
    // Single point of demotion: releases every Performance-only actuation back
    // to its captured baseline. Each step is individually "modified"-guarded so
    // hardware WattCurb never touched is never written.
    static void release_performance_unleash() noexcept;

    // ---------------------------------------------------------------------
    // REF-REQ-096: Audio Continuity Guarantee (every profile)
    //
    // C3 on this class of silicon costs 350 us to exit, against 18 us for C2 and
    // 1 us for C1. Outside Performance mode nothing held a cpu_dma_latency
    // constraint, so the idle governor was free to park in C3 and a PipeWire
    // quantum could miss its deadline - audible as a dropout. While a PCM
    // playback stream is RUNNING the daemon therefore holds a bounded latency
    // ceiling in EVERY profile: deep enough to keep POLL/C1/C2 available, tight
    // enough to shut C3 out.
    // ---------------------------------------------------------------------
    static constexpr int32_t AUDIO_DMA_LATENCY_US = 100;

    // REF-REQ-108.2: bounded writeback coalescing for UltraEndurance. Named
    // constants so REF-TEST-061 asserts the same values the actuator writes.
    static constexpr uint32_t ULTRA_DIRTY_WRITEBACK_CS = 1500; // 15 s
    static constexpr uint32_t ULTRA_DIRTY_EXPIRE_CS    = 3000; // 30 s

    // REF-REQ-107.2: true while the Performance path holds a C0 clamp. It must
    // never be true after apply_power_profile(Performance).
    [[nodiscard]] static bool performance_pm_qos_held() noexcept;
    static constexpr size_t MAX_AUDIO_OWNERS = 8;

    struct AudioStreamState {
        bool active{false};
        size_t owner_count{0};
        int32_t owner_pids[MAX_AUDIO_OWNERS]{};
    };

    static void refresh_audio_stream_state() noexcept;
    [[nodiscard]] static const AudioStreamState& audio_stream_state() noexcept;
    [[nodiscard]] static bool is_audio_owner(int32_t pid) noexcept;
    static void set_audio_latency_floor(bool engage) noexcept;

    // REF-REQ-096.6: While a stream is RUNNING, anything that could be part of
    // the media pipeline keeps a normal scheduling class. The stream owner is the
    // sound server, not the player, and a player demoted to SCHED_IDLE stops
    // refilling its buffer just as surely. Genuine background workers (Tier 4/5)
    // stay throttleable, so playback does not suspend power saving wholesale.
    [[nodiscard]] static bool is_audio_shielded(int32_t pid) noexcept;
    // REF-REQ-108: Tier 0..3 are never demoted to the idle class, in any profile
    // and whether or not audio is playing. UltraEndurance may be slow; it may not
    // stop responding.
    [[nodiscard]] static bool is_stall_shielded(int32_t pid) noexcept;

    // REF-REQ-098: LIVENESS INVARIANT.
    // Input handling and window management must keep a working share of the
    // machine in EVERY profile, UltraEndurance included. Deep saving is about
    // allocating what is left well, not about starving the parts of the system
    // the user is actually touching: a compositor that cannot be scheduled is
    // indistinguishable from a hung machine. Tier 0..2 (critical daemons,
    // compositor, desktop shell) are therefore never demoted to SCHED_IDLE and
    // never frozen, in any profile.
    [[nodiscard]] static bool is_liveness_critical(int32_t pid) noexcept;

    // REF-REQ-101: True when the process belongs to a graphical user session.
    // The classifier defaults any name it does not recognise to Tier 5 with
    // SchedIdle - guilty until proven innocent - so an application the user is
    // actively working in gets demoted to the idle class and stops responding.
    // Session membership is evidence the name list does not have.
    [[nodiscard]] static bool is_graphical_session_process(int32_t pid) noexcept;

    // REF-REQ-102: Restores a recorded affinity mask. An empty mask is treated as
    // "all online CPUs", so a missing record can never strand a process.
    static bool restore_process_affinity(int32_t pid, const cpu_set_t& original) noexcept;

    // REF-REQ-099: The profile actually in force, published by whichever policy
    // driver computed it. The static actuators need it: they are reached from
    // FeatureManager, which does not share MitigationEngine's instance state, so
    // without this they cannot tell Performance from UltraEndurance.
    static void set_effective_profile(PowerProfileMode mode) noexcept;
    [[nodiscard]] static PowerProfileMode effective_profile() noexcept;

    // REF-REQ-098: Runtime PM is an ALLOWLIST over PCI class codes. A class must
    // be known-safe to suspend, rather than merely not yet known to be fatal.
    // `pci_class` is the raw 0xCCSSPP value from sysfs.
    [[nodiscard]] static bool pci_class_allows_runtime_pm(uint32_t pci_class,
                                                          bool audio_active) noexcept;

    // REF-REQ-098: Pins scaling_min_freq back to the driver's minimum on every
    // profile application, so nothing can leave the machine crawling.
    static void enforce_cpu_freq_floor() noexcept;

    // Process Immunity & Audio Protection (REF-REQ-049, REF-REQ-054)
    static bool is_immune_process(int32_t pid) noexcept;
    static void audit_and_heal_audio_stack() noexcept;

    // REF-REQ-103: Sweeps every process currently demoted to SCHED_IDLE and
    // restores any that must never have been demoted. This does not rely on the
    // tracking table: a mitigation applied by a path that forgot to record it
    // could otherwise never be released, and the reported case was exactly that -
    // a desktop application left with 338 threads at SCHED_IDLE.
    static void heal_over_throttled_processes() noexcept;
    // REF-REQ-110: affinity is process state and outlives the daemon. Restores
    // shielded processes left masked by a previous run. Walks /proc - bootstrap
    // only, never per cycle.
    static void repair_orphaned_affinity_masks() noexcept;

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
    ProfileDemotionLatch m_demotion_latch{};
    std::optional<PowerProfileMode> m_profile_override{std::nullopt};
    bool m_aspm_modified{false};
    bool m_backlight_capped{false};
    uint32_t m_scan_counter{0};

    core::FixedVector<TrackedMitigation, MAX_TRACKED_MITIGATIONS> m_tracked{};
};

} // namespace wattcurb::policy
