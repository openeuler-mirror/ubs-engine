#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
BUILD_DIR=${BUILD_DIR:-"$SCRIPT_DIR/build"}
CXX=${CXX:-g++}
INCLUDE_DIR="$SCRIPT_DIR/include"

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

compile_common=(-std=c++17 -Wall -Wextra -Wpedantic -I"$INCLUDE_DIR")

build_app()
{
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
