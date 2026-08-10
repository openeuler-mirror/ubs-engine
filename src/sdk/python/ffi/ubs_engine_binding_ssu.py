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

from ubse.ffi.ubs_binary_codec import BinaryPacker, BinaryUnpacker, unpack_list
from ubse.ffi.ubs_engine_exceptions import (
    UbsEngineExistedError, UbsEngineAllocateError, UbsEngineNotExistError,
    UbsEngineOutOfRangeError, UbsErrInvalidArg, UbsLengthExceededError,
    UbsEngineInternalError,
)
from ubse.ffi.ubs_error_registry import register_module_errors
from ubse.ipc.ubs_engine_ipc_codes import (UBSE_SSU_MODULE_CODE)
from ubse.models.ubs_engine_model_ssu import (
    UBS_SSU_MAX_NAME_LENGTH, UBS_SSU_MAX_TENANT_LENGTH, UBS_SSU_MAX_NQN_LENGTH,
    UBS_SSU_MAX_EID_LENGTH, UBS_SSU_MAX_UUID_LENGTH, UBS_SSU_MAX_DEV_PATH_LENGTH,
    UBS_SSU_MAX_DEV_NAME_LENGTH, UBS_SSU_GUID_LENGTH,
    UbsSsuLbaFormat, UbsSsuAllocStrategy, UbsSsuRaidLevel, UbsSsuChunkSize,
    UbsSsuAllocSpaceReq, UbsSsuSpaceReq, UbsSsuLinearSpaceReq, UbsSsuStripedSpaceReq,
    UbsSsuAllocResult, UbsSsuNamespaceInfo, UbsSsuConnectInfo, UbsSsuNsStats,
    UbsUbVfe, UbsUbFe,
)

_MAX_NAMESPACES = 1024
_MAX_STATS = 1024
_MAX_CONNECT_INFO = 1024
_MAX_FE = 1024
_MAX_NS_DEV_PATHS = 1024
_MAX_HOST_NQN = 1024


# ====================== 参数校验函数 ======================

def _is_ascii_alnum(ch: str) -> bool:
    """检查字符是否为 ASCII 字母或数字，避免 Unicode locale 差异。"""
    return ('a' <= ch <= 'z') or ('A' <= ch <= 'Z') or ('0' <= ch <= '9')


def validate_name(name: str) -> None:
    """校验名称参数是否合法，仅允许 [a-zA-Z0-9_-.:]。

    Args:
        name: 待校验的名称

    Raises:
        UbsErrInvalidArg: name为空、长度超出限制或包含非法字符
    """
    if not name:
        raise UbsErrInvalidArg("name is empty")
    if len(name) >= UBS_SSU_MAX_NAME_LENGTH:
        raise UbsErrInvalidArg("name length exceeds maximum allowed")
    for ch in name:
        if not (_is_ascii_alnum(ch) or ch == '_' or ch == '-' or ch == '.' or ch == ':'):
            raise UbsErrInvalidArg(f"name contains invalid character '{ch}'")


def _validate_optional_string(s: str, max_len: int, field_name: str) -> None:
    """校验可选字符串: 允许为空, 非空时长度不超过 max_len-1（含结尾'\0'）。

    Args:
        s: 待校验的字符串
        max_len: 缓冲区总容量（含结尾'\0'）
        field_name: 字段名（用于错误消息）

    Raises:
        UbsErrInvalidArg: 长度超出限制
    """
    if s and len(s) >= max_len:
        raise UbsErrInvalidArg(f"{field_name} length exceeds maximum allowed")


def validate_nqn(nqn: str) -> None:
    """校验NQN参数是否合法: 允许为空, 非空时长度不超过限制。

    Args:
        nqn: 待校验的NQN

    Raises:
        UbsErrInvalidArg: nqn长度超出限制
    """
    _validate_optional_string(nqn, UBS_SSU_MAX_NQN_LENGTH, "nqn")


def validate_nqn_not_empty(nqn: str) -> None:
    """校验NQN参数是否合法: 不允许为空, 非空时长度不超过限制。

    Args:
        nqn: 待校验的NQN

    Raises:
        UbsErrInvalidArg: nqn长度超出限制
    """
    if not nqn:
        raise UbsErrInvalidArg("nqn is empty")
    if len(nqn) >= UBS_SSU_MAX_NQN_LENGTH:
        raise UbsErrInvalidArg("nqn length exceeds maximum allowed")


def validate_eid(eid: str) -> None:
    """校验eid参数是否合法: 允许为空, 非空时长度不超过限制。

    Args:
        eid: 待校验的eid

    Raises:
        UbsErrInvalidArg: eid长度超出限制
    """
    _validate_optional_string(eid, UBS_SSU_MAX_EID_LENGTH, "eid")


def validate_tenant(tenant: str) -> None:
    """校验租户隔离标识参数是否合法: 允许为空, 非空时长度不超过限制, 仅允许 [a-zA-Z0-9_-.:]。

    Args:
        tenant: 待校验的租户隔离标识

    Raises:
        UbsErrInvalidArg: tenant长度超出限制或包含非法字符
    """
    if not tenant:
        return
    if len(tenant) >= UBS_SSU_MAX_TENANT_LENGTH:
        raise UbsErrInvalidArg("tenant length exceeds maximum allowed")
    for ch in tenant:
        if not (_is_ascii_alnum(ch) or ch == '_' or ch == '-' or ch == '.' or ch == ':'):
            raise UbsErrInvalidArg(f"tenant contains invalid character '{ch}'")


def validate_space_req(req: UbsSsuSpaceReq) -> None:
    """校验空间挂载/卸载请求参数是否合法。

    Args:
        req: 待校验的请求参数

    Raises:
        UbsErrInvalidArg: name或eid为空或长度超出限制
    """
    validate_name(req.name)
    validate_nqn(req.nqn)
    validate_eid(req.src_eid)


def validate_dev_name(dev_name: str) -> None:
    """校验设备名称参数是否合法，仅允许 [a-zA-Z0-9_/.-]。

    Args:
        dev_name: 待校验的设备名称

    Raises:
        UbsErrInvalidArg: dev_name为空、长度超出限制或包含非法字符
    """
    if not dev_name:
        raise UbsErrInvalidArg("dev_name is empty")
    if len(dev_name) >= UBS_SSU_MAX_DEV_NAME_LENGTH:
        raise UbsErrInvalidArg("dev_name length exceeds maximum allowed")
    for ch in dev_name:
        if not (_is_ascii_alnum(ch) or ch == '_' or ch == '/' or ch == '.' or ch == '-'):
            raise UbsErrInvalidArg(f"dev_name contains invalid character '{ch}'")


def validate_access_permission(name: str, nqn: str) -> None:
    """校验访问权限操作参数是否合法。

    Args:
       name: 待校验的名称
       nqn: 待校验的NQN

    Raises:
       UbsErrInvalidArg: name或nqn为空或长度超出限制
    """
    validate_name(name)
    validate_nqn_not_empty(nqn)


def validate_alloc_space_req(req: UbsSsuAllocSpaceReq) -> None:
    """校验分配存储空间请求参数是否合法。

    Args:
        req: 待校验的请求参数

    Raises:
        UbsErrInvalidArg: 参数校验失败
    """
    validate_name(req.name)
    if req.ns_num == 0:
        raise UbsErrInvalidArg("ns_num must be greater than 0")
    # ns_size 须为 1G 的整数倍
    ONE_G = 1024 * 1024 * 1024
    if req.ns_size == 0 or req.ns_size % ONE_G != 0:
        raise UbsErrInvalidArg("ns_size must be a multiple of 1G")
    # 仅条带化策略时, ns_size 须能整除 ns_num
    if req.strategy == UbsSsuAllocStrategy.STRIPED and req.ns_size % req.ns_num != 0:
        raise UbsErrInvalidArg("When using striping strategy only, ns_size must be divisible by ns_num")
    valid_lba_format = {
        UbsSsuLbaFormat.FORMAT_512, UbsSsuLbaFormat.FORMAT_4K,
    }
    if req.lba_format not in valid_lba_format:
        raise UbsErrInvalidArg("invalid lba format")
    valid_strategy = {
        UbsSsuAllocStrategy.STRIPED, UbsSsuAllocStrategy.LINEAR,
    }
    if req.strategy not in valid_strategy:
        raise UbsErrInvalidArg("invalid strategy")
    validate_tenant(req.tenant)


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
    validate_nqn(req.nqn)
    validate_eid(req.src_eid)
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


def validate_linear_space_req(req: UbsSsuLinearSpaceReq) -> None:
    """校验线性编址空间操作请求参数是否合法。

    Args:
        req: 待校验的请求参数

    Raises:
        UbsErrInvalidArg: name或dev_name为空或长度超出限制
    """
    validate_name(req.name)
    validate_nqn(req.nqn)
    validate_eid(req.src_eid)
    validate_dev_name(req.dev_name)


def validate_detach_striped_space_req(req: UbsSsuStripedSpaceReq) -> None:
    """校验条带化空间卸载请求参数（复用 linear 校验逻辑，不校验 level/chunk_size）。"""
    validate_linear_space_req(req)


def validate_fe_device_alloc_params(vfe: UbsUbVfe, guid: str) -> None:
    """校验FE设备分配参数是否合法。

    Args:
        vfe: VFE信息
        guid: 总线实例GUID字符串, 空字符串表示不传guid

    Raises:
        UbsErrInvalidArg: guid长度非法
    """
    # guid允许为空字符串(表示不传), 非空时长度必须为UBS_SSU_GUID_LENGTH
    if guid != "" and len(guid) != UBS_SSU_GUID_LENGTH:
        raise UbsErrInvalidArg(f"bus_instance_guid length must be {UBS_SSU_GUID_LENGTH}")


def validate_fe_device_free_params(vfe: UbsUbVfe) -> None:
    """校验FE设备释放参数是否合法。

    Args:
        vfe: VFE信息
    """
    if len(vfe.bind_bus_instance_guid) != UBS_SSU_GUID_LENGTH:
        raise UbsErrInvalidArg("invalid vfe.bind_bus_instance_guid")


# ====================== 请求打包函数 ======================


def pack_guid(guid: str) -> bytes:
    """将GUID字符串打包为32字节定长裸数据(不足补0, 超长截断)。

    协议格式: ``[32 bytes 裸数据]``, 无长度前缀。
    与C++端 ``GuidPack(StringToArrayForGuid)`` 对齐。

    Args:
        guid: GUID字符串

    Returns:
        32字节定长字节数组
    """
    encoded = guid.encode('utf-8') if isinstance(guid, str) else bytes(guid)
    if len(encoded) > UBS_SSU_GUID_LENGTH:
        encoded = encoded[:UBS_SSU_GUID_LENGTH]
    return encoded.ljust(UBS_SSU_GUID_LENGTH, b'\x00')


def unpack_guid(u: BinaryUnpacker) -> str:
    """从解包器中读取32字节定长GUID并转换为字符串。

    与C++端 ``GuidUnpack`` 逐字节读取对齐。

    Args:
        u: 解包器实例

    Returns:
        UTF-8解码后的GUID字符串

    Raises:
        UbsInsufficientDataError: 剩余数据不足32字节
    """
    raw = bytearray()
    for _ in range(UBS_SSU_GUID_LENGTH):
        raw.append(u.unpack_uint8())
    return raw.decode('utf-8', errors='replace')


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
            .pack_raw(pack_guid(vfe.vfe_guid))
            .pack_raw(pack_guid(vfe.bind_bus_instance_guid))
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


def pack_fe_device_req(upi: int, vfe: UbsUbVfe, bus_instance_guid: str) -> bytes:
    """将FE设备操作请求参数打包为字节数组。
    Args:
        upi: 租户隔离标识
        vfe: VFE信息
        bus_instance_guid: 总线实例GUID字符串

    Returns:
        打包后的字节数组
    """
    return (BinaryPacker()
            .pack_uint32(upi)
            .pack_raw(pack_vfe(vfe))
            .pack_raw(pack_guid(bus_instance_guid))
            .result())


def pack_fe_device_free_req(upi: int, vfe: UbsUbVfe) -> bytes:
    """将FE设备释放请求参数打包为字节数组。
    Args:
        upi: 租户隔离标识
        vfe: VFE信息

    Returns:
        打包后的字节数组
    """
    return (BinaryPacker()
            .pack_uint32(upi)
            .pack_raw(pack_vfe(vfe))
            .result())


# ====================== 响应解包函数（内部, 接收 BinaryUnpacker） ======================

def unpack_namespace_info(u: BinaryUnpacker) -> UbsSsuNamespaceInfo:
    """从解包器中读取命名空间信息。

    Args:
        u: 解包器实例

    Returns:
        解包后的命名空间信息
    """
    ns = UbsSsuNamespaceInfo(
        tgt_eid=u.unpack_string(UBS_SSU_MAX_EID_LENGTH),
        tgt_nqn=u.unpack_string(UBS_SSU_MAX_NQN_LENGTH),
        ns_uuid=u.unpack_string(UBS_SSU_MAX_UUID_LENGTH),
        namespace_id=u.unpack_uint32(),
        ns_dev_path=u.unpack_string(UBS_SSU_MAX_DEV_PATH_LENGTH),
        ns_size=u.unpack_uint64(),
        lba_format=UbsSsuLbaFormat(u.unpack_uint32()),
    )
    ns.allow_host_nqn_list = unpack_list(u, _MAX_HOST_NQN,
                                         lambda uu: uu.unpack_string(UBS_SSU_MAX_NQN_LENGTH))
    return ns


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
        vfe_guid=unpack_guid(u),
        bind_bus_instance_guid=unpack_guid(u),
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
    pfe_guid = unpack_guid(u)
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


def unpack_ns_dev_paths_response(response: bytes) -> List[str]:
    """从响应中解包命名空间设备路径列表。

    格式: uint32 count + [string]*count

    Args:
        response: 响应数据

    Returns:
        解包后的命名空间设备路径列表

    Raises:
        ValueError: 响应数据不足
    """
    u = BinaryUnpacker(response)
    return unpack_ns_dev_paths(u)


def unpack_ns_dev_paths(u: BinaryUnpacker) -> List[str]:
    """从解包器中读取命名空间设备路径列表（内部函数）。

    读取后解包器位置指向列表之后的数据。

    Args:
        u: 解包器

    Returns:
        解包后的命名空间设备路径列表
    """
    count = u.unpack_uint32()
    if count > _MAX_NS_DEV_PATHS:
        raise UbsLengthExceededError(f"ns_dev_paths count {count} exceeds max {_MAX_NS_DEV_PATHS}")
    return [u.unpack_string(UBS_SSU_MAX_DEV_PATH_LENGTH) for _ in range(count)]


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


# ====================== SSU 模块错误码注册 ======================
# 公共错误码(1/32/43等)已在 _COMMON_ERROR_MAP 注册, 此处仅注册SSU专属或需覆盖语义的错误码。
register_module_errors(UBSE_SSU_MODULE_CODE, {
    # --- SSU 专属业务错误码 (1000-1099) ---
    1000: ("UBSE_ERR_OUT_OF_RANGE", UbsEngineOutOfRangeError),
    1006: ("UBSE_ERR_EXISTED", UbsEngineExistedError),
    1007: ("UBSE_ERR_NOT_EXIST", UbsEngineNotExistError),
    1010: ("UBSE_ERR_CREATING", UbsEngineInternalError),  # 命名空间正在创建中, 调用方可稍后重试
    1013: ("UBSE_ERR_ALLOCATE", UbsEngineAllocateError),
})
