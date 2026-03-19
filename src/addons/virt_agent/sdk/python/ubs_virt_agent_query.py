#!/usr/bin/python3
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# virtagent is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

from ubse.models.ubs_virt_agent_model import NodeNumaInfoT, NodeVmInfoT
from ubse.ffi.ubs_virt_agent_node_info import UbsVirtAgentNodeInfo
from ubse.ffi.ubs_virt_agent_vm_info import UbsVirtAgentVmInfo

_node_info_interface = UbsVirtAgentNodeInfo()
_vm_info_interface = UbsVirtAgentVmInfo()


def ubs_node_info_list() -> NodeNumaInfoT:
    """Obtain the NUMA information of the local node.（Use the global interface）"""
    return _node_info_interface.ubs_node_info_list()


def ubs_vm_info_list() -> NodeVmInfoT:
    """Obtain the VM information of the local node.（Use the global interface）"""
    return _vm_info_interface.ubs_vm_info_list()
