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
from typing import Dict, Any, List
from ubse.ffi.ubs_virt_agent_base import UbsVirtAgentBase
from ubse.models.ubs_virt_agent_model import BorrowStrategyT, DstNumaInfoT
from ubse.ffi.ubs_virt_agent_types import DstNumaInfo, BorrowStrategy, SrcParam


class UbsVirtAgentMemBorrowStrategy(UbsVirtAgentBase):
    """MemBorrowStrategy related interface"""

    def __init__(self):
        super().__init__()
        self._setup_mem_borrow_strategy_functions()

    def ubs_mem_borrow_strategy(self, param: Dict[str, Any]) -> BorrowStrategyT:
        if not self.lib_ubse:
            raise ConnectionError("Native library not loaded")
        src_param = SrcParam(
            src_nid=param["srcParam"]["srcNid"].encode('utf-8'),
            src_socket_id=param["srcParam"]["srcSocketId"],
            src_numa_id=param["srcParam"]["srcNumaId"],
            borrow_size=param["borrowSize"]
        )
        borrow_strategy = BorrowStrategy()

        result = self.lib_ubse.ubs_virt_agent_mem_borrow_strategy(
            ctypes.byref(src_param),
            ctypes.byref(borrow_strategy)
        )
        self.ubs_virt_agent_handle_result(result, "ubs_mem_borrow_strategy")

        src_host_id = borrow_strategy.src_host_id.decode('utf-8') if borrow_strategy.src_host_id else ""
        src_socket_id = borrow_strategy.src_socket_id
        src_numa_id = borrow_strategy.src_numa_id
        borrow_size = borrow_strategy.borrow_size
        # to dest_numa_infos
        dest_numa_infos = []
        for i in range(borrow_strategy.dest_numa_infos_size):
            dst_numa_info = borrow_strategy.dest_numa_infos[i]
            host_id = dst_numa_info.host_id.decode('utf-8') if dst_numa_info.host_id else ""
            socket_id = dst_numa_info.socket_id
            numa_nums = dst_numa_info.numa_nums
            if numa_nums > 0:
                numa_ids = list(dst_numa_info.numa_ids)[:numa_nums]
                mem_sizes = list(dst_numa_info.mem_sizes)[:numa_nums]
            else:
                numa_ids = []
                mem_sizes = []
            dest_numa_infos.append(DstNumaInfoT(host_id, socket_id, numa_nums, numa_ids, mem_sizes))
        result = BorrowStrategyT(
            src_host_id=src_host_id,
            src_socket_id=src_socket_id,
            src_numa_id=src_numa_id,
            borrow_size=borrow_size,
            dest_numa_infos=dest_numa_infos
        )
        return result

    def _setup_mem_borrow_strategy_functions(self):
        """Set the prototype of the relevant function"""
        self.lib_ubse.ubs_virt_agent_mem_borrow_strategy.argtypes = [
            ctypes.POINTER(SrcParam),
            ctypes.POINTER(BorrowStrategy)
        ]
        self.lib_ubse.ubs_virt_agent_mem_borrow_strategy.restype = ctypes.c_int32

        # Set the free function(If it exists)
        if hasattr(self.lib_ubse, 'free'):
            self.lib_ubse.free.argtypes = [ctypes.c_void_p]
            self.lib_ubse.free.restype = None
