cmake_minimum_required(VERSION 3.25)
project(WattCurb
    VERSION 0.1.0
    DESCRIPTION "Ultra-low-overhead Linux power profiling and mitigation daemon"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler optimization and warning flags
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -O3
    -fno-omit-frame-pointer
)

# Core Library Sources
set(WATTCURB_LIB_SOURCES
    src/hw/hardware_probe.cpp
    src/proc/process_analyzer.cpp
    src/policy/attribution_engine.cpp
    src/report/report_generator.cpp
)

add_library(wattcurb_lib STATIC ${WATTCURB_LIB_SOURCES})
target_include_directories(wattcurb_lib PUBLIC src)

# Main Executable
add_executable(wattcurb src/main.cpp)
target_link_libraries(wattcurb PRIVATE wattcurb_lib)

# Testing & Oracle Gate Suite
enable_testing()
add_executable(wattcurb_tests tests/test_units.cpp)
target_link_libraries(wattcurb_tests PRIVATE wattcurb_lib)
add_test(NAME OracleGate COMMAND wattcurb_tests)
