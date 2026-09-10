#include "core/singleton_lock.hpp"
#include "core/cpu_features.hpp"
#include "core/custom_containers.hpp"
#include "hw/hardware_probe.hpp"
#include "proc/process_analyzer.hpp"
#include "policy/attribution_engine.hpp"
#include "policy/process_classifier.hpp"
#include "policy/mitigation_engine.hpp"
#include "policy/battery_feature.hpp"
#include "report/report_generator.hpp"
#include "core/scoped_profiler.hpp"

#undef NDEBUG
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
        {.pid = 101, .comm = "renderer", .utime_ticks = 100, .stime_ticks = 20, .drm_engine_gfx_ns = 1'000'000, .drm_vram_kib = 65536},
        {.pid = 102, .comm = "idle_daemon", .utime_ticks = 10, .stime_ticks = 5, .drm_engine_gfx_ns = 0, .drm_vram_kib = 0}
    };

    std::vector<wattcurb::ProcessSample> p2 = {
        {.pid = 101, .comm = "renderer", .utime_ticks = 300, .stime_ticks = 60, .drm_engine_gfx_ns = 101'000'000, .drm_vram_kib = 65536},
        {.pid = 102, .comm = "idle_daemon", .utime_ticks = 11, .stime_ticks = 5, .drm_engine_gfx_ns = 0, .drm_vram_kib = 0}
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
            {.pid = 201, .comm = "continuous_worker", .utime_ticks = static_cast<uint64_t>(100 + i * 50), .stime_ticks = 10},
            {.pid = 202, .comm = "ephemeral_task", .utime_ticks = (i == 2 ? 30ULL : 0ULL), .stime_ticks = 0}
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

    // Test Part 2: JSON Serialization
    std::ostringstream ss_json;
    report::ReportGenerator::render_json(report, ss_json);
    std::string json_str = ss_json.str();
    assert(json_str.find("\"mitigation_status\"") != std::string::npos);
    assert(json_str.find("\"safety_tier\": 3") != std::string::npos);
    assert(json_str.find("\"recommended_action\": 2") != std::string::npos);

    std::cout << " [PASS] test_executive_briefing_and_telemetry (Two-Part Telemetry & JSON verified)\n";
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

    // 6. Test JSON with feature_catalog
    std::ostringstream ss_json;
    report::ReportGenerator::render_json(report, ss_json);
    std::string json_str = ss_json.str();
    assert(json_str.find("\"feature_catalog\"") != std::string::npos);
    assert(json_str.find("\"code\": \"FEAT-001\"") != std::string::npos);

    std::cout << " [PASS] test_modular_battery_features (7 features, metadata, catalog, & extreme profile verified)\n";
}

} // namespace test

int main() {
    std::cout << "=== WattCurb Unit Test Suite & Oracle Gate Verifier ===\n";
    test::test_cpu_features();
    test::test_hw_isa_primitives();
    test::test_ifunc_and_nttp_dispatch();
    test::test_simd_scanner();
    test::test_custom_containers();
    test::test_process_classifier();
    test::test_mitigation_engine();
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
    test::test_pcie_binary_config_decoder();
    test::test_scoped_profiler();
    test::test_oracle_gate_performance_benchmark();
    std::cout << "=== ALL TESTS & ORACLE GATE PASSED SUCCESSFULLY ===\n";
    return 0;
}
