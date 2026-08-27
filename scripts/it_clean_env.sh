#!/bin/bash
# it_clean_env.sh — IT 环境清理：清除残留 IT daemon/launcher 进程并检查节点端口占用
#
# 背景：测试进程异常退出（崩溃、超时 kill、Ctrl-C）时 TearDownTestSuite 不会执行，
# ubse_it_daemon 成为孤儿进程（PPID=1）并持续占用节点 IP（127.0.0.x）的 1901 端口
# （UbseMasterRpcServer/选举端口）。后续多节点场景将全部在 SetUpTestSuite 阶段
# bind 失败，症状：Create engine UbseMasterRpcServer failed / 双 master / 节点互相不可见。
# run_it_test.sh 在每个场景二进制运行前后自动调用本脚本。
#
# 用法：
#   scripts/it_clean_env.sh          # 手动清理（环境干净时也输出状态行）
#   scripts/it_clean_env.sh -q       # 静默模式（环境干净时无输出，供自动挂钩使用）
#
# 退出码恒为 0（仅告警不阻断：单节点场景不依赖 1901，不应因端口被占而全量终止）。
set -u

QUIET=0
[ "${1:-}" = "-q" ] && QUIET=1

# 节点选举/RPC 端口，对应 test/IT/stubs/ubse_interface_preload.cpp 的 ELECTION_TCP_PORT。
# 未来若端口扩展，可通过环境变量追加，如 UBSE_IT_CLEAN_PORTS="1901 1902"
CLEAN_PORTS="${UBSE_IT_CLEAN_PORTS:-1901}"
GRACE_SECS=2
IT_PROCS="ubse_it_daemon ubse_it_node_launcher"

log()  { [ "$QUIET" -eq 1 ] || echo "[it-clean] $*"; }
warn() { echo "[it-clean][WARN] $*" >&2; }

# 注意：1) 部分 procps 版本的 pgrep 不支持多 pattern，须逐个查询；
#       2) comm 名上限 15 字符：ubse_it_node_launcher(21字符) 被内核截断为
#          ubse_it_node_la，且本机 pgrep 对超过 15 字符的 pattern（含正则）一律
#          零匹配，故须用恰好 15 字符的前缀做子串匹配。
collect_pids() {
    pgrep -x ubse_it_daemon 2>/dev/null || true
    pgrep 'ubse_it_node_la' 2>/dev/null || true
}

# 1. 先 SIGTERM 优雅终止（daemon 正常走清理路径）
pids=$(collect_pids)
if [ -n "$pids" ]; then
    log "SIGTERM 残留 IT 进程: $(echo "$pids" | tr '\n' ' ')"
    kill $pids 2>/dev/null || true
    i=0
    while [ $i -lt $((GRACE_SECS * 10)) ] && [ -n "$(collect_pids)" ]; do
        sleep 0.1
        i=$((i + 1))
    done
fi

# 2. 仍存活的 SIGKILL 强杀
pids=$(collect_pids)
if [ -n "$pids" ]; then
    log "SIGKILL 强杀: $(echo "$pids" | tr '\n' ' ')"
    kill -9 $pids 2>/dev/null || true
    sleep 0.5
fi

# 3. 强杀后仍在属异常（D 状态/权限），明确告警
if [ -n "$(collect_pids)" ]; then
    warn "以下 IT 进程无法终止，多节点场景可能失败："
    collect_pids | while read -r pid; do
        warn "  pid=$pid $(cat /proc/$pid/comm 2>/dev/null || echo '?') $(cat /proc/$pid/status 2>/dev/null | grep '^State:' || true)"
    done
fi

# 4. 端口占用检查：被非 IT 进程持有则提前告警（避免事后神秘的全 suite 失败）
if command -v ss >/dev/null 2>&1; then
    for port in $CLEAN_PORTS; do
        listeners=$(ss -tlnpH 2>/dev/null | awk -v p=":$port" '$4 ~ p"$"')
        if [ -n "$listeners" ]; then
            warn "端口 $port 仍被占用，对应节点 IP 的集群将 bind 失败："
            echo "$listeners" | while read -r line; do warn "  $line"; done
            warn "若非 IT 进程持有，请手动处理（多节点场景会全挂）。"
        fi
    done
elif command -v netstat >/dev/null 2>&1; then
    for port in $CLEAN_PORTS; do
        if netstat -tlnp 2>/dev/null | awk -v p=":$port" '$4 ~ p"$"' | grep -q .; then
            warn "端口 $port 仍被占用（ss 不可用，netstat 检测），多节点场景可能失败。"
        fi
    done
fi

[ "$QUIET" -eq 1 ] || echo "[it-clean] 环境已清理"
exit 0
