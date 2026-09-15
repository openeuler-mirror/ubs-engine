#!/bin/bash
#
# Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
# virtagent is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
#
set -euo pipefail

# DT Fuzz 覆盖率汇总（lines / funcs）
#
# 用法:
#   bash test/Fuzz/scripts/ft_coverage.sh <build_dir> [源码过滤路径 ...] [--html]
#
#   build_dir          FT_COVERAGE=ON 的构建目录（gcda 所在地）
#   源码过滤路径        统计范围，可多个，默认 src/addons/virt_agent
#   --html             额外生成 HTML 报告（需 lcov/genhtml，缺失时忽略）
#
# 前提:
#   1) cmake -DBUILD_TESTS=ON -DENABLE_FT=ON -DFT_COVERAGE=ON .. && ninja
#   2) 已运行过 ft_* 目标（driver 子进程退出时自动落盘 gcda）
#
# 实现:
#   优先 lcov；未安装 lcov 时退化为 gcov --json-format + python3 聚合
#   （openEuler 最小安装往往没有 lcov，gcov 随 gcc 必然存在）。
#
# 示例:
#   bash test/Fuzz/scripts/ft_coverage.sh cmake-build-debug src/addons/virt_agent
#   # 单接口模式跑完后看该消息类的覆盖率：
#   FT_MSG_SELECTOR=0 ./bin/ft_va_msg_deser --runs=1000000 --quiet
#   bash test/Fuzz/scripts/ft_coverage.sh cmake-build-debug

BUILD_DIR=${1:?usage: ft_coverage.sh <build_dir> [filter ...] [--html]}
shift

FILTERS=()
HTML=0
for arg in "$@"; do
    if [ "$arg" = "--html" ]; then
        HTML=1
    else
        FILTERS+=("$arg")
    fi
done
if [ ${#FILTERS[@]} -eq 0 ]; then
    FILTERS=("src/addons/virt_agent")
fi

OUT_DIR=${BUILD_DIR}/ft_coverage
mkdir -p "${OUT_DIR}"

have_lcov=0
command -v lcov >/dev/null 2>&1 && have_lcov=1

if [ ${have_lcov} -eq 1 ]; then
    # ---------- lcov 流程 ----------
    # 1) 采集构建目录下全部 gcda
    lcov --capture --directory "${BUILD_DIR}" --output-file "${OUT_DIR}/ft_all.info" \
        --ignore-errors mismatch unused 2>/dev/null

    # 2) 按过滤路径提取（多路径用 --append 累加）
    first=1
    for f in "${FILTERS[@]}"; do
        if [ ${first} -eq 1 ]; then
            lcov --extract "${OUT_DIR}/ft_all.info" "*/${f}/*" \
                --output-file "${OUT_DIR}/ft_final.info" --ignore-errors unused 2>/dev/null
            first=0
        else
            lcov --extract "${OUT_DIR}/ft_all.info" "*/${f}/*" \
                --output-file "${OUT_DIR}/ft_final.info" --append --ignore-errors unused 2>/dev/null
        fi
    done

    # 3) 排除测试自身代码，只统计被攻击的源码
    lcov --remove "${OUT_DIR}/ft_final.info" '*/test/*' \
        --output-file "${OUT_DIR}/ft_final.info" --ignore-errors unused 2>/dev/null

    # 4) 输出 lines / funcs 汇总
    echo "==== DT Fuzz coverage (lcov) ===="
    echo "scope: ${FILTERS[*]}"
    echo "info : ${OUT_DIR}/ft_final.info"
    lcov --summary "${OUT_DIR}/ft_final.info" 2>&1 | grep -E 'lines|functions' || true

    # 5) 可选 HTML 报告
    if [ ${HTML} -eq 1 ] && command -v genhtml >/dev/null 2>&1; then
        genhtml "${OUT_DIR}/ft_final.info" --output-directory "${OUT_DIR}/html" --quiet
        echo "html : ${OUT_DIR}/html/index.html"
    fi
    exit 0
fi

# ---------- gcov 兜底流程（无 lcov） ----------
# 对 build_dir 下每个 .gcda 跑 gcov --json-format --stdout，
# 在 python 内按（文件, 行/函数）聚合：命中 = 任一对象计数 > 0。
GCOV_BIN=$(command -v gcov || echo /usr/bin/gcov)

echo "==== DT Fuzz coverage (gcov) ===="
echo "scope: ${FILTERS[*]}"
BUILD_DIR="${BUILD_DIR}" GCOV_BIN="${GCOV_BIN}" FILTERS="${FILTERS[*]}" \
python3 - "${BUILD_DIR}" <<'PYEOF'
import glob
import json
import os
import subprocess
import sys

build_dir = sys.argv[1]
gcov_bin = os.environ["GCOV_BIN"]
filters = [f.strip("/") for f in os.environ["FILTERS"].split() if f.strip("/")]

# file -> {"lines": {ln: hit}, "funcs": {name_or_key: hit}}
agg = {}

gcdas = glob.glob(os.path.join(build_dir, "**", "*.gcda"), recursive=True)
if not gcdas:
    print(f"[ft-coverage] no .gcda found under {build_dir}; run ft_* first")
    sys.exit(0)

for gcda in gcdas:
    try:
        out = subprocess.run([gcov_bin, "--json-format", "--stdout", gcda],
                             capture_output=True, text=True, timeout=60, check=False)
        if out.returncode != 0:
            continue
        data = json.loads(out.stdout)
    except (subprocess.TimeoutExpired, json.JSONDecodeError, OSError):
        continue
    for f in data.get("files", []):
        path = f.get("file", "")
        # 归一路径分隔符与过滤匹配（gcov 输出绝对/相对路径均可能）
        norm = path.replace("\\", "/")
        if not any(fl in norm for fl in filters):
            continue
        if "/test/" in norm:  # 排除测试自身代码
            continue
        rec = agg.setdefault(norm, {"lines": {}, "funcs": {}})
        for line in f.get("lines", []):
            ln = line.get("line_number")
            if ln is None:
                continue
            hit = line.get("count", 0) > 0
            rec["lines"][ln] = rec["lines"].get(ln, False) or hit
        for func in f.get("functions", []):
            # 同名静态函数在不同文件：用 (name, start_line) 唯一化
            key = (func.get("name", "?"), func.get("start_line", 0))
            hit = func.get("execution_count", 0) > 0
            rec["funcs"][key] = rec["funcs"].get(key, False) or hit

if not agg:
    print("[ft-coverage] no instrumented source matches the filter scope")
    sys.exit(0)

total_lines = hit_lines = total_funcs = hit_funcs = 0
per_file = []
for path, rec in agg.items():
    fl = len(rec["lines"])
    fh = sum(1 for v in rec["lines"].values() if v)
    ff = len(rec["funcs"])
    ffc = sum(1 for v in rec["funcs"].values() if v)
    total_lines += fl
    hit_lines += fh
    total_funcs += ff
    hit_funcs += ffc
    per_file.append((path, fl, fh, ff, ffc))

def pct(h, t):
    return 100.0 * h / t if t else 0.0

print(f"lines    : {pct(hit_lines, total_lines):6.2f}% ({hit_lines} of {total_lines})")
print(f"functions: {pct(hit_funcs, total_funcs):6.2f}% ({hit_funcs} of {total_funcs})")
print(f"files    : {len(agg)}")
# 覆盖率最低的 5 个文件（指导补充种子）
per_file.sort(key=lambda x: pct(x[2], x[1]))
print("lowest coverage files:")
for path, fl, fh, ff, ffc in per_file[:5]:
    print(f"  {pct(fh, fl):6.2f}% lines  {path}")
PYEOF

# HTML 报告在无 lcov 时不可用，仅提示
if [ ${HTML} -eq 1 ]; then
    echo "[ft-coverage] --html requires lcov/genhtml (not installed); skipped"
fi
