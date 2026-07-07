#!/bin/bash
set -euo pipefail

BINARY="$1"
if [ -z "$BINARY" ]; then
    echo "Usage: $0 <binary> [args...]" >&2
    exit 1
fi
if [ ! -x "$BINARY" ]; then
    echo "Error: '$BINARY' is not executable" >&2
    exit 1
fi
shift

TMPFILE=$(mktemp)
trap 'rm -f "$TMPFILE"' EXIT

RESULT=0
"$BINARY" "$@" > "$TMPFILE" 2>&1 || RESULT=$?

grep -aE '\[==========\]|\[----------\]|\[ RUN \]|\[       OK \]|\[  PASSED  \]|\[  FAILED  \].*cluster|: Failure|: Error' "$TMPFILE" || true
if [ $RESULT -ne 0 ]; then
    echo "--- Full output (test failed) ---"
    cat "$TMPFILE"
fi
exit $RESULT
