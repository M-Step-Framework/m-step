#!/usr/bin/env bash
ROOT="$(realpath "$(dirname "$0")")"

BUILD_DIR="$(realpath "${ROOT}/build")"
SOURCE_DIR="$(realpath "${ROOT}/src")"

# Static Configuration Values
CLEAN=false
BUILD=false
RUN=false

#-------------------------------------------------------------------------------
# Functions
#-------------------------------------------------------------------------------
show_usage() {
    echo "Usage: $0 [-c|--clean] [-b|--build] [-r|--run] [-h|--help]"
}

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
        -b|--build)
            BUILD=true
            shift
            ;;  
        -r|--run)
            RUN=true
            shift
            ;;  
        -h|--help)
            show_usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1"
            show_usage
            exit 1
            ;;
    esac
done

#-------------------------------------------------------------------------------
# Clean up if requested
#-------------------------------------------------------------------------------
if [ "$CLEAN" = true ]; then
    echo "Cleaning logs directory..."
    rm -rf ${BUILD_DIR} | true
    echo "Clean completed."
    exit 0
fi

#-------------------------------------------------------------------------------
# Build
#-------------------------------------------------------------------------------
if [ "$BUILD" = true ]; then
    echo "Building project..."
    mkdir -p "${BUILD_DIR}"
    go build -o ${BUILD_DIR}/cli-go ${SOURCE_DIR}
    echo "Build completed."
    exit 0
fi

#-------------------------------------------------------------------------------
# Run
#-------------------------------------------------------------------------------
if [ "$RUN" = true ]; then
    ${BUILD_DIR}/cli-go
    exit 0
fi
