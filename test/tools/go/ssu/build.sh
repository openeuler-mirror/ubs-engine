#!/usr/bin/env bash
#
# Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
# ubs-engine is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

set -Eeuo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SDK_MODULE="atomgit.com/openeuler/ubs-engine.git"
TARGET_ARCH="native"
OUTPUT=""

usage()
{
    cat <<'EOF'
Usage:
  ./build.sh [--arch native|x86_64|arm64] [--output FILE]

Options:
  -a, --arch ARCH    Target architecture (default: native).
                     x86, x86_64 and amd64 build linux/amd64.
                     arm, ARM, arm64 and aarch64 build linux/arm64.
  -o, --output FILE  Output file. Relative paths are resolved from this
                     script's directory.
  -h, --help         Show this help.

Examples:
  ./build.sh
  ./build.sh --arch x86_64
  ./build.sh --arch ARM
  ./build.sh -a arm64 -o bin/ubse_ssu_test_go
EOF
}

while (($# > 0)); do
    case "$1" in
        -a | --arch)
            if (($# < 2)); then
                echo "error: $1 requires an architecture" >&2
                exit 2
            fi
            TARGET_ARCH="$2"
            shift 2
            ;;
        -o | --output)
            if (($# < 2)); then
                echo "error: $1 requires a file path" >&2
                exit 2
            fi
            OUTPUT="$2"
            shift 2
            ;;
        -h | --help)
            usage
            exit 0
            ;;
        *)
            echo "error: unknown argument: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

GO_ENV=(env GOWORK=off GOPROXY=off GOSUMDB=off GOFLAGS=-buildvcs=false)
BUILD_ENV=()
TARGET_NAME="native"

case "${TARGET_ARCH,,}" in
    native)
        ;;
    x86 | x86_64 | amd64)
        BUILD_ENV=(GOOS=linux GOARCH=amd64 CGO_ENABLED=0)
        TARGET_NAME="linux-amd64"
        ;;
    arm | arm64 | aarch64)
        BUILD_ENV=(GOOS=linux GOARCH=arm64 CGO_ENABLED=0)
        TARGET_NAME="linux-arm64"
        ;;
    *)
        echo "error: unsupported architecture: ${TARGET_ARCH}" >&2
        echo "supported architectures: native, x86_64, arm64" >&2
        exit 2
        ;;
esac

if [[ -z "${OUTPUT}" ]]; then
    if [[ "${TARGET_NAME}" == "native" ]]; then
        OUTPUT="bin/ubse_ssu_test_go"
    else
        OUTPUT="bin/ubse_ssu_test_go-${TARGET_NAME}"
    fi
fi

cd "${SCRIPT_DIR}"

EXPECTED_SDK_DIR="$(cd ../../../.. && pwd -P)"
ACTUAL_SDK_DIR="$("${GO_ENV[@]}" go list -m -f '{{with .Replace}}{{.Dir}}{{end}}' "${SDK_MODULE}")"
if [[ "${ACTUAL_SDK_DIR}" != "${EXPECTED_SDK_DIR}" ]]; then
    echo "error: SDK module is not mapped to ${EXPECTED_SDK_DIR}" >&2
    exit 1
fi

mkdir -p "$(dirname "${OUTPUT}")"
echo "Building ${TARGET_NAME}: ${OUTPUT}"
"${GO_ENV[@]}" "${BUILD_ENV[@]}" go build -o "${OUTPUT}" .
if [[ "${OUTPUT}" = /* ]]; then
    echo "Build succeeded: ${OUTPUT}"
else
    echo "Build succeeded: ${SCRIPT_DIR}/${OUTPUT}"
fi
