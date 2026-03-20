# !/usr/bin/python3
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
from typing import List

from ubse.models.ubs_engine_model_topo import UbsTopoNode, UbsTopoLink
from ubse.ffi.ubs_engine_exceptions import UbsError, UbsErrNullPointer, UbsEngineConnectionError, UbsEngineAuthError, \
    UbsEngineTimeoutError, UbsEngineInternalError
from ubse.ffi.ubs_engine_binding_base import UbsEngineBindingBase
from ubse.ffi.ubs_engine_types import UbsTopoNodeT, UbsTopoLinkT


class UbsEngineBindingTopology(UbsEngineBindingBase):
    """拓扑相关接口"""

    def __init__(self):
        super().__init__()
        self._setup_topo_functions()

    def ubs_handle_topo_result(self, result: int, operation: str):
        error_map = {
            3: ("UBS_ERR_NULL_POINTER", UbsErrNullPointer),
            1002: ("UBS_ENGINE_ERR_CONNECTION_FAILED", UbsEngineConnectionError),
            1003: ("UBS_ENGINE_ERR_AUTH_FAILED", UbsEngineAuthError),
            1004: ("UBS_ENGINE_ERR_TIMEOUT", UbsEngineTimeoutError),
            1005: ("UBS_ENGINE_ERR_INTERNAL", UbsEngineInternalError)
            # 添加更多错误码映射
        }
        if result != 0:
            error_info = error_map.get(result, (f"error: {result}", UbsError))
            raise error_info[1](f"{operation} failed: {error_info[0]}")

    def ubs_topo_node_list(self) -> List[UbsTopoNode]:
        """查询全量节点信息"""
        if not self.lib_ubse:
            raise ConnectionError("Native library not loaded")

        node_list_ptr = ctypes.POINTER(UbsTopoNodeT)()
        node_count = ctypes.c_uint32(0)

        try:
            result = self.lib_ubse.ubs_topo_node_list(
                ctypes.byref(node_list_ptr),
                ctypes.byref(node_count)
            )

            # 处理返回码
            self.ubs_handle_topo_result(result, "ubs_topo_node_list")

            return self.convert_node_list(node_list_ptr, node_count.value)

        finally:
            self.safe_free_node_list(node_list_ptr)

    def convert_node_list(self, node_list_ptr, node_count) -> List[UbsTopoNode]:
        """转换节点列表数据"""
        if not node_list_ptr or node_count == 0:
            return []

        # 使用ctypes数组方式简化访问
        array_type = (UbsTopoNodeT * node_count)
        node_list_array = ctypes.cast(node_list_ptr, ctypes.POINTER(array_type)).contents

        # 使用列表推导式简化转换逻辑
        return [UbsTopoNode.from_c_struct(node) for node in node_list_array]

    def safe_free_node_list(self, node_list_ptr):
        """安全释放节点列表内存"""
        if not node_list_ptr:
            return

        try:
            if hasattr(self.lib_ubse, 'free'):
                self.lib_ubse.free(node_list_ptr)
        except Exception as e:
            print(f"Warning: Failed to free node list memory: {e}")

    def ubs_topo_node_local_get(self) -> UbsTopoNode:
        """查询本节点信息"""
        if not self.lib_ubse:
            raise ConnectionError("Native library not loaded")

        # 分配一个ubs_topo_node_t结构体实例（栈上分配）
        local_node = UbsTopoNodeT()

        try:
            result = self.lib_ubse.ubs_topo_node_local_get(
                ctypes.byref(local_node)
            )

            # 处理返回码
            self.ubs_handle_topo_result(result, "ubs_topo_node_local_get")

            # 转换为Python模型并返回
            return UbsTopoNode.from_c_struct(local_node)

        except Exception as e:
            print(f"Error getting local node info: {e}")
            raise

    def ubs_topo_link_list(self) -> List[UbsTopoLink]:
        """查询节点CPU拓扑连接列表"""
        if not self.lib_ubse:
            raise ConnectionError("Native library not loaded")

        cpu_links_ptr = ctypes.POINTER(UbsTopoLinkT)()
        link_count = ctypes.c_uint32(0)
        try:
            result = self.lib_ubse.ubs_topo_link_list(
                ctypes.byref(cpu_links_ptr),
                ctypes.byref(link_count)
            )

            # 处理返回码
            self.ubs_handle_topo_result(result, "ubs_topo_link_list")
            if not cpu_links_ptr or link_count.value == 0:
                return []
            # 在释放内存前完成所有数据复制
            link_models = []
            for i in range(link_count.value):
                link_ptr = ctypes.cast(
                    ctypes.addressof(cpu_links_ptr.contents) + i * ctypes.sizeof(UbsTopoLinkT),
                    ctypes.POINTER(UbsTopoLinkT)
                )
                # 直接转换为Python模型
                link_models.append(UbsTopoLink.from_c_struct(link_ptr.contents))

            return link_models
        finally:
            if cpu_links_ptr:
                try:
                    self.lib_ubse.free(cpu_links_ptr)
                except Exception as e:
                    print(f"Warning: Failed to free topology memory: {e}")

    def _setup_topo_functions(self):
        """设置相关原型"""
        self.lib_ubse.ubs_topo_link_list.argtypes = [
            ctypes.POINTER(ctypes.POINTER(UbsTopoLinkT)),
            ctypes.POINTER(ctypes.c_uint32)
        ]
        self.lib_ubse.ubs_topo_link_list.restype = ctypes.c_int32

        # 设置free函数（如果存在）
        if hasattr(self.lib_ubse, 'free'):
            self.lib_ubse.free.argtypes = [ctypes.c_void_p]
            self.lib_ubse.free.restype = None
