#!/usr/bin/python3
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MatrixEngine is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
import ctypes
from typing import List, Optional
from ubse.models.ubs_engine_model_mem import UbsMemNumaDesc
from ubse.ffi.ubs_engine_exceptions import UbsError, UbsErrNullPointer, UbsEngineConnectionError, UbsEngineAuthError, \
    UbsEngineTimeoutError, UbsEngineInternalError, UbsEngineOutOfRangeError, UbsEngineNotExistError
from ubse.ffi.ubs_engine_types import UbsMemNumaDescT, UbsTopoNodeT
from ubse.ffi.ubs_engine_binding_base import UbsEngineBindingBase


class UbsEngineBindingMem(UbsEngineBindingBase):
    """mem相关接口"""

    def __init__(self):
        super().__init__()
        self.setup_function_prototypes()

    def ubs_handle_mem_result(self, result: int, operation: str):
        error_map = {
            3: ("UBS_ERR_NULL_POINTER", UbsErrNullPointer),
            1000: ("UBS_ENGINE_ERR_OUT_OF_RANGE", UbsEngineOutOfRangeError),
            1002: ("UBS_ENGINE_ERR_CONNECTION_FAILED", UbsEngineConnectionError),
            1003: ("UBS_ENGINE_ERR_AUTH_FAILED", UbsEngineAuthError),
            1004: ("UBS_ENGINE_ERR_TIMEOUT", UbsEngineTimeoutError),
            1005: ("UBS_ENGINE_ERR_INTERNAL", UbsEngineInternalError),
            1007: ("UBS_ENGINE_ERR_NOT_EXIST", UbsEngineNotExistError)
            # 添加更多错误码映射
        }
        if result != 0:
            error_info = error_map.get(result, (f"error: {result}", UbsError))
            raise error_info[1](f"{operation} failed: {error_info[0]}")

    def setup_function_prototypes(self):
        """设置C函数的参数和返回类型"""
        if not self.lib_ubse:
            return

        # 设置ubs_mem_numa_list的函数原型
        self.lib_ubse.ubs_mem_numa_list.argtypes = [
            ctypes.POINTER(ctypes.POINTER(UbsMemNumaDescT)),  # numa_descs
            ctypes.POINTER(ctypes.c_uint32)  # numa_desc_cnt
        ]
        self.lib_ubse.ubs_mem_numa_list.restype = ctypes.c_int32

        # 设置其他函数的原型
        self.lib_ubse.ubs_topo_node_list.argtypes = [
            ctypes.POINTER(ctypes.POINTER(UbsTopoNodeT)),  # node_list
            ctypes.POINTER(ctypes.c_uint32)  # node_cnt
        ]
        self.lib_ubse.ubs_topo_node_list.restype = ctypes.c_int32

        self.lib_ubse.ubs_topo_node_local_get.argtypes = [
            ctypes.POINTER(UbsTopoNodeT)  # node
        ]
        self.lib_ubse.ubs_topo_node_local_get.restype = ctypes.c_int32

        # 设置free函数的原型（如果存在）
        if hasattr(self.lib_ubse, 'free'):
            self.lib_ubse.free.argtypes = [ctypes.c_void_p]
            self.lib_ubse.free.restype = None

    def ubs_mem_numa_list(self) -> List[UbsMemNumaDesc]:
        """查询本节点所有numa形态远端内存"""
        if not self.lib_ubse:
            raise ConnectionError("Native library not loaded")

        numa_descs_ptr = ctypes.POINTER(UbsMemNumaDescT)()
        numa_desc_cnt = ctypes.c_uint32(0)

        try:
            result = self.lib_ubse.ubs_mem_numa_list(
                ctypes.byref(numa_descs_ptr),
                ctypes.byref(numa_desc_cnt)
            )
            self.ubs_handle_mem_result(result, "ubs_mem_numa_list")

            return self._convert_numa_descs(numa_descs_ptr, numa_desc_cnt.value)

        except Exception as e:
            print(f"调用拓扑接口失败: ubs_mem_numa_list failed: {e}")
            raise
        finally:
            self._safe_free_numa_descs(numa_descs_ptr)

    def _convert_numa_descs(self, numa_descs_ptr, desc_count) -> List[UbsMemNumaDesc]:
        """转换NUMA描述符数据"""
        if not numa_descs_ptr or desc_count == 0:
            return []

        numa_descs = []
        for i in range(desc_count):
            desc_ptr = ctypes.cast(
                ctypes.addressof(numa_descs_ptr.contents) + i * ctypes.sizeof(UbsMemNumaDescT),
                ctypes.POINTER(UbsMemNumaDescT)
            )
            numa_descs.append(UbsMemNumaDesc.from_c_struct(desc_ptr.contents))

        return numa_descs

    def _safe_free_numa_descs(self, numa_descs_ptr):
        """安全释放NUMA描述符内存"""
        if not numa_descs_ptr:
            return

        try:
            if hasattr(self.lib_ubse, 'free'):
                self.lib_ubse.free(numa_descs_ptr)
        except Exception as e:
            print(f"Warning: Failed to free numa desc memory: {e}")
