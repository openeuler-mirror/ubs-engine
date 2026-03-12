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
from typing import Dict, Any
from ubse.models.ubs_virt_agent_model import CaseConfInfoT
from ubse.ffi.ubs_virt_agent_case_conf_get import UbsVirtAgentCaseConfGet
from ubse.ffi.ubs_virt_agent_case_conf_set import UbsVirtAgentCaseConfSet

_case_conf_get_interface = UbsVirtAgentCaseConfGet()
_case_conf_set_interface = UbsVirtAgentCaseConfSet()


def ubs_case_conf_info() -> CaseConfInfoT:
    """Obtain the case_conf configuration information of the current node.（Use the global interface）"""
    return _case_conf_get_interface.ubs_case_conf_info()


def ubs_case_conf_set(param: Dict[str, Any]) -> int:
    """Set the case_conf configuration information of the current node.（Use the global interface）"""
    return _case_conf_set_interface.ubs_case_conf_set(param)
