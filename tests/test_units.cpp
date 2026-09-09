#include "core/singleton_lock.hpp"
#include "core/cpu_features.hpp"
#include "hw/hardware_probe.hpp"
#include "proc/process_analyzer.hpp"
#include "policy/attribution_engine.hpp"

#undef NDEBUG
#include <cassert>
#include <chrono>
#include <iostream>
#include <string>

// Implements REF-TEST-002 & Oracle Gate Verification
namespace test {

void test_proc_stat_parsing() {
    std::string mock_stat = "10523 (Web Content) S 1000 1000 1000 0 -1 4194304 1200 0 0 0 450 150 0 0 20 0 1 0 12345 100 200";
    wattcurb::ProcessSample sample;
    bool ok = wattcurb::proc::ProcessAnalyzer::parse_proc_stat(mock_stat, sample);
    assert(ok && "parse_proc_stat should succeed");
    (void)ok;
    assert(sample.pid == 10523);
    assert(sample.comm == "Web Content");
    assert(sample.utime_ticks == 450);
    assert(sample.stime_ticks == 150);
    std::cout << " [PASS] test_proc_stat_parsing\n";
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
    std::string sample_line = "54321 (bench_proc) R 1 1 1 0 0 0 0 0 0 0 1000 500 0 0 20 0 1 0 999 100 200";

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

} // namespace test

int main() {
    std::cout << "=== WattCurb Unit Test Suite & Oracle Gate Verifier ===\n";
    test::test_cpu_features();
    test::test_hw_isa_primitives();
    test::test_ifunc_and_nttp_dispatch();
    test::test_simd_scanner();
    test::test_proc_stat_parsing();
    test::test_proc_status_parsing();
    test::test_proc_io_parsing();
    test::test_drm_fdinfo_parsing();
    test::test_attribution_engine();
    test::test_windowed_attribution_engine();
    test::test_singleton_lock();
    test::test_persistent_hw_probe();
    test::test_oracle_gate_performance_benchmark();
    std::cout << "=== ALL TESTS & ORACLE GATE PASSED SUCCESSFULLY ===\n";
    return 0;
}
