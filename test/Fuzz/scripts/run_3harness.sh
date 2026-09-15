#!/bin/bash
# 重跑全部 4 个 harness 各 3000 万次（默认 --runs=30000000 --time=10800 先到为准）
# 结果分别写入 test/Fuzz/results/<harness>_final.log
# 构建目录可用 FT_BUILD_DIR 环境变量覆盖（默认 <脚本所在仓库根>/build-ft）
REPO_ROOT=$(cd "$(dirname "$0")/../../.." && pwd) || exit 1
BUILD_DIR=${FT_BUILD_DIR:-$REPO_ROOT/build-ft}
LOGDIR=$REPO_ROOT/test/Fuzz/results
mkdir -p "$LOGDIR"
cd "$BUILD_DIR" || exit 1
for t in ft_va_msg_deser ft_va_case_conf_json ft_va_sdk_unpack ft_va_ipc_raw; do
    echo "===== $t START $(date '+%F %T') ====="
    rm -rf ft_corpus_$t ft_crash_$t   # 清理旧语料/崩溃目录，从零开始
    ./bin/$t --quiet > "$LOGDIR/${t}_final.log" 2>&1
    rc=$?
    echo "$t EXIT=$rc"
    echo "===== $t END $(date '+%F %T') ====="
done
echo "ALL_DONE $(date '+%F %T')"
