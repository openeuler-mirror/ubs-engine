#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
BUILD_DIR=${BUILD_DIR:-"$SCRIPT_DIR/build"}
CXX=${CXX:-g++}
INCLUDE_DIR="$SCRIPT_DIR/include"
# SDK 头文件源目录 (相对脚本位置), 用于软链接同步, 避免手工副本漂移
# test/tools/c/ssu 距仓库根 4 层, 需回退 4 级
SRC_INCLUDE_DIR="$SCRIPT_DIR/../../../../src/sdk/c/include"

usage()
{
    cat <<'EOF'
用法: bash build.sh [app|clean]

  app    构建通过 dlopen 加载 libubse-ssu-client.so 的 CLI（默认）
  clean  删除本示例的 build 目录

可配置环境变量:
  CXX, BUILD_DIR
EOF
}

# 确保 include/ 中的头文件以软链接形式指向 SDK 源头, 避免手工副本漂移。
# 若源目录不存在 (如独立分发场景), 回退使用 include/ 中已有文件。
ensure_includes()
{
    if [[ ! -d "$SRC_INCLUDE_DIR" ]]; then
        return 0
    fi
    mkdir -p "$INCLUDE_DIR"
    local hdr
    for hdr in ubs_engine.h ubs_engine_ssu.h ubs_error.h; do
        local src="$SRC_INCLUDE_DIR/$hdr"
        local dst="$INCLUDE_DIR/$hdr"
        [[ -e "$src" ]] || continue
        # 已是正确软链接则跳过; 否则 (缺失/普通文件/指向过期) 重建
        if [[ -L "$dst" ]] && [[ "$(readlink -f "$dst")" == "$(readlink -f "$src")" ]]; then
            continue
        fi
        rm -f "$dst"
        ln -s "$SRC_INCLUDE_DIR/$hdr" "$dst"
    done
}

compile_common=(-std=c++17 -Wall -Wextra -Wpedantic -I"$INCLUDE_DIR")

build_app()
{
    ensure_includes
    "$CXX" "${compile_common[@]}" -O2 "$SCRIPT_DIR/main.cpp" \
        -ldl \
        -o "$BUILD_DIR/ubse_ssu_test_c"
    echo "已生成 $BUILD_DIR/ubse_ssu_test_c"
}

command=${1:-app}
case "$command" in
    app)
        mkdir -p "$BUILD_DIR"
        build_app
        ;;
    clean)
        normalized_build_dir=$(realpath -m -- "$BUILD_DIR")
        normalized_script_dir=$(realpath -m -- "$SCRIPT_DIR")
        [[ "$normalized_build_dir" == "$normalized_script_dir/"* ]] || {
            echo "拒绝删除示例目录之外的 BUILD_DIR: $BUILD_DIR" >&2
            exit 1
        }
        rm -rf -- "$normalized_build_dir"
        ;;
    -h|--help)
        usage
        ;;
    *)
        usage >&2
        exit 2
        ;;
esac
