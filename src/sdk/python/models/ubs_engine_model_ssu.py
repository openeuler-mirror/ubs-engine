#!/usr/bin/python3
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# ubs-engine is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
import ctypes
from dataclasses import dataclass, field
from typing import List
# ====================== SSU 类型定义 ======================
# 常量定义, 与C头文件ubs_engine_ssu.h保持一致
UBS_SSU_MAX_NAME_LENGTH = 48 # 请求标识最大48个字符, 含结尾字符'\0'
UBS_SSU_MAX_RESULT_NAME_LENGTH = 32 # 结果名称最大32个字符, 含结尾字符'\0'
UBS_SSU_MAX_USER_NAME_LENGTH = 32 # 使用方进程运行用户名称最大长度, 含结尾字符'\0'
UBS_SSU_MAX_TENANT_LENGTH = 17 # 请求方UPI(租户隔离标识)最大长度, 含结尾字符'\0'
UBS_SSU_MAX_NQN_LENGTH = 69 # NVMe NQN最大长度69个字符, 含结尾字符'\0'
UBS_SSU_MAX_EID_LENGTH = 17 # EID最大长度, 含结尾字符'\0'
UBS_SSU_MAX_UUID_LENGTH = 37 # UUID标准长度37个字符, 含结尾字符'\0'
UBS_SSU_MAX_DEV_PATH_LENGTH = 63 # 设备路径最大长度, 含结尾字符'\0'
UBS_SSU_MAX_DEV_NAME_LENGTH = 33 # 聚合块设备名称最大长度, 含结尾字符'\0'
UBS_SSU_RAID5_MIN_MEMBER_NUM = 3 # RAID5最少成员设备数
UBS_SSU_BUS_INSTANCE_GUID_LENGTH = 32 # 总线实例GUID长度32个字符
# LBA格式, 值为对应字节数
class UbsSsuLbaFormat(IntEnum):
    FORMAT_512 = 512  # 512B
    FORMAT_4K = 4096  # 4K

# 聚合RAID级别
class UbsSsuRaidLevel(IntEnum):
    RAID0 = 0  # RAID0条带化
    RAID5 = 5  # RAID5条带化带校验

# 条带化chunk大小(KB), 需为LBA格式的整数倍
class UbsSsuChunkSize(IntEnum):
    CHUNK_4K = 4     # 4KB
    CHUNK_16K = 16   # 16KB
    CHUNK_32K = 32   # 32KB
    CHUNK_64K = 64   # 64KB
    CHUNK_128K = 128 # 128KB
    CHUNK_256K = 256 # 256KB
    CHUNK_512K = 512 # 512KB

# 分配策略
class UbsSsuAllocStrategy(IntEnum):
    STRIPED = 0  # 分布式策略, 尽量从多个设备均等分配, 适用于条带化编址使用场景
    LINEAR = 1   # 顺序策略, 尽量从单个设备分配, 适用于线性编址使用场景


# 分配存储空间请求参数 C结构体
class UbsSsuAllocSpaceReqT(ctypes.Structure):
    _fields_ = [
        ("name", ctypes.c_char * UBS_SSU_MAX_NAME_LENGTH),
        ("ns_size", ctypes.c_uint64),
        ("ns_num", ctypes.c_uint32),
        ("lba_format", ctypes.c_int32),
        ("strategy", ctypes.c_int32),
        ("tenant", ctypes.c_char * UBS_SSU_MAX_TENANT_LENGTH),
    ]


# 命名空间信息 C结构体
class UbsSsuNamespaceInfoT(ctypes.Structure):
    _fields_ = [
        ("tgt_eid", ctypes.c_char * UBS_SSU_MAX_EID_LENGTH),
        ("tgt_nqn", ctypes.c_char * UBS_SSU_MAX_NQN_LENGTH),
        ("ns_uuid", ctypes.c_char * UBS_SSU_MAX_UUID_LENGTH),
        ("ns_id", ctypes.c_uint32),
        ("ns_dev_path", ctypes.c_char * UBS_SSU_MAX_DEV_PATH_LENGTH),
        ("ns_size", ctypes.c_uint64),
        ("lba_format", ctypes.c_int32),
    ]


# 分配存储空间结果 C结构体
class UbsSsuAllocResultT(ctypes.Structure):
    _fields_ = [
        ("name", ctypes.c_char * UBS_SSU_MAX_NAME_LENGTH),
        ("strategy", ctypes.c_int32),
        ("namespace_cnt", ctypes.c_uint32),
        ("namespaces", ctypes.POINTER(UbsSsuNamespaceInfoT)),
    ]


# 条带化挂载请求参数 C结构体
class UbsSsuStripedAttachReqT(ctypes.Structure):
    _fields_ = [
        ("dev_name", ctypes.c_char * UBS_SSU_MAX_DEV_NAME_LENGTH),
        ("level", ctypes.c_int32),
        ("chunk_size", ctypes.c_int32),
    ]

class UbsUbVfeT(ctypes.Structure):
    _fields_ = [
        ("slot_id", ctypes.c_uint8),   # 槽位ID
        ("chip_id", ctypes.c_uint8),   # 芯片ID
        ("die_id", ctypes.c_uint8),    # Die ID
        ("pfe_id", ctypes.c_uint16),   # 物理功能单元ID
        ("vfe_id", ctypes.c_uint16),   # 虚拟功能单元ID
    ]

class UbsUbFeT(ctypes.Structure):
    _fields_ = [
        ("slot_id", ctypes.c_uint8),          # 槽位ID
        ("chip_id", ctypes.c_uint8),          # 芯片ID
        ("die_id", ctypes.c_uint8),           # Die ID
        ("pfe_id", ctypes.c_uint16),          # 物理功能单元ID
        ("vfe_cnt", ctypes.c_uint8),          # VFE数量
        ("vfe_list", ctypes.POINTER(UbsUbVfeT)),  # VFE列表指针（**关键：必须用POINTER**）
    ]
class UbsSsuConnectInfoT(ctypes.Structure):
    _fields_ = [
        ("src_eid", ctypes.c_char * UBS_SSU_MAX_EID_LENGTH),      # Source EID
        ("tgt_eid", ctypes.c_char * UBS_SSU_MAX_EID_LENGTH),      # Target EID
        ("tgt_nqn", ctypes.c_char * UBS_SSU_MAX_NQN_LENGTH),      # Target NQN
        ("host_nqn", ctypes.c_char * UBS_SSU_MAX_NQN_LENGTH),     # 默认NQN
        ("ns_uuid", ctypes.c_char * UBS_SSU_MAX_UUID_LENGTH),     # 物理设备UUID
        ("ns_id", ctypes.c_uint32),                               # 命名空间ID
    ]

class UbsSsuNsStatsT(ctypes.Structure):
    _fields_ = [
        ("ns_uuid", ctypes.c_char * UBS_SSU_MAX_UUID_LENGTH),  # 物理设备UUID
        ("ns_id", ctypes.c_uint32),                           # 命名空间ID
        ("total_size", ctypes.c_uint64),                      # 总容量（字节）
        ("used_size", ctypes.c_uint64),                       # 已用容量（字节）
    ]
@dataclass
class UbsSsuNamespaceInfo:
    """命名空间信息"""
    tgt_eid: str = ""  # Target Eid
    tgt_nqn: str = ""  # 子系统NQN
    ns_uuid: str = ""  # 物理设备UUID
    namespace_id: int = 0  # 命名空间ID
    ns_dev_path: str = ""  # 命名空间设备路径
    ns_size: int = 0  # 分配的容量，单位字节
    lba_format: UbsSsuLbaFormat  # LBA格式

    def __str__(self):
        return (
            f"Namespace(id={self.namespace_id}, path='{self.ns_dev_path}', "
            f"size={self.ns_size}, lba={self.lba_format.name})"
        )

    @staticmethod
    def from_c_struct(c_info: UbsSsuNamespaceInfoT) -> "UbsSsuNamespaceInfo":
        return UbsSsuNamespaceInfo(
            tgt_eid=c_info.tgt_eid.decode('utf-8', errors='ignore'),
            tgt_nqn=c_info.tgt_nqn.decode('utf-8', errors='ignore'),
            ns_uuid=c_info.ns_uuid.decode('utf-8', errors='ignore'),
            namespace_id=c_info.namespace_id,
            ns_dev_path=c_info.ns_dev_path.decode('utf-8', errors='ignore'),
            ns_size=c_info.ns_size,
            lba_format=UbsSsuLbaFormat(c_info.lba_format),
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
    name: str = ""  # 请求标识，最大48个字符
    strategy: UbsSsuAllocStrategy # 分配策略
    namespaces: List[UbsSsuNamespaceInfo] = field(default_factory=list)  # 命名空间信息列表

    def __str__(self):
        return (
            f"AllocResult(name='{self.name}', "
            f"strategy={self.strategy.name}, "
            f"namespace_cnt={len(self.namespaces)})"
        )

    @staticmethod
    def from_c_struct(c_result: UbsSsuAllocResultT) -> "UbsSsuAllocResult":
        namespaces = []
        cnt = c_result.namespace_cnt
        if cnt > 0 and c_result.namespaces:
            for i in range(cnt):
                ns_ptr = ctypes.cast(
                    ctypes.addressof(c_result.namespaces.contents) + i * ctypes.sizeof(UbsSsuNamespaceInfoT),
                    ctypes.POINTER(UbsSsuNamespaceInfoT)
                )
                namespaces.append(UbsSsuNamespaceInfo.from_c_struct(ns_ptr.contents))
        return UbsSsuAllocResult(
            name=c_result.name.decode('utf-8', errors='ignore'),
            strategy=UbsSsuAllocStrategy(c_result.strategy),
            namespaces=namespaces,
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
    name: str = ""  # 请求标识，最大48个字符
    ns_size: int = 0 # 申请总容量，单位字节, 条带化策略时，需整除nsNum且整除后需要为chunkSize的整数倍
    ns_num: int = 1  # 命名空间数量，等于1时，strategy不生效
    lba_format: UbsSsuLbaFormat # LBA 格式
    strategy: UbsSsuAllocStrategy # 分配策略
    tenant: bytes = b''  # 请求方tenant（租户隔离标识）

    def to_c_struct(self) -> UbsSsuAllocSpaceReqT:
        c_req = UbsSsuAllocSpaceReqT()
        # 先将整个name数组清零,确保null终止
        name_bytes = self.name.encode('utf-8')[:UBS_SSU_MAX_NAME_LENGTH - 1]
        for i in range(UBS_SSU_MAX_NAME_LENGTH):
            c_req.name[i] = b'\0'
        for i, b in enumerate(name_bytes):
            c_req.name[i] = b
        c_req.ns_size = self.ns_size
        c_req.ns_num = self.ns_num
        c_req.lba_format = int(self.lba_format)
        c_req.strategy = int(self.strategy)
        c_req.using_type = int(self.using_type)
        # 先将整个tenant数组清零
        for i in range(UBS_SSU_MAX_TENANT_LENGTH):
            c_req.tenant[i] = b'\0'
        tenant_bytes = self.tenant[:UBS_SSU_MAX_TENANT_LENGTH - 1]
        for i, b in enumerate(tenant_bytes):
            c_req.tenant[i] = b
        return c_req


@dataclass
class UbsSsuStripedAttachReq:
    """条带化挂载请求参数"""
    dev_name: str = ""  # 聚合后的块设备名称
    level: UbsSsuRaidLevel  # RAID级别
    chunk_size: UbsSsuChunkSize = UbsSsuChunkSize.CHUNK_4K  # chunk大小，单位KB

    def to_c_struct(self) -> UbsSsuStripedAttachReqT:
        c_req = UbsSsuStripedAttachReqT()
        dev_name_bytes = self.dev_name.encode('utf-8')[:UBS_SSU_MAX_DEV_NAME_LENGTH - 1]
        for i in range(UBS_SSU_MAX_DEV_NAME_LENGTH):
            c_req.dev_name[i] = b'\0'
        for i, b in enumerate(dev_name_bytes):
            c_req.dev_name[i] = b
        c_req.level = int(self.level)
        c_req.chunk_size = int(self.chunk_size)
        return c_req
@dataclass
class UbsUbVfe:
    """虚拟功能单元(VFE)信息"""
    slot_id: int = 0
    chip_id: int = 0
    die_id: int = 0
    pfe_id: int = 0
    vfe_id: int = 0

    @staticmethod
    def from_c_struct(c_vfe: "UbsUbVfeT") -> "UbsUbVfe":
        return UbsUbVfe(
            slot_id=c_vfe.slot_id,
            chip_id=c_vfe.chip_id,
            die_id=c_vfe.die_id,
            pfe_id=c_vfe.pfe_id,
            vfe_id=c_vfe.vfe_id,
        )

    def to_dict(self) -> dict:
        return {
            "slot_id": self.slot_id,
            "chip_id": self.chip_id,
            "die_id": self.die_id,
            "pfe_id": self.pfe_id,
            "vfe_id": self.vfe_id,
        }

@dataclass
class UbsUbFe:
    """功能单元(FE)信息，包含所属PFE及其下的VFE列表"""
    slot_id: int = 0
    chip_id: int = 0
    die_id: int = 0
    pfe_id: int = 0
    vfe_list: Optional[List[UbsUbVfe]] = None

    def __post_init__(self):
        if self.vfe_list is None:
            self.vfe_list = []

    @staticmethod
    def from_c_struct(c_fe: "UbsUbFeT") -> "UbsUbFe":
        vfe_list = []
        if c_fe.vfe_cnt > 0:
            if not c_fe.vfe_list:
                raise UbsEngineInternalError("vfe_cnt > 0 but vfe_list is NULL from C SDK")
            for i in range(c_fe.vfe_cnt):
                vfe_list.append(UbsUbVfe.from_c_struct(c_fe.vfe_list[i]))

        return UbsUbFe(
            slot_id=c_fe.slot_id,
            chip_id=c_fe.chip_id,
            die_id=c_fe.die_id,
            pfe_id=c_fe.pfe_id,
            vfe_list=vfe_list,
        )

    def to_dict(self) -> dict:
        return {
            "slot_id": self.slot_id,
            "chip_id": self.chip_id,
            "die_id": self.die_id,
            "pfe_id": self.pfe_id,
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

    @staticmethod
    def from_c_struct(c_info: "UbsSsuConnectInfoT") -> "UbsSsuConnectInfo":
        return UbsSsuConnectInfo(
            src_eid=c_info.src_eid.decode("utf-8", errors="replace").rstrip("\x00"),
            tgt_eid=c_info.tgt_eid.decode("utf-8", errors="replace").rstrip("\x00"),
            tgt_nqn=c_info.tgt_nqn.decode("utf-8", errors="replace").rstrip("\x00"),
            host_nqn=c_info.host_nqn.decode("utf-8", errors="replace").rstrip("\x00"),
            ns_uuid=c_info.ns_uuid.decode("utf-8", errors="replace").rstrip("\x00"),
            ns_id=c_info.ns_id,
        )

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

    @staticmethod
    def from_c_struct(c_stats: "UbsSsuNsStatsT") -> "UbsSsuNsStats":
        return UbsSsuNsStats(
            ns_uuid=c_stats.ns_uuid.decode("utf-8", errors="replace").rstrip("\x00"),
            ns_id=c_stats.ns_id,
            total_size=c_stats.total_size,
            used_size=c_stats.used_size,
        )

    def to_dict(self) -> dict:
        return {
            "ns_uuid": self.ns_uuid,
            "ns_id": self.ns_id,
            "total_size": self.total_size,
            "used_size": self.used_size,
            "usage_percent": round(self.used_size / self.total_size * 100, 2)
            if self.total_size > 0 else 0.0,
        }