#!/usr/bin/env bash
set -euo pipefail

# WattCurb 30-Minute Continuous Function-Level Scoped Profiling Session Runner
# Implements REF-REQ-142 & REF-ARCH-089

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build_profile"
OUTPUT_LOG="${ROOT_DIR}/tmp/wattcurb_profile_30min.log"
SESSION_DURATION_SEC="${1:-1800}" # Default: 30 minutes (1800s)

mkdir -p "${ROOT_DIR}/tmp"
export TMPDIR="${ROOT_DIR}/tmp"

echo "==================================================================="
echo "  ⚡ WattCurb 30-Minute Scoped Profiler Session (REF-REQ-142)       "
echo "==================================================================="
echo "  • Duration     : ${SESSION_DURATION_SEC} seconds ($(( SESSION_DURATION_SEC / 60 )) minutes)"
echo "  • Profiler     : Ultra-Low-Overhead O(1) TSC Direct Slot Dispatch (< 15 ns/op)"
echo "  • Log Output   : ${OUTPUT_LOG}"
echo "==================================================================="
echo ""

# 1. Build Profiling-Enabled Binary with -O3 -march=native
echo "[1/3] Building profiling-enabled high-performance binary..."
cmake -B "${BUILD_DIR}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DWATTCURB_ENABLE_DEV_PROFILER=ON \
    -DWATTCURB_ENABLE_LTO=ON \
    "${ROOT_DIR}" >/dev/null

ninja -C "${BUILD_DIR}" wattcurb >/dev/null

echo "  ✓ Built binary: ${BUILD_DIR}/wattcurb"
echo ""

# 2. Run Session
echo "[2/3] Launching profiling observation session for ${SESSION_DURATION_SEC}s..."
echo "      (Press Ctrl+C at any time to finish early and dump collected telemetry)"
echo ""

PROFILER_BIN="${BUILD_DIR}/wattcurb"

# Run one-shot observation or background monitoring for the target duration
# Passing --dev-profile --duration ${SESSION_DURATION_SEC}
START_TIME=$(date +%s)
"${PROFILER_BIN}" --dev-profile -w "${SESSION_DURATION_SEC}" -i 1.0 > "${OUTPUT_LOG}" 2>&1 &
PID=$!

trap 'echo ""; echo "[*] Interrupted by user. Finalizing profile dump..."; kill -INT "${PID}" 2>/dev/null || true; wait "${PID}" 2>/dev/null || true; finish_session; exit 0' INT TERM

# Monitoring loop with progress bar
while kill -0 "${PID}" 2>/dev/null; do
    NOW=$(date +%s)
    ELAPSED=$(( NOW - START_TIME ))
    REMAINING=$(( SESSION_DURATION_SEC - ELAPSED ))
    [ "${REMAINING}" -lt 0 ] && REMAINING=0
    PCT=$(( ELAPSED * 100 / SESSION_DURATION_SEC ))
    [ "${PCT}" -gt 100 ] && PCT=100

    # Read live stats if available
    LIVE_STATS=""
    if [ -f "/dev/shm/wattcurb_shared_state.bin" ]; then
        LIVE_STATS=" | Live Daemon Active"
    fi

    printf "\r\033[K[*] Profiling Session Progress: [%3d%%] %4ds / %4ds elapsed (Remaining: %4ds)%s" \
           "${PCT}" "${ELAPSED}" "${SESSION_DURATION_SEC}" "${REMAINING}" "${LIVE_STATS}"
    sleep 5
done

wait "${PID}" 2>/dev/null || true

function finish_session() {
    echo ""
    echo "==================================================================="
    echo "  ✅ 30-Minute Continuous Scoped Profiler Session Completed!       "
    echo "==================================================================="
    echo "  Report File: ${OUTPUT_LOG}"
    echo ""

    if [ -f "${OUTPUT_LOG}" ]; then
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        echo "  Top Hot-Path Functions Breakdown (Ordered by Cumulative Cycles) "
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        # Print the breakdown table from the report
        grep -A 40 "WTCURB PROFILER" "${OUTPUT_LOG}" || cat "${OUTPUT_LOG}" | tail -n 45
    else
        echo "[!] Warning: Profile log not found at ${OUTPUT_LOG}"
    fi
    echo "==================================================================="
}

finish_session
