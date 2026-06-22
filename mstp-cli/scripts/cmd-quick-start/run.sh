#!/usr/bin/env bash
ROOT="$(realpath "$(dirname "$0")")"

MSTP_ROOT="$(realpath "${ROOT}/../../..")"
NSPE_MSTP_APP="$(realpath "${MSTP_ROOT}/ns-world-bare")"

echo "Arguments: $@"

# # Configure the build system
${NSPE_MSTP_APP}/0-config.sh -c
${NSPE_MSTP_APP}/0-config.sh

# # Compile the Secure and Non-Secure images
${NSPE_MSTP_APP}/1-compile.sh

# # Deploy to the target board
${NSPE_MSTP_APP}/2-deploy.sh

# Start the serial monitor
${NSPE_MSTP_APP}/3-monitor.sh -o trace.txt