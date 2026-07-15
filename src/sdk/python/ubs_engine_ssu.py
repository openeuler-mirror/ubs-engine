#!/usr/bin/python3
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
# ubs-engine is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
"""SSU Python SDK。

通过IPC直接与UBSE守护进程通信，提供存储空间分配、释放、挂载、卸载等功能。
所有函数遵循标准调用流程: 参数校验 -> 请求数据打包 -> 发送IPC请求 -> 响应数据解包 -> 返回结果。
"""
from typing import List, Optional

from ubse.ipc.ubs_engine_ipc import invoke_call
from ubse.models.ubs_engine_model_ssu import (
    UBS_SSU_MAX_NAME_LENGTH, UBS_SSU_MAX_NQN_LENGTH, UBS_SSU_GUID_LENGTH,
    UbsSsuAllocSpaceReq, UbsSsuSpaceReq, UbsSsuLinearSpaceReq, UbsSsuStripedSpaceReq,
    UbsSsuAllocResult, UbsSsuConnectInfo, UbsSsuNsStats,
    UbsUbVfe, UbsUbFe,
)
from ubs_engine_codes_ssu import (
    UBSE_MODULE_CODE,
    OP_ALLOC_REQ, OP_FREE_REQ,
    OP_LIST_ALLOC_INFO_REQ, OP_GET_NS_STATS_REQ, OP_GET_CONNECT_INFO_REQ,
    OP_ADD_ACCESS_PERMISSION_REQ, OP_REMOVE_ACCESS_PERMISSION_REQ,
    OP_ATTACH_SPACE_REQ, OP_DETACH_SPACE_REQ,
    OP_ATTACH_LINEAR_SPACE_REQ, OP_DETACH_LINEAR_SPACE_REQ,
    OP_ATTACH_STRIPED_SPACE_REQ, OP_DETACH_STRIPED_SPACE_REQ,
    OP_GET_FE_DEVICE_LIST_REQ, OP_FE_DEVICE_ALLOC_REQ, OP_FE_DEVICE_FREE_REQ,
)
from ubse.ffi.ubs_engine_binding_ssu import (
    pack_string, pack_connect_info_req,
    pack_alloc_space_req, pack_space_req,
    pack_linear_space_req, pack_striped_space_req,
    pack_fe_device_req,
    unpack_alloc_result, unpack_alloc_result_list,
    unpack_dev_path_response,
    unpack_ns_stats_list, unpack_connect_info_list,
    unpack_fe_device_list, validate_name, validate_dev_name,
    validate_nqn, validate_fe_device_alloc_params, validate_fe_device_free_params, validate_striped_space_req
)


# ====================== 主函数（核心业务逻辑） ======================

def ubs_ssu_alloc_info_list() -> List[UbsSsuAllocResult]:
    """列出所有已分配的存储空间信息

    获取系统中所有已分配的SSU存储空间详细信息, 包括命名空间列表、容量、LBA格式和使用类型等。

    Returns:
        已分配空间信息列表

    Raises:
        UbsEngineConnectionError: 连接UBSE服务端失败
        UbsEngineAuthError: UBSE服务端鉴权不通过
        UbsEngineTimeoutError: UBSE服务端处理超时
        UbsEngineInternalError: UBSE服务端内部错误
    """
    response = invoke_call(UBSE_MODULE_CODE, OP_LIST_ALLOC_INFO_REQ)
    return unpack_alloc_result_list(response)


def ubs_ssu_space_alloc(req: UbsSsuAllocSpaceReq) -> UbsSsuAllocResult:
    """分配SSU存储空间

    根据请求参数分配指定数量和大小的命名空间, 支持顺序分配和分布式分配两种策略。

    Args:
        req: 分配请求参数

    Returns:
        分配结果, 包含已分配的命名空间信息列表

    Raises:
        UbsErrInvalidArg: 参数校验错误
        UbsEngineConnectionError: 连接UBSE服务端失败
        UbsEngineAuthError: UBSE服务端鉴权不通过
        UbsEngineAllocateError: 算法分配失败
        UbsEngineTimeoutError: UBSE服务端处理超时
        UbsEngineInternalError: UBSE服务端内部错误

    Note:
        当ns_num为1时, strategy参数不生效
    """
    validate_alloc_space_req(req)
    request = pack_alloc_space_req(req)
    response = invoke_call(UBSE_MODULE_CODE, OP_ALLOC_REQ, request)
    return unpack_alloc_result(response)


def ubs_ssu_space_free(name: str) -> None:
    """释放已分配的存储空间

    释放之前通过ubs_ssu_space_alloc分配的存储空间及其关联的所有命名空间。

    Args:
        name: 要释放的存储空间标识

    Raises:
        UbsErrInvalidArg: 参数校验错误
        UbsEngineConnectionError: 连接UBSE服务端失败
        UbsEngineAuthError: UBSE服务端鉴权不通过
        UbsEngineTimeoutError: UBSE服务端处理超时
        UbsEngineInternalError: UBSE服务端内部错误

    Note:
        释放操作具有幂等性, 释放不存在的空间应返回成功
    """
    validate_name(name)
    request = pack_string(name, UBS_SSU_MAX_NAME_LENGTH)
    invoke_call(UBSE_MODULE_CODE, OP_FREE_REQ, request)


def ubs_ssu_access_permission_add(name: str, nqn: str) -> None:
    """添加存储空间访问权限

    为指定的Host授予对已分配存储空间的访问权限。

    Args:
        name: 存储空间标识
        nqn: Host的NVMe Qualified Name

    Raises:
        UbsErrInvalidArg: 参数校验错误 
        UbsEngineConnectionError: 连接UBSE服务端失败
        UbsEngineAuthError: UBSE服务端鉴权不通过
        UbsEngineTimeoutError: UBSE服务端处理超时
        UbsEngineInternalError: UBSE服务端内部错误

    Note:
        重复添加同一Host的访问权限应返回成功(幂等性保证)
    """
    validate_name(name)
    validate_nqn(nqn)
    buf = pack_string(name, UBS_SSU_MAX_NAME_LENGTH) + pack_string(nqn, UBS_SSU_MAX_NQN_LENGTH)
    invoke_call(UBSE_MODULE_CODE, OP_ADD_ACCESS_PERMISSION_REQ, buf)


def ubs_ssu_access_permission_remove(name: str, nqn: str) -> None:
    """移除存储空间访问权限

    撤销指定Host对已分配存储空间的访问权限。

    Args:
        name: 存储空间标识
        nqn: Host的NVMe Qualified Name

    Raises:
        UbsErrInvalidArg: 参数校验错误
        UbsEngineConnectionError: 连接UBSE服务端失败
        UbsEngineAuthError: UBSE服务端鉴权不通过
        UbsEngineTimeoutError: UBSE服务端处理超时
        UbsEngineInternalError: UBSE服务端内部错误

    Note:
        移除不存在的访问权限应返回成功(幂等性保证)
    """
    validate_name(name)
    validate_nqn(nqn)
    buf = pack_string(name, UBS_SSU_MAX_NAME_LENGTH) + pack_string(nqn, UBS_SSU_MAX_NQN_LENGTH)
    invoke_call(UBSE_MODULE_CODE, OP_REMOVE_ACCESS_PERMISSION_REQ, buf)


def ubs_ssu_space_attach(req: UbsSsuSpaceReq) -> str:
    """挂载已分配的存储空间

    将指定的存储空间挂载到系统, 使其可被主机访问。

    Args:
        req: 挂载请求参数, 包含存储空间标识、Host的NVMe Qualified Name和源EID

    Returns:
        挂载后的设备路径

    Raises:
        UbsErrInvalidArg: 参数校验错误
        UbsEngineConnectionError: 连接UBSE服务端失败
        UbsEngineAuthError: UBSE服务端鉴权不通过
        UbsEngineTimeoutError: UBSE服务端处理超时
        UbsEngineInternalError: UBSE服务端内部错误
    """
    validate_name(req.name)
    request = pack_space_req(req)
    response = invoke_call(UBSE_MODULE_CODE, OP_ATTACH_SPACE_REQ, request)
    return unpack_dev_path_response(response)


def ubs_ssu_space_detach(req: UbsSsuSpaceReq) -> None:
    """卸载已分配的存储空间

    将指定的存储空间从系统卸载, 释放设备占用。

    Args:
        req: 卸载请求参数, 包含存储空间标识、Host的NVMe Qualified Name和源EID

    Raises:
        UbsErrInvalidArg: 参数校验错误
        UbsEngineConnectionError: 连接UBSE服务端失败
        UbsEngineAuthError: UBSE服务端鉴权不通过
        UbsEngineTimeoutError: UBSE服务端处理超时
        UbsEngineInternalError: UBSE服务端内部错误

    Note:
        卸载前需确保没有进程正在使用该存储空间
    """
    validate_name(req.name)
    request = pack_space_req(req)
    invoke_call(UBSE_MODULE_CODE, OP_DETACH_SPACE_REQ, request)


def ubs_ssu_linear_space_attach(req: UbsSsuLinearSpaceReq) -> str:
    """挂载线性编址的存储空间

    将多个命名空间设备以线性拼接方式聚合为一个逻辑块设备并挂载。

    Args:
        req: 挂载请求参数, 包含存储空间标识、Host的NVMe Qualified Name、源EID和聚合后的块设备名称

    Returns:
        挂载后的聚合设备路径

    Raises:
        UbsErrInvalidArg: 参数校验错误
        UbsEngineConnectionError: 连接UBSE服务端失败
        UbsEngineAuthError: UBSE服务端鉴权不通过
        UbsEngineNotExistError: 存储空间不存在
        UbsEngineTimeoutError: UBSE服务端处理超时
        UbsEngineInternalError: UBSE服务端内部错误
    """
    validate_name(req.name)
    validate_dev_name(req.dev_name)
    request = pack_linear_space_req(req)
    response = invoke_call(UBSE_MODULE_CODE, OP_ATTACH_LINEAR_SPACE_REQ, request)
    return unpack_dev_path_response(response)


def ubs_ssu_linear_space_detach(req: UbsSsuLinearSpaceReq) -> None:
    """卸载线性编址的存储空间

    将线性聚合的块设备卸载并释放。

    Args:
        req: 卸载请求参数, 包含存储空间标识、Host的NVMe Qualified Name、源EID和聚合后的块设备名称

    Raises:
        UbsErrInvalidArg: 参数校验错误
        UbsEngineConnectionError: 连接UBSE服务端失败
        UbsEngineAuthError: UBSE服务端鉴权不通过
        UbsEngineNotExistError: 存储空间不存在
        UbsEngineTimeoutError: UBSE服务端处理超时
        UbsEngineInternalError: UBSE服务端内部错误
    """
    validate_name(req.name)
    validate_dev_name(req.dev_name)
    request = pack_linear_space_req(req)
    invoke_call(UBSE_MODULE_CODE, OP_DETACH_LINEAR_SPACE_REQ, request)


def ubs_ssu_striped_space_attach(req: UbsSsuStripedSpaceReq) -> str:
    """挂载条带化编址的存储空间

    将多个命名空间设备以条带化方式聚合为一个逻辑块设备并挂载, 支持RAID0和RAID5两种级别。

    Args:
        req: 条带化挂载请求参数, 包含存储空间标识、Host的NVMe Qualified Name、源EID、
             聚合后的块设备名称、RAID级别和chunk大小

    Returns:
        挂载后的聚合设备路径

    Raises:
        UbsErrInvalidArg: 参数校验错误
        UbsEngineConnectionError: 连接UBSE服务端失败
        UbsEngineAuthError: UBSE服务端鉴权不通过
        UbsEngineTimeoutError: UBSE服务端处理超时
        UbsEngineInternalError: UBSE服务端内部错误

    Note:
        RAID5至少需要3个成员设备(UBS_SSU_RAID5_MIN_MEMBER_NUM)
    """
    validate_striped_space_req(req)
    request = pack_striped_space_req(req)
    response = invoke_call(UBSE_MODULE_CODE, OP_ATTACH_STRIPED_SPACE_REQ, request)
    return unpack_dev_path_response(response)


def ubs_ssu_striped_space_detach(req: UbsSsuStripedSpaceReq) -> None:
    """卸载条带化编址的存储空间

    将条带化聚合的块设备卸载并释放。

    Args:
        req: 卸载请求参数, 包含存储空间标识、Host的NVMe Qualified Name、源EID和聚合后的块设备名称

    Raises:
        UbsErrInvalidArg: 参数校验错误
        UbsEngineConnectionError: 连接UBSE服务端失败
        UbsEngineAuthError: UBSE服务端鉴权不通过
        UbsEngineNotExistError: 存储空间不存在
        UbsEngineTimeoutError: UBSE服务端处理超时
        UbsEngineInternalError: UBSE服务端内部错误
    """
    validate_name(req.name)
    validate_dev_name(req.dev_name)
    request = pack_striped_space_req(req)
    invoke_call(UBSE_MODULE_CODE, OP_DETACH_STRIPED_SPACE_REQ, request)


def ubs_ssu_ns_stats_get(name: str) -> List[UbsSsuNsStats]:
    """获取存储空间的命名空间统计信息。

    查询指定存储空间下各命名空间的容量使用情况, 包括总容量和已用容量。

    Args:
        name: 存储空间标识

    Returns:
        命名空间统计信息列表

    Raises:
        UbsErrInvalidArg: 参数校验错误
        UbsEngineConnectionError: 连接UBSE服务端失败
        UbsEngineAuthError: UBSE服务端鉴权不通过
        UbsEngineNotExistError: 存储空间不存在
        UbsEngineTimeoutError: UBSE服务端处理超时
        UbsEngineInternalError: UBSE服务端内部错误
    """
    validate_name(name)
    request = pack_string(name, UBS_SSU_MAX_NAME_LENGTH)
    response = invoke_call(UBSE_MODULE_CODE, OP_GET_NS_STATS_REQ, request)
    return unpack_ns_stats_list(response)


def ubs_ssu_connect_info_get(name: str, vfe: Optional[UbsUbVfe] = None) -> List[UbsSsuConnectInfo]:
    """获取存储空间的连接信息。

    查询指定存储空间在指定VFE上的NVMe连接信息, 包括子系统NQN、Host NQN、命名空间ID等。

    Args:
        name: 存储空间标识
        vfe: VFE信息指针，指定查询的虚拟功能单元，传None时使用host侧分配给ssu的fe的eid

    Returns:
        连接信息列表

    Raises:
        UbsErrInvalidArg: 参数校验错误
        UbsEngineConnectionError: 连接UBSE服务端失败
        UbsEngineAuthError: UBSE服务端鉴权不通过
        UbsEngineNotExistError: 存储空间不存在
        UbsEngineTimeoutError: UBSE服务端处理超时
        UbsEngineInternalError: UBSE服务端内部错误
    """
    validate_name(name)
    request = pack_connect_info_req(name, vfe)
    response = invoke_call(UBSE_MODULE_CODE, OP_GET_CONNECT_INFO_REQ, request)
    return unpack_connect_info_list(response)


def ubs_ssu_fe_device_list() -> List[UbsUbFe]:
    """获取功能单元设备列表

    查询系统中所有FE设备信息, 包括每个PFE下的VFE列表

    Returns:
        FE设备信息列表

    Raises:
        UbsEngineConnectionError: 连接UBSE服务端失败
        UbsEngineAuthError: UBSE服务端鉴权不通过
        UbsEngineTimeoutError: UBSE服务端处理超时
        UbsEngineInternalError: UBSE服务端内部错误
    """
    response = invoke_call(UBSE_MODULE_CODE, OP_GET_FE_DEVICE_LIST_REQ)
    return unpack_fe_device_list(response)


def ubs_ssu_fe_device_alloc(upi: int, vfe: UbsUbVfe, guid: bytearray) -> None:
    """将VFE绑定到虚拟机

    将指定的虚拟功能单元绑定到目标虚拟机，使虚拟机可通过该VFE访问存储资源。

    Args:
        upi: 租户隔离标识
        vfe: 要绑定的VFE信息（UbsUbVfe 对象）
        guid: 输入输出参数，总线实例GUID，标识目标虚拟机。
              调用前应分配长度为 UBS_SSU_GUID_LENGTH 的 bytearray，
              函数将修改其内容。

    Raises:
        UbsErrInvalidArg: 参数校验错误
        UbsEngineConnectionError: 连接UBSE服务端失败
        UbsEngineAuthError: UBSE服务端鉴权不通过
        UbsEngineNotExistError: VFE或虚拟机不存在
        UbsEngineTimeoutError: UBSE服务端处理超时
        UbsEngineInternalError: UBSE服务端内部错误
    """
    validate_fe_device_alloc_params(vfe, bytes(guid))
    request = pack_fe_device_req(upi, vfe, bytes(guid))
    response = invoke_call(UBSE_MODULE_CODE, OP_FE_DEVICE_ALLOC_REQ, request)
    if len(response) >= UBS_SSU_GUID_LENGTH:
        guid[:UBS_SSU_GUID_LENGTH] = response[:UBS_SSU_GUID_LENGTH]


def ubs_ssu_fe_device_free(upi: int, vfe: UbsUbVfe, guid: bytes) -> None:
    """释放VFE设备

    将已分配的虚拟功能单元从目标虚拟机释放, 回收VFE设备资源。

    Args:
        upi: 租户隔离标识
        vfe: 要释放的VFE信息
        guid: 总线实例GUID, 标识目标虚拟机

    Raises:
        UbsErrInvalidArg: 参数校验错误
        UbsEngineConnectionError: 连接UBSE服务端失败
        UbsEngineAuthError: UBSE服务端鉴权不通过
        UbsEngineNotExistError: 存储空间不存在
        UbsEngineTimeoutError: UBSE服务端处理超时
        UbsEngineInternalError: UBSE服务端内部错误
    """
    validate_fe_device_free_params(vfe, guid)
    request = pack_fe_device_req(upi, vfe, guid)
    invoke_call(UBSE_MODULE_CODE, OP_FE_DEVICE_FREE_REQ, request)
