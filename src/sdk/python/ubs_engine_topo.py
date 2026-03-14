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
from typing import List

from ubse.ffi.ubs_engine_binding_topo import UbsEngineBindingTopology
from ubse.models.ubs_engine_model_topo import UbsTopoNode, UbsTopoLink

_topo_interface = UbsEngineBindingTopology()


def ubs_topo_node_list() -> List[UbsTopoNode]:
    """获取拓扑节点列表（使用全局的topo_interface）"""
    return _topo_interface.ubs_topo_node_list()


def ubs_topo_node_local_get() -> UbsTopoNode:
    """获取本地拓扑节点（使用全局的topo_interface）"""
    return _topo_interface.ubs_topo_node_local_get()


def ubs_topo_link_list() -> List[UbsTopoLink]:
    """获取拓扑链接列表（使用全局的topo_interface）"""
    return _topo_interface.ubs_topo_link_list()
