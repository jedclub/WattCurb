cmake_minimum_required(VERSION 3.25)
project(WattCurb
    VERSION 0.1.0
    DESCRIPTION "Ultra-low-overhead Linux power profiling and mitigation daemon"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization Options
option(WATTCURB_ENABLE_LTO "Enable Link-Time Optimization" ON)
option(WATTCURB_ENABLE_PGO_GEN "Enable PGO Profile Generation (-fprofile-generate)" OFF)
option(WATTCURB_ENABLE_PGO_USE "Enable PGO Profile Usage (-fprofile-use)" OFF)
option(WATTCURB_ENABLE_DEV_PROFILER "Enable Subsystem Scoped Development Profiler" OFF)

if(WATTCURB_ENABLE_DEV_PROFILER)
    message(STATUS "Development Profiler: Subsystem Scoped Profiler ENABLED (-DWATTCURB_DEV_PROFILE)")
    add_compile_definitions(WATTCURB_DEV_PROFILE=1)
endif()

# Compiler optimization and warning flags
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -O3
    -march=native
    -fno-omit-frame-pointer
)

# REF-REQ-008: Total Elimination of Debug/Trace Residue & Binary Hardening
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(NDEBUG)
    add_compile_options(
        -fvisibility=hidden
        -fvisibility-inlines-hidden
        -ffunction-sections
        -fdata-sections
        -fno-rtti
        -fno-exceptions
        -fomit-frame-pointer
    )
    add_link_options(
        -Wl,--gc-sections
        -Wl,--as-needed
    )
endif()

if(WATTCURB_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
    if(ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endif()

if(WATTCURB_ENABLE_PGO_GEN)
    message(STATUS "PGO: Instrumentation generation enabled (-fprofile-generate)")
    add_compile_definitions(WATTCURB_PGO_INSTRUMENTATION=1)
    add_compile_options(-fprofile-generate)
    add_link_options(-fprofile-generate)
elseif(WATTCURB_ENABLE_PGO_USE)
    message(STATUS "PGO: Profile feedback use enabled (-fprofile-use)")
    add_compile_options(-fprofile-use -fprofile-correction -Wno-error=coverage-mismatch)
    add_link_options(-fprofile-use -fprofile-correction)
endif()


find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD REQUIRED libsystemd)

# Core Library Sources
set(WATTCURB_LIB_SOURCES
    src/core/singleton_lock.cpp
    src/core/daemon_runner.cpp
    src/hw/hardware_probe.cpp
    src/proc/process_analyzer.cpp
    src/policy/attribution_engine.cpp
    src/policy/process_classifier.cpp
    src/policy/mitigation_engine.cpp
    src/policy/window_aware_governor.cpp
    src/policy/unified_rollback_coordinator.cpp
    src/policy/battery_feature.cpp
    src/report/report_generator.cpp
)

add_library(wattcurb_lib STATIC ${WATTCURB_LIB_SOURCES})
target_include_directories(wattcurb_lib PUBLIC src)

# Main Executable (Daemon - zero D-Bus / GUI dependency)
add_executable(wattcurb src/main.cpp)
target_link_libraries(wattcurb PRIVATE wattcurb_lib)

# Desktop SNI Tray Client Executable (REF-REQ-035, REF-ARCH-025)
add_executable(wattcurb-tray
    src/tray/main_tray.cpp
    src/tray/tray_client.cpp
)
target_include_directories(wattcurb-tray PRIVATE src ${SYSTEMD_INCLUDE_DIRS})
target_link_libraries(wattcurb-tray PRIVATE ${SYSTEMD_LIBRARIES})

# Native KDE Plasma 6 Matrix Dashboard (REF-REQ-036, REF-ARCH-026)
find_package(Qt6 COMPONENTS Core Gui Qml Quick QuickControls2 QUIET)
if(Qt6_FOUND)
    message(STATUS "Qt6 found: Enabling wattcurb-dashboard target")
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    add_executable(wattcurb-dashboard
        src/ui/main_dashboard.cpp
        src/ui/dashboard_backend.cpp
        src/ui/dashboard_backend.hpp
        src/core/singleton_lock.cpp
        src/ui/qml.qrc
    )
    target_include_directories(wattcurb-dashboard PRIVATE src)
    target_link_libraries(wattcurb-dashboard PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
    )
    target_compile_options(wattcurb-dashboard PRIVATE -frtti -fexceptions)
else()
    message(WARNING "Qt6 not found: wattcurb-dashboard target will be skipped")
endif()

# Testing & Oracle Gate Suite
enable_testing()
add_executable(wattcurb_tests
    tests/test_units.cpp
    src/tray/tray_client.cpp
)
target_include_directories(wattcurb_tests PRIVATE src ${SYSTEMD_INCLUDE_DIRS})
target_link_libraries(wattcurb_tests PRIVATE wattcurb_lib ${SYSTEMD_LIBRARIES})
add_test(NAME OracleGate COMMAND wattcurb_tests)

# Custom Target: Generate Verbose Assembly for Hot Paths
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/asm)
add_custom_target(wattcurb_asm
    COMMAND ${CMAKE_CXX_COMPILER} -std=c++23 -O3 -march=native -fverbose-asm -masm=intel -S
            -I${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/src/proc/process_analyzer.cpp -o ${CMAKE_BINARY_DIR}/asm/process_analyzer.s
    COMMAND ${CMAKE_CXX_COMPILER} -std=c++23 -O3 -march=native -fverbose-asm -masm=intel -S
            -I${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/src/policy/attribution_engine.cpp -o ${CMAKE_BINARY_DIR}/asm/attribution_engine.s
    COMMENT "Generating verbose Intel-syntax assembly dumps in build/asm/..."
)

