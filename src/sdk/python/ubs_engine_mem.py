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

from ubse.ffi.ubs_engine_binding_mem import UbsEngineBindingMem
from ubse.models.ubs_engine_model_mem import UbsMemNumaDesc

_mem_interface = UbsEngineBindingMem()


def ubs_mem_numa_list() -> List[UbsMemNumaDesc]:
    """获取NUMA节点列表（使用全局的mem_interface）"""
    return _mem_interface.ubs_mem_numa_list()
