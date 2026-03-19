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
from ubse.ffi.ubs_virt_agent_types import TaskInfo
from ubse.models.ubs_virt_agent_model import BorrowExecuteResT, TaskInfoT


class UbsVirtAgentTaskQuery(UbsVirtAgentBase):

    def __init__(self):
        super().__init__()
        self._setup_task_query_functions()

    def task_query(self, task_id: str) -> Tuple[int, TaskInfoT]:
        """
        Calls the C function ubs_virt_agent_sync_task_query

        Args:
            task_id (str): Task id

        Returns:
            int: Call result, 0 indicates success, non-zero indicates failure
            TaskInfoT: Task info.
        """
        if not self.lib_ubse:
            raise ConnectionError("Native library not loaded")
        if not isinstance(task_id, str):
            raise TypeError(f"task_id must be a string, got {type(task_id).__name__}: {task_id}")
        task_id_bytes = task_id.encode('utf-8')
        task_id_c = ctypes.c_char_p(task_id_bytes)
        task_id_len = ctypes.c_uint32(len(task_id_bytes))
        task_info = TaskInfo()
        result = self.lib_ubse.ubs_virt_agent_sync_task_query(task_id_c, task_id_len, ctypes.byref(task_info))
        self.ubs_virt_agent_handle_result(result, "task_query")
        task_result = TaskInfoT.from_c_struct(task_info)

        return result, task_result

    def _setup_task_query_functions(self):
        self.lib_ubse.ubs_virt_agent_sync_task_query.argtypes = [
            ctypes.c_char_p,
            ctypes.c_uint32,
            ctypes.POINTER(TaskInfo)
        ]
        self.lib_ubse.ubs_virt_agent_sync_task_query.restype = ctypes.c_int32
