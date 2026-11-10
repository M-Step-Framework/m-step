#!/usr/bin/env bash
ROOT="$(realpath "$(dirname "$0")")"
MEBDTLS="${MEBDTLS:-$(realpath "${ROOT}/../s-world/mbedtls")}"
TFM="${TFM:-$(realpath "${ROOT}/../s-world/trusted-firmware-m")}"
NSPE_MSTP_APP="${NSPE_MSTP_APP:-$(realpath "${ROOT}/../ns-world-bare")}"
MSTP="${MSTP:-$(realpath "${ROOT}/../m-step")}"

# Default values
TARGET="STM32L5"
PROFILE="bare"

#-------------------------------------------------------------------------------
# Parse arguments
#-------------------------------------------------------------------------------
set -e

DEPLOY=false  # default
MONITOR=false  # default
VISUALIZER=false  # default
RAW_TRACE=""  # default

while [[ $# -gt 0 ]]; do
    case "$1" in
        -b|--build)
            BUILD_TYPE="$2"
            if [[   "$BUILD_TYPE" != "s"        && 
                    "$BUILD_TYPE" != "ns"       ]]; then
                echo "Error: Invalid build type '$BUILD_TYPE'. Supported values are 's' and 'ns'"
                exit 1
            fi
            shift 2
            ;;
        -c|--config)
            CONFIG_TYPE="$2"
            if [[   "$CONFIG_TYPE" != "s" && 
                    "$CONFIG_TYPE" != "ns" ]]; then
                echo "Error: Invalid config type '$CONFIG_TYPE'. Supported values are 's' or 'ns'."
                exit 1
            fi
            shift 2
            ;;
        -p|--profile)
            PROFILE="$2"
            if [[   "$PROFILE" != "bare"    && 
                    "$PROFILE" != "crypto"  &&
                    "$PROFILE" != "mstp" 
                ]]; then
                echo "Error: Invalid profile '$PROFILE'. Supported values are 'bare', 'crypto' or mstp."
                exit 1
            fi
            shift 2
            ;;
        -t|--target)
            TARGET="$2"
            shift 2
            ;;
        -d|--deploy)
            DEPLOY=true
            shift
            ;;
        -m|--monitor)
            MONITOR=true
            RAW_TRACE="$2"
            if [[ -z "$RAW_TRACE" ]]; then
                echo "Error: -m|--monitor requires an output file path."
                exit 1
            fi
            shift 2
            ;;
        -vi|--visualizer)
            VISUALIZER=true
            RAW_TRACE="$2"
            ELF="$3"
            VIS_OUT="$4"
            if [[ -z "$RAW_TRACE" || -z "$ELF" || -z "$VIS_OUT" ]]; then
            echo "Error: -vi|--visualizer requires a trace, an .elf file, and a out dir."
            exit 1
            fi
            shift 4
            ;;
        *)
            echo "Unknown argument: $1"
            echo "Usage: $0 [-b|--build <s|ns>] [-c|--config <s|ns>] [-t|--target <BoardName>] [-d|--deploy] [-p|--profile <bare|crypto|mstp>] [-m|--monitor <output_file>] [-vi|--visualizer <trace_file> <elf_file> <output_dir>]"
            exit 1
            ;;
    esac
done


#-------------------------------------------------------------------------------
# Decode target 
#-------------------------------------------------------------------------------
if [[ "${TARGET}" == *"L5"* ]]; then
        PLATFORM="nucleo_l552ze_q"
elif [[ "${TARGET}" == *"U5"* ]]; then
        PLATFORM="b_u585i_iot02a"
else
        echo "Unknown target: ${TARGET}"
        exit 1
fi

CONFIG="${ROOT}/build/${PLATFORM}_${PROFILE}_${CONFIG_TYPE}"

#-------------------------------------------------------------------------------
# Configure SPE build system 
#-------------------------------------------------------------------------------
if [[ "${CONFIG_TYPE}" == "s" ]]; then

        # Out of tree Custom Secure Services Build
        APPROT_TA=${MSTP}/mstp-victims/TAs/appRoT
        PROT_CRYPTO_ATTACK_TA=${MSTP}/mstp-victims/TAs/PRoT_crypto_attack

        PARTITION_PATHS="${APPROT_TA};${PROT_CRYPTO_ATTACK_TA}"
        MANIFEST_LIST_FILE="${APPROT_TA}/appRoT_manifest_list.yaml;${PROT_CRYPTO_ATTACK_TA}/PRoT_crypto_attack_manifest_list.yaml"

        cmake   -S ${TFM}                                               \
                -B ${CONFIG}                                            \
                -DTFM_EXTRA_PARTITION_PATHS=${PARTITION_PATHS}          \
                -DTFM_EXTRA_MANIFEST_LIST_FILES=${MANIFEST_LIST_FILE}   \
                -C ${ROOT}/tfm-configs/${PLATFORM}-${PROFILE}-config.cmake
fi

#-------------------------------------------------------------------------------
# Configure NS build system
#-------------------------------------------------------------------------------
if [[ "${CONFIG_TYPE}" == "ns" ]]; then
    cmake   -G "Eclipse CDT4 - Unix Makefiles" ${NSPE_MSTP_APP}/src \
            -B ${NSPE_MSTP_APP}/build/
fi

BUILD="${ROOT}/build/${PLATFORM}_${PROFILE}_${BUILD_TYPE}"

#-------------------------------------------------------------------------------
# Build SPE build system 
#-------------------------------------------------------------------------------
if [[ "${BUILD_TYPE}" == "s" ]]; then
        cmake --build ${BUILD} -- install

        # This file is missing from the SPE installation, maybe a TFM bug when
        # building Mbed TLS out of tree. 
        if [[ ! -f ${BUILD}/api_ns/interface/include/mbedtls/mbedtls_config.h ]]; then
            cp ${MEBDTLS}/include/mbedtls/mbedtls_config.h \
               ${BUILD}/api_ns/interface/include/mbedtls
        fi
fi

#-------------------------------------------------------------------------------
# Build NS (Baremetal)
#-------------------------------------------------------------------------------
if [[ "${BUILD_TYPE}" == "ns" ]]; then
        BUILD_S="${ROOT}/build/${PLATFORM}_${PROFILE}_s"
        BUILD_NS_BIN="${ROOT}/build/build_ns/bin"
        mkdir -p "${BUILD_NS_BIN}"

        cmake --build ${NSPE_MSTP_APP}/build

        arm-none-eabi-objcopy -O binary \
        ${NSPE_MSTP_APP}/src/NonSecure/build/tfm_ns.elf \
        ${NSPE_MSTP_APP}/src/NonSecure/build/tfm_ns.bin

        imgtool sign \
          -k ${BUILD_S}/api_ns/image_signing/keys/image_ns_signing_private_key.pem \
          --public-key-format full \
          --align 1 \
          -v 0.0.0 \
          -s 1 \
          -H 1024 \
          --pad-header \
          -S 0x10000 \
          --pad \
          --boot-record boot \
          ${NSPE_MSTP_APP}/src/NonSecure/build/tfm_ns.bin \
          ${BUILD_NS_BIN}/tfm_ns_signed.bin
fi

#-------------------------------------------------------------------------------
# Flash TFM
#-------------------------------------------------------------------------------
if $DEPLOY; then        
        BUILD_S="${ROOT}/build/${PLATFORM}_${PROFILE}_s"
        TFM_SPE="$(realpath ${BUILD_S})"

        echo "Deploying for target ${TARGET} with profile ${TFM_SPE}"
        
        ${TFM_SPE}/api_ns/postbuild.sh

        #-----------------------------------------------------------------------
        # ATTENTION!
        #-----------------------------------------------------------------------
        # The script below only needs to run once to set up the option bytes.
        # If this is your first time running it, please uncomment the line below.
        # Otherwise, you likely won't be able to run the code on your board.
        # You can leave it uncommented, but it will add unnecessary time to each 
        # run. Once the board's option bytes are configured, they persist across 
        # power cycles, so you can leave this line commented out after the 
        # initial setup.
        #
        ${TFM_SPE}/api_ns/regression.sh
        #-----------------------------------------------------------------------

        (cd "${TFM_SPE}/api_ns" && ./TFM_UPDATE.sh)
fi

#-------------------------------------------------------------------------------
# Log results
#-------------------------------------------------------------------------------
if $MONITOR; then     
        python3 ${MSTP}/mstp-monitor/monitor.py \
                -o ${RAW_TRACE}
fi

#-------------------------------------------------------------------------------
# Visualize Trace with GTKwave
#-------------------------------------------------------------------------------
if $VISUALIZER; then     
    echo "Launching GTKwave Visualizer..."
    RAW_TRACE="$(realpath "${RAW_TRACE}")"
    ELF="$(realpath "${ELF}")"
    VIS_OUT="$(realpath "${VIS_OUT}")"
    echo "${RAW_TRACE}"
    echo "${ELF}"
    echo "${VIS_OUT}"
    ${MSTP}/mstp-visualizer/1-gen-vcd.sh -c
    ${MSTP}/mstp-visualizer/1-gen-vcd.sh -t ${RAW_TRACE} -elf ${ELF} -o ${VIS_OUT}
    ${MSTP}/mstp-visualizer/2-gtkwave.sh -i ${VIS_OUT}/trace.vcd
fi
