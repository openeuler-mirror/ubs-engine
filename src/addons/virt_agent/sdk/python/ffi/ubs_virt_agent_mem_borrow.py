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
from ubse.models.ubs_virt_agent_model import BorrowParamT, MemBorrowResultT
from ubse.ffi.ubs_virt_agent_base import UbsVirtAgentBase
from ubse.ffi.ubs_virt_agent_types import MemBorrowParamC, MemBorrowResultC, NumaMetaInfoC


class UbsVirtAgentMemBorrow(UbsVirtAgentBase):
    """Memory borrow execution interface (large memory VM)"""

    def __init__(self):
        super().__init__()
        self._setup_mem_borrow_functions()

    def ubs_mem_borrow(self, param: BorrowParamT, is_async: bool = False) -> List[MemBorrowResultT]:
        """
        Execute memory borrow operation

        Borrow memory from target node according to specified borrow parameters, supporting both synchronous and asynchronous modes.
        In asynchronous mode, a task ID will be returned, which can be queried using ubs_task_result_query.

        Args:
            param (BorrowParamT): Memory borrow parameters, containing:
                - node_id: Source node ID
                - numa_meta_infos: NUMA meta information list (socket_id, numa_id)
                - borrow_size: Borrow size (MB)
            is_async (bool): Whether to execute asynchronously, default is False (synchronous)

        Returns:
            List[MemBorrowResultT]: Memory borrow result list, each element contains:
                - borrow_ids: Borrow ID list
                - present_numa_ids: Current NUMA ID list
                - task_id: Task ID (valid in asynchronous mode)

        Raises:
            ConnectionError: Raised when native library is not loaded
            TypeError: Raised when parameter type is incorrect
            Exception: Raised when borrow operation fails
        """
        if not self.lib_ubse:
            raise ConnectionError("Native library not loaded")

        if not isinstance(param, BorrowParamT):
            raise TypeError(f"param must be BorrowParamT, got {type(param).__name__}")

        borrow_param_c = self._convert_borrow_param_to_c(param)
        borrow_result_c = MemBorrowResultC()
        result = self.lib_ubse.ubs_virt_agent_mem_borrow(
            ctypes.byref(borrow_param_c),
            ctypes.c_bool(is_async),
            ctypes.byref(borrow_result_c)
        )

        self.ubs_virt_agent_handle_result(result, "ubs_mem_borrow")
        result_list = []

        try:
            if not borrow_result_c.mem_borrow_result_list or borrow_result_c.mem_borrow_result_list_len == 0:
                return result_list

            for i in range(borrow_result_c.mem_borrow_result_list_len):
                mem_borrow_result_t = MemBorrowResultT.from_c_struct(borrow_result_c.mem_borrow_result_list[i])
                result_list.append(mem_borrow_result_t)

            return result_list
        finally:
            if hasattr(self.lib_ubse, 'free'):
                borrow_result_c.free_c_memory(self.lib_ubse.free)

    @staticmethod
    def _convert_borrow_param_to_c(param: BorrowParamT) -> MemBorrowParamC:
        """
        Convert Python borrow parameters to C structure

        Args:
            param (BorrowParamT): Python borrow parameters

        Returns:
            MemBorrowParamC: C structure borrow parameters
        """
        borrow_param_c = MemBorrowParamC()

        borrow_param_c.src_nid = param.node_id.encode('utf-8')
        borrow_param_c.borrow_size = param.borrow_size
        borrow_param_c.numa_len = len(param.numa_meta_infos)

        if param.numa_meta_infos:
            numa_meta_array = (NumaMetaInfoC * len(param.numa_meta_infos))()
            for i, numa_meta in enumerate(param.numa_meta_infos):
                numa_meta_array[i].numa_id = numa_meta.numa_id
            borrow_param_c.numa_meta_infos = ctypes.cast(
                numa_meta_array,
                ctypes.POINTER(NumaMetaInfoC)
            )
        else:
            borrow_param_c.numa_meta_infos = None

        return borrow_param_c

    def _setup_mem_borrow_functions(self):
        """Setup function prototypes for memory borrow"""
        self.lib_ubse.ubs_virt_agent_mem_borrow.argtypes = [
            ctypes.POINTER(MemBorrowParamC),
            ctypes.c_bool,
            ctypes.POINTER(MemBorrowResultC)
        ]
        self.lib_ubse.ubs_virt_agent_mem_borrow.restype = ctypes.c_int32

        if hasattr(self.lib_ubse, 'free'):
            self.lib_ubse.free.argtypes = [ctypes.c_void_p]
            self.lib_ubse.free.restype = None