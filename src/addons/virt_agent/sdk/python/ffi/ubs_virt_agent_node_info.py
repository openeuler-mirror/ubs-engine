# !/usr/bin/python3
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
import ctypes

from ubse.models.ubs_virt_agent_model import NodeNumaInfoT, NumaInfoT
from ubse.ffi.ubs_virt_agent_base import UbsVirtAgentBase
from ubse.ffi.ubs_virt_agent_types import NumaInfo


class UbsVirtAgentNodeInfo(UbsVirtAgentBase):
    """node info query interface"""

    def __init__(self):
        super().__init__()
        self._setup_topo_functions()

    def ubs_node_info_list(self) -> NodeNumaInfoT:
        if not self.lib_ubse:
            raise ConnectionError("Native library not loaded")
        try:
            node_info_ptr = ctypes.POINTER(NumaInfo)()
            info_count = ctypes.c_uint32(0)
            result = self.lib_ubse.ubs_virt_agent_mem_fragmentation_node_info(
                ctypes.byref(node_info_ptr),
                ctypes.byref(info_count)
            )

            self.ubs_virt_agent_handle_result(result, "ubs_node_info_list")
            if not node_info_ptr or info_count.value == 0:
                return None
            link_models = []

            for i in range(info_count.value):
                link_model = node_info_ptr[i]
                link_models.append(NumaInfoT.from_c_struct(link_model))

            return NodeNumaInfoT(
                numa_infos=link_models,
                host_id=link_models[0].host_id,
                hostname=link_models[0].hostname
            )
        finally:
            if node_info_ptr:
                self.lib_ubse.free(node_info_ptr)

    def _setup_topo_functions(self):
        """Set up prototypes for log-related functions"""
        self.lib_ubse.ubs_virt_agent_mem_fragmentation_node_info.argtypes = [
            ctypes.POINTER(ctypes.POINTER(NumaInfo)),
            ctypes.POINTER(ctypes.c_uint32)
        ]
        self.lib_ubse.ubs_virt_agent_mem_fragmentation_node_info.restype = ctypes.c_int32

        # Set up the free function (if it exists)
        if hasattr(self.lib_ubse, 'free'):
            self.lib_ubse.free.argtypes = [ctypes.c_void_p]
            self.lib_ubse.free.restype = None
