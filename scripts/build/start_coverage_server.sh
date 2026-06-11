#!/bin/bash

# ***********************************************************************
# Copyright: (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
# script for start http server for coverage
# ***********************************************************************

set -e

# 定义颜色
BLUE='\033[0;34m'   # 蓝色
NC='\033[0m'        # 无颜色

# 获取当前脚本的工作目录
current_directory="$(pwd)"

HTML_PATH="${1:-"${current_directory}/cmake-build-debug/coverage/index.html"}"

# 查找所有 python3 -m http.server 进程
process_info=$(pgrep -af "python3 -m http.server") || process_info=""

# 标志变量，表示是否找到匹配的进程
found_matching_process=false

if [[ -n "$process_info" ]]; then

    # 遍历每个进程
    while read -r line; do
        # 提取进程的 PID
        pid=$(echo "$line" | awk '{print $1}')

        # 获取工作目录
        working_directory=$(readlink -f /proc/"$pid"/cwd)

        # 判断工作目录是否相同
        if [[ "$working_directory" == "$current_directory" ]]; then
            # 提取 IP 和 PORT
            PORT=$(echo "$line" | grep -oP '\d{4,5}(?= --bind)')
            if [[ "$line" =~ --bind\ ([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+) ]]; then
                IP="${BASH_REMATCH[1]}"
            fi

            echo "Server is already running on IP: $IP and Port: $PORT"
            found_matching_process=true
            break
        fi
    done <<< "$process_info"

    if ! $found_matching_process; then
        echo "No matching server processes found in the current working directory."
    fi
else
    echo "No python3 -m http.server processes are running."
fi

# 如果没有找到与当前工作目录相同的进程，启动服务器
if ! $found_matching_process; then
    # 获取可用的 IP 地址
    IP=$(hostname -I | awk '{print $1}')

    # 查找可用的端口
    PORT=$(seq 8000 9000 | grep -v -x -f <(nc -z localhost -w 1 -v $(seq 8000 9000) 2>&1 | awk '{print $3}') | head -n 1)

    if [ -n "$PORT" ]; then
        echo "Starting HTTP server at $IP:$PORT"
        python3 -m http.server "$PORT" --bind "$IP" >/dev/null 2>/dev/null &
        echo "Server started successfully."
    else
        echo "No available port found."
    fi
fi

echo -e "\nOpen the link below to view the coverage report:\n\n${BLUE}http://$IP:$PORT${HTML_PATH}${NC}\n\n"