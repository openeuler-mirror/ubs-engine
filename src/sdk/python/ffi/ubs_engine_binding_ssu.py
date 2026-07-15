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
"""SSU二进制序列化/反序列化层。

负责服务端与python之间的二进制序列化/反序列化转换，包括:
- 字符串/字节数组的打包与解包
- 请求参数的打包（pack）
- 响应数据的解包（unpack）
"""
from typing import List, Optional

from ubse.models.ubs_engine_model_ssu import (
    UBS_SSU_MAX_NAME_LENGTH, UBS_SSU_MAX_TENANT_LENGTH, UBS_SSU_MAX_NQN_LENGTH,
    UBS_SSU_MAX_EID_LENGTH, UBS_SSU_MAX_UUID_LENGTH, UBS_SSU_MAX_DEV_PATH_LENGTH,
    UBS_SSU_MAX_DEV_NAME_LENGTH, UBS_SSU_GUID_LENGTH,
    UbsSsuLbaFormat, UbsSsuAllocStrategy, UbsSsuRaidLevel, UbsSsuChunkSize,
    UbsSsuAllocSpaceReq, UbsSsuSpaceReq, UbsSsuLinearSpaceReq, UbsSsuStripedSpaceReq,
    UbsSsuAllocResult, UbsSsuNamespaceInfo, UbsSsuConnectInfo, UbsSsuNsStats,
    UbsUbVfe, UbsUbFe,
)
from ubse.ipc.ubs_engine_ipc_codes import (UBSE_MODULE_CODE)
from ubse.ffi.ubs_binary_codec import BinaryPacker, BinaryUnpacker, unpack_list

from ubse.ffi.ubs_error_registry import register_module_errors
from ubse.ffi.ubs_engine_exceptions import (
    UbsEngineExistedError, UbsEngineAllocateError, UbsEngineNotExistError,
    UbsEngineOutOfRangeError, UbsErrInvalidArg
)

_MAX_NAMESPACES = 1024
_MAX_STATS = 1024
_MAX_CONNECT_INFO = 1024
_MAX_FE = 1024
# ====================== 参数校验函数 ======================

def validate_name(name: str) -> None:
    """校验名称参数是否合法。

    Args:
        name: 待校验的名称

    Raises:
        UbsErrNullPointer: name为空
        UbsEngineOutOfRangeError: name长度超出限制
    """
    if not name:
        raise UbsErrInvalidArg("name is empty")
    if len(name) >= UBS_SSU_MAX_NAME_LENGTH:
        raise UbsErrInvalidArg("name length exceeds maximum allowed")


def validate_nqn(nqn: str) -> None:
    """校验NQN参数是否合法。

    Args:
        nqn: 待校验的NQN

    Raises:
        UbsErrNullPointer: nqn为空
        UbsEngineOutOfRangeError: nqn长度超出限制
    """
    if not nqn:
        raise UbsErrInvalidArg("nqn is empty")
    if len(nqn) >= UBS_SSU_MAX_NQN_LENGTH:
        raise UbsErrInvalidArg("nqn length exceeds maximum allowed")


def validate_dev_name(dev_name: str) -> None:
    """校验设备名称参数是否合法。

    Args:
        dev_name: 待校验的设备名称

    Raises:
        UbsErrNullPointer: dev_name为空
        UbsEngineOutOfRangeError: dev_name长度超出限制
    """
    if not dev_name:
        raise UbsErrInvalidArg("dev_name is empty")
    if len(dev_name) >= UBS_SSU_MAX_DEV_NAME_LENGTH:
        raise UbsErrInvalidArg("dev_name length exceeds maximum allowed")


def validate_alloc_space_req(req: UbsSsuAllocSpaceReq) -> None:
    """校验分配存储空间请求参数是否合法。

    Args:
        req: 待校验的请求参数

    Raises:
        UbsErrNullPointer: name为空
        UbsEngineOutOfRangeError: name长度超出限制
        UbsErrInvalidArg: ns_num不大于0
    """
    validate_name(req.name)
    if req.ns_num == 0:
        raise UbsErrInvalidArg("ns_num must be greater than 0")


def validate_striped_space_req(req: UbsSsuStripedSpaceReq) -> None:
    """校验条带化空间请求参数是否合法。

    Args:
        req: 待校验的请求参数

    Raises:
        UbsErrNullPointer: name或dev_name为空
        UbsEngineOutOfRangeError: name或dev_name长度超出限制
        UbsErrInvalidArg: level或chunk_size无效
    """
    validate_name(req.name)
    validate_dev_name(req.dev_name)
    if req.level not in (UbsSsuRaidLevel.RAID0, UbsSsuRaidLevel.RAID5):
        raise UbsErrInvalidArg("invalid raid level")
    valid_chunk_sizes = {
        UbsSsuChunkSize.CHUNK_4K, UbsSsuChunkSize.CHUNK_16K, UbsSsuChunkSize.CHUNK_32K,
        UbsSsuChunkSize.CHUNK_64K, UbsSsuChunkSize.CHUNK_128K, UbsSsuChunkSize.CHUNK_256K,
        UbsSsuChunkSize.CHUNK_512K,
    }
    if req.chunk_size not in valid_chunk_sizes:
        raise UbsErrInvalidArg("invalid chunk size")

def validate_fe_device_alloc_params(vfe: Optional[UbsUbVfe], guid: Optional[bytes]) -> None:
    """校验FE设备分配参数是否合法。

    Args:
        vfe: VFE信息
        guid: 总线实例GUID

    Raises:
        UbsErrNullPointer: vfe或guid为None
    """
    if vfe is None:
        raise UbsErrInvalidArg("vfe is None")
    if guid is not None and len(guid) != UBS_SSU_GUID_LENGTH:
        raise UbsErrInvalidArg(f"bus_instance_guid length must be {UBS_SSU_GUID_LENGTH}")

def validate_fe_device_free_params(vfe: Optional[UbsUbVfe], guid: Optional[bytes]) -> None:
    """校验FE设备释放参数是否合法。

    Args:
        vfe: VFE信息
        guid: 总线实例GUID

    Raises:
        UbsErrNullPointer: vfe或guid为None
    """
    if vfe is None:
        raise UbsErrInvalidArg("vfe is None")
    if guid is None:
        raise UbsErrInvalidArg("bus_instance_guid is None")
    if len(guid) != UBS_SSU_GUID_LENGTH:
        raise UbsErrInvalidArg(f"bus_instance_guid length must be {UBS_SSU_GUID_LENGTH}")

# ====================== 请求打包函数 ======================

def pack_string(s: str, max_len: int) -> bytes:
    """将字符串打包为长度前缀字节数组。"""
    return BinaryPacker().pack_string(s, max_len).result()


def pack_bytes(data: bytes, max_len: int) -> bytes:
    """将字节数组打包为长度前缀字节数组。"""
    return BinaryPacker().pack_bytes(data, max_len).result()


def pack_alloc_space_req(req: UbsSsuAllocSpaceReq) -> bytes:
    """将分配存储空间请求参数打包为字节数组。

    Args:
        req: 请求参数

    Returns:
        打包后的字节数组
    """
    return (BinaryPacker()
            .pack_string(req.name, UBS_SSU_MAX_NAME_LENGTH)
            .pack_uint64(req.ns_size)
            .pack_uint32(req.ns_num)
            .pack_uint32(int(req.lba_format))
            .pack_uint8(int(req.strategy))
            .pack_string(req.tenant, UBS_SSU_MAX_TENANT_LENGTH)
            .result())


def pack_space_req(req: UbsSsuSpaceReq) -> bytes:
    """将挂载|卸载存储空间请求参数打包为字节数组。

    Args:
        req: 请求参数

    Returns:
        打包后的字节数组
    """
    return (BinaryPacker()
            .pack_string(req.name, UBS_SSU_MAX_NAME_LENGTH)
            .pack_string(req.nqn, UBS_SSU_MAX_NQN_LENGTH)
            .pack_string(req.src_eid, UBS_SSU_MAX_EID_LENGTH)
            .result())


def pack_linear_space_req(req: UbsSsuLinearSpaceReq) -> bytes:
    """将挂载|卸载线性编址存储空间请求参数打包为字节数组。

    Args:
        req: 请求参数

    Returns:
        打包后的字节数组
    """
    return (BinaryPacker()
            .pack_raw(pack_space_req(req))
            .pack_string(req.dev_name, UBS_SSU_MAX_DEV_NAME_LENGTH)
            .result())


def pack_striped_space_req(req: UbsSsuStripedSpaceReq) -> bytes:
    """将挂载|卸载条带化编址存储空间请求参数打包为字节数组。

    Args:
        req: 请求参数

    Returns:
        打包后的字节数组
    """
    return (BinaryPacker()
            .pack_raw(pack_linear_space_req(req))
            .pack_uint8(int(req.level))
            .pack_uint32(int(req.chunk_size))
            .result())


def pack_vfe(vfe: UbsUbVfe) -> bytes:
    """将VFE信息打包为字节数组（仅结构体字段，与C++ ``VfePack`` 对齐）。

    本函数仅打包VFE结构体字段（slotId/chipId/dieId/pfeId/vfeId/vfeGuid/
    bindBusInstanceGuid），**不包含** hasVfe标志位。
    hasVfe标志位属于消息层面逻辑，由 ``pack_connect_info_req`` 等调用方处理，


    Args:
        vfe: VFE信息（非None）

    Returns:
        打包后的字节数组
    """
    return (BinaryPacker()
            .pack_uint8(vfe.slot_id)
            .pack_uint8(vfe.chip_id)
            .pack_uint8(vfe.die_id)
            .pack_uint16(vfe.pfe_id)
            .pack_uint16(vfe.vfe_id)
            .pack_string(vfe.vfe_guid, UBS_SSU_GUID_LENGTH)
            .pack_string(vfe.bind_bus_instance_guid, UBS_SSU_GUID_LENGTH)
            .result())


def pack_connect_info_req(name: str, vfe: Optional[UbsUbVfe]) -> bytes:
    """打包获取连接信息请求参数。

    消息格式: ``name + hasVfe标志位 + (可选)VFE结构体``

    - ``hasVfe=0``: 无VFE（vfe为None），后续无VFE字段
    - ``hasVfe=1``: 有VFE，后续紧跟VFE结构体字段（由 ``pack_vfe`` 打包）
    Args:
        name: 存储空间标识
        vfe: VFE信息，为None时hasVfe=0

    Returns:
        打包后的字节数组
    """
    packer = BinaryPacker().pack_string(name, UBS_SSU_MAX_NAME_LENGTH)
    if vfe is None:
        packer.pack_uint8(0)
    else:
        packer.pack_uint8(1)
        packer.pack_raw(pack_vfe(vfe))
    return packer.result()


def pack_fe_device_req(upi: int, vfe: UbsUbVfe, bus_instance_guid: bytes) -> bytes:
    """将FE设备操作请求参数打包为字节数组。
    Args:
        upi: 租户隔离标识
        vfe: VFE信息
        bus_instance_guid: 总线实例GUID

    Returns:
        打包后的字节数组
    """
    return (BinaryPacker()
            .pack_uint32(upi)
            .pack_raw(pack_vfe(vfe))
            .pack_string(bus_instance_guid, UBS_SSU_GUID_LENGTH)
            .result())


# ====================== 响应解包函数（内部, 接收 BinaryUnpacker） ======================

def unpack_namespace_info(u: BinaryUnpacker) -> UbsSsuNamespaceInfo:
    """从解包器中读取命名空间信息。

    Args:
        u: 解包器实例

    Returns:
        解包后的命名空间信息
    """
    return UbsSsuNamespaceInfo(
        tgt_eid=u.unpack_string(UBS_SSU_MAX_EID_LENGTH),
        tgt_nqn=u.unpack_string(UBS_SSU_MAX_NQN_LENGTH),
        ns_uuid=u.unpack_string(UBS_SSU_MAX_UUID_LENGTH),
        namespace_id=u.unpack_uint32(),
        ns_dev_path=u.unpack_string(UBS_SSU_MAX_DEV_PATH_LENGTH),
        ns_size=u.unpack_uint64(),
        lba_format=UbsSsuLbaFormat(u.unpack_uint32()),
    )


def unpack_alloc_result_impl(u: BinaryUnpacker) -> UbsSsuAllocResult:
    """从解包器中读取分配结果。

    Args:
        u: 解包器实例

    Returns:
        解包后的分配结果
    """
    name = u.unpack_string(UBS_SSU_MAX_NAME_LENGTH)
    strategy = UbsSsuAllocStrategy(u.unpack_uint8())
    namespace_cnt = u.unpack_uint32()

    namespaces = [unpack_namespace_info(u) for _ in range(namespace_cnt)]

    return UbsSsuAllocResult(
        name=name,
        strategy=strategy,
        namespaces=namespaces,
    )


def unpack_vfe(u: BinaryUnpacker) -> UbsUbVfe:
    """从解包器中读取VFE信息

    本函数仅解包VFE结构体字段，**不读取** hasVfe标志位。
    hasVfe标志位属于消息层面逻辑，由调用方处理。

    Args:
        u: 解包器实例

    Returns:
        解包后的VFE信息
    """
    return UbsUbVfe(
        slot_id=u.unpack_uint8(),
        chip_id=u.unpack_uint8(),
        die_id=u.unpack_uint8(),
        pfe_id=u.unpack_uint16(),
        vfe_id=u.unpack_uint16(),
        vfe_guid=u.unpack_string(UBS_SSU_GUID_LENGTH),
        bind_bus_instance_guid=u.unpack_string(UBS_SSU_GUID_LENGTH),
    )


def unpack_fe(u: BinaryUnpacker) -> UbsUbFe:
    """从解包器中读取FE信息（与C++ ``FePack`` 对齐）。

    注意: C++ ``FePack`` 中vfeCount使用 ``uint32`` 打包，
    Python端须使用 ``unpack_uint32`` 对应，不可使用uint8。

    Args:
        u: 解包器实例

    Returns:
        解包后的FE信息
    """
    slot_id = u.unpack_uint8()
    chip_id = u.unpack_uint8()
    die_id = u.unpack_uint8()
    pfe_id = u.unpack_uint16()
    pfe_guid = u.unpack_string(UBS_SSU_GUID_LENGTH)
    vfe_cnt = u.unpack_uint32()

    vfe_list = [unpack_vfe(u) for _ in range(vfe_cnt)]

    return UbsUbFe(
        slot_id=slot_id,
        chip_id=chip_id,
        die_id=die_id,
        pfe_id=pfe_id,
        pfe_guid=pfe_guid,
        vfe_list=vfe_list,
    )


def unpack_connect_info(u: BinaryUnpacker) -> UbsSsuConnectInfo:
    """从解包器中读取连接信息。

    Args:
        u: 解包器实例

    Returns:
        解包后的连接信息
    """
    return UbsSsuConnectInfo(
        src_eid=u.unpack_string(UBS_SSU_MAX_EID_LENGTH),
        tgt_eid=u.unpack_string(UBS_SSU_MAX_EID_LENGTH),
        tgt_nqn=u.unpack_string(UBS_SSU_MAX_NQN_LENGTH),
        host_nqn=u.unpack_string(UBS_SSU_MAX_NQN_LENGTH),
        ns_uuid=u.unpack_string(UBS_SSU_MAX_UUID_LENGTH),
        ns_id=u.unpack_uint32(),
    )


def unpack_ns_stats(u: BinaryUnpacker) -> UbsSsuNsStats:
    """从解包器中读取命名空间统计信息。

    Args:
        u: 解包器实例

    Returns:
        解包后的命名空间统计信息
    """
    return UbsSsuNsStats(
        ns_uuid=u.unpack_string(UBS_SSU_MAX_UUID_LENGTH),
        ns_id=u.unpack_uint32(),
        total_size=u.unpack_uint64(),
        used_size=u.unpack_uint64(),
    )


# ====================== 响应解包函数（公开接口, 接收 bytes） ======================

def unpack_alloc_result(response: bytes) -> UbsSsuAllocResult:
    """从响应中解包分配结果。

    Args:
        response: 响应数据

    Returns:
        解包后的分配结果

    Raises:
        ValueError: 响应数据不足或格式无效
    """
    return unpack_alloc_result_impl(BinaryUnpacker(response))


def unpack_alloc_result_list(response: bytes) -> List[UbsSsuAllocResult]:
    """从响应中解包分配结果列表。

    Args:
        response: 响应数据

    Returns:
        解包后的分配结果列表

    Raises:
        ValueError: 响应数据不足或格式无效
    """
    return unpack_list(BinaryUnpacker(response), _MAX_NAMESPACES, unpack_alloc_result_impl)


def unpack_dev_path_response(response: bytes) -> str:
    """从响应中解包设备路径。

    Args:
        response: 响应数据

    Returns:
        解包后的设备路径

    Raises:
        ValueError: 响应数据不足
    """
    return BinaryUnpacker(response).unpack_string(UBS_SSU_MAX_DEV_PATH_LENGTH)


def unpack_ns_stats_list(response: bytes) -> List[UbsSsuNsStats]:
    """从响应中解包命名空间统计信息列表。

    Args:
        response: 响应数据

    Returns:
        解包后的命名空间统计信息列表

    Raises:
        ValueError: 响应数据不足或格式无效
    """
    return unpack_list(BinaryUnpacker(response), _MAX_STATS, unpack_ns_stats)


def unpack_connect_info_list(response: bytes) -> List[UbsSsuConnectInfo]:
    """从响应中解包连接信息列表。

    Args:
        response: 响应数据

    Returns:
        解包后的连接信息列表

    Raises:
        ValueError: 响应数据不足或格式无效
    """
    return unpack_list(BinaryUnpacker(response), _MAX_CONNECT_INFO, unpack_connect_info)


def unpack_fe_device_list(response: bytes) -> List[UbsUbFe]:
    """从响应中解包FE设备列表。

    Args:
        response: 响应数据

    Returns:
        解包后的FE设备列表

    Raises:
        ValueError: 响应数据不足或格式无效
    """
    return unpack_list(BinaryUnpacker(response), _MAX_FE, unpack_fe)

# TODO 错误码整改
# ====================== SSU 模块错误码注册 ======================
register_module_errors(UBSE_MODULE_CODE, {
    1000: ("UBSE_ERR_OUT_OF_RANGE", UbsEngineOutOfRangeError),
    1006: ("UBSE_ERR_EXISTED", UbsEngineExistedError),
    1007: ("UBSE_ERR_NOT_EXIST", UbsEngineNotExistError),
    1013: ("UBSE_ERR_ALLOCATE", UbsEngineAllocateError),
})
