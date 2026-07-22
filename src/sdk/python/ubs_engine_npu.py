#!/usr/bin/python3
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
# MatrixEngine is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#       http: // license.coscl.org.cn / MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
from typing import List, Tuple

from ubse.ffi.ubs_engine_binding_npu import UbsEngineBindingNpu
from ubse.models.ubs_engine_model_npu import DeviceInfo

_npu_interface = UbsEngineBindingNpu()


def get_host_ub_devices() -> List[DeviceInfo]:
    """
    获取主机上的UB设备列表
    
    Returns:
        List[DeviceInfo]: 设备信息对象列表，包含NIC、NPU、BUSI、UBCTRL等设备
    Raises:
        ConnectionError: 本地库未加载
        RuntimeError: 查询失败
        Exception: 其他未知错误
    """
    return _npu_interface.ubs_device_list()


def alloc_devices(upi, bus_guid, device_list) -> Tuple[str, List[DeviceInfo]]:
    """
    分配UB设备
    
    Returns:
        Tuple[str, List[DeviceInfo]]: (新的总线实例GUID, 分配的设备信息对象列表)
    Raises:
        ConnectionError: 本地库未加载
        RuntimeError: 绑定失败
        Exception: 其他未知错误
    """
    return _npu_interface.ubs_device_alloc(upi, bus_guid, device_list)


def free_devices(bus_instance_guid, device_list) -> None:
    """
    释放UB设备
    
    Args:
        bus_instance_guid: 总线实例GUID
        device_list: 要释放的设备列表
    
    Returns:
        None
    
    Raises:
        ConnectionError: 本地库未加载
        RuntimeError: 释放设备失败
        Exception: 其他未知错误
    """
    return _npu_interface.ubs_device_free(bus_instance_guid, device_list)


def query_uba_tid_size(bus_instance_guid) -> Tuple[int, int, int]:
    """
    查询UBA TID大小
    
    Args:
        bus_instance_guid: 总线实例GUID
    
    Returns:
        Tuple[int, int, int]: (tid, uba, size)
    
    Raises:
        ConnectionError: 本地库未加载
        RuntimeError: 查询失败
        Exception: 其他未知错误
    """
    return _npu_interface.ubs_query_uba_tid_size(bus_instance_guid)
