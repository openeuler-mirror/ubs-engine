# !/usr/bin/python3
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


class DeviceInfo:
    """设备信息基类"""
    def __init__(self, device_type):
        self.device_type = device_type
        
    def to_dict(self):
        """转换为字典格式"""
        return {
            "device_type": self.device_type
        }


class NICInfo(DeviceInfo):
    """NIC设备信息类"""
    def __init__(self, device_id, guid, bus_instance):
        super().__init__("NIC")
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


class NPUInfo(DeviceInfo):
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


class BUSIInfo(DeviceInfo):
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


class UBCTRLInfo(DeviceInfo):
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
        if device_type == "NIC":
            return NICInfo(**kwargs)
        elif device_type == "NPU":
            return NPUInfo(**kwargs)
        elif device_type == "BUSI":
            return BUSIInfo(**kwargs)
        elif device_type == "UBCTRL":
            return UBCTRLInfo(**kwargs)
        else:
            raise ValueError(f"Unknown device type: {device_type}")
