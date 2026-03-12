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

from ubse.models.ubs_virt_agent_model import NodeVmInfoT, VmInfoT
from ubse.ffi.ubs_virt_agent_base import UbsVirtAgentBase
from ubse.ffi.ubs_virt_agent_types import VmInfo


class UbsVirtAgentVmInfo(UbsVirtAgentBase):

    def __init__(self):
        super().__init__()
        self._setup_vm_info_functions()

    def ubs_vm_info_list(self) -> NodeVmInfoT:
        if not self.lib_ubse:
            raise ConnectionError("Native library not loaded")
        try:
            vm_info_ptr = ctypes.POINTER(VmInfo)()
            info_count = ctypes.c_uint32(0)
            result = self.lib_ubse.ubs_virt_agent_mem_fragmentation_vm_info(
                ctypes.byref(vm_info_ptr),
                ctypes.byref(info_count)
            )

            self.ubs_virt_agent_handle_result(result, "ubs_vm_info_list")
            if not vm_info_ptr or info_count.value == 0:
                return NodeVmInfoT(
                    vm_infos=[]
                )

            vm_info_cnt = info_count.value
            vm_info_list = vm_info_ptr[:vm_info_cnt]

            vm_info_t_list = []

            for i in range(vm_info_cnt):
                vm_info_c = vm_info_list[i]
                vm_info_t = VmInfoT.from_c_struct(vm_info_c)
                vm_info_t_list.append(vm_info_t)

            return NodeVmInfoT(
                vm_infos=vm_info_t_list
            )
        finally:
            if vm_info_ptr:
                for i in range(info_count.value):
                    vm_info_c = vm_info_list[i]
                    self._safe_free_memory(vm_info_c.numaInfo)
                self.lib_ubse.free(vm_info_ptr)

    def _setup_vm_info_functions(self):
        """Set up prototypes for log-related functions"""
        self.lib_ubse.ubs_virt_agent_mem_fragmentation_vm_info.argtypes = [
            ctypes.POINTER(ctypes.POINTER(VmInfo)),
            ctypes.POINTER(ctypes.c_uint32)
        ]
        self.lib_ubse.ubs_virt_agent_mem_fragmentation_vm_info.restype = ctypes.c_int32

        # Set up the free function (if it exists)
        if hasattr(self.lib_ubse, 'free'):
            self.lib_ubse.free.argtypes = [ctypes.c_void_p]
            self.lib_ubse.free.restype = None

    def _safe_free_memory(self, ptr):
        if ptr is not None:
            self.lib_ubse.free(ptr)