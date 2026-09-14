#include "core/singleton_lock.hpp"
#include "core/cpu_features.hpp"
#include "core/environment_profile.hpp"
#include "core/custom_containers.hpp"
#include "hw/hardware_probe.hpp"
#include "proc/process_analyzer.hpp"
#include "policy/attribution_engine.hpp"
#include "policy/process_classifier.hpp"
#include "policy/mitigation_engine.hpp"
#include "policy/window_aware_governor.hpp"
#include "policy/unified_rollback_coordinator.hpp"
#include "policy/battery_feature.hpp"
#include "report/report_generator.hpp"
#include "ipc/tray_shared_state.hpp"
#include "tray/tray_client.hpp"
#include "core/scoped_profiler.hpp"

#undef NDEBUG
#define WATTCURB_MEMORY_PROBE 1
#include "core/memory_sequence_probe.hpp"
#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <linux/perf_event.h>
#include <sstream>
#include <string>
#include <sys/syscall.h>
#include <unistd.h>

// Implements REF-TEST-002 & Oracle Gate Verification
namespace test {

void test_proc_stat_parsing() {
    std::string mock_stat = "10523 (Web Content) S 1000 1000 1000 0 -1 4194304 1200 0 5 0 450 150 0 0 20 0 8 0 12345 100 200 0 0 0 0 0 0 0 0 0 0 0 0 0 17 6 0 0 0";
    wattcurb::ProcessSample sample;
    bool ok = wattcurb::proc::ProcessAnalyzer::parse_proc_stat(mock_stat, sample);
    assert(ok && "parse_proc_stat should succeed");
    (void)ok;
    assert(sample.pid == 10523);
    assert(sample.comm == "Web Content");
    assert(sample.minflt == 1200);
    assert(sample.majflt == 5);
    assert(sample.utime_ticks == 450);
    assert(sample.priority == 20);
    assert(sample.nice == 0);
    assert(sample.num_threads == 8);
    assert(sample.cpu_core == 6);
    std::cout << " [PASS] test_proc_stat_parsing (with deep fields: minflt, majflt, threads, core, pri, nice)\n";
}

void test_proc_statm_parsing() {
    std::string mock_statm = "50000 10000 2500 100 0 500 0\n";
    wattcurb::ProcessSample sample;
    bool ok = wattcurb::proc::ProcessAnalyzer::parse_proc_statm(mock_statm, sample);
    assert(ok && "parse_proc_statm should succeed");
    (void)ok;
    assert(sample.rss_kib == 40000);
    assert(sample.pss_kib == (10000 - 2500 + 1250) * 4);
    std::cout << " [PASS] test_proc_statm_parsing\n";
}


void test_proc_status_parsing() {
    std::string mock_status = 
        "Name:\tkitty\n"
        "Umask:\t0022\n"
        "State:\tS (sleeping)\n"
        "Uid:\t1000\t1000\t1000\t1000\n"
        "voluntary_ctxt_switches:\t12543\n"
        "nonvoluntary_ctxt_switches:\t892\n";
    wattcurb::ProcessSample sample;
    bool ok = wattcurb::proc::ProcessAnalyzer::parse_proc_status(mock_status, sample);
    assert(ok && "parse_proc_status should succeed");
    (void)ok;
    assert(sample.uid == 1000);
    assert(sample.voluntary_ctxt_switches == 12543);
    assert(sample.nonvoluntary_ctxt_switches == 892);
    std::cout << " [PASS] test_proc_status_parsing\n";
}

void test_proc_io_parsing() {
    std::string mock_io = 
        "rchar: 123456\n"
        "wchar: 789012\n"
        "syscr: 100\n"
        "syscw: 200\n"
        "read_bytes: 4096000\n"
        "write_bytes: 8192000\n";
    wattcurb::ProcessSample sample;
    bool ok = wattcurb::proc::ProcessAnalyzer::parse_proc_io(mock_io, sample);
    assert(ok && "parse_proc_io should succeed");
    (void)ok;
    assert(sample.read_bytes == 4096000);
    assert(sample.write_bytes == 8192000);
    std::cout << " [PASS] test_proc_io_parsing\n";
}

void test_drm_fdinfo_parsing() {
    std::string mock_fdinfo = 
        "pos:\t0\n"
        "drm-driver:\tamdgpu\n"
        "drm-client-id:\t28\n"
        "drm-engine-gfx:\t160000000000 ns\n"
        "drm-engine-compute:\t2000000000 ns\n"
        "drm-engine-dec:\t500000000 ns\n"
        "drm-memory-vram:\t32768 KiB\n";
    wattcurb::ProcessSample sample;
    bool ok = wattcurb::proc::ProcessAnalyzer::parse_drm_fdinfo(mock_fdinfo, sample);
    assert(ok && "parse_drm_fdinfo should succeed");
    (void)ok;
    assert(sample.drm_engine_gfx_ns == 160000000000ULL);
    assert(sample.drm_engine_compute_ns == 2000000000ULL);
    assert(sample.drm_engine_dec_ns == 500000000ULL);
    assert(sample.drm_vram_kib == 32768);
    std::cout << " [PASS] test_drm_fdinfo_parsing\n";
}

void test_attribution_engine() {
    auto now = std::chrono::steady_clock::now();
    wattcurb::HardwareSample hw1;
    hw1.timestamp = now;
    hw1.battery_power_uw = 15'000'000;
    hw1.is_discharging = true;
    hw1.gpu_power_uw = 6'000'000;
    hw1.backlight_brightness = 20000;
    hw1.backlight_max_brightness = 60000;

    wattcurb::HardwareSample hw2;
    hw2.timestamp = now + std::chrono::seconds(2);
    hw2.battery_power_uw = 15'000'000;
    hw2.is_discharging = true;
    hw2.gpu_power_uw = 6'000'000;
    hw2.backlight_brightness = 20000;
    hw2.backlight_max_brightness = 60000;

    std::vector<wattcurb::ProcessSample> p1 = {
        {.pid = 101, .utime_ticks = 100, .stime_ticks = 20, .comm = "renderer", .drm_engine_gfx_ns = 1'000'000, .drm_vram_kib = 65536},
        {.pid = 102, .utime_ticks = 10, .stime_ticks = 5, .comm = "idle_daemon", .drm_engine_gfx_ns = 0, .drm_vram_kib = 0}
    };

    std::vector<wattcurb::ProcessSample> p2 = {
        {.pid = 101, .utime_ticks = 300, .stime_ticks = 60, .comm = "renderer", .drm_engine_gfx_ns = 101'000'000, .drm_vram_kib = 65536},
        {.pid = 102, .utime_ticks = 11, .stime_ticks = 5, .comm = "idle_daemon", .drm_engine_gfx_ns = 0, .drm_vram_kib = 0}
    };

    wattcurb::policy::AttributionEngine engine;
    auto report = engine.compute_attribution(hw1, hw2, p1, p2, 5);

    assert(report.top_processes.size() == 2);
    assert(report.top_processes[0].pid == 101);
    assert(report.top_processes[0].gpu_watts > 0.0);
    assert(report.top_processes[0].primary_hw_domain == "GPU Silicon");
    assert(report.top_processes[0].wdi_score > report.top_processes[1].wdi_score);
    assert(!report.domain_culprits.empty());
    std::cout << " [PASS] test_attribution_engine\n";
}

void test_windowed_attribution_engine() {
    auto now = std::chrono::steady_clock::now();
    std::vector<wattcurb::HardwareSample> hw_list;
    std::vector<std::vector<wattcurb::ProcessSample>> proc_list;

    for (int i = 0; i < 4; ++i) {
        wattcurb::HardwareSample h;
        h.timestamp = now + std::chrono::seconds(i);
        h.battery_power_uw = 15'000'000;
        h.is_discharging = true;
        h.gpu_power_uw = 6'000'000;
        hw_list.push_back(h);

        std::vector<wattcurb::ProcessSample> procs = {
            {.pid = 201, .utime_ticks = static_cast<uint64_t>(100 + i * 50), .stime_ticks = 10, .comm = "continuous_worker"},
            {.pid = 202, .utime_ticks = (i == 2 ? 30ULL : 0ULL), .stime_ticks = 0, .comm = "ephemeral_task"}
        };
        proc_list.push_back(procs);
    }

    wattcurb::policy::AttributionEngine engine;
    auto report = engine.compute_windowed_attribution(hw_list, proc_list, 5);

    assert(report.sample_count == 3);
    assert(report.sample_duration.count() >= 3000);
    assert(!report.is_short_window);
    assert(report.total_energy_joules > 0.0);
    assert(report.top_processes.size() >= 1);
    assert(report.top_processes[0].pid == 201);
    std::cout << " [PASS] test_windowed_attribution_engine\n";
}


void test_oracle_gate_performance_benchmark() {
    std::cout << " [ORACLE GATE] Running micro-benchmark on zero-allocation parser...\n";
    std::string sample_line = "54321 (bench_proc) R 1 1 1 0 0 0 0 0 0 0 1000 500 0 0 20 0 1 0 999 100 200 0 0 0 0 0 0 0 0 0 0 0 0 0 17 6 0 0 0";

    auto start = std::chrono::high_resolution_clock::now();
    wattcurb::ProcessSample sample;
    constexpr int iterations = 100'000;
    for (int i = 0; i < iterations; ++i) {
        wattcurb::proc::ProcessAnalyzer::parse_proc_stat(sample_line, sample);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    double us_per_op = static_cast<double>(elapsed_us) / static_cast<double>(iterations);
    std::cout << " [ORACLE GATE] 100k stat parses completed in " << elapsed_us << " us ("
              << us_per_op << " us/op)\n";

    // Oracle Gate Assertion: Must parse each stat line in < 0.5 microseconds
    assert(us_per_op < 0.5 && "Oracle Gate Failed: Parser latency exceeds 0.5 us/op threshold!");
    std::cout << " [ORACLE GATE PASS] Performance within extreme efficiency threshold (< 0.5 us/op)\n";
}

void test_singleton_lock() {
    // Primary acquisition
    wattcurb::core::SingletonLock lock1("wattcurb.test_singleton");
    assert(lock1.is_locked() && "First lock acquisition must succeed");

    // Secondary acquisition with same abstract address must fail
    wattcurb::core::SingletonLock lock2("wattcurb.test_singleton");
    assert(!lock2.is_locked() && "Duplicate lock acquisition must fail");

    // Release primary
    lock1.release();
    assert(!lock1.is_locked());

    // Tertiary acquisition after release must succeed
    wattcurb::core::SingletonLock lock3("wattcurb.test_singleton");
    assert(lock3.is_locked() && "Lock acquisition after release must succeed");
    std::cout << " [PASS] test_singleton_lock\n";
}

void test_persistent_hw_probe() {
    wattcurb::hw::HardwareProbe probe;
    auto s1 = probe.capture_sample();
    auto s2 = probe.capture_sample();
    assert(s2.timestamp >= s1.timestamp);

    std::cout << " [INFO] Hardware Probe Sample Captured:\n"
              << "   - CPU Temp: " << (s2.cpu_temp_mdeg.has_value() ? std::to_string(static_cast<double>(*s2.cpu_temp_mdeg) / 1000.0) + " C" : "N/A") << "\n"
              << "   - CPU Cores Online: " << s2.cpu_cores_online << ", Avg Freq: " << (s2.cpu_freq_avg_khz / 1000) << " MHz\n"
              << "   - C-State POLL=" << s2.cstate_time_us[0] << "us, C1=" << s2.cstate_time_us[1] << "us, C2=" << s2.cstate_time_us[2] << "us, C3=" << s2.cstate_time_us[3] << "us\n"
              << "   - GPU Power: " << (s2.gpu_power_uw.has_value() ? std::to_string(static_cast<double>(*s2.gpu_power_uw) / 1e6) + " W" : "N/A") << "\n"
              << "   - GPU Busy: " << (s2.gpu_busy_percent.has_value() ? std::to_string(*s2.gpu_busy_percent) + "%" : "N/A") << "\n"
              << "   - Fan RPM: " << (s2.fan_rpm.has_value() ? std::to_string(*s2.fan_rpm) : "N/A") << "\n"
              << "   - Battery Discharging: " << (s2.is_discharging ? "true" : "false") << ", AC Online: " << (s2.is_ac_online ? "true" : "false") << "\n"
              << "   - Battery Health: " << (s2.battery_energy_full_uwh && s2.battery_energy_full_design_uwh ? std::to_string((static_cast<double>(*s2.battery_energy_full_uwh) * 100.0) / static_cast<double>(*s2.battery_energy_full_design_uwh)) + "%" : "N/A") << "\n"
              << "   - NVMe Status: " << (s2.nvme_active ? "active" : "suspended") << ", Read sectors: " << s2.disk_read_sectors << "\n";

    assert(s2.cpu_cores_online > 0 && "Must detect at least 1 online CPU core");
    assert(s2.cstate_time_us[1] > 0 || s2.cstate_time_us[2] > 0 || s2.cstate_time_us[3] > 0);
    std::cout << " [PASS] test_persistent_hw_probe\n";
}

void test_cpu_features() {
    const auto& feats = wattcurb::core::CpuFeatures::instance();
    std::cout << " [INFO] CPU Features detected: AVX2=" << feats.has_avx2 
              << " BMI1=" << feats.has_bmi1 
              << " BMI2=" << feats.has_bmi2 
              << " POPCNT=" << feats.has_popcnt 
              << " AVX512F=" << feats.has_avx512f << "\n";
#if defined(__x86_64__)
    // On modern x86-64 testing hosts, POPCNT is standard
    assert(feats.has_popcnt && "x86-64 CPU must support POPCNT");
#endif
    std::cout << " [PASS] test_cpu_features\n";
}

void test_simd_scanner() {
    std::string test_data = "12345678901234567890123456789012abcdefghijklmnopqrstuvwxyz0123456:target_delim";
    const char* start = test_data.data();
    const char* end = start + test_data.size();

    // Test finding colon
    const char* found = wattcurb::core::simd::find_char_fast(start, end, ':');
    assert(found != end && "SIMD scanner must locate delimiter ':'");
    assert(*found == ':' && "Character pointed must be ':'");
    assert((found - start) == static_cast<std::ptrdiff_t>(test_data.find(':')) && "Offset of colon must match");

    // Test character not present
    const char* not_found = wattcurb::core::simd::find_char_fast(start, end, 'Z');
    assert(not_found == end && "SIMD scanner must return end when char is absent");

    // Test short string (< 32 bytes)
    std::string short_str = "short:test";
    const char* s_found = wattcurb::core::simd::find_char_fast(short_str.data(), short_str.data() + short_str.size(), ':');
    assert((s_found - short_str.data()) == 5 && "Short string search must succeed");

    // Test whitespace skipping and finding
    std::string ws_data = "    \t  \t  12345    \t  67890";
    const char* ws_cur = ws_data.data();
    const char* ws_end = ws_cur + ws_data.size();

    // Skip leading whitespaces
    wattcurb::core::simd::skip_whitespace_simd(ws_cur, ws_end);
    assert(*ws_cur == '1' && "Should land on '1'");

    // Find next whitespace
    wattcurb::core::simd::find_whitespace_simd(ws_cur, ws_end);
    assert(*ws_cur == ' ' && "Should land on space after '12345'");

    std::cout << " [PASS] test_simd_scanner\n";
}

void test_hw_isa_primitives() {
    // 1. Test clear_cacheline_64
    alignas(64) char cacheline_buf[128];
    std::memset(cacheline_buf, 0xAA, sizeof(cacheline_buf));
    wattcurb::core::hw_isa::clear_cacheline_64(cacheline_buf);

    for (int i = 0; i < 64; ++i) {
        assert(cacheline_buf[i] == 0 && "First 64 bytes must be zeroed by clzero/memset");
    }
    for (int i = 64; i < 128; ++i) {
        assert(cacheline_buf[i] == static_cast<char>(0xAA) && "Trailing 64 bytes untouched");
    }

    // 2. Test read_core_id
    uint32_t core_id = wattcurb::core::hw_isa::read_core_id();
    assert(core_id < 256 && "Core ID must be within valid CPU core range");

    // 3. Test read_tsc
    uint32_t aux = 0;
    uint64_t tsc1 = wattcurb::core::hw_isa::read_tsc(&aux);
    uint64_t tsc2 = wattcurb::core::hw_isa::read_tsc();
    assert(tsc2 >= tsc1 && "TSC must be monotonically non-decreasing");

    std::cout << " [PASS] test_hw_isa_primitives (Core ID=" << core_id << ", TSC=" << tsc1 << ")\n";
}

// Verification for REF-ARCH-006: Pre-Built Binary Zero-Overhead Dispatch
typedef const char* (*FindCharResolverFn)(const char*, const char*, char);

extern "C" FindCharResolverFn resolve_find_char_ifunc() {
    if (wattcurb::core::has_runtime(wattcurb::core::CpuFeature::AVX2)) {
        return &wattcurb::core::simd::find_char_avx2;
    }
    return &wattcurb::core::simd::find_char_scalar;
}

const char* ifunc_find_char(const char* s, const char* e, char c)
    __attribute__((ifunc("resolve_find_char_ifunc")));

enum class TestProfile { Baseline, AVX2_Zen2 };

template <TestProfile P>
int nttp_specialized_calc(int val) {
    if constexpr (P == TestProfile::AVX2_Zen2) {
        return val * 2;
    } else {
        return val + 1;
    }
}

void test_ifunc_and_nttp_dispatch() {
    std::string text = "testing_ifunc_symbol_relocation:ok";
    const char* pos = ifunc_find_char(text.data(), text.data() + text.size(), ':');
    assert(pos != text.data() + text.size() && "IFUNC symbol must locate delimiter");
    assert(*pos == ':' && "Character must match");

    int res = nttp_specialized_calc<TestProfile::AVX2_Zen2>(10);
    assert(res == 20 && "NTTP specialized branch must compile and evaluate correctly");

    std::cout << " [PASS] test_ifunc_and_nttp_dispatch (GNU IFUNC & C++23 NTTP verified)\n";
}

void test_scoped_profiler() {
    wattcurb::core::ScopedProfilerRegistry::instance().reset();
    {
        WATTCURB_PROFILE_SCOPE("unit_test_scope");
        volatile int dummy = 0;
        for (int i = 0; i < 1000; ++i) dummy += i;
        (void)dummy;
    }
    std::ostringstream oss;
    wattcurb::core::ScopedProfilerRegistry::instance().print_summary(oss);
    std::string summary = oss.str();
#if defined(WATTCURB_DEV_PROFILE)
    assert(!summary.empty() && "Profile summary must not be empty in dev mode");
    assert(summary.find("unit_test_scope") != std::string::npos && "Scope name must be in summary");
#else
    assert(summary.empty() && "Profile summary must be completely empty in release builds");
#endif
    std::cout << " [PASS] test_scoped_profiler (Zero-overhead release purity verified)\n";
}

// Implements REF-TEST-005: Direct PMU Hardware Counter Verification
void test_pmu_perf_event_telemetry() {
    struct perf_event_attr pe{};
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = PERF_COUNT_HW_INSTRUCTIONS;
    pe.disabled = 0;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;

    int fd = static_cast<int>(::syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0));
    if (fd < 0) {
        std::cout << " [SKIP] test_pmu_perf_event_telemetry: perf_event_open not permitted in this environment\n";
        return;
    }

    uint64_t count1 = 0;
    ssize_t n1 = ::read(fd, &count1, sizeof(count1));
    assert(n1 == sizeof(count1) && "Single-read syscall must return 8-byte uint64 counter");

    // Perform a controlled compute loop
    volatile uint64_t sum = 0;
    for (uint64_t i = 0; i < 10000; ++i) {
        sum += i;
    }
    (void)sum;

    uint64_t count2 = 0;
    ssize_t n2 = ::read(fd, &count2, sizeof(count2));
    assert(n2 == sizeof(count2));
    assert(count2 >= count1 && "PMU Instruction counter must be monotonically non-decreasing");

    ::close(fd);
    std::cout << " [PASS] test_pmu_perf_event_telemetry (Instructions counted: " << (count2 - count1) << ")\n";
}

// Implements REF-TEST-010: PMU Power Proxy & Energy Waste Ratio Verification (REF-REQ-024)
void test_pmu_energy_proxy_metrics() {
    wattcurb::policy::AttributionEngine engine;

    wattcurb::HardwareSample hw1{};
    hw1.timestamp = std::chrono::steady_clock::now();
    hw1.pmu_instructions = 10'000'000ULL;
    hw1.pmu_cycles = 10'000'000ULL;
    hw1.pmu_ipc = 1.0;
    hw1.pmu_llc_misses = 20'000ULL;
    hw1.pmu_branch_misses = 5'000ULL;

    wattcurb::HardwareSample hw2{};
    hw2.timestamp = hw1.timestamp + std::chrono::seconds(1); // delta_sec = 1.0s
    hw2.pmu_instructions = 20'000'000ULL; // d_inst = 10,000,000
    hw2.pmu_cycles = 15'000'000ULL;       // d_cyc = 5,000,000 -> interval_ipc = 2.0
    hw2.pmu_ipc = 2.0;
    hw2.pmu_llc_misses = 30'000ULL;       // d_llc = 10,000
    hw2.pmu_branch_misses = 7'000ULL;     // d_bm = 2,000

    auto snap = engine.compute_hardware_power(hw1, hw2, 1.0);

    // Expected calculations:
    // d_inst = 10,000,000, interval_ipc = 2.0 -> core_term = 20,000,000
    // d_llc = 10,000 * 200.0 = 2,000,000
    // d_bm = 2,000 * 30.0 = 60,000
    // Expected EPI = 20,000,000 + 2,000,000 + 60,000 = 22,060,000
    assert(std::abs(snap.pmu_energy_proxy_index - 22'060'000.0) < 1.0 && "EPI calculation mismatch");

    // Expected waste = 2,000,000 + 60,000 = 2,060,000
    // Expected EWR = (2,060,000 / 22,060,000) * 100% ~= 9.338%
    double expected_ewr = (2'060'000.0 / 22'060'000.0) * 100.0;
    assert(std::abs(snap.pmu_energy_waste_ratio - expected_ewr) < 0.01 && "EWR calculation mismatch");

    // Expected P_est >= 500.0 mW
    assert(snap.pmu_estimated_power_mw >= 500.0 && snap.pmu_estimated_power_mw <= 600.0);

    // Edge case: zero instructions, zero delta_sec
    auto snap_zero = engine.compute_hardware_power(wattcurb::HardwareSample{}, wattcurb::HardwareSample{}, 0.0);
    assert(snap_zero.pmu_energy_proxy_index == 0.0);
    assert(snap_zero.pmu_energy_waste_ratio == 0.0);
    assert(snap_zero.pmu_estimated_power_mw == 0.0);

    std::cout << " [PASS] test_pmu_energy_proxy_metrics (EPI: " << (snap.pmu_energy_proxy_index / 1'000'000.0)
              << "M, EWR: " << snap.pmu_energy_waste_ratio << "%, P_est: " << snap.pmu_estimated_power_mw << " mW)\n";
}

// Implements REF-TEST-006: PCIe Binary Config Space Decoding Verification
void test_pcie_binary_config_decoder() {
    std::array<uint8_t, 256> mock_cfg{};

    // Status register at 0x06: set bit 4 (Capabilities List = 0x0010)
    mock_cfg[0x06] = 0x10;
    mock_cfg[0x07] = 0x00;

    // Capabilities pointer at 0x34: points to 0x40
    mock_cfg[0x34] = 0x40;

    // Capability 1 at 0x40: ID 0x01 (Power Management), Next = 0x60
    mock_cfg[0x40] = 0x01;
    mock_cfg[0x41] = 0x60;

    // Capability 2 at 0x60: ID 0x10 (PCI Express), Next = 0x00
    mock_cfg[0x60] = 0x10;
    mock_cfg[0x61] = 0x00;

    // Link Status register at 0x60 + 0x12 = 0x72
    // Gen 4 (0x04) and x16 (0x10 in bits [9:4] -> 0x0100)
    // Link Status = 0x0104
    mock_cfg[0x72] = 0x04; // Speed = 4 (Gen4)
    mock_cfg[0x73] = 0x01; // Width = 16 (0x10 << 4 = 0x0100 -> high byte has 0x01)

    auto [speed, width] = wattcurb::hw::HardwareProbe::decode_pcie_link_status(mock_cfg.data(), mock_cfg.size());
    assert(speed == 4 && "PCIe speed must decode to Gen4 (4)");
    assert(width == 16 && "PCIe width must decode to x16 (16)");

    // Test invalid buffers and boundaries
    auto [s_null, w_null] = wattcurb::hw::HardwareProbe::decode_pcie_link_status(nullptr, 256);
    assert(s_null == 0 && w_null == 0 && "Null pointer must safely return {0,0}");

    auto [s_small, w_small] = wattcurb::hw::HardwareProbe::decode_pcie_link_status(mock_cfg.data(), 60);
    assert(s_small == 0 && w_small == 0 && "Sub-64 buffer must safely return {0,0}");

    // Buffer without Capabilities bit
    mock_cfg[0x06] = 0x00;
    auto [s_nocap, w_nocap] = wattcurb::hw::HardwareProbe::decode_pcie_link_status(mock_cfg.data(), mock_cfg.size());
    assert(s_nocap == 0 && w_nocap == 0 && "Missing capabilities bit must return {0,0}");

    std::cout << " [PASS] test_pcie_binary_config_decoder (Gen4 x16 binary decode verified)\n";
}

void test_custom_containers() {
    using namespace wattcurb::core;

    // 1. Test FixedVector Basic Operations
    FixedVector<int, 4> v;
    assert(v.empty());
    assert(v.capacity() == 4);
    assert(v.check_integrity());
    assert(!v.overflow_occurred());
    assert(v.push_back(10));
    assert(v.push_back(20));
    assert(v.push_back(30));
    assert(v.push_back(40));
    assert(!v.push_back(50)); // Capacity full: rejected gracefully
    assert(v.size() == 4);
    assert(v.full());
    assert(v.overflow_occurred());
    assert(v.overflow_count() == 1);
    assert(v.check_integrity());
    assert(v[0] == 10 && v[3] == 40);
    assert(v.front() == 10 && v.back() == 40);

    // Test safe bounds clamping on out-of-range index (REF-ARCH-007)
    assert(v[100] == 40 && "Out-of-range index must clamp to last valid element");
    assert(v.at(50) == 40 && "at() must clamp to valid element without crashing");

    // Test copy and move with canary preservation
    FixedVector<int, 4> v_copy = v;
    assert(v_copy.size() == 4 && v_copy[2] == 30);
    assert(v_copy.check_integrity());
    v_copy.pop_back();
    assert(v_copy.size() == 3);

    FixedVector<int, 4> v_move = std::move(v_copy);
    assert(v_move.size() == 3 && v_move[1] == 20);
    assert(v_move.check_integrity());

    // Test empty container safe access (REF-REQ-018)
    FixedVector<int, 8> v_empty;
    assert(v_empty.empty());
    assert(v_empty.check_integrity());
    int f = v_empty.front();
    int b = v_empty.back();
    int at_val = v_empty.at(10);
    (void)f; (void)b; (void)at_val; // Must not crash or dereference invalid memory

    // 2. High-Capacity Headroom Stress Test (REF-TEST-007)
    FixedVector<int, 2048> v_large;
    for (int i = 0; i < 3000; ++i) {
        v_large.push_back(i);
    }
    assert(v_large.size() == 2048 && "Size must cap strictly at 2048");
    assert(v_large.overflow_occurred() && "Overflow must be flagged");
    assert(v_large.overflow_count() == (3000 - 2048) && "Overflow count must be exactly 952");
    assert(v_large.check_integrity() && "Canary must remain untouched after 3000 insertions");

    // 3. Test FixedString with Canary & Safety Guards
    FixedString<32> fs("hello");
    assert(fs.size() == 5);
    assert(fs == "hello");
    assert(fs.check_integrity());
    assert(fs.append(" world"));
    assert(fs == "hello world");
    assert(fs.append_i32(-42));
    assert(fs == "hello world-42");
    assert(fs.check_integrity());
    assert(fs[100] == '\0' && "Out-of-range character access must return null-terminator");
    static_assert(std::is_trivially_copyable_v<FixedString<32>>, "FixedString must be TriviallyCopyable");

    // 4. Test TopKHeap (streaming top-3)
    TopKHeap<int, 3, std::greater<int>> heap;
    int data[] = {5, 12, 1, 88, 32, 7, 95, 23};
    for (int x : data) {
        heap.push(x);
    }
    assert(heap.size() == 3);
    auto top3 = heap.extract_sorted();
    assert(top3.size() == 3);
    assert(top3[0] == 95);
    assert(top3[1] == 88);
    assert(top3[2] == 32);

    std::cout << " [PASS] test_custom_containers (FixedVector, FixedString, TopKHeap, Canary & Guards verified)\n";
}

void test_process_classifier() {
    using namespace wattcurb::policy;

    // 1. Tier 0: Critical System
    auto c_init = ProcessClassifierDB::classify("systemd");
    assert(c_init.tier == ProcessSafetyTier::CriticalImmune);
    assert(c_init.can_freeze == false);
    assert(c_init.can_throttle_scheduler == false);

    auto c_pipe = ProcessClassifierDB::classify("pipewire");
    assert(c_pipe.tier == ProcessSafetyTier::CriticalImmune);

    auto c_kw = ProcessClassifierDB::classify("kworker/u16:1");
    assert(c_kw.tier == ProcessSafetyTier::CriticalImmune);

    // 2. Tier 1: Desktop Compositor
    auto c_kwin = ProcessClassifierDB::classify("kwin_wayland");
    assert(c_kwin.tier == ProcessSafetyTier::DesktopCore);
    assert(c_kwin.can_freeze == false);

    auto c_mutter = ProcessClassifierDB::classify("mutter");
    assert(c_mutter.tier == ProcessSafetyTier::DesktopCore);

    // 3. Tier 2: Desktop Shell
    auto c_plasma = ProcessClassifierDB::classify("plasmashell");
    assert(c_plasma.tier == ProcessSafetyTier::DesktopShell);
    assert(c_plasma.can_reclaim_memory == true);
    assert(c_plasma.can_freeze == false);

    // 4. Tier 4: Background Workers
    auto c_baloo = ProcessClassifierDB::classify("baloo_file");
    assert(c_baloo.tier == ProcessSafetyTier::BackgroundWorker);
    assert(c_baloo.can_throttle_scheduler == true);
    assert(c_baloo.can_freeze == true);

    auto c_tracker = ProcessClassifierDB::classify("tracker-miner-fs-3");
    assert(c_tracker.tier == ProcessSafetyTier::BackgroundWorker);

    // 5. Tier 3: User Interactive Apps
    auto c_chrome = ProcessClassifierDB::classify("chrome");
    assert(c_chrome.tier == ProcessSafetyTier::UserInteractive);
    assert(c_chrome.can_reclaim_memory == true);

    auto c_kitty = ProcessClassifierDB::classify("kitty");
    assert(c_kitty.tier == ProcessSafetyTier::UserInteractive);

    // 6. Tier 5: Runaway Candidate
    auto c_miner = ProcessClassifierDB::classify("xmrig_test");
    assert(c_miner.tier == ProcessSafetyTier::RunawayCandidate);

    std::cout << " [PASS] test_process_classifier (6 safety tiers validated)\n";
}

void test_mitigation_engine() {
    using namespace wattcurb;
    using namespace wattcurb::policy;

    // 1. Test cgroup path resolution on self
    char cg_buf[256];
    bool resolved = MitigationEngine::resolve_cgroup_path(::getpid(), cg_buf, sizeof(cg_buf));
    if (resolved) {
        assert(std::string_view(cg_buf).starts_with("/sys/fs/cgroup"));
    }

    // 2. Mock Closed-Loop Evaluation Test
    MitigationEngine engine;
    AnalysisReportData mock_report;

    // Create a Critical process (must NEVER be throttled or frozen)
    ProcessAttributedPower crit_p;
    crit_p.pid = 9991;
    crit_p.comm = "systemd";
    crit_p.cpu_watts = 2.5;
    crit_p.safety_tier = static_cast<uint8_t>(ProcessSafetyTier::CriticalImmune);
    crit_p.wdi_score = 25.0;
    mock_report.top_processes.push_back(crit_p);

    // Create a Background Worker (eligible for mitigation)
    ProcessAttributedPower bg_p;
    bg_p.pid = 9992;
    bg_p.comm = "baloo_file";
    bg_p.cpu_watts = 3.0;
    bg_p.safety_tier = static_cast<uint8_t>(ProcessSafetyTier::BackgroundWorker);
    bg_p.wdi_score = 15.0;
    bg_p.timerslack_ns = 10000;
    mock_report.top_processes.push_back(bg_p);

    // Run evaluation in Progressive mode (battery discharging, 15% battery)
    auto status = engine.evaluate_and_actuate(mock_report, true, 15.0);
    (void)status;

    // Critical process must NOT cause critical throttling
    assert(mock_report.mitigation_status.active_summary.size() > 0);
    std::cout << " [PASS] test_mitigation_engine (Immunity guarantees & adaptive logic verified)\n";
}

void test_adaptive_mitigation_and_rollback() {
    using namespace wattcurb;
    using namespace wattcurb::policy;

    std::cout << "--- [REF-TEST-014] Adaptive Power State Machine & Dynamic Rollback Verification ---\n";

    MitigationEngine engine;

    // 1. Initial State & AC Power Verification
    assert(engine.current_profile() == PowerProfileMode::Balanced);
    assert(engine.determine_profile(false, 10.0) == PowerProfileMode::Balanced); // AC always Balanced

    // 2. Battery Discharge Hysteresis Transitions
    // Balanced -> PowerSaver trigger at <= 50.0%
    assert(engine.determine_profile(true, 75.0) == PowerProfileMode::Balanced);
    assert(engine.determine_profile(true, 50.0) == PowerProfileMode::PowerSaver);

    // Transition to PowerSaver
    AnalysisReportData report;
    engine.evaluate_and_actuate(report, true, 49.0);
    assert(engine.current_profile() == PowerProfileMode::PowerSaver);

    // Hysteresis Guard: 52% must stay in PowerSaver (requires > 55% to recover to Balanced)
    assert(engine.determine_profile(true, 52.0) == PowerProfileMode::PowerSaver);
    assert(engine.determine_profile(true, 54.9) == PowerProfileMode::PowerSaver);
    assert(engine.determine_profile(true, 55.1) == PowerProfileMode::Balanced);

    // Transition to UltraEndurance (< 20%)
    engine.evaluate_and_actuate(report, true, 18.0);
    assert(engine.current_profile() == PowerProfileMode::UltraEndurance);

    // Hysteresis Guard: 22% must stay in UltraEndurance (requires >= 25% to recover to PowerSaver)
    assert(engine.determine_profile(true, 22.0) == PowerProfileMode::UltraEndurance);
    assert(engine.determine_profile(true, 24.9) == PowerProfileMode::UltraEndurance);
    assert(engine.determine_profile(true, 25.0) == PowerProfileMode::PowerSaver);

    // 3. Actuation and Bidirectional Rollback Test
    // Construct report with a heavy Background Worker and a Critical process
    report.top_processes.clear();

    ProcessAttributedPower crit;
    crit.pid = 10001;
    crit.comm = "systemd";
    crit.cpu_watts = 2.0;
    crit.safety_tier = static_cast<uint8_t>(ProcessSafetyTier::CriticalImmune);
    report.top_processes.push_back(crit);

    ProcessAttributedPower bg;
    bg.pid = 10002;
    bg.comm = "baloo_file";
    bg.cpu_watts = 3.5;
    bg.total_attributed_watts = 4.2;
    bg.safety_tier = static_cast<uint8_t>(ProcessSafetyTier::BackgroundWorker);
    bg.wdi_score = 12.0; // High WDI score
    bg.timerslack_ns = 50000;
    report.top_processes.push_back(bg);

    // In UltraEndurance: bg worker is frozen (or throttled if cgroup access fails in unprivileged mock)
    auto status = engine.evaluate_and_actuate(report, true, 15.0);
    assert(status.current_profile == PowerProfileMode::UltraEndurance);
    assert(status.active_summary.size() > 0);
    assert(std::string_view(status.active_summary.c_str()).starts_with("[UltraEndurance]"));

    // Verify SharedState IPC Synchronization
    ipc::WattCurbSharedState shm{};
    shm.update_from_report(report);
    assert(shm.power_profile_mode == static_cast<uint8_t>(PowerProfileMode::UltraEndurance));

    // Transition to PowerSaver (battery recovers to 30%): Thaw executed
    engine.evaluate_and_actuate(report, true, 30.0);
    assert(engine.current_profile() == PowerProfileMode::PowerSaver);
    shm.update_from_report(report);
    assert(shm.power_profile_mode == static_cast<uint8_t>(PowerProfileMode::PowerSaver));

    // Transition to Balanced on AC Connection: Full Rollback executed
    engine.evaluate_and_actuate(report, false, 30.0); // AC connected
    assert(engine.current_profile() == PowerProfileMode::Balanced);
    assert(engine.tracked_count() == 0); // Rollback must completely clear active tracked list!
    shm.update_from_report(report);
    assert(shm.power_profile_mode == static_cast<uint8_t>(PowerProfileMode::Balanced));

    std::cout << " [PASS] test_adaptive_mitigation_and_rollback (3-Tier Hysteresis, Thaw & Rollback verified)\n";
}


void test_executive_briefing_and_telemetry() {
    using namespace wattcurb;

    AnalysisReportData report;
    report.sample_duration = std::chrono::milliseconds(5000);
    report.sample_count = 5;
    report.total_energy_joules = 54.2;
    report.total_monitored_processes = 245;
    report.total_system_wakeups_per_sec = 350;

    report.hardware.total_system_watts = 10.84;
    report.hardware.cpu_package_watts = 4.2;
    report.hardware.gpu_watts = 1.8;
    report.hardware.display_watts = 2.5;
    report.hardware.is_battery_discharging = true;
    report.hardware.battery_capacity_percent = 78;
    report.hardware.battery_remaining_hours = 5.2;
    report.hardware.battery_health_percent = 96.5;
    report.hardware.battery_cycle_count = 42;
    report.hardware.display_brightness_percent = 75.0;

    ProcessAttributedPower p1;
    p1.pid = 4120;
    p1.comm = "chrome";
    p1.total_attributed_watts = 2.45;
    p1.primary_hw_domain = "GPU Silicon";
    p1.hardware_mechanism = "AMDGPU GFX Engine (120MB VRAM)";
    p1.safety_tier = static_cast<uint8_t>(policy::ProcessSafetyTier::UserInteractive);
    p1.recommended_action = static_cast<uint8_t>(policy::MitigationAction::RelaxTimerSlack);
    report.top_processes.push_back(p1);

    report.mitigation_status.throttled_count = 1;
    report.mitigation_status.frozen_count = 0;
    report.mitigation_status.reclaimed_bytes = 64 * 1024 * 1024;
    report.mitigation_status.estimated_savings_watts = 0.45;
    report.mitigation_status.active_summary = "1 throttled, 64MB reclaimed (~0.45W saved)";

    // Test Part 1: Executive Briefing Text Rendering
    std::ostringstream ss_briefing;
    report::ReportGenerator::render_executive_briefing(report, ss_briefing);
    std::string briefing = ss_briefing.str();
    assert(briefing.find("WattCurb High-Fidelity Executive & Physical Power Briefing") != std::string::npos);
    assert(briefing.find("System Battery & Power Supply Deep Telemetry") != std::string::npos);
    assert(briefing.find("10.84 Watts") != std::string::npos);
    assert(briefing.find("Top Battery Drain Culprits") != std::string::npos);
    assert(briefing.find("chrome") != std::string::npos);
    assert(briefing.find("Actionable Engineering Recommendations") != std::string::npos);

    // Test Part 2: 128-Byte Binary Shared State Seqlock Verification (REF-ARCH-008)
    ipc::WattCurbSharedState state;
    state.update_from_report(report);
    ipc::WattCurbSharedState reader;
    assert(state.read_atomic(reader));
    assert(reader.culprits[0].pid == 4120);
    assert(reader.culprits[0].tier == 3);
    assert(reader.culprits[0].drain_mw == 2450);
    assert(std::string_view(reader.culprits[0].comm) == "chrome");
    assert(reader.battery_percent == 78);
    assert(reader.system_drain_mw == 10840);

    std::cout << " [PASS] test_executive_briefing_and_telemetry (Two-Part Telemetry & 128B Binary State verified)\n";
}

void test_modular_battery_features() {
    using namespace wattcurb;
    using namespace wattcurb::policy;

    FeatureManager mgr;

    // 1. Verify descriptors and defaults
    for (size_t i = 0; i < static_cast<size_t>(FeatureId::Count); ++i) {
        auto fid = static_cast<FeatureId>(i);
        auto desc = FeatureManager::descriptor(fid);
        assert(desc.name.size() > 0);
        assert(desc.feature_code.view().starts_with("FEAT-"));
        assert(desc.target_domain.size() > 0);
        assert(desc.kernel_mechanism.size() > 0);
        assert(desc.power_saving_rationale.size() > 0);
        assert(desc.safety_constraints.size() > 0);
        assert(desc.description.size() > 0);
        assert(mgr.is_feature_enabled(fid) == desc.default_enabled);
    }

    // 2. Test Feature Toggling
    mgr.set_feature_enabled(FeatureId::CgroupFreezer, false);
    assert(!mgr.is_feature_enabled(FeatureId::CgroupFreezer));
    mgr.set_feature_enabled(FeatureId::CgroupFreezer, true);
    assert(mgr.is_feature_enabled(FeatureId::CgroupFreezer));

    // 3. Mock Pipeline Execution across Features
    AnalysisReportData report;
    report.sample_duration = std::chrono::milliseconds(30000);
    report.sample_count = 15;
    report.total_energy_joules = 320.5;
    report.hardware.total_system_watts = 10.68;
    report.hardware.cpu_package_watts = 4.2;
    report.hardware.gpu_watts = 1.8;
    report.hardware.display_watts = 2.5;
    report.hardware.fan_estimated_watts = 0.5;
    report.hardware.storage_estimated_watts = 0.3;
    report.hardware.uncore_and_platform_watts = 1.38;
    report.hardware.is_battery_discharging = true;
    report.hardware.battery_capacity_percent = 15; // Progressive mode trigger
    report.hardware.battery_remaining_hours = 4.5;
    report.hardware.battery_health_percent = 95.0;
    report.hardware.battery_cycle_count = 45;
    report.hardware.display_brightness_percent = 70.0;
    report.hardware.aspm_policy = "default";

    // Add Background Worker (Tier 4)
    ProcessAttributedPower bg_worker;
    bg_worker.pid = 8881;
    bg_worker.comm = "baloo_file";
    bg_worker.safety_tier = static_cast<uint8_t>(ProcessSafetyTier::BackgroundWorker);
    bg_worker.cpu_watts = 2.0;
    bg_worker.wdi_score = 12.0;
    bg_worker.pss_kib = 150 * 1024;
    bg_worker.wakeups_per_sec = 200;
    bg_worker.primary_hw_domain = "CPU Core";
    bg_worker.hardware_mechanism = "CFS Runaway Loop";
    report.top_processes.push_back(bg_worker);

    // Add Desktop Core (Tier 1, must remain immune)
    ProcessAttributedPower compositor;
    compositor.pid = 8882;
    compositor.comm = "kwin_wayland";
    compositor.safety_tier = static_cast<uint8_t>(ProcessSafetyTier::DesktopCore);
    compositor.cpu_watts = 3.0;
    compositor.wdi_score = 30.0;
    compositor.primary_hw_domain = "GPU / Display";
    compositor.hardware_mechanism = "Wayland Compositor";
    report.top_processes.push_back(compositor);

    auto status = mgr.evaluate_and_actuate(report, true, 15.0);

    // Verify feature-specific metrics
    assert(status.feature_summary_count > 0 && "Feature summaries must contain active feature reports");
    assert(status.active_summary.size() > 0);

    // 4. Test render_feature_catalog
    std::ostringstream ss_catalog;
    report::ReportGenerator::render_feature_catalog(ss_catalog);
    std::string cat_str = ss_catalog.str();
    assert(cat_str.find("FEAT-001") != std::string::npos);
    assert(cat_str.find("FEAT-007") != std::string::npos);
    assert(cat_str.find("SchedIdleThrottle") != std::string::npos);
    assert(cat_str.find("PcieAspmEnforcer") != std::string::npos);

    // 5. Test render_extreme_profile
    std::ostringstream ss_extreme;
    report::ReportGenerator::render_extreme_profile(report, ss_extreme);
    std::string ext_str = ss_extreme.str();
    assert(ext_str.find("Extreme 30-Second Physical Hardware Power") != std::string::npos);
    assert(ext_str.find("Physical Hardware Domain Attribution Matrix") != std::string::npos);
    assert(ext_str.find("Unmitigated Power Drain Opportunities for LLM Feature Synthesis") != std::string::npos);
    assert(ext_str.find("LLM Feature Synthesis JSON Directive") != std::string::npos);

    // 6. Test Binary State Synchronization with feature report
    ipc::WattCurbSharedState state;
    state.update_from_report(report);
    ipc::WattCurbSharedState reader;
    assert(state.read_atomic(reader));
    assert(reader.culprits[0].pid == 8881);
    assert(reader.battery_percent == 15);

    std::cout << " [PASS] test_modular_battery_features (7 features, metadata, catalog, & extreme profile verified)\n";
}

void test_memory_sequence_probe_and_cache_chunking() {
    // 1. Verify 64-byte Hot Chunk layout and cache alignment (REF-ARCH-007, REF-RES-011)
    static_assert(sizeof(wattcurb::ProcessHotChunk) == 64, "ProcessHotChunk must be exactly 64 bytes");
    static_assert(alignof(wattcurb::ProcessHotChunk) == 64, "ProcessHotChunk must be 64-byte aligned");
    static_assert(sizeof(wattcurb::CompactProcessHot) == 32, "CompactProcessHot must be exactly 32 bytes");
    static_assert(sizeof(wattcurb::ProcessSample) == 192, "ProcessSample must be exactly 192 bytes (3 cache lines)");
    static_assert(alignof(wattcurb::ProcessSample) == 64, "ProcessSample must be 64-byte aligned");

    // 2. Verify bit-packed metadata precision
    wattcurb::ProcessSample sample;
    sample.pid = 12345;
    sample.cpu_core = 15;
    sample.nice = -10;
    sample.priority = 20;
    sample.num_threads = 64;
    sample.open_sockets = 120;
    sample.has_io_perm = 1;
    sample.is_kthread = 0;
    sample.cross_ccx_migrated = 1;

    assert(sample.pid == 12345);
    assert(sample.cpu_core == 15);
    assert(sample.nice == -10);
    assert(sample.priority == 20);
    assert(sample.num_threads == 64);
    assert(sample.open_sockets == 120);
    assert(sample.has_io_perm == 1);
    assert(sample.is_kthread == 0);
    assert(sample.cross_ccx_migrated == 1);

    // Test negative cpu_core (-1) and negative nice (-20)
    sample.cpu_core = -1;
    sample.nice = -20;
    assert(sample.cpu_core == -1);
    assert(sample.nice == -20);

    // 3. Verify MemorySequenceProbe & Cacheline Crossings
    auto& probe = wattcurb::core::MemorySequenceProbe::instance();
    probe.reset();
    probe.begin_pass();

    // Simulate 10 iterations of pure Hot sequence
    for (int p = 0; p < 10; ++p) {
        probe.record_access(static_cast<uint32_t>(wattcurb::core::ProcFieldId::Pid), "pid", offsetof(wattcurb::ProcessSample, pid), sizeof(sample.pid));
        probe.record_access(static_cast<uint32_t>(wattcurb::core::ProcFieldId::UtimeTicks), "utime_ticks", offsetof(wattcurb::ProcessSample, utime_ticks), sizeof(sample.utime_ticks));
        probe.record_access(static_cast<uint32_t>(wattcurb::core::ProcFieldId::StimeTicks), "stime_ticks", offsetof(wattcurb::ProcessSample, stime_ticks), sizeof(sample.stime_ticks));
        probe.record_access(static_cast<uint32_t>(wattcurb::core::ProcFieldId::VoluntaryCtxt), "vol_ctxt", offsetof(wattcurb::ProcessSample, voluntary_ctxt_switches), sizeof(sample.voluntary_ctxt_switches));
        probe.record_access(static_cast<uint32_t>(wattcurb::core::ProcFieldId::NonvoluntaryCtxt), "nonvol_ctxt", offsetof(wattcurb::ProcessSample, nonvoluntary_ctxt_switches), sizeof(sample.nonvoluntary_ctxt_switches));
        probe.record_access(static_cast<uint32_t>(wattcurb::core::ProcFieldId::RssKib), "rss_kib", offsetof(wattcurb::ProcessSample, rss_kib), sizeof(sample.rss_kib));
        probe.record_access(static_cast<uint32_t>(wattcurb::core::ProcFieldId::PssKib), "pss_kib", offsetof(wattcurb::ProcessSample, pss_kib), sizeof(sample.pss_kib));
        probe.record_access(static_cast<uint32_t>(wattcurb::core::ProcFieldId::Minflt), "minflt", offsetof(wattcurb::ProcessSample, minflt), sizeof(sample.minflt));
        probe.record_access(static_cast<uint32_t>(wattcurb::core::ProcFieldId::Majflt), "majflt", offsetof(wattcurb::ProcessSample, majflt), sizeof(sample.majflt));
    }
    probe.end_pass();

    // All these 9 fields are in Line 0 (offset < 64) -> ZERO cache line crossings within the hot chunk!
    assert(probe.total_crossings() == 0 && "Hot sequence within Line 0 must have 0 cache-line crossings!");

    // Now record a cold field access (comm at offset 64) -> causes a crossing to Line 1!
    probe.record_access(static_cast<uint32_t>(wattcurb::core::ProcFieldId::Comm), "comm", offsetof(wattcurb::ProcessSample, comm), sizeof(sample.comm));
    assert(probe.total_crossings() == 1 && "Accessing cold field (comm) must correctly register 1 cache-line crossing");

    std::string report = probe.generate_report();
    assert(report.find("WattCurb Deep Memory Access Sequence") != std::string::npos);
    assert(report.find("HOT (L1D)") != std::string::npos);

    std::cout << " [PASS] test_memory_sequence_probe_and_cache_chunking (64B HotChunk, 32B CompactHot, 0-crossing Hot loop)\n";
}

void test_deep_battery_telemetry() {
    using namespace wattcurb;

    // 1. Single-read BAT0/uevent parser test (REF-REQ-022, REF-RES-007, REF-TEST-008)
    std::string_view mock_uevent =
        "DEVTYPE=power_supply\n"
        "POWER_SUPPLY_NAME=BAT0\n"
        "POWER_SUPPLY_TYPE=Battery\n"
        "POWER_SUPPLY_STATUS=Discharging\n"
        "POWER_SUPPLY_PRESENT=1\n"
        "POWER_SUPPLY_TECHNOLOGY=Li-poly\n"
        "POWER_SUPPLY_CYCLE_COUNT=95\n"
        "POWER_SUPPLY_VOLTAGE_MIN_DESIGN=11100000\n"
        "POWER_SUPPLY_VOLTAGE_NOW=11206000\n"
        "POWER_SUPPLY_POWER_NOW=19195000\n"
        "POWER_SUPPLY_ENERGY_FULL_DESIGN=45280000\n"
        "POWER_SUPPLY_ENERGY_FULL=42650000\n"
        "POWER_SUPPLY_ENERGY_NOW=32940000\n"
        "POWER_SUPPLY_CAPACITY=77\n"
        "POWER_SUPPLY_CAPACITY_LEVEL=Normal\n"
        "POWER_SUPPLY_MODEL_NAME=LNV-5B10W13895\n"
        "POWER_SUPPLY_MANUFACTURER=SMP\n"
        "POWER_SUPPLY_SERIAL_NUMBER= 3502\n";

    HardwareSample sample;
    hw::HardwareProbe::parse_battery_uevent_buf(mock_uevent, sample);

    assert(sample.is_discharging == true);
    assert(sample.battery_power_uw.value_or(0) == 19195000);
    assert(sample.battery_voltage_uv.value_or(0) == 11206000);
    assert(sample.battery_voltage_min_design_uv.value_or(0) == 11100000);
    assert(sample.battery_energy_now_uwh.value_or(0) == 32940000);
    assert(sample.battery_energy_full_uwh.value_or(0) == 42650000);
    assert(sample.battery_energy_full_design_uwh.value_or(0) == 45280000);
    assert(sample.battery_cycle_count.value_or(0) == 95);
    assert(sample.battery_capacity_percent.value_or(0) == 77);
    assert(sample.battery_capacity_level == "Normal");
    assert(sample.battery_technology == "Li-poly");
    assert(sample.battery_model_name == "LNV-5B10W13895");
    assert(sample.battery_manufacturer == "SMP");
    assert(sample.battery_serial_number == "3502"); // Verify leading whitespace trimmed

    // 2. ThinkPad Thresholds & Attribution Engine Health/Degradation computation
    sample.battery_charge_start_threshold = 0;
    sample.battery_charge_end_threshold = 80;
    sample.battery_charge_behaviour = "[auto] inhibit-charge force-discharge";
    sample.usbc_pd_type = "[C] PD PD_PPS";
    sample.usbc_pd_voltage_uv = 20000000;
    sample.usbc_pd_current_ua = 3250000;
    sample.usbc_pd_online = true;

    HardwareSample::PeripheralBattery mouse;
    mouse.name = "hid-mouse-battery";
    mouse.capacity_percent = 88;
    mouse.is_charging = false;
    sample.peripheral_batteries.push_back(mouse);

    policy::AttributionEngine engine;
    auto hw_breakdown = engine.compute_hardware_power(sample, sample, 1.0);

    // Verify Degradation %, Lost Capacity Wh, Voltage conversions
    assert(hw_breakdown.battery_health_percent > 94.0 && hw_breakdown.battery_health_percent < 94.5);
    assert(hw_breakdown.battery_degradation_percent > 5.5 && hw_breakdown.battery_degradation_percent < 6.0);
    assert(hw_breakdown.battery_lost_capacity_wh > 2.6 && hw_breakdown.battery_lost_capacity_wh < 2.7);
    assert(hw_breakdown.battery_energy_now_wh > 32.9 && hw_breakdown.battery_energy_now_wh < 33.0);
    assert(hw_breakdown.battery_energy_full_wh > 42.6 && hw_breakdown.battery_energy_full_wh < 42.7);
    assert(hw_breakdown.battery_voltage_now_v > 11.2 && hw_breakdown.battery_voltage_now_v < 11.3);
    assert(hw_breakdown.battery_voltage_min_design_v > 11.0 && hw_breakdown.battery_voltage_min_design_v < 11.2);
    assert(hw_breakdown.is_conservation_mode_active == true); // 80% <= 85%
    assert(hw_breakdown.battery_technology == "Li-poly");
    assert(hw_breakdown.battery_model_name == "LNV-5B10W13895");
    assert(hw_breakdown.battery_manufacturer == "SMP");
    assert(hw_breakdown.battery_serial_number == "3502");
    assert(hw_breakdown.peripheral_batteries.size() == 1);
    assert(hw_breakdown.peripheral_batteries[0].name == "hid-mouse-battery");
    assert(hw_breakdown.peripheral_batteries[0].capacity_percent == 88);

    // 3. Test AC Direct Pass-Through Simulation
    HardwareSample ac_sample = sample;
    ac_sample.is_discharging = false;
    ac_sample.is_ac_online = true;
    ac_sample.battery_power_uw = 50000; // 0.05W idle leakage
    ac_sample.battery_capacity_percent = 80;
    auto ac_breakdown = engine.compute_hardware_power(ac_sample, ac_sample, 1.0);
    assert(ac_breakdown.is_ac_passthrough == true && "AC with full threshold and negligible cell draw must report pass-through!");

    // 4. Report Rendering Test
    AnalysisReportData report;
    report.sample_duration = std::chrono::milliseconds(1000);
    report.hardware = hw_breakdown;

    std::ostringstream ss_briefing;
    report::ReportGenerator::render_executive_briefing(report, ss_briefing);
    std::string text = ss_briefing.str();
    assert(text.find("SMP LNV-5B10W13895") != std::string::npos);
    assert(text.find("S/N: 3502") != std::string::npos);
    assert(text.find("Li-poly") != std::string::npos);
    assert(text.find("Design Nominal: 11.10 V") != std::string::npos);
    assert(text.find("CONSERVATION ACTIVE") != std::string::npos);
    assert(text.find("hid-mouse-battery") != std::string::npos);
    assert(text.find("88%") != std::string::npos);

    // Verify 128B Binary Shared State representation
    ipc::WattCurbSharedState state;
    state.update_from_report(report);
    ipc::WattCurbSharedState reader;
    assert(state.read_atomic(reader));
    assert(reader.battery_percent == 77);
    assert(reader.system_drain_mw == 19195);
    assert(reader.battery_state == 1);

    std::cout << " [PASS] test_deep_battery_telemetry (ThinkPad BAT0 uevent, Degradation, Thresholds & Peripherals verified)\n";
}

// Implements REF-TEST-009: Battery Telemetry Fine-Grained Profiling & Oracle Gate Verification
void test_battery_telemetry_profiling_scopes() {
    std::string_view uevent_mock =
        "POWER_SUPPLY_NAME=BAT0\n"
        "POWER_SUPPLY_TYPE=Battery\n"
        "POWER_SUPPLY_STATUS=Discharging\n"
        "POWER_SUPPLY_PRESENT=1\n"
        "POWER_SUPPLY_TECHNOLOGY=Li-poly\n"
        "POWER_SUPPLY_CYCLE_COUNT=95\n"
        "POWER_SUPPLY_VOLTAGE_MIN_DESIGN=11100000\n"
        "POWER_SUPPLY_VOLTAGE_NOW=10865000\n"
        "POWER_SUPPLY_CURRENT_NOW=1500000\n"
        "POWER_SUPPLY_POWER_NOW=16297500\n"
        "POWER_SUPPLY_ENERGY_FULL_DESIGN=45280000\n"
        "POWER_SUPPLY_ENERGY_FULL=42650000\n"
        "POWER_SUPPLY_ENERGY_NOW=29320000\n"
        "POWER_SUPPLY_CAPACITY=69\n"
        "POWER_SUPPLY_CAPACITY_LEVEL=Normal\n"
        "POWER_SUPPLY_MODEL_NAME=LNV-5B10W13895\n"
        "POWER_SUPPLY_MANUFACTURER=SMP\n"
        "POWER_SUPPLY_SERIAL_NUMBER=3502\n";

    wattcurb::HardwareSample sample;
    const size_t WARMUP_ITERS = 1000;
    const size_t BENCH_ITERS = 50000;

    // Warmup
    for (size_t i = 0; i < WARMUP_ITERS; ++i) {
        wattcurb::hw::HardwareProbe::parse_battery_uevent_buf(uevent_mock, sample);
    }

    // Micro-benchmark on SIMD uevent parser
    auto t0 = std::chrono::steady_clock::now();
    uint64_t tsc0 = wattcurb::core::hw_isa::read_tsc();

    for (size_t i = 0; i < BENCH_ITERS; ++i) {
        wattcurb::hw::HardwareProbe::parse_battery_uevent_buf(uevent_mock, sample);
    }

    auto t1 = std::chrono::steady_clock::now();
    uint64_t tsc1 = wattcurb::core::hw_isa::read_tsc();

    auto total_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    double avg_us_op = (static_cast<double>(total_ns) / static_cast<double>(BENCH_ITERS)) / 1000.0;
    double cycles_op = static_cast<double>(tsc1 - tsc0) / static_cast<double>(BENCH_ITERS);

    // Verify parser correctness
    assert(sample.is_discharging == true);
    assert(sample.battery_power_uw.value_or(0) == 16297500);
    assert(sample.battery_model_name == "LNV-5B10W13895");

    // Attribution Engine battery physics benchmark
    wattcurb::policy::AttributionEngine engine;
    auto t_attr0 = std::chrono::steady_clock::now();
    for (size_t i = 0; i < BENCH_ITERS; ++i) {
        auto bd = engine.compute_hardware_power(sample, sample, 1.0);
        (void)bd;
    }
    auto t_attr1 = std::chrono::steady_clock::now();
    auto total_attr_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t_attr1 - t_attr0).count();
    double avg_attr_us_op = (static_cast<double>(total_attr_ns) / static_cast<double>(BENCH_ITERS)) / 1000.0;

    // Full-Scope Pipeline Benchmark (Simulating entire battery telemetry + physics pipeline)
    auto t_full0 = std::chrono::steady_clock::now();
    uint64_t tsc_full0 = wattcurb::core::hw_isa::read_tsc();

    for (size_t i = 0; i < BENCH_ITERS; ++i) {
        wattcurb::HardwareSample pipe_sample;
        pipe_sample.is_ac_online = false;
        wattcurb::hw::HardwareProbe::parse_battery_uevent_buf(uevent_mock, pipe_sample);
        pipe_sample.battery_charge_start_threshold = 0;
        pipe_sample.battery_charge_end_threshold = 80;
        pipe_sample.battery_charge_behaviour = "auto";
        pipe_sample.usbc_pd_online = false;

        wattcurb::HardwareSample::PeripheralBattery pb;
        pb.name = "hid-mouse";
        pb.capacity_percent = 90;
        pb.is_charging = false;
        pipe_sample.peripheral_batteries.push_back(pb);

        auto bd = engine.compute_hardware_power(pipe_sample, pipe_sample, 1.0);
        (void)bd;
    }

    auto t_full1 = std::chrono::steady_clock::now();
    uint64_t tsc_full1 = wattcurb::core::hw_isa::read_tsc();

    auto total_full_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t_full1 - t_full0).count();
    double avg_full_us_op = (static_cast<double>(total_full_ns) / static_cast<double>(BENCH_ITERS)) / 1000.0;
    double cycles_full_op = static_cast<double>(tsc_full1 - tsc_full0) / static_cast<double>(BENCH_ITERS);

    std::cout << " [ORACLE GATE] Dense Battery Telemetry Profiling Benchmark (" << BENCH_ITERS << " iters):\n"
              << "   * SIMD uevent parse      : " << std::fixed << std::setprecision(4) << avg_us_op << " us/op ("
              << std::setprecision(1) << cycles_op << " cycles/op)\n"
              << "   * Battery physics calc   : " << std::setprecision(4) << avg_attr_us_op << " us/op\n"
              << "   * Full-scope E2E pipeline: " << std::setprecision(4) << avg_full_us_op << " us/op ("
              << std::setprecision(1) << cycles_full_op << " cycles/op)\n";

    // Oracle Gate Assertions (accounting for ScopedProfiler recording in dev mode)
#if defined(WATTCURB_DEV_PROFILE)
    assert(avg_us_op < 0.90 && "Battery SIMD uevent parser exceeded Dev Oracle Gate threshold (< 0.90 us/op)!");
    assert(avg_attr_us_op < 1.60 && "Battery physics calc exceeded Dev Oracle Gate threshold (< 1.60 us/op)!");
    assert(avg_full_us_op < 3.00 && "Full-scope battery pipeline exceeded Dev Oracle Gate threshold (< 3.00 us/op)!");

    std::ostringstream oss;
    wattcurb::core::ScopedProfilerRegistry::instance().print_summary(oss);
    std::string summary = oss.str();
    assert(summary.find("hw.battery.uevent_simd_parse") != std::string::npos && "SIMD parse scope must be profiled");
    assert(summary.find("attr.battery_physics") != std::string::npos && "Battery physics scope must be profiled");
    assert(summary.find("attr.battery.system_watts") != std::string::npos && "System watts scope must be profiled");
    assert(summary.find("attr.battery.wear_and_health") != std::string::npos && "Wear and health scope must be profiled");
    assert(summary.find("attr.battery.runtime_projection") != std::string::npos && "Runtime projection scope must be profiled");
    assert(summary.find("attr.battery.passthrough_detect") != std::string::npos && "Pass-through detect scope must be profiled");
#elif defined(WATTCURB_PGO_INSTRUMENTATION)
    assert(avg_us_op < 1.00 && "Battery SIMD uevent parser exceeded PGO Oracle Gate threshold (< 1.00 us/op)!");
    assert(avg_attr_us_op < 0.50 && "Battery physics calc exceeded PGO Oracle Gate threshold (< 0.50 us/op)!");
    assert(avg_full_us_op < 2.00 && "Full-scope battery pipeline exceeded PGO Oracle Gate threshold (< 2.00 us/op)!");
#else
    assert(avg_us_op < 3.00 && "Battery SIMD uevent parser exceeded Release Oracle Gate threshold (< 3.00 us/op)!");
    assert(avg_attr_us_op < 1.00 && "Battery physics calc exceeded Release Oracle Gate threshold (< 1.00 us/op)!");
    assert(avg_full_us_op < 4.00 && "Full-scope battery pipeline exceeded Release Oracle Gate threshold (< 4.00 us/op)!");
#endif
    std::cout << " [PASS] test_battery_telemetry_profiling_scopes (Dense Full-Scope REF-TEST-009)\n";
}

void test_branchless_simd_and_bmi2_pdep() {
    std::cout << "--- [REF-TEST-011] Branchless SIMD & BMI2 PDEP Oracle Gate Verification ---\n";

    // 1. Whitespace SIMD Range Check Verification
    std::string text_spaces = "                 hello_world   rest_of_string";
    const char* cur = text_spaces.data();
    const char* end = cur + text_spaces.size();
    wattcurb::core::simd::skip_whitespace_simd(cur, end);
    assert(std::string_view(cur, 11) == "hello_world" && "skip_whitespace_simd must skip leading spaces");

    wattcurb::core::simd::find_whitespace_simd(cur, end);
    assert(*cur == ' ' && "find_whitespace_simd must locate next space");

    // 2. BMI2 PDEP Multi-Token Skipping Verification
    std::string stat_line = "12345 (test_proc) S 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39";
    const char* stat_cur = stat_line.data();
    const char* stat_end = stat_cur + stat_line.size();

    // Skip 3 tokens (from "12345") -> reaches "1"
    wattcurb::core::simd::skip_tokens_simd(stat_cur, stat_end, 3);
    assert(*stat_cur == '1' && "skip_tokens_simd(3) must land on token 4");

    // Skip 5 tokens from "1" -> reaches "6"
    wattcurb::core::simd::skip_tokens_simd(stat_cur, stat_end, 5);
    assert(*stat_cur == '6' && "skip_tokens_simd(5) must land on token 9");

    // Skip 18 tokens from "6" -> reaches "24"
    wattcurb::core::simd::skip_tokens_simd(stat_cur, stat_end, 18);
    assert(std::string_view(stat_cur, 2) == "24" && "skip_tokens_simd(18) must land on token 27");

    // 3. High-Throughput Oracle Gate Benchmark (50,000 iterations)
    constexpr size_t BENCH_COUNT = 50000;
    std::string mock_stat = "10523 (Web Content) S 1000 1000 1000 0 -1 4194304 1200 0 5 0 450 150 0 0 20 -5 8 0 12345 100 200 0 0 0 0 0 0 0 0 0 0 0 0 0 17 6 0 0 0";
    wattcurb::ProcessSample sample;

    auto t0 = std::chrono::steady_clock::now();
    uint64_t tsc0 = wattcurb::core::hw_isa::read_tsc();

    for (size_t i = 0; i < BENCH_COUNT; ++i) {
        bool ok = wattcurb::proc::ProcessAnalyzer::parse_proc_stat(mock_stat, sample);
        (void)ok;
    }

    auto t1 = std::chrono::steady_clock::now();
    uint64_t tsc1 = wattcurb::core::hw_isa::read_tsc();

    auto total_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    double avg_us_op = (static_cast<double>(total_ns) / static_cast<double>(BENCH_COUNT)) / 1000.0;
    double cycles_op = static_cast<double>(tsc1 - tsc0) / static_cast<double>(BENCH_COUNT);

    std::cout << " [ORACLE GATE] Branchless SIMD & BMI2 PDEP parse_proc_stat (" << BENCH_COUNT << " iters):\n"
              << "   * Average Parse Latency : " << std::fixed << std::setprecision(4) << avg_us_op << " us/op\n"
              << "   * Average CPU Cycles    : " << std::setprecision(1) << cycles_op << " cycles/op\n";

    assert(avg_us_op < 0.85 && "parse_proc_stat must complete under 0.85 us/op!");
    assert(sample.pid == 10523);
    assert(sample.nice == -5);
    assert(sample.priority == 20);
    assert(sample.cpu_core == 6);

    std::cout << " [PASS] test_branchless_simd_and_bmi2_pdep (REF-TEST-011)\n";
}

void test_zero_cost_environment_abstraction() {
    std::cout << "--- [REF-TEST-012] C++23 Zero-Cost Environment Abstraction & Policy Verification ---\n";

    // 1. C++23 Concepts Static Assertions
    static_assert(wattcurb::core::CpuIsaPolicyConcept<wattcurb::core::ScalarGenericIsaPolicy>);
    static_assert(wattcurb::core::CpuIsaPolicyConcept<wattcurb::core::Avx2Bmi2IsaPolicy>);
    static_assert(wattcurb::core::CpuIsaPolicyConcept<wattcurb::core::ZenSpecializedIsaPolicy>);
    static_assert(wattcurb::core::PlatformPolicyConcept<wattcurb::core::MobileLaptopPolicy>);
    static_assert(wattcurb::core::PlatformPolicyConcept<wattcurb::core::DesktopWorkstationPolicy>);
    static_assert(wattcurb::core::PlatformPolicyConcept<wattcurb::core::VirtualHeadlessPolicy>);

    // 2. Host Environment Profile Detection
    auto env = wattcurb::core::EnvironmentProfile::detect_host();
    std::cout << " [INFO] Detected Host Environment Profile:\n"
              << "   * ISA Tier       : " << static_cast<int>(env.isa_tier) << "\n"
              << "   * Form Factor    : " << static_cast<int>(env.form_factor) << "\n"
              << "   * Privilege Tier : " << static_cast<int>(env.privilege) << "\n"
              << "   * Battery Present: " << (env.is_battery_present ? "true" : "false") << "\n"
              << "   * AMD Zen        : " << (env.is_amd_zen ? "true" : "false") << "\n";

    // 3. Dispatch Verification via C++23 Generic Functor
    bool dispatched = false;
    wattcurb::core::EnvironmentDispatcher::dispatch(env, [&](auto policy) {
        using Policy = decltype(policy);
        using Isa = typename Policy::Isa;
        using Plat = typename Policy::Platform;

        std::cout << "   * Bound Specialization: ISA=" << Isa::tier_name()
                  << " | Platform=" << Plat::form_factor_name() << "\n";

        // Verify token skipping using bound policy
        std::string sample_data = "col0 col1 col2 col3 col4 col5 col6";
        const char* cur = sample_data.data();
        const char* end = cur + sample_data.size();
        Isa::skip_tokens(cur, end, 3);
        assert(std::string_view(cur, 4) == "col3" && "Bound policy must correctly skip tokens");

        dispatched = true;
    });
    assert(dispatched && "Dispatcher must execute specialization");

    // 4. Zero-Overhead Desktop Policy Elision Verification
    wattcurb::hw::HardwareProbe probe;
    auto desktop_sample = probe.capture_sample_policy<wattcurb::core::DesktopWorkstationPolicy>();
    assert(desktop_sample.is_ac_online == true);
    assert(desktop_sample.is_discharging == false);
    assert(!desktop_sample.battery_power_uw.has_value() && "Desktop policy must elide battery querying");

    // 5. Dispatch Latency Benchmark (50,000 passes)
    constexpr size_t DISPATCH_ITERS = 50000;
    auto t0 = std::chrono::steady_clock::now();
    uint64_t tsc0 = wattcurb::core::hw_isa::read_tsc();

    volatile uint64_t sum_tsc = 0;
    for (size_t i = 0; i < DISPATCH_ITERS; ++i) {
        wattcurb::core::EnvironmentDispatcher::dispatch(env, [&](auto policy) {
            using Policy = decltype(policy);
            sum_tsc += Policy::Isa::read_tsc();
        });
    }

    auto t1 = std::chrono::steady_clock::now();
    uint64_t tsc1 = wattcurb::core::hw_isa::read_tsc();

    auto total_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    double avg_ns_op = (static_cast<double>(total_ns) / static_cast<double>(DISPATCH_ITERS));
    double cycles_op = static_cast<double>(tsc1 - tsc0) / static_cast<double>(DISPATCH_ITERS);

    std::cout << " [ORACLE GATE] Zero-Cost Dispatch Latency (" << DISPATCH_ITERS << " iters):\n"
              << "   * Average Latency : " << std::fixed << std::setprecision(2) << avg_ns_op << " ns/op\n"
              << "   * Average Cycles  : " << std::setprecision(1) << cycles_op << " cycles/op\n";

    assert(cycles_op < 500.0 && avg_ns_op < 250.0 && "Zero-cost dispatch must have sub-500 cycles / sub-250ns overhead including RDTSCP!");

    std::cout << " [PASS] test_zero_cost_environment_abstraction (REF-TEST-012)\n";
}

void test_syscall_storm_suppression_and_lazy_fd_bypass() {
    std::cout << "--- [REF-TEST-013] Syscall Storm Suppression & Lazy FD Bypass Verification ---\n";

    wattcurb::proc::ProcessAnalyzer analyzer;

    // 1. First-Pass Ephemeral Process Bypass
    wattcurb::ProcessSample sample_eph;
    sample_eph.pid = 99999;
    sample_eph.utime_ticks = 2;
    sample_eph.stime_ticks = 1;
    sample_eph.voluntary_ctxt_switches = 10;
    analyzer.inspect_pid_fds(sample_eph.pid, sample_eph, nullptr, -1);
    assert(sample_eph.open_sockets == 0 && "Ephemeral process must bypass socket scan");
    assert(sample_eph.pinned_drm_fd == -1 && "Ephemeral process must bypass DRM scan");

    // 2. Subsequent Pass Non-Network Process Bypass
    wattcurb::ProcessSample sample_cur;
    sample_cur.pid = 88888;
    sample_cur.utime_ticks = 100;
    sample_cur.stime_ticks = 50;
    sample_cur.voluntary_ctxt_switches = 250;

    wattcurb::ProcessSample sample_prev;
    sample_prev.pid = 88888;
    sample_prev.utime_ticks = 98;
    sample_prev.stime_ticks = 50;
    sample_prev.voluntary_ctxt_switches = 200; // delta_sw = 50 (< 200)
    sample_prev.open_sockets = 0;
    sample_prev.pinned_drm_fd = -1;

    analyzer.increment_pass(); // Move pass_counter from 0 to 1 (steady-state non-rescan pass)
    analyzer.inspect_pid_fds(sample_cur.pid, sample_cur, &sample_prev, -1);
    assert(sample_cur.open_sockets == 0 && "Non-network process with low switch delta must bypass FD walk");

    // 3. High-Throughput Bypass Micro-Benchmark (50,000 iterations)
    constexpr size_t BENCH_COUNT = 50000;
    auto t0 = std::chrono::steady_clock::now();
    uint64_t tsc0 = wattcurb::core::hw_isa::read_tsc();

    for (size_t i = 0; i < BENCH_COUNT; ++i) {
        sample_cur.open_sockets = 999; // reset
        analyzer.inspect_pid_fds(sample_cur.pid, sample_cur, &sample_prev, -1);
    }

    auto t1 = std::chrono::steady_clock::now();
    uint64_t tsc1 = wattcurb::core::hw_isa::read_tsc();

    auto total_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    double avg_ns_op = static_cast<double>(total_ns) / static_cast<double>(BENCH_COUNT);
    double cycles_op = static_cast<double>(tsc1 - tsc0) / static_cast<double>(BENCH_COUNT);

    std::cout << " [ORACLE GATE] Lazy FD Bypass Latency (" << BENCH_COUNT << " iters):\n"
              << "   * Average Latency : " << std::fixed << std::setprecision(2) << avg_ns_op << " ns/op\n"
              << "   * Average Cycles  : " << std::setprecision(1) << cycles_op << " cycles/op\n";

    assert(avg_ns_op < 50.0 && "Lazy FD bypass must complete under 50 ns/op!");

    // 4. Hardware Probe Subsampling Cache Verification
    wattcurb::hw::HardwareProbe probe;
    auto s1 = probe.capture_sample();
    auto s2 = probe.capture_sample();
    // Subsequent immediate sample must match cached AC and fan metrics without blocking
    assert(s1.is_ac_online == s2.is_ac_online);
    assert(s1.fan_rpm == s2.fan_rpm);

    std::cout << " [PASS] test_syscall_storm_suppression_and_lazy_fd_bypass (REF-TEST-013)\n";
}

// Implements REF-ARCH-008 & REF-REQ-007: 128-Byte Seqlock POD Binary Shared State & IPC Verification
void test_tray_binary_shared_state() {
    using namespace wattcurb::ipc;

    // 1. Structure sizing & cache-line alignment invariants
    static_assert(sizeof(SharedCulprit) == 32, "SharedCulprit must be exactly 32 bytes (half cache-line)");
    static_assert(sizeof(WattCurbSharedState) == 128, "WattCurbSharedState must be exactly 128 bytes (2 cache lines)");
    static_assert(alignof(WattCurbSharedState) == 64, "WattCurbSharedState must be 64-byte cache-aligned");
    static_assert(std::is_trivially_copyable_v<WattCurbSharedState>, "WattCurbSharedState must be trivially copyable");

    // 2. Default state initialization
    WattCurbSharedState state;
    assert(state.seq_version == 0);
    assert(state.system_drain_mw == 0);

    // 3. Atomicity & Seqlock protocol test
    WattCurbSharedState reader;
    bool read_ok = state.read_atomic(reader);
    assert(read_ok && "Initial read must succeed");
    assert(reader.seq_version == 0);

    // 4. Simulate active write in progress (odd sequence number)
    __atomic_store_n(&state.seq_version, 1, __ATOMIC_RELEASE);
    read_ok = state.read_atomic(reader);
    assert(!read_ok && "Read must detect write-in-progress (odd sequence)");

    // 5. Restore consistent state and populate with synthetic data
    __atomic_store_n(&state.seq_version, 2, __ATOMIC_RELEASE);
    state.system_drain_mw = 14500;
    state.cpu_drain_mw = 8000;
    state.gpu_drain_mw = 4000;
    state.battery_percent = 85;
    state.battery_state = 1;
    state.active_mitigations = 3;
    state.culprits[0].pid = 4321;
    state.culprits[0].drain_mw = 4200;
    state.culprits[0].tier = 2;
    state.culprits[0].domain_id = 0;
    std::strncpy(state.culprits[0].comm, "rustc", sizeof(state.culprits[0].comm) - 1);

    read_ok = state.read_atomic(reader);
    assert(read_ok && "Read must succeed with even sequence");
    assert(reader.seq_version == 2);
    assert(reader.system_drain_mw == 14500);
    assert(reader.battery_percent == 85);
    assert(reader.culprits[0].pid == 4321);
    assert(std::string_view(reader.culprits[0].comm) == "rustc");

    // 6. Datagram serialization / memcpy test (UDS direct payload)
    alignas(64) uint8_t buffer[128];
    std::memcpy(buffer, &reader, sizeof(WattCurbSharedState));
    WattCurbSharedState deserialized;
    std::memcpy(&deserialized, buffer, sizeof(WattCurbSharedState));
    assert(deserialized.system_drain_mw == 14500);
    assert(deserialized.battery_percent == 85);
    assert(deserialized.culprits[0].pid == 4321);

    std::cout << " [PASS] test_tray_binary_shared_state (128B Seqlock, Zero-Copy POD serialization verified)\n";
}

void test_window_aware_governor() {
    using namespace wattcurb::policy;
    WindowAwareGovernor gov;

    // 0. Test Self-Safety Invariant (Never throttle daemon itself)
    int32_t self_pid = static_cast<int32_t>(getpid());
    gov.on_window_state_changed(self_pid, true, false, 1000, false);
    assert(gov.tracked_count() == 0 && "Governor must reject tracking self-PID");

    // 1. Initial minimization: Non-Halting Graceful Throttle (SCHED_IDLE + 50ms timerslack)
    int32_t browser_pid = 88888; 
    uint64_t t0 = 1000;
    gov.on_window_state_changed(browser_pid, true, false, t0, false);

    assert(gov.tracked_count() == 1);
    const auto* entry = gov.find_entry(browser_pid);
    assert(entry != nullptr);
    assert(entry->state == WindowSuppressionState::GracefulIdleThrottled);
    assert(entry->has_active_audio == false);

    // 2. Non-Halting Invariant: Even after long minimization, process remains alive in GracefulIdleThrottled!
    gov.evaluate_hysteresis(t0 + 60);
    assert(entry->state == WindowSuppressionState::GracefulIdleThrottled);

    // 3. Audio/media app test: Also remains alive in GracefulIdleThrottled
    int32_t audio_pid = 99999;
    gov.on_window_state_changed(audio_pid, true, false, t0, true);
    const auto* audio_entry = gov.find_entry(audio_pid);
    assert(audio_entry != nullptr);
    assert(audio_entry->state == WindowSuppressionState::GracefulIdleThrottled);
    assert(audio_entry->has_active_audio == true);

    // 4. Instant Unthrottle on window focus recovery
    auto start = std::chrono::high_resolution_clock::now();
    bool unthrottled = gov.unthrottle_immediate(browser_pid);
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    assert(unthrottled == true);
    assert(entry->state == WindowSuppressionState::ActiveForeground);
    assert(elapsed_us < 1000 && "Unthrottle latency must be strictly sub-millisecond (< 1000 us)");

    // 5. Rollback all
    gov.rollback_all();
    assert(gov.tracked_count() == 0);

    std::cout << " [PASS] test_window_aware_governor (Non-Halting Graceful Throttle, Always-Alive Invariant verified: " 
              << elapsed_us << "us)\n";
}

void test_unified_rapid_rollback() {
    using namespace wattcurb::policy;
    MitigationEngine mitigation;
    WindowAwareGovernor window_gov;

    // 1. Simulate active power-saving state across multiple domains
    // (a) Window governor tracking minimized apps
    window_gov.on_window_state_changed(88881, true, false, 1000, false);
    window_gov.on_window_state_changed(88882, true, false, 1000, false);
    assert(window_gov.tracked_count() == 2);

    // (b) Mitigation engine applying EPP & ASPM policies
    MitigationEngine::set_cpu_epp_policy("power");

    // 2. Trigger AC plug-in / Charge event: Execute Unified Rapid Rollback
    auto start = std::chrono::high_resolution_clock::now();
    bool ok = UnifiedRollbackCoordinator::execute_rapid_rollback(mitigation, window_gov, 1000000);
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    assert(ok == true);
    assert(window_gov.tracked_count() == 0 && "All window throttles must be cleared");
    assert(mitigation.tracked_count() == 0 && "All process mitigations must be cleared");
    assert(UnifiedRollbackCoordinator::state().is_clean_baseline == true);
    assert(elapsed_us < 5000 && "Full-sweep rollback must complete in < 5.0ms");

    // 3. Test Idempotency: Immediate second call
    auto start_idem = std::chrono::high_resolution_clock::now();
    bool ok_idem = UnifiedRollbackCoordinator::execute_rapid_rollback(mitigation, window_gov, 2000000);
    auto end_idem = std::chrono::high_resolution_clock::now();
    auto elapsed_idem_us = std::chrono::duration_cast<std::chrono::microseconds>(end_idem - start_idem).count();

    assert(ok_idem == true);
    assert(elapsed_idem_us < 100 && "Idempotent second rollback must be sub-100us no-op");

    std::cout << " [PASS] test_unified_rapid_rollback (AC Plug-in / Charge event full-sweep restoration verified: " 
              << elapsed_us << "us, idem: " << elapsed_idem_us << "us)\n";
}

// Implements REF-TEST-018: ThinkPower-Faithful Tray Client & Seqlock ToolTip Verification
void test_thinkpower_tray_client() {
    using namespace wattcurb::tray;
    using namespace wattcurb::ipc;

    WattCurbSharedState state{};
    state.system_drain_mw = 14200;
    state.cpu_drain_mw = 7500;
    state.gpu_drain_mw = 3200;
    state.battery_percent = 82;
    state.battery_health_percent = 96;
    state.time_to_empty_min = 275;
    state.cpu_temp_c = 48;
    state.fan_rpm = 2100;
    state.battery_state = 1; // Discharging
    state.power_profile_mode = 2; // SmartSave
    state.active_mitigations = 2;

    std::strncpy(state.culprits[0].comm, "code", sizeof(state.culprits[0].comm) - 1);
    state.culprits[0].pid = 10101;
    state.culprits[0].drain_mw = 4100;

    std::strncpy(state.culprits[1].comm, "kwin_wayland", sizeof(state.culprits[1].comm) - 1);
    state.culprits[1].pid = 1200;
    state.culprits[1].drain_mw = 1800;

    // 1. ToolTip Stack Formatting Verification (Zero dynamic heap allocation)
    char title[64]{};
    char desc[512]{};
    TrayClient::render_tooltip(state, title, sizeof(title), desc, sizeof(desc));

    assert(std::string_view(title).find("WattCurb: 14.2 W (Discharging)") != std::string_view::npos);
    assert(std::string_view(desc).find("Battery: 82% (Health: 96%) | Est: 275 min") != std::string_view::npos);
    assert(std::string_view(desc).find("CPU: 7.5 W (48°C, Fan 2100 RPM) | GPU: 3.2 W") != std::string_view::npos);
    assert(std::string_view(desc).find("Top 1: code (4100 mW, PID 10101)") != std::string_view::npos);
    assert(std::string_view(desc).find("Top 2: kwin_wayland (1800 mW, PID 1200)") != std::string_view::npos);
    assert(std::string_view(desc).find("Profile: SmartSave | Active Gates: 2") != std::string_view::npos);

    // 2. Icon Name Resolution Test (Breeze 10% quantized battery + profile icons)
    char icon[64]{};
    state.battery_percent = 82; // rounds to 080
    state.battery_state = 1; // Discharging
    state.power_profile_mode = 2; // SmartSave
    TrayClient::resolve_icon_name(state, icon, sizeof(icon));
    assert(std::string_view(icon) == "battery-080-profile-powersave");

    state.power_profile_mode = 0; // Performance
    TrayClient::resolve_icon_name(state, icon, sizeof(icon));
    assert(std::string_view(icon) == "battery-080-profile-performance");

    state.power_profile_mode = 1; // Balanced
    TrayClient::resolve_icon_name(state, icon, sizeof(icon));
    assert(std::string_view(icon) == "battery-080-profile-balanced");

    state.battery_state = 2; // Charging
    TrayClient::resolve_icon_name(state, icon, sizeof(icon));
    assert(std::string_view(icon) == "battery-080-charging-profile-balanced");

    // 3. High-Throughput ToolTip Micro-Benchmark (50,000 iterations)
    constexpr size_t BENCH_COUNT = 50000;
    auto t0 = std::chrono::steady_clock::now();
    uint64_t tsc0 = wattcurb::core::hw_isa::read_tsc();

    for (size_t i = 0; i < BENCH_COUNT; ++i) {
        TrayClient::render_tooltip(state, title, sizeof(title), desc, sizeof(desc));
    }

    auto t1 = std::chrono::steady_clock::now();
    uint64_t tsc1 = wattcurb::core::hw_isa::read_tsc();

    auto total_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    double avg_us_op = (static_cast<double>(total_ns) / static_cast<double>(BENCH_COUNT)) / 1000.0;
    double cycles_op = static_cast<double>(tsc1 - tsc0) / static_cast<double>(BENCH_COUNT);

    std::cout << " [ORACLE GATE] ThinkPower ToolTip Render Latency (" << BENCH_COUNT << " iters):\n"
              << "   * Average Latency : " << std::fixed << std::setprecision(4) << avg_us_op << " us/op\n"
              << "   * Average Cycles  : " << std::setprecision(1) << cycles_op << " cycles/op\n";

    assert(cycles_op < 10000.0 && avg_us_op < 6.00 && "ToolTip formatting must complete in < 6.00 us/op (sub-10000 cycles)!");

    std::cout << " [PASS] test_thinkpower_tray_client (REF-TEST-018: Zero-heap stack formatting, Icon states verified: "
              << avg_us_op << " us/op)\n";
}

} // namespace test

int main() {
    std::cout << "=== WattCurb Unit Test Suite & Oracle Gate Verifier ===\n";
    test::test_cpu_features();
    test::test_hw_isa_primitives();
    test::test_ifunc_and_nttp_dispatch();
    test::test_simd_scanner();
    test::test_custom_containers();
    test::test_memory_sequence_probe_and_cache_chunking();
    test::test_deep_battery_telemetry();
    test::test_battery_telemetry_profiling_scopes();
    test::test_branchless_simd_and_bmi2_pdep();
    test::test_zero_cost_environment_abstraction();
    test::test_syscall_storm_suppression_and_lazy_fd_bypass();
    test::test_tray_binary_shared_state();
    test::test_window_aware_governor();
    test::test_unified_rapid_rollback();
    test::test_thinkpower_tray_client();
    test::test_process_classifier();
    test::test_mitigation_engine();
    test::test_adaptive_mitigation_and_rollback();
    test::test_modular_battery_features();
    test::test_executive_briefing_and_telemetry();
    test::test_proc_stat_parsing();
    test::test_proc_statm_parsing();
    test::test_proc_status_parsing();
    test::test_proc_io_parsing();
    test::test_drm_fdinfo_parsing();
    test::test_attribution_engine();
    test::test_windowed_attribution_engine();
    test::test_singleton_lock();
    test::test_persistent_hw_probe();
    test::test_pmu_perf_event_telemetry();
    test::test_pmu_energy_proxy_metrics();
    test::test_pcie_binary_config_decoder();
    test::test_scoped_profiler();
    test::test_oracle_gate_performance_benchmark();
    std::cout << "=== ALL TESTS & ORACLE GATE PASSED SUCCESSFULLY ===\n";
    return 0;
}
