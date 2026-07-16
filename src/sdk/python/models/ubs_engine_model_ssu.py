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
from dataclasses import dataclass, field
from enum import IntEnum
from typing import List, Optional

# ====================== SSU 常量定义 ======================
# 与C头文件ubs_engine_ssu.h保持一致
UBS_SSU_MAX_NAME_LENGTH = 48  # 请求标识最大48个字符, 含结尾字符'\0'
UBS_SSU_MAX_RESULT_NAME_LENGTH = 32  # 结果名称最大32个字符, 含结尾字符'\0'
UBS_SSU_MAX_USER_NAME_LENGTH = 32  # 使用方进程运行用户名称最大长度, 含结尾字符'\0'
UBS_SSU_MAX_TENANT_LENGTH = 17  # 请求方UPI(租户隔离标识)最大长度, 含结尾字符'\0'
UBS_SSU_MAX_NQN_LENGTH = 69  # NVMe NQN最大长度69个字符, 含结尾字符'\0'
UBS_SSU_MAX_EID_LENGTH = 17  # EID最大长度, 含结尾字符'\0'
UBS_SSU_MAX_UUID_LENGTH = 37  # UUID标准长度37个字符, 含结尾字符'\0'
UBS_SSU_MAX_DEV_PATH_LENGTH = 63  # 设备路径最大长度, 含结尾字符'\0'
UBS_SSU_MAX_DEV_NAME_LENGTH = 33  # 聚合块设备名称最大长度, 含结尾字符'\0'
UBS_SSU_RAID5_MIN_MEMBER_NUM = 3  # RAID5最少成员设备数
UBS_SSU_GUID_LENGTH = 32  # GUID最大长度32个字符, 不含结尾字符'\0'

# 基本数据类型字节大小
UBS_SSU_BYTE_SIZE = 1  # byte/uint8类型字节大小
UBS_SSU_UINT16_SIZE = 2  # uint16类型字节大小
UBS_SSU_UINT32_SIZE = 4  # uint32类型字节大小
UBS_SSU_UINT64_SIZE = 8  # uint64类型字节大小


# ====================== SSU 枚举类型 ======================

class UbsSsuLbaFormat(IntEnum):
    """LBA格式, 值为对应字节数"""
    FORMAT_512 = 512  # 512B
    FORMAT_4K = 4096  # 4K


class UbsSsuRaidLevel(IntEnum):
    """聚合RAID级别"""
    RAID0 = 0  # RAID0条带化
    RAID5 = 5  # RAID5条带化带校验


class UbsSsuChunkSize(IntEnum):
    """条带化chunk大小(KB), 需为LBA格式的整数倍"""
    CHUNK_4K = 4
    CHUNK_16K = 16
    CHUNK_32K = 32
    CHUNK_64K = 64
    CHUNK_128K = 128
    CHUNK_256K = 256
    CHUNK_512K = 512


class UbsSsuAllocStrategy(IntEnum):
    """分配策略"""
    STRIPED = 0  # 分布式策略, 尽量从多个设备均等分配, 适用于条带化编址使用场景
    LINEAR = 1  # 顺序策略, 尽量从单个设备分配, 适用于线性编址使用场景


# ====================== SSU 数据结构 ======================

@dataclass
class UbsSsuNamespaceInfo:
    """命名空间信息"""
    tgt_eid: str = ""  # Target Eid
    tgt_nqn: str = ""  # 子系统NQN
    ns_uuid: str = ""  # 物理设备UUID
    namespace_id: int = 0  # 命名空间ID
    ns_dev_path: str = ""  # 命名空间设备路径
    ns_size: int = 0  # 分配的容量, 单位字节
    lba_format: UbsSsuLbaFormat = UbsSsuLbaFormat.FORMAT_4K  # LBA格式

    def __str__(self):
        return (
            f"Namespace(id={self.namespace_id}, path='{self.ns_dev_path}', "
            f"size={self.ns_size}, lba={self.lba_format.name})"
        )

    def to_dict(self) -> dict:
        return {
            'tgt_eid': self.tgt_eid,
            'tgt_nqn': self.tgt_nqn,
            'ns_uuid': self.ns_uuid,
            'namespace_id': self.namespace_id,
            'ns_dev_path': self.ns_dev_path,
            'ns_size': self.ns_size,
            'lba_format': self.lba_format.name,
        }


@dataclass
class UbsSsuAllocResult:
    """分配存储空间结果"""
    name: str = ""  # 请求标识, 最大48个字符
    strategy: UbsSsuAllocStrategy = UbsSsuAllocStrategy.STRIPED  # 分配策略
    namespaces: List[UbsSsuNamespaceInfo] = field(default_factory=list)  # 命名空间信息列表

    def __str__(self):
        return (
            f"AllocResult(name='{self.name}', "
            f"strategy={self.strategy.name}, "
            f"namespace_cnt={len(self.namespaces)})"
        )

    def to_dict(self) -> dict:
        return {
            'name': self.name,
            'strategy': self.strategy.name,
            'namespaces': [ns.to_dict() for ns in self.namespaces],
        }


@dataclass
class UbsSsuAllocSpaceReq:
    """分配存储空间请求参数"""
    name: str = ""  # 请求标识, 最大48个字符
    ns_size: int = 0  # 申请总容量, 单位字节
    ns_num: int = 1  # 命名空间数量, 等于1时strategy不生效
    lba_format: UbsSsuLbaFormat = UbsSsuLbaFormat.FORMAT_4K  # LBA格式
    strategy: UbsSsuAllocStrategy = UbsSsuAllocStrategy.STRIPED  # 分配策略
    tenant: bytes = b''  # 请求方tenant(租户隔离标识)


@dataclass
class UbsSsuSpaceReq:
    """挂载|卸载存储空间请求参数"""
    name: str = ""  # 需挂载|卸载的存储空间标识, 最大48个字符
    nqn: str = ""  # Host 的 NVMe Qualified Name
    src_eid: str = ""  # 源EID


@dataclass
class UbsSsuLinearSpaceReq:
    """挂载|卸载线性编址存储空间请求参数"""
    name: str = ""  # 需挂载|卸载的存储空间标识, 最大48个字符
    nqn: str = ""  # Host 的 NVMe Qualified Name
    src_eid: str = ""  # 源EID
    dev_name: str = ""  # 聚合后的块设备名称, 由外部指定


@dataclass
class UbsSsuStripedSpaceReq:
    """挂载|卸载条带化编址存储空间请求参数"""
    name: str = ""  # 需挂载|卸载的存储空间标识, 最大48个字符
    nqn: str = ""  # Host 的 NVMe Qualified Name
    src_eid: str = ""  # 源EID
    dev_name: str = ""  # 聚合后的块设备名称, 由外部指定
    level: UbsSsuRaidLevel = UbsSsuRaidLevel.RAID0  # RAID级别
    chunk_size: UbsSsuChunkSize = UbsSsuChunkSize.CHUNK_4K  # chunk大小, 单位KB


@dataclass
class UbsUbVfe:
    """虚拟功能单元(VFE)信息"""
    slot_id: int = 0
    chip_id: int = 0
    die_id: int = 0
    pfe_id: int = 0
    vfe_id: int = 0
    vfe_guid: str = ""  # vfe GUID
    bind_bus_instance_guid: str = ""  # 绑定的总线实例GUID

    def to_dict(self) -> dict:
        return {
            "slot_id": self.slot_id,
            "chip_id": self.chip_id,
            "die_id": self.die_id,
            "pfe_id": self.pfe_id,
            "vfe_id": self.vfe_id,
            "vfe_guid": self.vfe_guid,
            "bind_bus_instance_guid": self.bind_bus_instance_guid,
        }


@dataclass
class UbsUbFe:
    """功能单元(FE)信息，包含所属PFE及其下的VFE列表"""
    slot_id: int = 0
    chip_id: int = 0
    die_id: int = 0
    pfe_id: int = 0
    pfe_guid: str = ""  # pfe GUID
    vfe_list: Optional[List[UbsUbVfe]] = None

    def __post_init__(self):
        if self.vfe_list is None:
            self.vfe_list = []

    def to_dict(self) -> dict:
        return {
            "slot_id": self.slot_id,
            "chip_id": self.chip_id,
            "die_id": self.die_id,
            "pfe_id": self.pfe_id,
            "pfe_guid": self.pfe_guid,
            "vfe_count": len(self.vfe_list),
            "vfe_list": [vfe.to_dict() for vfe in self.vfe_list],
        }


@dataclass
class UbsSsuConnectInfo:
    """存储空间连接信息"""
    src_eid: str = ""
    tgt_eid: str = ""
    tgt_nqn: str = ""
    host_nqn: str = ""
    ns_uuid: str = ""
    ns_id: int = 0

    def to_dict(self) -> dict:
        return {
            "src_eid": self.src_eid,
            "tgt_eid": self.tgt_eid,
            "tgt_nqn": self.tgt_nqn,
            "host_nqn": self.host_nqn,
            "ns_uuid": self.ns_uuid,
            "ns_id": self.ns_id,
        }


@dataclass
class UbsSsuNsStats:
    """存储空间状态"""
    ns_uuid: str = ""
    ns_id: int = 0
    total_size: int = 0
    used_size: int = 0

    def to_dict(self) -> dict:
        return {
            "ns_uuid": self.ns_uuid,
            "ns_id": self.ns_id,
            "total_size": self.total_size,
            "used_size": self.used_size,
            "usage_percent": round(self.used_size / self.total_size * 100, 2)
            if self.total_size > 0 else 0.0,
        }
