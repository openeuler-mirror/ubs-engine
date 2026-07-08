#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
XML_DIR="${1:-}"
shift 2>/dev/null || true

if [ $# -eq 0 ]; then
    echo "Usage: $0 <xml_dir> <binary1> [binary2 ...]" >&2
    exit 1
fi

RESULT=0
FAILED_TESTS=()

for binary in "$@"; do
    binary_name=$(basename "$binary")
    if [ -n "$XML_DIR" ]; then
        gtest_args="--gtest_output=xml:${XML_DIR}/${binary_name}.xml"
    else
        gtest_args=""
    fi

    if ! "$SCRIPT_DIR/run_it_test.sh" "$binary" $gtest_args; then
        RESULT=1
        FAILED_TESTS+=("$binary_name")
    fi
done

total=$#
passed=$((total - ${#FAILED_TESTS[@]}))
echo ""
echo "=== IT Summary: ${passed}/${total} passed ==="

if [ ${#FAILED_TESTS[@]} -gt 0 ]; then
    echo "Failed:"
    for t in "${FAILED_TESTS[@]}"; do
        echo "  - $t"
    done
fi

exit $RESULT
