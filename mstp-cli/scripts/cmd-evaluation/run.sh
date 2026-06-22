#!/usr/bin/env bash
set -euo pipefail

echo "Running test: $1"

ROOT="$(realpath "$(dirname "$0")")"
MSTP_ROOT="$(realpath "${ROOT}/../../..")"
EVAL_ROOT="$(realpath "${MSTP_ROOT}/evaluation")"

usage() {
    cat <<'EOF'
Usage:
  run.sh [t1|t2|t3|t4|t5|t6|t7|ALL] [args...]

Examples:
  run.sh t1
  run.sh ALL
  run.sh t2 -c
EOF
}

resolve_test_script() {
    local selector="$1"
    case "${selector}" in
        t1) echo "${EVAL_ROOT}/t1-mstp-metrics/1-run-test.sh" ;;
        t2) echo "${EVAL_ROOT}/t2-covert-udiv/1-run-test.sh" ;;
        t3) echo "${EVAL_ROOT}/t3-covert-inst/1-run-test.sh" ;;
        t4) echo "${EVAL_ROOT}/t4-covert-cache/1-run-test.sh" ;;
        t5) echo "${EVAL_ROOT}/t5-covert-cont/1-run-test.sh" ;;
        t6) echo "${EVAL_ROOT}/t6_pocs/1-run-pocs.sh" ;;
        t7) echo "${EVAL_ROOT}/t7-printf-gtkwave/1-run-test.sh" ;;
        all) echo "${EVAL_ROOT}/1-run-all-tests.sh" ;;
        *) return 1 ;;
    esac
}

run_one() {
    local selector="$1"
    shift

    local test_script
    test_script="$(resolve_test_script "${selector}")"

    if [[ ! -x "${test_script}" ]]; then
        echo "[ERROR] Missing or non-executable test runner: ${test_script}" >&2
        return 1
    fi

    echo "[INFO] Running ${selector}: ${test_script} $*"
    "${test_script}" "$@"
}

main() {
    if [[ $# -eq 0 ]]; then
        selector="ALL"
    else
        selector="$1"
        shift
    fi

    selector_lower="$(echo "${selector}" | tr '[:upper:]' '[:lower:]')"

    case "${selector_lower}" in
        -h|--help)
            usage
            exit 0
            ;;
        t1|t2|t3|t4|t5|t6|t7|all)
            run_one "${selector_lower}" "$@"
            ;;
        *)
            echo "[ERROR] Invalid evaluation target: ${selector}" >&2
            usage >&2
            exit 1
            ;;
    esac
}

main "$@"
