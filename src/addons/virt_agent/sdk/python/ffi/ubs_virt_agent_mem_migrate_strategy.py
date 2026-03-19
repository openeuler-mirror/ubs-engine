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
from ubse.models.ubs_virt_agent_model import VmMigrateStrategyT, MemMigrateStrategyT
from ubse.ffi.ubs_virt_agent_base import UbsVirtAgentBase
from ubse.ffi.ubs_virt_agent_types import (MigrateStrategyVmInfo, MigrateStrategySrcParam, VmMigrateStrategy,
                                           MemMigrateStrategy, VIRT_MAX_NODE_ID_LENGTH, MAX_VM_NUM)
from ubse.ffi.ubs_virt_agent_exceptions import UbsVANullPointerError, UbsVAInvalidParamError


class UbsVirtAgentMemMigrateStrategy(UbsVirtAgentBase):

    def __init__(self):
        super().__init__()
        self._setup_mem_migrate_strategy_functions()

    def get_migrate_strategy_src_param(self, param: Any) -> Any:
        try:
            src_param = MigrateStrategySrcParam()

            src_param.borrowSize = ctypes.c_uint64(param.get("borrowSize"))

            borrow_in_node = param.get("borrowInNode")
            if len(borrow_in_node.encode('utf-8')) >= VIRT_MAX_NODE_ID_LENGTH:
                raise ValueError(f"borrowInNode '{borrow_in_node}' exceeds or equals maximum length "
                                 f"of {VIRT_MAX_NODE_ID_LENGTH}")
            src_param.borrowInNode = borrow_in_node.encode('utf-8') + b'\0'

            vm_info_list = param.get("vmInfoList")
            vm_info_count = len(vm_info_list)
            if vm_info_count > MAX_VM_NUM:
                raise ValueError(f"vmInfoList exceeds maximum count of {MAX_VM_NUM}")
            for i, vm_info in enumerate(vm_info_list):
                src_param.vmInfoList[i].pid = vm_info.get("pid")
                src_param.vmInfoList[i].ratio = vm_info.get("ratio")
            src_param.vmInfoListSize = ctypes.c_uint32(vm_info_count)

            return src_param
        except Exception as e:
            raise RuntimeError(f"Error: Missing required key in param: {e}") from e

    def ubs_mem_migrate_strategy(self, param: Dict[str, Any]) -> MemMigrateStrategyT:
        try:
            if not self.lib_ubse:
                raise ConnectionError("Native library not loaded")
            src_param = self.get_migrate_strategy_src_param(param)
            if src_param is None:
                raise UbsVAInvalidParamError("get src_param from input is null")
            mem_migrate_strategy_ptr = MemMigrateStrategy()
            result = self.lib_ubse.ubs_virt_agent_mem_migrate_strategy(
                ctypes.byref(src_param),
                ctypes.byref(mem_migrate_strategy_ptr)
            )
            self.ubs_virt_agent_handle_result(result, "ubs_mem_migrate_strategy")
            if not mem_migrate_strategy_ptr:
                raise UbsVANullPointerError("mem_migrate_strategy_ptr is null")
            # Convert C structure to Python data types
            vm_info_list_size = mem_migrate_strategy_ptr.vm_info_list_size
            vm_info_list = []
            for i in range(vm_info_list_size):
                vm_migrate_strategy = mem_migrate_strategy_ptr.vm_info_list[i]
                vm_info_list.append(VmMigrateStrategyT(
                    dest_numa_id=vm_migrate_strategy.dest_numa_id,
                    mem_size=vm_migrate_strategy.mem_size,
                    pid=vm_migrate_strategy.pid
                ))
            waiting_time = mem_migrate_strategy_ptr.waiting_time

            # Create MemMigrateStrategyT object
            migrate_strategy = MemMigrateStrategyT(
                vm_info_list=vm_info_list,
                waiting_time=waiting_time
            )
            return migrate_strategy
        finally:
            if ('mem_migrate_strategy_ptr' in locals() and hasattr(mem_migrate_strategy_ptr, 'vm_info_list') and
                    mem_migrate_strategy_ptr.vm_info_list is not None):
                self.lib_ubse.free(mem_migrate_strategy_ptr.vm_info_list)

    def _setup_mem_migrate_strategy_functions(self):
        self.lib_ubse.ubs_virt_agent_mem_migrate_strategy.argtypes = [
            ctypes.POINTER(MigrateStrategySrcParam),
            ctypes.POINTER(MemMigrateStrategy)
        ]
        self.lib_ubse.ubs_virt_agent_mem_migrate_strategy.restype = ctypes.c_int32

        if hasattr(self.lib_ubse, 'free'):
            self.lib_ubse.free.argtypes = [ctypes.c_void_p]
            self.lib_ubse.free.restype = None
