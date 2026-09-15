#!/bin/bash
# 按 harness 采集覆盖率并追加到对应的 30M 全量日志（*_final.log）
# 流程（每个 harness）：
#   1. 清空 build-ft-cov 中所有 gcda（避免跨 harness 累计）
#   2. 在 build-ft-cov 中短跑 100 万次（覆盖率早已饱和，无需 3000 万）
#   3. gcov -t -f 解析（stdout 模式，不落 .gcov 文件，避免污染仓库）
#      - 行覆盖：按 (源文件, 行号) 跨 TU 去重（与 lcov 口径一致）
#      - 函数覆盖：每个 gcda 先输出全部 Function 块、再输出各 Source 段；
#        函数归属到其后首个 Source:（即该 gcda 的主编译单元），
#        按符号名跨 TU 全局去重
#   4. 追加到 test/Fuzz/results/<harness>_final.log 末尾
set -e
# 构建目录可用 FT_COV_BUILD_DIR 环境变量覆盖（默认 <脚本所在仓库根>/build-ft-cov）
REPO_ROOT=$(cd "$(dirname "$0")/../../.." && pwd) || exit 1
COV=${FT_COV_BUILD_DIR:-$REPO_ROOT/build-ft-cov}
LOGDIR=$REPO_ROOT/test/Fuzz/results
mkdir -p "$LOGDIR"
RUNS=1000000

CollectOne() { # $1=harness $2=源码路径过滤
    local t=$1 filter=$2
    find "$COV" -name '*.gcda' -delete
    (cd "$COV" && rm -rf "ft_corpus_$t" "ft_crash_$t" && ./bin/$t --quiet --runs=$RUNS >/dev/null 2>&1) || true
    local tmpd
    tmpd=$(mktemp -d)
    # -n1 逐 gcda 调用：gcov 多 gcda 合并输出时所有 Function 块会集中在最前，
    # 导致无法归属到各自的主编译单元；单 gcda 输出为 [该 gcda 函数][该 gcda 数据段]
    (cd "$tmpd" && find "$COV" -name '*.gcda' -print0 | xargs -0 -r -n1 gcov -t -f > gcov_raw.txt 2>/dev/null) || true
    python3 - "$tmpd/gcov_raw.txt" "$filter" >> "$LOGDIR/${t}_final.log" <<'EOF'
import re, sys
raw, filt = sys.argv[1], sys.argv[2]
line_hit = {}   # (src, lineno) -> bool，跨 TU 去重
func_hit = {}   # 符号名 -> bool，跨 TU 去重
cur_src = None
in_scope = False
pending = []    # [name, pct]，归属到随后首个 Source: 行（主 TU）
for ln in open(raw, encoding='utf-8', errors='ignore'):
    m = re.match(r'^\s*-:\s*0:Source:(.+)$', ln)
    if m:  # 逐行数据段的源文件头；函数块之后的首个 Source = 主 TU
        src = m.group(1).strip()
        if pending:
            if filt in src:
                for name, pct in pending:
                    # 排除 std 模板实例与编译器生成符号，仅统计项目自身函数（对齐 lcov 口径）
                    if name.startswith(('_ZNSt', '_ZSt', '__', '_GLOBAL__')) or '__gnu_cxx' in name:
                        continue
                    func_hit[name] = func_hit.get(name, False) or (pct is not None and pct > 0)
            pending = []
        cur_src = src
        in_scope = filt in src
        continue
    if ln.startswith('Function '):
        s = ln.strip()  # 形如 Function '_ZN2vm...'
        sym = s[s.find("'") + 1:s.rfind("'")] if "'" in s else s
        pending.append([sym, None])
        continue
    m = re.match(r'^Lines executed:([\d.]+)% of \d+', ln)
    if m and pending and pending[-1][1] is None:
        pending[-1][1] = float(m.group(1))
        continue
    m = re.match(r'^\s*(#####|\d+):\s*(\d+):', ln)  # 逐行数据：count:行号:源码
    if m and in_scope:
        key = (cur_src, int(m.group(2)))
        line_hit[key] = line_hit.get(key, False) or m.group(1) != '#####'
lt, lh = len(line_hit), sum(1 for v in line_hit.values() if v)
ft_, fh = len(func_hit), sum(1 for v in func_hit.values() if v)
print(f"lines    : {lh*100.0/lt:.2f}% ({lh} of {lt})" if lt else "lines    : n/a (0 instrumented lines in scope)")
print(f"functions: {fh*100.0/ft_:.2f}% ({fh} of {ft_})" if ft_ else "functions: n/a (0 functions in scope)")
EOF
    rm -rf "$tmpd"
    echo "--- $t done ---"
    tail -2 "$LOGDIR/${t}_final.log"
}

# 各 harness 的覆盖率统计范围 = 其攻击面对应源码
CollectOne ft_va_msg_deser      /src/addons/virt_agent/
CollectOne ft_va_case_conf_json /src/addons/virt_agent/
CollectOne ft_va_sdk_unpack     /src/addons/virt_agent/
CollectOne ft_va_ipc_raw        /src/framework/ipc/
echo "ALL_COVERAGE_DONE"
