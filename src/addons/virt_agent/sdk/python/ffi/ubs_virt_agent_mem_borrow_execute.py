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
from typing import Dict, Any, Tuple
from ubse.ffi.ubs_virt_agent_base import UbsVirtAgentBase
from ubse.models.ubs_virt_agent_model import BorrowExecuteResT
from ubse.ffi.ubs_virt_agent_types import BorrowStrategy, MemBorrowSetting, MemBorrowResult
from ubse.ffi.ubs_virt_agent_exceptions import UbsVirtAgentError


class UbsVirtAgentMemBorrowExecute(UbsVirtAgentBase):
    """MemBorrowExecute related interface"""

    def __init__(self):
        super().__init__()
        self._setup_mem_borrow_execute_functions()

    def ubs_mem_borrow_execute(self, param: Dict[str, Any], is_async: bool = False) -> Tuple[BorrowExecuteResT, str]:
        if not self.lib_ubse:
            raise ConnectionError("Native library not loaded")
        if not isinstance(is_async, bool):
            raise TypeError("is_async must be a bool")
        borrow_strategy = self._build_borrow_strategy(param)

        mem_borrow_setting = MemBorrowSetting()
        mem_borrow_setting.borrow_strategy = borrow_strategy
        mem_borrow_setting.is_async = is_async

        mem_borrow_result = MemBorrowResult()

        result = self.lib_ubse.ubs_virt_agent_mem_borrow_execute(ctypes.byref(mem_borrow_setting),
                                                                 ctypes.byref(mem_borrow_result))
        self.ubs_virt_agent_handle_result(result, "ubs_mem_borrow_execute")

        if not mem_borrow_result.borrow_ids or not mem_borrow_result.present_numa_ids:
            raise UbsVirtAgentError("borrow_ids is null or present_numa_ids is null")

        borrow_ids = []
        for i in range(mem_borrow_result.borrow_ids_num):
            borrow_id = ctypes.string_at(mem_borrow_result.borrow_ids[i])
            borrow_ids.append(borrow_id.decode('utf-8'))

        present_numa_ids = []
        for i in range(mem_borrow_result.present_numa_ids_num):
            present_numa_ids.append(mem_borrow_result.present_numa_ids[i])

        task_id = ctypes.string_at(mem_borrow_result.task_id).decode('utf-8')

        borrow_execute_res = BorrowExecuteResT(
            borrow_ids=borrow_ids,
            present_numa_ids=present_numa_ids
        )
        return borrow_execute_res, task_id


    def _build_borrow_strategy(self, param: Dict[str, Any]) -> BorrowStrategy:
        borrow_strategy = BorrowStrategy()

        # Parse source node information
        src_nid = param.get("srcParam", {}).get("srcNid")
        if not isinstance(src_nid, str):
            raise ValueError("srcNid must be a string")
        borrow_strategy.src_host_id = src_nid.encode('utf-8')
        borrow_strategy.src_socket_id = param.get("srcParam", {}).get("srcSocketId")
        borrow_strategy.src_numa_id = param.get("srcParam", {}).get("srcNumaId")

        # Set borrow size
        borrow_strategy.borrow_size = param.get("borrowSize")

        # Parse destination NUMA node information
        dest_numa_infos = param.get("destParam")
        borrow_strategy.dest_numa_infos_size = len(dest_numa_infos)

        for i, dest_info in enumerate(dest_numa_infos):
            borrow_strategy.dest_numa_infos[i].host_id = dest_info.get("destNid").encode('utf-8')
            borrow_strategy.dest_numa_infos[i].socket_id = dest_info.get("destSocketId")
            borrow_strategy.dest_numa_infos[i].numa_nums = dest_info.get("destNumaNum")

            # Set NUMA ID list
            for j, numa_id in enumerate(dest_info.get("destNumaId")):
                borrow_strategy.dest_numa_infos[i].numa_ids[j] = numa_id

            # Set memory size list
            for j, mem_size in enumerate(dest_info.get("memSize")):
                borrow_strategy.dest_numa_infos[i].mem_sizes[j] = mem_size

        return borrow_strategy

    def _setup_mem_borrow_execute_functions(self):
        """Set the prototype of the relevant function"""
        self.lib_ubse.ubs_virt_agent_mem_borrow_execute.argtypes = [
            ctypes.POINTER(MemBorrowSetting),
            ctypes.POINTER(MemBorrowResult)
        ]
        self.lib_ubse.ubs_virt_agent_mem_borrow_execute.restype = ctypes.c_int32
