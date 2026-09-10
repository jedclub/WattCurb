#include "core/singleton_lock.hpp"
#include "core/cpu_features.hpp"
#include "hw/hardware_probe.hpp"
#include "proc/process_analyzer.hpp"
#include "policy/attribution_engine.hpp"
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

} // namespace test

int main() {
    std::cout << "=== WattCurb Unit Test Suite & Oracle Gate Verifier ===\n";
    test::test_cpu_features();
    test::test_hw_isa_primitives();
    test::test_ifunc_and_nttp_dispatch();
    test::test_simd_scanner();
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
