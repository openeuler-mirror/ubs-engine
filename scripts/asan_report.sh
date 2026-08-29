#!/bin/bash
set -uo pipefail

# ASAN 报告汇总/提取脚本。
# 用法: bash scripts/asan_report.sh [报告目录]
#   默认报告目录: cmake-build-debug/asan_logs（ASAN 复用构建类型对应目录，-D 即 cmake-build-debug）
#
# 输出文件（均在 <报告目录>/ 下）:
#   asan_all_logs.log 全量日志汇总：合并 asan_log.* 全部内容（不经过滤）
#   asan_report.log 当前扫描报告：问题清单（类型 + 关键源码栈帧 + SUMMARY，标注
#                   [剩余]（未命中白名单，疑似真实/新增）/ [屏蔽]（已知误报））
#                   + 关键信息摘要（统计 + 各问题精简清单）
#
# 功能:
#   1. 合并 <报告目录>/asan_log.* 中的全部发现 -> asan_all_logs.log
#   2. 按白名单（scripts/asan_whitelist.conf）区分"已知误报"与"剩余问题"
#   3. 当前扫描报告写入 asan_report.log

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

WHITELIST_CONF="$SCRIPT_DIR/asan_whitelist.conf"

REPORT_DIR="${1:-${PROJECT_ROOT}/cmake-build-debug/asan_logs}"
if [ ! -d "$REPORT_DIR" ]; then
    echo "Error: report dir '$REPORT_DIR' does not exist." >&2
    exit 1
fi

MERGED="$REPORT_DIR/asan_all_logs.log"
REPORT="$REPORT_DIR/asan_report.log"
: > "$MERGED"
: > "$REPORT"

scan_time=$(date '+%Y-%m-%d %H:%M:%S')

# ---- 1. 合并全部原始报告 ----
shopt -s nullglob
for f in "$REPORT_DIR"/asan_log.*; do
    {
        echo "########## $(basename "$f") ##########"
        cat "$f"
        echo ""
    } >> "$MERGED"
done

total_errors=$(grep -acE 'ERROR: (Address|Leak)Sanitizer|AddressSanitizer:DEADLYSIGNAL' "$MERGED" || true)

# ---- 2. 当前扫描报告文件头 ----
{
    echo "#####################################################################"
    echo "# ASAN 当前扫描报告"
    echo "# 扫描时间 : ${scan_time}"
    echo "# 报告目录 : ${REPORT_DIR}"
    echo "# 完整收集 : ${MERGED}"
    echo "# 说明     : [剩余] = 未命中白名单（疑似真实/新增），[屏蔽] = 白名单已知误报"
    echo "#####################################################################"
} > "$REPORT"

if [ "$total_errors" -eq 0 ]; then
    {
        echo ""
        echo "未发现任何 ASAN/LeakSanitizer 错误。"
        echo "====================================================================="
        echo "发现总数           : 0"
        echo "白名单屏蔽(误报)   : 0"
        echo "剩余问题(疑似真实) : 0"
        echo "====================================================================="
        echo "结论: 未发现任何 ASAN/LeakSanitizer 错误。"
        echo "====================================================================="
    } >> "$REPORT"

    echo "报告目录: $REPORT_DIR"
    echo "完整收集: $MERGED"
    echo "扫描报告: $REPORT"
    echo "发现总数: 0"
    echo ""
    echo "未发现任何 ASAN/LeakSanitizer 错误。"
    exit 0
fi

# ---- 3. 加载白名单规则（scripts/asan_whitelist.conf） ----
# 每行一条扩展正则（grep -E），'#' 开头行与空行忽略。
declare -a WL_RULES=()
if [ -f "$WHITELIST_CONF" ]; then
    while IFS= read -r rule; do
        case "$rule" in
            ''|\#*) continue ;;
        esac
        WL_RULES+=("$rule")
    done < "$WHITELIST_CONF"
else
    echo "Warning: whitelist conf '$WHITELIST_CONF' not found, 全部错误将保留。" >&2
fi

# 判断一个错误块是否命中任意白名单规则（块内任意一行命中即视为已知误报）
is_whitelisted() {
    local blk="$1" rule
    for rule in "${WL_RULES[@]}"; do
        if echo "$blk" | grep -aqE "$rule"; then
            return 0
        fi
    done
    return 1
}

# 过滤框架噪音（保留项目源码栈帧）
filter_noise() {
    grep -aE '/src/|/test/' || true
}

# 提取一个错误块的关键信息（类型 + 项目源码栈帧 + SUMMARY）
extract_one() {
    local err="$1"
    [ -n "$err" ] || return
    local type_line summary top_frames
    type_line=$(echo "$err" | grep -aE 'ERROR: (Address|Leak)Sanitizer|AddressSanitizer:DEADLYSIGNAL' | head -1 || true)
    summary=$(echo "$err" | grep -aE '^SUMMARY: ' | head -1 || true)
    top_frames=$(echo "$err" | filter_noise | grep -aE '^    #[0-9]+ ' | head -3 || true)

    echo "  - ${type_line:-<unknown type>}"
    if [ -n "$top_frames" ]; then
        echo "$top_frames" | sed 's/^    /      /'
    fi
    if [ -n "$summary" ]; then
        echo "      ${summary}"
    fi
}

# 生成单条问题的精简信息（类型 + SUMMARY + 首个项目源码位置），用于摘要文件
compact_one() {
    local err="$1"
    local type_line summary top_loc
    type_line=$(echo "$err" | grep -aE 'ERROR: (Address|Leak)Sanitizer|AddressSanitizer:DEADLYSIGNAL' | head -1 || true)
    summary=$(echo "$err" | grep -aE '^SUMMARY: ' | head -1 || true)
    top_loc=$(echo "$err" | filter_noise | grep -aoE '/[^ ]+\.(cpp|cc|c|h):[0-9]+' | head -1 || true)

    echo "  - ${type_line#*ERROR: }  |  ${summary#*SUMMARY: }"
    if [ -n "$top_loc" ]; then
        echo "      位置: ${top_loc}"
    fi
}

# ---- 4. 遍历合并报告，按白名单区分并写入扫描报告 ----
block=""
wl_filtered=0
wl_remaining=0
remaining_text=""
blocked_text=""

emit_block() {
    if [ -n "$block" ]; then
        local idx=$((wl_filtered + wl_remaining + 1))
        if is_whitelisted "$block"; then
            status="[屏蔽]"
            wl_filtered=$((wl_filtered + 1))
            blocked_text+="$(compact_one "$block")"$'\n'
        else
            status="[剩余]"
            wl_remaining=$((wl_remaining + 1))
            remaining_text+="$(compact_one "$block")"$'\n'
        fi
        {
            echo ""
            echo "########## ${status} 问题 #${idx} ##########"
            extract_one "$block"
        } >> "$REPORT"
    fi
    block=""
}

while IFS= read -r line; do
    case "$line" in
        "########## "*)
            emit_block
            ;;
        *"ERROR: AddressSanitizer"*|*"ERROR: LeakSanitizer"*|*"AddressSanitizer:DEADLYSIGNAL"*)
            if [ -n "$block" ]; then
                emit_block
            fi
            block="$line"
            ;;
        *)
            if [ -n "$block" ]; then
                block="$block
$line"
            fi
            ;;
    esac
done < "$MERGED"
emit_block

# ---- 5. 追加关键信息摘要到扫描报告 ----
{
    echo ""
    echo "====================================================================="
    echo " 关键信息摘要"
    echo "---------------------------------------------------------------------"
    echo "扫描时间           : ${scan_time}"
    echo "报告目录           : ${REPORT_DIR}"
    echo "完整收集文件       : ${MERGED}"
    echo "发现总数           : ${total_errors}"
    echo "白名单屏蔽(误报)   : ${wl_filtered}"
    echo "剩余问题(疑似真实) : ${wl_remaining}"
    echo "---------------------------------------------------------------------"
    if [ "${wl_remaining}" -eq 0 ]; then
        echo "结论: 所有报告均已命中白名单（已知误报），无剩余问题。"
    else
        echo "【剩余问题】${wl_remaining} 条，请优先确认:"
        printf '%s' "${remaining_text}"
        echo ""
    fi
    if [ "${wl_filtered}" -gt 0 ]; then
        echo "【已屏蔽误报】${wl_filtered} 条（白名单命中）:"
        printf '%s' "${blocked_text}"
        echo ""
    fi
    echo "====================================================================="
} >> "$REPORT"

# ---- 6. 控制台输出 ----
echo "报告目录: $REPORT_DIR"
echo "完整收集: $MERGED"
echo "扫描报告: $REPORT"
echo "发现总数: $total_errors  白名单屏蔽(误报): $wl_filtered  剩余问题(疑似真实): $wl_remaining"
echo ""
if [ "$wl_remaining" -gt 0 ]; then
    echo "---------- 剩余问题(疑似真实/新增) ----------"
    printf '%s' "$remaining_text"
    echo "--------------------------------------------"
fi
if [ "$wl_remaining" -eq 0 ]; then
    echo "剩余问题: 无。所有报告均已命中白名单（已知误报）。"
fi
echo "详见 $REPORT"
