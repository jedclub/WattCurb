#include "proc/process_analyzer.hpp"
#include "policy/attribution_engine.hpp"

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
        "drm-memory-vram:\t32768 KiB\n";
    wattcurb::ProcessSample sample;
    bool ok = wattcurb::proc::ProcessAnalyzer::parse_drm_fdinfo(mock_fdinfo, sample);
    assert(ok && "parse_drm_fdinfo should succeed");
    assert(sample.drm_engine_gfx_ns == 160000000000ULL);
    assert(sample.drm_engine_compute_ns == 2000000000ULL);
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
        {.pid = 101, .comm = "renderer", .utime_ticks = 100, .stime_ticks = 20, .drm_engine_gfx_ns = 1'000'000},
        {.pid = 102, .comm = "idle_daemon", .utime_ticks = 10, .stime_ticks = 5, .drm_engine_gfx_ns = 0}
    };

    std::vector<wattcurb::ProcessSample> p2 = {
        {.pid = 101, .comm = "renderer", .utime_ticks = 300, .stime_ticks = 60, .drm_engine_gfx_ns = 101'000'000},
        {.pid = 102, .comm = "idle_daemon", .utime_ticks = 11, .stime_ticks = 5, .drm_engine_gfx_ns = 0}
    };

    wattcurb::policy::AttributionEngine engine;
    auto report = engine.compute_attribution(hw1, hw2, p1, p2, 5);

    assert(report.top_processes.size() == 2);
    assert(report.top_processes[0].pid == 101);
    assert(report.top_processes[0].gpu_watts > 0.0);
    assert(report.top_processes[0].wdi_score > report.top_processes[1].wdi_score);
    std::cout << " [PASS] test_attribution_engine\n";
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

} // namespace test

int main() {
    std::cout << "=== WattCurb Unit Test Suite & Oracle Gate Verifier ===\n";
    test::test_proc_stat_parsing();
    test::test_proc_status_parsing();
    test::test_proc_io_parsing();
    test::test_drm_fdinfo_parsing();
    test::test_attribution_engine();
    test::test_oracle_gate_performance_benchmark();
    std::cout << "=== ALL TESTS & ORACLE GATE PASSED SUCCESSFULLY ===\n";
    return 0;
}
