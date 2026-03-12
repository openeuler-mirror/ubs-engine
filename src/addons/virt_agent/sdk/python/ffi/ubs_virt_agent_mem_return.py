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

import ctypes
from typing import Tuple
from ubse.ffi.ubs_virt_agent_base import UbsVirtAgentBase


class UbsVirtAgentMemReturn(UbsVirtAgentBase):

    def __init__(self):
        super().__init__()
        self._setup_mem_return_functions()

    def ubs_mem_return(self, is_async: bool = False) -> Tuple[int, str]:
        """
        Calls the C++ function ubs_virt_agent_mem_return

        Args:
            is_async (bool): Synchronous or asynchronous invocation

        Returns:
            int: Call result, 0 indicates success, non-zero indicates failure
            str: If the input parameter is_async=True, the task ID is returned. Otherwise, the value is an empty string.
        """
        if not self.lib_ubse:
            raise ConnectionError("Native library not loaded")
        if not isinstance(is_async, bool):
            raise TypeError("is_async must be a bool")

        is_async_c = ctypes.c_bool(is_async)
        task_id_ptr = ctypes.POINTER(ctypes.c_char)()
        task_id_len = ctypes.c_uint32(0)
        result = self.lib_ubse.ubs_virt_agent_mem_return(is_async_c,
                                                         ctypes.byref(task_id_ptr), ctypes.byref(task_id_len))
        self.ubs_virt_agent_handle_result(result, "ubs_mem_return")
        task_id_str = ""
        if result == 0:
            task_id_str = ctypes.string_at(task_id_ptr).decode('utf-8')

        self.lib_ubse.free(ctypes.cast(task_id_ptr, ctypes.c_void_p))
        return result, task_id_str


    def _setup_mem_return_functions(self):
        self.lib_ubse.ubs_virt_agent_mem_return.argtypes = [
            ctypes.c_bool,
            ctypes.POINTER(ctypes.POINTER(ctypes.c_char)),
            ctypes.POINTER(ctypes.c_uint32)
        ]
        self.lib_ubse.ubs_virt_agent_mem_return.restype = ctypes.c_int32

        # Set up the free function (if it exists)
        if hasattr(self.lib_ubse, 'free'):
            self.lib_ubse.free.argtypes = [ctypes.c_void_p]
            self.lib_ubse.free.restype = None