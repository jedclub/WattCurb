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
    // file_only appends "swappiness=0", which makes the kernel reclaim
    // FILE-backed pages and leave anonymous pages alone. Under swap pressure
    // a plain reclaim pushes anon pages into the very tier that is running
    // out, so REF-REQ-112 uses the file-only form once the guard escalates.
    static bool apply_memory_reclaim(int32_t pid, uint64_t bytes, bool file_only = false) noexcept;
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
        // REF-REQ-112: the cpufreq driver's own ceiling, read from
        // cpuinfo_max_freq. scaling_max_freq_khz above is whatever sysfs held
        // when the daemon started, and after an unclean exit in PowerSaver or
        // UltraEndurance that value is WattCurb's OWN leftover cap, not the
        // user's configuration. Capturing it as "the baseline" launders the cap
        // into permanence: Performance and Balanced then "restore" 1.4 GHz and
        // the CPU never boosts again. Performance and Balanced assert this
        // field instead, and a captured ceiling below it is repaired at capture.
        uint32_t hw_max_freq_khz{0};
        bool orphaned_freq_cap_repaired{false};
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
        // REF-REQ-114: ThinkPad fan level for the Performance unleash. The EC's
        // automatic curve tops out around 4.3k RPM on this model; the fan can do
        // ~5.4k, which is the headroom the raised thermal limit needs. The level
        // is only touched when /proc/acpi/ibm/fan is writable (thinkpad_acpi
        // fan_control=1); otherwise set_fan_level() is a no-op.
        char fan_level_baseline[16]{"auto"};
        bool fan_level_modified{false};
        // REF-REQ-115: captured SMU limits from `ryzenadj -i` (0 = not captured /
        // ryzenadj absent). Restored verbatim when the saving profiles resume.
        // smu_tctl_c is in Celsius; the power fields are milliwatts. Each field
        // is only restored when it was actually captured (non-zero), so an
        // unrecognised column can never be written back as a guessed value.
        uint32_t smu_stapm_mw{0};
        uint32_t smu_fast_mw{0};
        uint32_t smu_slow_mw{0};
        uint32_t smu_apu_slow_mw{0};
        uint32_t smu_tctl_c{0};
        bool smu_limits_modified{false};
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
    // REF-REQ-112: assert "no WattCurb-imposed ceiling" - scaling_max_freq at
    // the driver's cpuinfo_max_freq and the boost bit set. Idempotent: reads
    // first and writes only the CPUs that diverge, so it can run every cycle
    // without turning into a sysfs write storm. Returns true if it had to
    // repair anything.
    static bool assert_unrestricted_cpu_ceiling() noexcept;
    // Number of times the assertion has been invoked this process. Exists so
    // REF-TEST-068 can prove the re-assertion is reached from the PRODUCTION
    // entry point (FeatureManager::evaluate_and_actuate) rather than only from
    // MitigationEngine::evaluate_and_actuate, which has no production caller.
    // The first implementation of REF-REQ-112.4 sat in the latter and never ran
    // outside the test suite.
    [[nodiscard]] static uint64_t ceiling_assertion_count() noexcept;

    // REF-REQ-112.10: load-aware frequency watchdog. A profile can be "correct"
    // on every knob - governor=performance, ceiling at cpuinfo_max, boost set,
    // platform_profile=performance - and still have the CPU pinned near its idle
    // clock by the EC or the firmware, which is exactly how the 2026-09-22
    // all-core collapse to ~400 MHz presented. This predicate is the trip
    // condition: the machine is loaded, yet even the highest core clock observed
    // this cycle is far below the hardware maximum. Pure, so the Oracle Gate can
    // prove the decision without a machine that is actually throttling.
    //
    // REF-REQ-126 (2026-09-23): both thresholds were calibrated for the
    // catastrophic collapse and missed the ordinary one. Measured on the host with
    // the EC's 6 W STAPM in force:
    //   * the delivered clock settles at the driver's lowest P-state, 1400 MHz,
    //     which is 0.824 of the 1700 MHz table maximum - above the old 0.6 gate
    //     (1020 MHz), so the watchdog saw nothing;
    //   * it does so under load1 3.5-7, below the old gate of 0.5 * 16 = 8.0.
    // The machine therefore sat at the P-state floor for as long as the EC kept
    // its own limit, and the repair path never ran. The gates now sit above the
    // floor and below the load levels that actually occur:
    //   * clock gate 0.85 * 1700 MHz = 1445 MHz -> the floor (1400/1397) trips it;
    //   * load gate 0.25 * ncpu = 4.0 on 16 threads -> load 4-7 trips it.
    // A trip only re-asserts the intended limits, so a false positive costs one
    // idempotent write and one log line, not a throttled machine.
    static constexpr double FREQ_STARVED_MIN_LOAD_RATIO = 0.25; // load1 >= 0.25 * ncpu
    static constexpr double FREQ_STARVED_CLOCK_FRACTION = 0.85; // max_clock < 0.85 * hw_max
    static constexpr uint32_t FREQ_STARVED_TRIP_CYCLES = 5;
    [[nodiscard]] static bool is_frequency_starved(uint64_t observed_max_khz,
                                                   uint64_t hw_max_khz,
                                                   double load1,
                                                   int32_t ncpu) noexcept;
    static bool set_panel_power_savings(uint32_t level) noexcept;
    static bool set_pcie_aspm_policy(const char* policy) noexcept;
    static bool set_cpu_epp_policy(const char* policy) noexcept;
    static bool set_gpu_max_clock(uint32_t mhz) noexcept;
    static bool restore_gpu_max_clock() noexcept;
    static bool set_gpu_dpm_level(const char* level) noexcept;
    static bool set_smt_control(const char* state) noexcept;

    // REF-REQ-114 / REF-REQ-118 / REF-REQ-124: ThinkPad fan curve. It applies in
    // ALL power profiles - it is a thermal-safety mapping, not a performance perk:
    //   * at or above FAN_FULL_TEMP_C (60 C)      -> level 7 (full speed)
    //   * between FAN_FULL_TEMP_C and FAN_CURVE_MIN_TEMP_C (35 C) -> linear
    //     100% .. FAN_CURVE_MIN_FRACTION*100 % (20%), mapped onto steps 1..7
    //   * at or below FAN_CURVE_MIN_TEMP_C (35 C) -> level 1 (20%, never 0)
    // UltraEndurance adds one exception (REF-REQ-118.3): at or below
    // FAN_ULTRA_STOP_TEMP_C (45 C) the fan is stopped outright (level 0), because
    // that profile exists to minimise every load and the EC's floor keeps the fan
    // turning for no thermal reason.
    //
    // REF-REQ-124 (2026-09-22): the full-speed threshold was lowered from 70 C to
    // 60 C and the ramp now spans 60 C -> 35 C instead of 70 C -> 35 C. The reason
    // is measured: the firmware clamps the SMU thermal limit (Tctl) to 70 C in
    // every platform profile and the OS cannot raise it (REF-REQ-123 2.4), so
    // 70 C is not "the temperature at which cooling should start" - it is the
    // temperature at which the part is already at its hard ceiling and begins
    // throttling. Reaching full fan only at the ceiling leaves no margin to hold
    // the clock; starting at 60 C gives 10 C of headroom, which is where the
    // boost budget now lives.
    //
    // Levels are the thinkpad_acpi discrete steps 0..7. The top step MUST be the
    // numeric 7: on this host `level full-speed` is accepted but resolves to
    // `disengaged` (see apply_fan_for_temp). Writing needs thinkpad_acpi
    // fan_control=1; without it the write is refused (no-op).
    static constexpr double FAN_CURVE_MIN_TEMP_C = 35.0;
    static constexpr double FAN_CURVE_MIN_FRACTION = 0.2;
    static constexpr double FAN_FULL_TEMP_C = 60.0;
    // REF-REQ-125: the sentinel for "the EC's full speed", written as the
    // `full-speed` keyword rather than as a number. Measured on this host:
    //   level 6 -> 4789 RPM, level 7 -> 4780 RPM,
    //   full-speed -> 5346 RPM (stable for 60 s, reproducible across writes).
    // The discrete steps do NOT reach the fan's maximum: numeric 7 sits ~560 RPM
    // below it. thinkpad_acpi maps `full-speed` (and its synonym `disengaged`) to
    // TP_EC_FAN_FULLSPEED, which is the real maximum. The earlier code comment
    // claimed "numeric 7 is the real full speed (measured ~5.3k RPM)" - that
    // 5.3k reading was taken while the fan was still coasting down from a
    // full-speed write, and the conclusion was wrong. The curve therefore uses
    // the keyword for the top step and numeric levels 1..6 for the ramp.
    static constexpr int FAN_LEVEL_FULL_SPEED = 8;
    // REF-REQ-118.3: UltraEndurance-only cold stop threshold.
    static constexpr double FAN_ULTRA_STOP_TEMP_C = 45.0;
    [[nodiscard]] static int fan_level_for_temp(double cpu_temp_c) noexcept;
    // Profile-aware variant: identical to the above except in UltraEndurance,
    // where <= FAN_ULTRA_STOP_TEMP_C returns 0 (fan stopped).
    [[nodiscard]] static int fan_level_for_temp_in_profile(double cpu_temp_c,
                                                           PowerProfileMode mode) noexcept;
    static bool set_fan_level(const char* level) noexcept;
    static bool restore_fan_level() noexcept;
    // Applies the curve for one temperature reading in the given profile. Returns
    // the level in force (0..7) or -1 when the fan is not controllable / no
    // reading. Writes only on a level change, so a steady temperature costs no
    // sysfs write.
    static int apply_fan_for_temp(double cpu_temp_c,
                                  PowerProfileMode mode) noexcept;
    // Number of times the curve was evaluated from the PRODUCTION entry point
    // (FeatureManager::evaluate_and_actuate). Mirrors ceiling_assertion_count()
    // so REF-TEST-073 can falsify a wiring that exists only under test.
    [[nodiscard]] static uint64_t fan_curve_application_count() noexcept;

    // REF-REQ-115: SMU thermal/power limits via ryzenadj. Performance and
    // Balanced raise the core thermal limit to SMU_TCTL_PERF_C so throttling only
    // starts there, with the fan at full speed keeping the part below it. Saving
    // profiles and exit restore the captured baseline. ryzenadj is optional:
    // when it is not on a system path these calls are no-ops.
    static constexpr uint32_t SMU_TCTL_PERF_C = 85;
    static constexpr uint32_t SMU_STAPM_PERF_MW = 25000;
    static constexpr uint32_t SMU_FAST_PERF_MW = 35000;
    static constexpr uint32_t SMU_SLOW_PERF_MW = 30000;
    [[nodiscard]] static bool ryzenadj_available() noexcept;
    static bool apply_smu_performance_limits() noexcept;
    static bool restore_smu_limits() noexcept;

    // REF-REQ-126 (2026-09-23): the EC owns STAPM and takes it back.
    //
    // `apply_smu_performance_limits()` writes the SMU mailbox once, at profile
    // application, and nothing checked afterwards that the value stayed. Measured
    // on the reference host: with Performance in force the EC moved STAPM back to
    // its own table value (6 W) and the CPU settled at the 1400 MHz P-state floor
    // (780-1400 MHz observed) with 25 C of thermal headroom unused, because the
    // package could not draw enough power to clock higher. The write is therefore
    // verified by reading the limit back, and re-asserted when the EC has taken it.
    //
    // The verification is periodic rather than per-cycle: it costs one `ryzenadj
    // -i` exec, so it runs every SMU_VERIFY_INTERVAL_CYCLES observation cycles
    // (~30 s at the 10 s cadence) and only while the machine is actually loaded
    // (an idle box does not need the power budget) and an unrestricted profile is
    // in force (the saving profiles leave the EC's limit in place on purpose).
    static constexpr uint32_t SMU_VERIFY_INTERVAL_CYCLES = 3;
    static constexpr double SMU_VERIFY_MIN_LOAD1 = 1.0;
    // Floor below which the read-back means "the EC has taken it back". The value
    // is not SMU_STAPM_PERF_MW: our own accepted write reads back at 22-25 W
    // (SMU granularity), so a tight comparison would re-write every cycle forever.
    // The EC's own band is 6-10 W, which this separates cleanly from 22 W.
    static constexpr uint32_t SMU_STAPM_CLAWED_BACK_MW = 18000;
    // Pure decision, so the Oracle Gate can prove it without a live SMU.
    [[nodiscard]] static constexpr bool smu_limit_needs_reassert(uint32_t observed_mw) noexcept {
        return observed_mw != 0 && observed_mw < SMU_STAPM_CLAWED_BACK_MW;
    }
    // Pure row reader for `ryzenadj -i` output ("| STAPM LIMIT | 6.000 |"), so the
    // parser is testable without the tool. Returns the raw number printed, which
    // is a watt (STAPM/PPT) or degree (THM) value with three decimals - 6.000 is
    // 6 W, not 6000 mW - and 0.0 when the row is absent. Each caller scales it:
    // ryzenadj's argument space is milliwatts, the table's is watts.
    [[nodiscard]] static double parse_smu_limit_row(const char* text, const char* row_name) noexcept;
    enum class SmuVerifyResult : uint8_t {
        Unavailable,     // no tool, no captured baseline, or sandboxed
        NotUnrestricted, // saving profile - the EC's limit is intentional
        ReadFailed,      // read-back produced no value; nothing is assumed
        Healthy,         // the limit is still ours
        Reasserted,      // the EC had taken it; the raise was re-applied
        Refused,         // re-apply attempted and failed
    };
    // REF-REQ-126: read the enforced STAPM back and re-assert the raise if the EC
    // has reclaimed it. Returns what it did, for the cycle's log decision.
    [[nodiscard]] static SmuVerifyResult verify_and_reassert_smu_limits(PowerProfileMode mode) noexcept;

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

    // REF-REQ-071: shell-safety predicates for the desktop-session tokens that
    // are interpolated into a root-spawned command line. Exposed so the Oracle
    // Gate can prove a hostile Wayland socket name / runtime dir is rejected,
    // independently of whatever /run/user happens to contain.
    [[nodiscard]] static bool is_safe_wayland_component(const char* s) noexcept;
    [[nodiscard]] static bool is_safe_run_user_dir(const char* s) noexcept;

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
    // REF-REQ-110.2: true when this mask is exactly one the engine can apply.
    // Used to tell WattCurb's own damage from a deliberate taskset by the user.
    [[nodiscard]] static bool mask_matches_engine_pattern(const cpu_set_t& mask) noexcept;

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
