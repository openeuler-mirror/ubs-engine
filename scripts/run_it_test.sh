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
# 每个场景二进制运行前后自动清理残留 IT 进程并检查 1901 端口：
# 测试进程异常退出（崩溃/超时 kill/Ctrl-C）时 daemon 不会自清理，孤儿进程占用
# 127.0.0.x:1901 会使后续多节点场景全部 SetUpTestSuite 失败（详见 it_clean_env.sh 头注释）。
trap 'bash "$(dirname "$0")/it_clean_env.sh" -q; rm -f "$TMPFILE"' EXIT
bash "$(dirname "$0")/it_clean_env.sh" -q

# ASAN 模式下关闭地址空间随机化：新内核（6.x，如 WSL2）默认 vm.mmap_rnd_bits=32，
# 高熵 ASLR 与 gcc 12 的 libasan 不兼容，ASAN 插桩二进制（含测试拉起的 daemon、
# ubsectl 等子进程）在 main 之前随机 SEGV（DEADLYSIGNAL）。ADDR_NO_RANDOMIZE 经
# fork/exec 继承，一次包装覆盖整棵进程树。
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

grep -aE '\[==========\]|\[----------\]|\[ RUN \]|\[       OK \]|\[  PASSED  \]|\[  FAILED  \].*cluster|: Failure|: Error' "$TMPFILE" || true
if [ $RESULT -ne 0 ]; then
    echo "--- Full output (test failed) ---"
    cat "$TMPFILE"
fi
exit $RESULT
