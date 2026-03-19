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
from typing import Optional, Dict, List, Union

from ubse.ffi.ubs_virt_agent_base import UbsVirtAgentBase
from ubse.ffi.ubs_virt_agent_types import (RollbackSrcParam, VIRT_MAX_NODE_ID_LENGTH, MAX_BORROW_ID_COUNT,
                                           MAX_BORROW_ID_LENGTH)


class UbsVirtAgentMemRollback(UbsVirtAgentBase):

    def __init__(self):
        super().__init__()
        self._setup_mem_rollback_functions()

    def ubs_mem_rollback(self, node_id: str, borrow_id_list: List[str]) -> int:
        """
        Calls the C++ function ubs_virt_agent_mem_rollback

        Args:
            node_id (str): The node ID to perform memory rollback operation
            borrow_id_list (List[str]): List of borrow IDs to rollback

        Returns:
            int: Call result, 0 indicates success, non-zero indicates failure
        """
        if not self.lib_ubse:
            raise ConnectionError("Native library not loaded")
        if not isinstance(node_id, str):
            raise TypeError("node_id must be a string")
        if not isinstance(borrow_id_list, list):
            raise TypeError("borrow_id_list must be a list of strings")

        param = RollbackSrcParam()

        if len(node_id.encode('utf-8')) >= VIRT_MAX_NODE_ID_LENGTH:
            raise ValueError(f"node_id exceeds or equals maximum length of {VIRT_MAX_NODE_ID_LENGTH}")
        param.node_id = node_id.encode('utf-8') + b'\0'

        borrow_id_count = len(borrow_id_list)
        if borrow_id_count > MAX_BORROW_ID_COUNT:
            raise ValueError(f"borrow_id_list exceeds maximum count of {MAX_BORROW_ID_COUNT}")
        for i, borrow_id in enumerate(borrow_id_list):
            if len(borrow_id.encode('utf-8')) >= MAX_BORROW_ID_LENGTH:
                raise ValueError(f"borrow_id '{borrow_id}' exceeds or equals maximum length of {MAX_BORROW_ID_LENGTH}")
            param.borrow_id_list[i] = ctypes.create_string_buffer(borrow_id.encode('utf-8'), MAX_BORROW_ID_LENGTH)

        param.borrow_id_size = ctypes.c_uint32(borrow_id_count)
        result = self.lib_ubse.ubs_virt_agent_mem_rollback(ctypes.byref(param))
        self.ubs_virt_agent_handle_result(result, "ubs_mem_rollback")
        return result

    def _setup_mem_rollback_functions(self):
        self.lib_ubse.ubs_virt_agent_mem_rollback.argtypes = [
            ctypes.POINTER(RollbackSrcParam)
        ]
        self.lib_ubse.ubs_virt_agent_mem_rollback.restype = ctypes.c_int32
