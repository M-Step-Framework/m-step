#!/usr/bin/env bash
ROOT="$(realpath "$(dirname "$0")")"

T1_DIR="$(realpath "${ROOT}/t1-mstp-metrics")"
T2_DIR="$(realpath "${ROOT}/t2-covert-udiv")"
T3_DIR="$(realpath "${ROOT}/t3-covert-inst")"
T4_DIR="$(realpath "${ROOT}/t4-covert-cache")"
T5_DIR="$(realpath "${ROOT}/t5-covert-cont")"
T6_DIR="$(realpath "${ROOT}/t6_pocs")"

# Static Configuration Values
CLEAN=false
TIMING=false
TIMING_LOG="${ROOT}/log_test_timing.txt"

#-------------------------------------------------------------------------------
# Parse arguments
#-------------------------------------------------------------------------------
set -e

while [[ $# -gt 0 ]]; do
    case "$1" in
        -c|--clean)
            CLEAN=true
            shift
            ;;
        -ti|--time)
            TIMING=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [-c|--clean] [-ti|--time]"
            exit 0
            ;;
        *)
            echo "Unknown argument: $1"
            echo "Usage: $0 [-c|--clean] [-ti|--time]"
            exit 1
            ;;
    esac
done

#-------------------------------------------------------------------------------
# Clean up if requested
#-------------------------------------------------------------------------------
if [ "$CLEAN" = true ]; then
    echo "Cleaning logs directory..."
    ${T1_DIR}/1-run-test.sh -c
    ${T2_DIR}/1-run-test.sh -c
    ${T3_DIR}/1-run-test.sh -c
    ${T4_DIR}/1-run-test.sh -c
    ${T5_DIR}/1-run-test.sh -c
    ${T6_DIR}/1-run-pocs.sh -c
    rm -rf "${TIMING_LOG}" || true
    echo "Clean completed."
    exit 0
fi

# Initialize timing log if enabled
if [ "$TIMING" = true ]; then
    rm -rf "${TIMING_LOG}" || true
    echo "Test Execution Timing - $(date)" > "${TIMING_LOG}"
    echo "======================================" >> "${TIMING_LOG}"
fi

# Run tests with optional timing
if [ "$TIMING" = true ]; then start_time=$(date +%s); fi
${T1_DIR}/1-run-test.sh
if [ "$TIMING" = true ]; then
    end_time=$(date +%s)
    elapsed=$((end_time - start_time))
    minutes=$((elapsed / 60))
    echo "T1 (mstp_metrics): ${elapsed} seconds (${minutes} min)" >> "${TIMING_LOG}"
fi

if [ "$TIMING" = true ]; then start_time=$(date +%s); fi
${T2_DIR}/1-run-test.sh
if [ "$TIMING" = true ]; then
    end_time=$(date +%s)
    elapsed=$((end_time - start_time))
    minutes=$((elapsed / 60))
    echo "T2 (covert-udiv): ${elapsed} seconds (${minutes} min)" >> "${TIMING_LOG}"
fi

if [ "$TIMING" = true ]; then start_time=$(date +%s); fi
${T3_DIR}/1-run-test.sh
if [ "$TIMING" = true ]; then
    end_time=$(date +%s)
    elapsed=$((end_time - start_time))
    minutes=$((elapsed / 60))
    echo "T3 (covert-inst): ${elapsed} seconds (${minutes} min)" >> "${TIMING_LOG}"
fi

if [ "$TIMING" = true ]; then start_time=$(date +%s); fi
${T4_DIR}/1-run-test.sh
if [ "$TIMING" = true ]; then
    end_time=$(date +%s)
    elapsed=$((end_time - start_time))
    minutes=$((elapsed / 60))
    echo "T4 (covert-cache): ${elapsed} seconds (${minutes} min)" >> "${TIMING_LOG}"
fi

if [ "$TIMING" = true ]; then start_time=$(date +%s); fi
${T5_DIR}/1-run-test.sh
if [ "$TIMING" = true ]; then
    end_time=$(date +%s)
    elapsed=$((end_time - start_time))
    minutes=$((elapsed / 60))
    echo "T5 (covert-cont): ${elapsed} seconds (${minutes} min)" >> "${TIMING_LOG}"
fi

if [ "$TIMING" = true ]; then start_time=$(date +%s); fi
${T6_DIR}/1-run-pocs.sh
if [ "$TIMING" = true ]; then
    end_time=$(date +%s)
    elapsed=$((end_time - start_time))
    minutes=$((elapsed / 60))
    echo "T6 (pocs): ${elapsed} seconds (${minutes} min)" >> "${TIMING_LOG}"
fi

if [ "$TIMING" = true ]; then
    echo "" >> "${TIMING_LOG}"
    echo "Log saved to: ${TIMING_LOG}"
    cat "${TIMING_LOG}"
fi
