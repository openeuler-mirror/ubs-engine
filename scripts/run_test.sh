#!/bin/bash
set -euo pipefail

# 统一测试运行器：运行测试二进制，捕获并保留输出。
# 说明：
#   - ASAN 报告由运行期 ASAN_OPTIONS=log_path=... 写入 <dir>/asan_log.<pid>，
#     本运行器只负责运行与展示测试结果，不做任何拦截/阻断。
#   - 若设置了 UBSE_TEST_OUTPUT_DIR，则把完整输出另存为 <dir>/<binary>.log 供留存。

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

# ASAN 模式下关闭地址空间随机化：新内核（6.x，如 WSL2）默认 vm.mmap_rnd_bits=32，
# 高熵 ASLR 与 gcc 12 的 libasan 不兼容，ASAN 插桩二进制（含 UT/IT 二进制及其子进程）
# 在 main 之前随机 SEGV（DEADLYSIGNAL）。ADDR_NO_RANDOMIZE 经 fork/exec 继承，
# 一次包装覆盖整棵进程树。与 run_it_test.sh 保持一致的策略。
RUN_CMD=()
if [ -n "${UBSE_ASAN_LIBASAN:-}" ] && command -v setarch >/dev/null 2>&1; then
    RUN_CMD=(setarch "$(uname -m)" -R)
fi

RESULT=0
if [ ${#RUN_CMD[@]} -gt 0 ]; then
    "${RUN_CMD[@]}" "$BINARY" "$@" > "$TMPFILE" 2>&1 || RESULT=$?
else
    "$BINARY" "$@" > "$TMPFILE" 2>&1 || RESULT=$?
fi

if [[ -n "${UBSE_TEST_OUTPUT_DIR:-}" ]]; then
    mkdir -p "$UBSE_TEST_OUTPUT_DIR"
    cp "$TMPFILE" "$UBSE_TEST_OUTPUT_DIR/$(basename "$BINARY").log"
fi

grep -aE '\[==========\]|\[----------\]|\[ RUN \]|\[       OK \]|\[  PASSED  \]|\[  FAILED  \].*cluster|: Failure|: Error' "$TMPFILE" || true

if [ $RESULT -ne 0 ]; then
    echo "--- Full output (test failed) ---"
    cat "$TMPFILE"
fi
exit $RESULT
