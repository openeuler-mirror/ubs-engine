#!/usr/bin/python3
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MatrixEngine is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
import sys
import os

from ubse.ubs_engine_topo import ubs_topo_node_list, ubs_topo_node_local_get, ubs_topo_link_list
from ubse.ubs_engine_log import LogLevel, ubs_engine_log_callback_register


def simple_log_handler(log_level: LogLevel, message: str):
    """简单的日志处理函数，只打印INFO及以上的日志"""
    # 只处理INFO级别及以上的日志（INFO, WARN, ERROR, CRIT）
    if log_level.value >= LogLevel.INFO.value:
        print(f"[simple_log_handler] {log_level.name}: {message}")


def main():
    # 注册自定义日志处理器
    ubs_engine_log_callback_register(simple_log_handler)
    print("自定义日志处理函数注册成功")

    try:
        # 获取 CPU 拓扑列表
        cpu_links = ubs_topo_link_list()

        print(f"获取到 {len(cpu_links)} 条 CPU 拓扑信息：")
        for link in cpu_links:
            print(link)

    except Exception as e:
        print(f"调用拓扑接口失败: {e}")

    try:
        # 获取节点列表
        node_list = ubs_topo_node_list()

        print(f"获取到 {len(node_list)} 条 节点信息：")
        for info in node_list:
            print(info)

    except Exception as e:
        print(f"调用拓扑接口失败: {e}")

    try:
        # 获取节点
        node_ = ubs_topo_node_local_get()
        print(node_)

    except Exception as e:
        print(f"调用拓扑接口失败: {e}")


if __name__ == "__main__":
    main()
