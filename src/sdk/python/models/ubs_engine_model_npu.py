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

from _ctypes import POINTER

# 定义C函数原型
UBSE_UB_DEVICE_GUID_SIZE = 32
UBSE_UB_UPI_STR_SIZE = 4


class UbsBusinstanceId:
    # 各段固定长度(共6段)
    _FORMAT = [4, 4, 1, 1, 6, 16]  # 总长32
    _TOTAL_LEN = sum(_FORMAT)  # 32

    def __init__(self, id_str: str = ""):
        """
         初始化UbsBusinstanceId
         :param id_str:
             -  空字符串"" 表示空ID
             -  或格式为'xxxx-xxxx-x-x-xxxxxx-xxxxxxxxxxxxxxxx'的字符串
         """
        if not isinstance(id_str, str):
            raise TypeError("Input must be a string")

        if id_str == "":
            self._is_empty = True
            self._parts = None
        else:
            self._is_empty = False
            parts = id_str.split('-')
            if len(parts) != len(self._FORMAT):
                raise ValueError(f"Expected {len(self._FORMAT)} parts, got {len(parts)} in '{id_str}'")

            validated_parts = []
            for i, (part, expected_len) in enumerate(zip(parts, self._FORMAT)):
                if len(part) != expected_len:
                    raise ValueError(f"Part {i + 1} has length {len(part)}, expected {expected_len}: '{part}'")
                if not all(c in '0123456789abcdefABCDEF' for c in part):
                    raise ValueError(f"Part {i + 1} contains non-hex characters: '{part}'")
                validated_parts.append(part.lower())

            self._parts = validated_parts

    @classmethod
    def from_raw(cls, raw_str: str):
        """从32位连续字符串创建实例 (如'cc08a000000000000000000000010000')"""
        if len(raw_str) == 0:
            return raw_str

        if len(raw_str) != UBSE_UB_DEVICE_GUID_SIZE:
            raise ValueError("Raw string must be exactly 32 characters long")
        if not all(c in '0123456789abcdefABCDEF' for c in raw_str):
            raise ValueError("Raw string must be hexadecimal")

        raw = raw_str.lower()
        # 按照_FORMAT切分
        parts = []
        start = 0
        for length in cls._FORMAT:
            parts.append(raw[start:start + length])
            start += length

        #  用'-'拼接成标准格式再初始化
        formatted = '-'.join(parts)
        return formatted

    @classmethod
    def empty(cls):
        """ 创建一个空的UbsBusinstanceId"""
        return ""

    @property
    def is_empty(self) -> bool:
        """判断是否为空"""
        return self._is_empty

    def to_formatted(self) -> str:
        """ 返回带'-'的标准格式， 空时返回'' """
        if self._is_empty:
            return ""
        return '-'.join(self._parts)

    def to_raw(self) -> str:
        """返回32位连续字符串，空时返回 ''"""
        if self._is_empty:
            return ""
        return ''.join(self._parts)

    def __str__(self):
        return self.to_formatted() if not self._is_empty else "(empty)"

    def __repr__(self):
        if self._is_empty:
            return "UbsBusinstanceId.empty()"
        return f"UbsBusinstanceId('{self.to_formatted()}')"

    def __eq__(self, other):
        if not isinstance(other, UbsBusinstanceId):
            return False
        if self._is_empty and other._is_empty:
            return True
        if self._is_empty or other._is_empty:
            return False
        return self._parts == other._parts

    def __bool__(self):
        """ 支持if device_id: 判断非空"""
        return not self._is_empty


class UbsUbDevicesTypeT(ctypes.Structure):
    _fields_ = [
        ("device_type", ctypes.c_uint32),
        ("slot_id", ctypes.c_uint8),
        ("chip_id", ctypes.c_uint8),
        ("die_id", ctypes.c_uint8),
        ("pf_id", ctypes.c_uint16),
        ("vf_id", ctypes.c_uint16)
    ]

    def __str__(self):
        return (
            f"ubs_ub_device_type(\n"
            f"  device type={self.device_type}\n"
            f"  slot id={self.slot_id}\n"
            f"  chip id={self.chip_id}\n"
            f"  die id={self.die_id}\n"
            f"  pf id={self.pf_id}\n"
            f"  vf id={self.vf_id}\n"
            f")"
        )


class UbsUbAllocDevicesInfoT(ctypes.Structure):
    _fields_ = [
        ("upi_str", ctypes.c_uint8 * UBSE_UB_UPI_STR_SIZE),
        ("bus_instance_guid", ctypes.c_uint8 * UBSE_UB_DEVICE_GUID_SIZE),
        ("ub_dev_list_count", ctypes.c_uint8),
        ("ub_dev_list", POINTER(UbsUbDevicesTypeT))
    ]

    def __str__(self):
        dev_list_str = []
        for i in range(self.ub_dev_list_count):
            dev = self.ub_dev_list[i]
            dev_list_str.append(str(dev))
        return (
            f"ubs_alloc_dev_info(\n"
            f"  upi str={list(self.upi_str)[:UBSE_UB_UPI_STR_SIZE]}\n"
            f"  bus instance guid={list(self.bus_instance_guid)[:UBSE_UB_DEVICE_GUID_SIZE]}\n"
            f"  ub dev list count={self.ub_dev_list_count}\n"
            f"  ub dev list ={dev_list_str}\n"
            f")"
        )


class BusiAttrT(ctypes.Structure):
    _fields_ = [
        ("guid", ctypes.c_uint8 * UBSE_UB_DEVICE_GUID_SIZE),
        ("sub_devices_count", ctypes.c_uint8),
        # 柔性数组在ctypes中表示0长度数组
        ("sub_devices", UbsUbDevicesTypeT * 0)
    ]


class UbsBusiT(ctypes.Structure):
    _fields_ = [
        ("type", ctypes.c_uint8),
        ("attr", POINTER(BusiAttrT))
    ]


class NpuAttrT(ctypes.Structure):
    _fields_ = [
        ("slot_id", ctypes.c_uint8),
        ("chip_id", ctypes.c_uint8),
        ("guid", ctypes.c_uint8 * UBSE_UB_DEVICE_GUID_SIZE),
        ("bus_instance_guid", ctypes.c_uint8 * UBSE_UB_DEVICE_GUID_SIZE),
        ("affinity_devices_count", ctypes.c_uint8),
        ("affinity_devices", UbsUbDevicesTypeT * 0)
    ]


class UbsNpuT(ctypes.Structure):
    _fields_ = [
        ("type", ctypes.c_uint8),
        ("attr", POINTER(NpuAttrT))
    ]


class NicPfeAttrT(ctypes.Structure):
    _fields_ = [
        ("slot_id", ctypes.c_uint8),
        ("chip_id", ctypes.c_uint8),
        ("pf_id", ctypes.c_uint16),
        ("guid", ctypes.c_uint8 * UBSE_UB_DEVICE_GUID_SIZE),
        ("bus_instance_guid", ctypes.c_uint8 * UBSE_UB_DEVICE_GUID_SIZE),
        ("affinity_devices_count", ctypes.c_uint8),
        ("affinity_devices", UbsUbDevicesTypeT * 0)
    ]


class NicVfeAttrT(ctypes.Structure):
    _fields_ = [
        ("slot_id", ctypes.c_uint8),
        ("chip_id", ctypes.c_uint8),
        ("pf_id", ctypes.c_uint16),
        ("vf_id", ctypes.c_uint16),
        ("guid", ctypes.c_uint8 * UBSE_UB_DEVICE_GUID_SIZE),
        ("bus_instance_guid", ctypes.c_uint8 * UBSE_UB_DEVICE_GUID_SIZE),
        ("affinity_devices_count", ctypes.c_uint8),
        ("affinity_devices", UbsUbDevicesTypeT * 0)
    ]


class UbsNicPfeT(ctypes.Structure):
    _fields_ = [
        ("type", ctypes.c_uint8),
        ("attr", POINTER(NicPfeAttrT))
    ]


class UbsNicVfeT(ctypes.Structure):
    _fields_ = [
        ("type", ctypes.c_uint8),
        ("attr", POINTER(NicVfeAttrT))
    ]


class UbctrlAttrT(ctypes.Structure):
    _fields_ = [
        ("slot_id", ctypes.c_uint8),
        ("chip_id", ctypes.c_uint8),
        ("die_id", ctypes.c_uint8),
        ("guid", ctypes.c_uint8 * UBSE_UB_DEVICE_GUID_SIZE),
    ]


class UbsUbctrlT(ctypes.Structure):
    _fields_ = [
        ("type", ctypes.c_uint8),
        ("attr", POINTER(UbctrlAttrT))
    ]


class UbsUbDevicesListT(ctypes.Structure):
    _fields_ = [
        ("ubctrl_ptr", POINTER(UbsUbctrlT)),
        ("ubctrl_cnt", ctypes.c_uint8),
        ("nic_pfe_ptr", POINTER(UbsNicPfeT)),
        ("nic_pfe_cnt", ctypes.c_uint8),
        ("nic_vfe_ptr", POINTER(UbsNicVfeT)),
        ("nic_vfe_cnt", ctypes.c_uint8),
        ("npu_ptr", POINTER(UbsNpuT)),
        ("npu_cnt", ctypes.c_uint8),
        ("busi_ptr", POINTER(UbsBusiT)),
        ("busi_cnt", ctypes.c_uint8)
    ]


class DeviceInfo:
    """设备信息基类"""

    def __init__(self, device_type):
        self.device_type = device_type

    def to_dict(self):
        """转换为字典格式"""
        return {
            "device_type": self.device_type
        }


class NicPfeInfo(DeviceInfo):
    """NIC_PFE设备信息类"""

    def __init__(self, device_id, guid, bus_instance):
        super().__init__("NIC_PFE")
        self.device_id = device_id
        self.guid = guid
        self.bus_instance = bus_instance
        self.affinity_devs = []

    def to_dict(self):
        return {
            "device_type": self.device_type,
            "device_id": self.device_id,
            "guid": self.guid,
            "bus_instance": self.bus_instance,
            "affinity_devs": self.affinity_devs
        }


class NicVfeInfo(DeviceInfo):
    """NIC_VFE设备信息类"""

    def __init__(self, device_id, guid, bus_instance):
        super().__init__("NIC_VFE")
        self.device_id = device_id
        self.guid = guid
        self.bus_instance = bus_instance
        self.affinity_devs = []

    def to_dict(self):
        return {
            "device_type": self.device_type,
            "device_id": self.device_id,
            "guid": self.guid,
            "bus_instance": self.bus_instance,
            "affinity_devs": self.affinity_devs
        }


class NpuInfo(DeviceInfo):
    """NPU设备信息类"""

    def __init__(self, device_id, guid, bus_instance):
        super().__init__("NPU")
        self.device_id = device_id
        self.guid = guid
        self.bus_instance = bus_instance
        self.affinity_devs = []

    def to_dict(self):
        return {
            "device_type": self.device_type,
            "device_id": self.device_id,
            "guid": self.guid,
            "bus_instance": self.bus_instance,
            "affinity_devs": self.affinity_devs
        }


class BusInstanceInfo(DeviceInfo):
    """BUSI设备信息类"""

    def __init__(self, guid):
        super().__init__("BUSI")
        self.guid = guid
        self.sub_devices = []

    def to_dict(self):
        return {
            "device_type": self.device_type,
            "guid": self.guid,
            "sub_devices": self.sub_devices
        }


class UbctrlInfo(DeviceInfo):
    """UBCTRL设备信息类"""

    def __init__(self, device_id):
        super().__init__("UBCTRL")
        self.device_id = device_id

    def to_dict(self):
        return {
            "device_type": self.device_type,
            "device_id": self.device_id
        }


class DeviceFactory:
    """设备信息工厂类"""

    @staticmethod
    def create_device(device_type, **kwargs):
        if device_type == "NIC_PFE":
            return NicPfeInfo(**kwargs)
        elif device_type == "NIC_VFE":
            return NicVfeInfo(**kwargs)
        elif device_type == "NPU":
            return NpuInfo(**kwargs)
        elif device_type == "BUSI":
            return BusInstanceInfo(**kwargs)
        elif device_type == "UBCTRL":
            return UbctrlInfo(**kwargs)
        else:
            raise ValueError(f"Unknown device type: {device_type}")
