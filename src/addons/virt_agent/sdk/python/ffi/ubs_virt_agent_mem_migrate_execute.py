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
from typing import Dict, Any
from ubse.ffi.ubs_virt_agent_base import UbsVirtAgentBase
from ubse.ffi.ubs_virt_agent_types import (VmStrategyInfo, MigrateExecuteSrcParam, VIRT_MAX_NODE_ID_LENGTH,
                                           MAX_BORROW_ID_COUNT, MAX_BORROW_ID_LENGTH, MAX_VM_NUM)
from ubse.ffi.ubs_virt_agent_exceptions import UbsVAInvalidParamError


class UbsVirtAgentMemMigrateExecute(UbsVirtAgentBase):

    def __init__(self):
        super().__init__()
        self._setup_topo_functions()

    def get_migrate_execute_src_param(self, param: Dict) -> Any:
        try:
            borrow_in_node = param.get("borrowInNode")
            if len(borrow_in_node.encode('utf-8')) >= VIRT_MAX_NODE_ID_LENGTH:
                raise ValueError(f"borrowInNode '{borrow_in_node}' exceeds or equals maximum length "
                                 f"of {VIRT_MAX_NODE_ID_LENGTH}")
            src_param = MigrateExecuteSrcParam()
            src_param.borrowInNode = borrow_in_node.encode('utf-8') + b'\0'

            borrow_ids = param.get("borrowIds")
            borrow_ids_count = len(borrow_ids)
            if borrow_ids_count > MAX_BORROW_ID_COUNT:
                raise ValueError(f"borrowIds exceeds maximum count of {MAX_BORROW_ID_COUNT}")
            for i, borrow_id in enumerate(borrow_ids):
                if len(borrow_id.encode('utf-8')) >= MAX_BORROW_ID_LENGTH:
                    raise ValueError(f"borrowId '{borrow_id}' exceeds or equals maximum length "
                                     f"of {MAX_BORROW_ID_LENGTH}")
                src_param.borrowIds[i] = ctypes.create_string_buffer(borrow_id.encode('utf-8'), MAX_BORROW_ID_LENGTH)
            src_param.borrowIdsCount = ctypes.c_uint32(borrow_ids_count)

            vm_info_list = param.get("vmInfoList")
            vm_info_count = len(vm_info_list)
            if vm_info_count > MAX_VM_NUM:
                raise ValueError(f"vmInfoList exceeds maximum count of {MAX_VM_NUM}")
            for i, vm_info in enumerate(vm_info_list):
                src_param.vmInfoList[i].destNumaId = vm_info.get("destNumaId")
                src_param.vmInfoList[i].memSize = vm_info.get("memSize")
                src_param.vmInfoList[i].pid = vm_info.get("pid")
            src_param.vmInfoListSize = ctypes.c_uint32(vm_info_count)

            src_param.waiting_time = ctypes.c_uint64(param.get("waitingTime"))

            return src_param
        except Exception as e:
            msg = f"Error: Missing required key in param: {e}"
            raise RuntimeError(msg) from e

    def ubs_mem_migrate_execute(self, param: Dict[str, Any]) -> int:
        if not self.lib_ubse:
            raise ConnectionError("Native library not loaded")
        src_param = self.get_migrate_execute_src_param(param)
        if src_param is None:
            raise UbsVAInvalidParamError("Get src_param from input is null")
        result = self.lib_ubse.ubs_virt_agent_mem_migrate_execute(
            ctypes.byref(src_param)
        )
        self.ubs_virt_agent_handle_result(result, "ubs_mem_migrate_execute")
        return result

    def _setup_topo_functions(self):
        self.lib_ubse.ubs_virt_agent_mem_migrate_execute.argtypes = [
            ctypes.POINTER(MigrateExecuteSrcParam)
        ]
        self.lib_ubse.ubs_virt_agent_mem_migrate_execute.restype = ctypes.c_int32

        if hasattr(self.lib_ubse, 'free'):
            self.lib_ubse.free.argtypes = [ctypes.c_void_p]
            self.lib_ubse.free.restype = None
