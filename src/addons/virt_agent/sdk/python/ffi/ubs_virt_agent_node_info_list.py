#  Copyright (c) Huawei Technologies Co., Ltd. $tody.year. All rights reserved.
#
#  virtagent is licensed under Mulan PSL v2.
#  You can use this software according to the terms and conditions of the Mulan PSL v2.
#  You may obtain a copy of Mulan PSL v2 at:
#           http://license.coscl.org.cn/MulanPSL2
#  THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
#  EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
#  MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
#  See the Mulan PSL v2 for more details.
#
#  virtagent is licensed under Mulan PSL v2.
#  You can use this software according to the terms and conditions of the Mulan PSL v2.
#  You may obtain a copy of Mulan PSL v2 at:
#           http://license.coscl.org.cn/MulanPSL2
#  THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
#  EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
#  MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
#  See the Mulan PSL v2 for more details.

# !/usr/bin/python3
import ctypes
from typing import List
from ubse.ffi.ubs_virt_agent_base import UbsVirtAgentBase
from ubse.ffi.ubs_virt_agent_types import NodeInfoC, NodeInfoListC
from ubse.models.ubs_virt_agent_model import NodeInfoT


class UbsVirtAgentNodeInfoList(UbsVirtAgentBase):
    """Memory fragmentation node information list query interface"""

    def __init__(self):
        super().__init__()
        self._setup_node_info_list_functions()

    def ubs_mem_fragmentation_node_info_list(self) -> List[NodeInfoT]:
        """
        Query memory fragmentation node information list

        Get NUMA topology information and memory status of all nodes in the system, including node ID, hostname,
        NUMA node details (total memory, free memory, huge page information, etc.), and identify the current node.

        Returns:
            List[NodeInfoT]: Node information list, each element contains:
                - node_id: Node ID
                - numa_infos: NUMA information list
                - is_current: Whether it is the current node

        Raises:
            ConnectionError: Raised when native library is not loaded
            Exception: Raised when query fails
        """
        if not self.lib_ubse:
            raise ConnectionError("Native library not loaded")

        node_info_list = NodeInfoListC()
        result = self.lib_ubse.ubs_virt_agent_mem_fragmentation_node_info_list(
            ctypes.byref(node_info_list)
        )

        self.ubs_virt_agent_handle_result(result, "ubs_mem_fragmentation_node_info_list")
        node_info_list_result = []

        try:
            if not node_info_list.node_infos or node_info_list.node_len == 0:
                return node_info_list_result

            for i in range(node_info_list.node_len):
                node_info_ptr = ctypes.cast(
                    ctypes.addressof(node_info_list.node_infos.contents) + i * ctypes.sizeof(NodeInfoC),
                    ctypes.POINTER(NodeInfoC)
                )
                node_info_t = NodeInfoT.from_c_struct(node_info_ptr.contents)
                node_info_list_result.append(node_info_t)

            return node_info_list_result
        finally:
            if hasattr(self.lib_ubse, 'free'):
                node_info_list.free_c_memory(self.lib_ubse.free)

    def _setup_node_info_list_functions(self):
        """Setup function prototypes for node information list"""
        self.lib_ubse.ubs_virt_agent_mem_fragmentation_node_info_list.argtypes = [
            ctypes.POINTER(NodeInfoListC)
        ]
        self.lib_ubse.ubs_virt_agent_mem_fragmentation_node_info_list.restype = ctypes.c_int32

        if hasattr(self.lib_ubse, 'free'):
            self.lib_ubse.free.argtypes = [ctypes.c_void_p]
            self.lib_ubse.free.restype = None