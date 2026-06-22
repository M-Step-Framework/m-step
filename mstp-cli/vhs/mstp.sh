#!/usr/bin/env bash
ROOT="$(realpath "$(dirname "$0")")"

BUILD_DIR="$(realpath "${ROOT}/build")"
SOURCE_DIR="$(realpath "${ROOT}/src")"

../mstp-cli-go/1-cli.sh -r

