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
import ctypes
import logging

from _ctypes import POINTER
from ctypes import create_string_buffer, c_ubyte, cast

# 设置日志记录器
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

from ubse.models.ubs_engine_model_npu import UbsUbDevicesListT, UbsUbAllocDevicesInfoT, UBSE_UB_UPI_STR_SIZE, \
    UBSE_UB_DEVICE_GUID_SIZE, UbsUbDevicesTypeT, UbsDeviceIdT, UbsBusinstanceId, NicAttrT, NpuAttrT, BusiAttrT, \
    DeviceFactory
from ubse.ffi.ubs_engine_exceptions import UbsError, UbsErrNullPointer, UbsEngineConnectionError, UbsEngineAuthError, \
    UbsEngineTimeoutError, UbsEngineInternalError
from ubse.ffi.ubs_engine_binding_base import UbsEngineBindingBase


class UbsEngineBindingNpu(UbsEngineBindingBase):
    """npu相关接口"""

    def __init__(self):
        super().__init__()
        self._setup_npu_functions()

    def ubs_device_list(self):
        """
        查询NPU设备列表
        
        Returns:
            List[DeviceInfo]: 设备信息对象列表
        """
        if not self.lib_ubse:
            raise ConnectionError("Native library not loaded")
        try:
            # 调用设备列表查询函数
            logger.info("Querying NPU device list...")
            result, devlist = self._get_device_list()
            if not devlist:
                logger.debug("NPU device list is null")

            if result != 0:
                raise RuntimeError("Querying device list failed.")
            return self._convert_ubs_list_to_device_list(devlist)
        except Exception as ex:
            logger.error(f"Unexpected error in ubs_device_list: {ex}")
            raise
        finally:
            self._ubs_npu_device_list_free(devlist)

    def ubs_device_alloc(self, upi_bytes, bus_guid_bytes, device_list):
        """
        分配NPU设备
        
        Returns:
            Tuple[str, List[DeviceInfo]]: (新的总线实例GUID, 设备信息对象列表)
        """
        if not self.lib_ubse:
            raise ConnectionError("Native library not loaded")
        try:
            # 调用设备分配函数
            logger.info("Allocating NPU devices...")
            result, new_guid_str, devlist = self._allocate_npu_device(
                upi_bytes, bus_guid_bytes, device_list)
            if result != 0:
                raise RuntimeError("Allocating devices failed.")

            return new_guid_str, self._convert_ubs_list_to_device_list(devlist)
        except Exception as ex:
            logger.error(f"Unexpected error in ubs_device_alloc: {ex}")
            raise
        finally:
            self._ubs_npu_device_list_free(devlist)

    def _prepare_alloc_info(self, bus_instance_guid, device_list, upi_str=None):
        """准备UbsUbAllocDevicesInfoT结构体
        
        Args:
            bus_instance_guid: 总线实例GUID
            device_list: 设备列表
            upi_str: UPI字符串（可选）
            
        Returns:
            UbsUbAllocDevicesInfoT: 准备好的分配信息结构体
        """
        alloc_info = UbsUbAllocDevicesInfoT()

        # 复制UPI字符串（如果提供）
        if upi_str:
            if len(upi_str) > UBSE_UB_UPI_STR_SIZE:
                raise ValueError("The UPI is too long")
            ctypes.memmove(alloc_info.upi_str, upi_str, len(upi_str))

        # 复制总线实例GUID
        raw_bus_instance_guid = UbsBusinstanceId(bus_instance_guid).to_raw()
        if len(raw_bus_instance_guid) > UBSE_UB_DEVICE_GUID_SIZE:
            raise ValueError("The businstance guid is too long")
        ctypes.memmove(alloc_info.bus_instance_guid, raw_bus_instance_guid, len(raw_bus_instance_guid))

        # 设置设备列表
        alloc_info.ub_dev_list_count = len(device_list)
        device_array = (UbsUbDevicesTypeT * len(device_list))()
        for i, dev in enumerate(device_list):
            device_array[i] = self.dict_to_ubs_ub_devices_type_t(dev)
        alloc_info.ub_dev_list = ctypes.cast(device_array, POINTER(UbsUbDevicesTypeT))

        return alloc_info

    def ubs_device_free(self, bus_instance_guid, device_list):
        if not self.lib_ubse:
            raise ConnectionError("Native library not loaded")
        try:
            # 准备分配信息
            alloc_info = self._prepare_alloc_info(bus_instance_guid, device_list)

            # 调用设备释放函数
            result = self.lib_ubse.ubs_npu_device_free(alloc_info)
            if result != 0:
                raise RuntimeError("Freeing NPU device failed.")
        except Exception as ex:
            logger.error(f"Unexpected error in ubs_device_free: {ex}")
            raise

    def ubs_query_uba_tid_size(self, bus_instance_guid):
        if not self.lib_ubse:
            raise ConnectionError("Native library not loaded")
        try:
            logger.debug(f"bus_instance_guid type: {type(bus_instance_guid)}")
            raw_bus_instance_guid = UbsBusinstanceId(bus_instance_guid).to_raw()
            tid = ctypes.c_uint32()
            uba = ctypes.c_uint32()
            size = ctypes.c_uint32()
            buf = create_string_buffer(raw_bus_instance_guid.encode('utf-8'))
            ubyte_ptr = cast(buf, POINTER(c_ubyte))
            result = self.lib_ubse.ubs_uba_tid_size_query(ubyte_ptr, ctypes.byref(tid),
                                                          ctypes.byref(uba), ctypes.byref(size))

            if result != 0:
                raise RuntimeError("Querying uba tid size failed.")
            return tid.value, uba.value, size.value

        except Exception as ex:
            logger.error(f"Unexpected error in ubs_query_uba_tid_size: {ex}")
            raise

    def _ubs_handle_npu_result(self, result: int, operation: str):
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

    def _ubs_npu_device_list_free(self, devlist):
        if not self.lib_ubse:
            raise ConnectionError("Native library not loaded")
        self.lib_ubse.ubs_npu_device_list_free(ctypes.byref(devlist))
        logger.debug("Successfully freed NPU device list")

    def _convert_ubs_list_to_py_list(self, devlist):
        """将UBS设备列表转换为Python字典列表"""
        reslist = []

        # 处理不同类型的设备
        reslist.extend(self._process_nic_devices(devlist, as_dict=True))
        reslist.extend(self._process_npu_devices(devlist, as_dict=True))
        reslist.extend(self._process_busi_devices(devlist, as_dict=True))
        reslist.extend(self._process_ubctrl_devices(devlist, as_dict=True))

        return reslist

    def _convert_ubs_list_to_device_list(self, devlist):
        """将UBS设备列表转换为设备信息对象列表"""
        reslist = []

        # 处理不同类型的设备
        reslist.extend(self._process_nic_devices(devlist, as_dict=False))
        reslist.extend(self._process_npu_devices(devlist, as_dict=False))
        reslist.extend(self._process_busi_devices(devlist, as_dict=False))
        reslist.extend(self._process_ubctrl_devices(devlist, as_dict=False))

        return reslist

    def _process_nic_devices(self, devlist, as_dict=True):
        """处理NIC设备列表"""
        nic_devices = []
        for i in range(devlist.nic_cnt):
            nic_ptr = devlist.nic_ptr[i]
            attr = nic_ptr.attr.contents

            # 创建设备信息
            device_info = self._create_nic_info(attr)

            # 添加关联设备
            self._add_affinity_devices(
                device_info, attr, NicAttrT.affinity_devices.offset,
                attr.affinity_devices_count
            )

            if as_dict:
                nic_devices.append(device_info.to_dict())
            else:
                nic_devices.append(device_info)

        return nic_devices

    def _process_npu_devices(self, devlist, as_dict=True):
        """处理NPU设备列表"""
        npu_devices = []
        for i in range(devlist.npu_cnt):
            npu_ptr = devlist.npu_ptr[i]
            attr = npu_ptr.attr.contents

            # 创建设备信息
            device_info = self._create_npu_info(attr)

            # 添加关联设备
            self._add_affinity_devices(
                device_info, attr, NpuAttrT.affinity_devices.offset,
                attr.affinity_devices_count
            )

            if as_dict:
                npu_devices.append(device_info.to_dict())
            else:
                npu_devices.append(device_info)

        return npu_devices

    def _process_busi_devices(self, devlist, as_dict=True):
        """处理BUSI设备列表"""
        busi_devices = []
        for i in range(devlist.busi_cnt):
            busi_ptr = devlist.busi_ptr[i]
            attr = busi_ptr.attr.contents

            # 创建设备信息
            device_info = self._create_busi_info(attr)

            # 添加子设备
            self._add_sub_devices(
                device_info, attr, BusiAttrT.sub_devices.offset,
                attr.sub_devices_count
            )

            if as_dict:
                busi_devices.append(device_info.to_dict())
            else:
                busi_devices.append(device_info)

        return busi_devices

    def _process_ubctrl_devices(self, devlist, as_dict=True):
        """处理UBCTRL设备列表"""
        ubctrl_devices = []
        for i in range(devlist.ubctrl_cnt):
            ubctrl_ptr = devlist.ubctrl_ptr[i]
            attr = ubctrl_ptr.attr.contents

            # 创建设备信息
            device_info = self._create_ubctrl_info(attr)

            if as_dict:
                ubctrl_devices.append(device_info.to_dict())
            else:
                ubctrl_devices.append(device_info)

        return ubctrl_devices

    def _get_guid_string(self, guid_bytes):
        """将GUID字节数组转换为字符串"""
        return bytes(guid_bytes).rstrip(b'\x00').decode('utf-8')

    def _get_guid_and_bus_instance(self, attr):
        """获取GUID和bus_instance字符串"""
        guid_str = self._get_guid_string(attr.guid)
        bus_ins_guid = self._get_guid_string(attr.bus_instance_guid)
        return guid_str, bus_ins_guid

    def _get_device_id(self, dev_type, devId):
        """根据设备类型和设备ID结构体构建设备ID字符串
        
        Args:
            dev_type: 设备类型字符串，必须是 "BUSI", "NPU", "NIC", "UBCTRL" 之一
            devId: 设备ID结构体
        
        Returns:
            str: 设备ID字符串
        
        Raises:
            ValueError: 如果设备类型不是字符串或不是合法值
        """
        # 验证设备类型是否为字符串
        if not isinstance(dev_type, str):
            raise ValueError(f"Device type must be a string, got {type(dev_type).__name__}")

        # 验证设备类型是否为合法值
        valid_device_types = {"BUSI", "NPU", "NIC", "UBCTRL"}
        device_type = dev_type.upper()

        if device_type not in valid_device_types:
            raise ValueError(f"Invalid device type: {dev_type}. Must be one of {valid_device_types}")

        # 直接判断是否是NPU设备来决定ID格式
        if device_type == "NPU":
            return f"{devId.slot_id}-{devId.chip_id}"
        else:
            return f"{devId.slot_id}-{devId.chip_id}-{devId.index}"

    def _convert_device_type_and_id(self, dev_type_num, devId):
        """根据设备类型和设备ID结构体转换为设备类型字符串和设备ID字符串
        
        Args:
            dev_type_num: 设备类型数字(1-4)
            devId: 设备ID结构体
        
        Returns:
            tuple: (device_type, device_id)
        
        Raises:
            ValueError: 如果设备类型不是合法值
        """
        # 设备类型映射字典
        DEVICE_TYPE_MAP = {
            1: "BUSI",
            2: "NPU",
            3: "NIC",
            4: "UBCTRL"
        }

        # 验证设备类型是否为合法值
        if dev_type_num not in DEVICE_TYPE_MAP:
            raise ValueError(
                f"Invalid device type number: {dev_type_num}. Must be one of {set(DEVICE_TYPE_MAP.keys())}")

        device_type = DEVICE_TYPE_MAP[dev_type_num]

        # 构建设备ID字符串
        if device_type == "NPU":
            device_id = f"{devId.slot_id}-{devId.chip_id}"
        else:
            device_id = f"{devId.slot_id}-{devId.chip_id}-{devId.index}"

        return device_type, device_id

    def _create_nic_info(self, attr):
        """创建NIC设备信息"""
        _device_id = self._get_device_id("NIC", attr.device_id)
        guid_str, bus_ins_guid = self._get_guid_and_bus_instance(attr)

        return DeviceFactory.create_device(
            "NIC",
            device_id=_device_id,
            guid=UbsBusinstanceId.from_raw(guid_str),
            bus_instance=UbsBusinstanceId.from_raw(bus_ins_guid)
        )

    def _create_npu_info(self, attr):
        """创建NPU设备信息"""
        _device_id = self._get_device_id("NPU", attr.device_id)
        guid_str, bus_ins_guid = self._get_guid_and_bus_instance(attr)

        return DeviceFactory.create_device(
            "NPU",
            device_id=_device_id,
            guid=UbsBusinstanceId.from_raw(guid_str),
            bus_instance=UbsBusinstanceId.from_raw(bus_ins_guid)
        )

    def _create_busi_info(self, attr):
        """创建BUSI设备信息"""
        guid_str = self._get_guid_string(attr.guid)

        return DeviceFactory.create_device(
            "BUSI",
            guid=UbsBusinstanceId.from_raw(guid_str)
        )

    def _create_ubctrl_info(self, attr):
        """创建UBCTRL设备信息"""
        _device_id = self._get_device_id("UBCTRL", attr.device_id)

        return DeviceFactory.create_device(
            "UBCTRL",
            device_id=_device_id
        )

    def _add_affinity_devices(self, device_info, attr, offset, count):
        """添加关联设备信息"""
        for j in range(count):
            affinity_device_ptr = ctypes.cast(
                ctypes.addressof(attr) + offset +
                ctypes.sizeof(UbsUbDevicesTypeT) * j,
                POINTER(UbsUbDevicesTypeT)
            )

            try:
                _device_type, _device_id = self._convert_device_type_and_id(
                    affinity_device_ptr.contents.device_type,
                    affinity_device_ptr.contents.device_id
                )
                device_info.affinity_devs.append({
                    "device_type": _device_type,
                    "device_id": _device_id
                })
            except ValueError:
                # 忽略无效的设备类型
                continue

    def _add_sub_devices(self, device_info, attr, offset, count):
        """添加子设备信息"""
        for j in range(count):
            sub_device_ptr = ctypes.cast(
                ctypes.addressof(attr) + offset +
                ctypes.sizeof(UbsUbDevicesTypeT) * j,
                POINTER(UbsUbDevicesTypeT)
            )

            try:
                _device_type, _device_id = self._convert_device_type_and_id(
                    sub_device_ptr.contents.device_type,
                    sub_device_ptr.contents.device_id
                )
                device_info.sub_devices.append({
                    "device_type": _device_type,
                    "device_id": _device_id
                })
            except ValueError:
                # 忽略无效的设备类型
                continue

    def _get_device_list(self):
        """
        获取NPU设备列表
        Returns:
            (ret_code, ubse_devices, device_cnt)
        """
        if self.lib_ubse is None:
            raise RuntimeError("npu library is not loaded")

        # 准备输出参数
        devlist = UbsUbDevicesListT()

        # 调用设备列表查询函数
        result = self.lib_ubse.ubs_npu_device_list_query(
            ctypes.byref(devlist)
        )
        if not devlist:
            logger.debug("NPU ubse_devices is null")

        return result, devlist

    def parse_device_id(self, device_id_str):
        slot_id, chip_id, index = map(int, device_id_str.split('-'))
        return UbsDeviceIdT(slot_id, chip_id, index)

    def dict_to_ubs_ub_devices_type_t(self, device_dict):
        device_type = device_dict["device_type"]
        device_id_str = device_dict["device_id"]

        if device_type == "NPU":
            device_id_str += "-255"

        # 将device_type转换为c_ubyte
        device_type_value = {
            "BUSI": 1,
            "NPU": 2,
            "NIC": 3,
            "UBCTRL": 4
        }.get(device_type, 0)  # 默认值为 0

        # 将 device_id 转换为UbsDeviceIdT
        device_id = self.parse_device_id(device_id_str)

        return UbsUbDevicesTypeT(device_type_value, device_id)

    def _allocate_npu_device(self, upi_str, bus_instance_guid, device_list):
        """
        获取NPU设备列表
        Returns:
            (ret_code, ubse_devices, device_cnt)
        """
        if self.lib_ubse is None:
            raise RuntimeError("npu library is not loaded")

        # 准备分配信息
        alloc_info = self._prepare_alloc_info(bus_instance_guid, device_list, upi_str)

        # 准备输出参数
        new_bus_instance_guid = (ctypes.c_uint8 * UBSE_UB_DEVICE_GUID_SIZE)()
        devList = UbsUbDevicesListT()

        # 调用设备分配函数
        result = self.lib_ubse.ubs_npu_device_alloc(
            ctypes.byref(alloc_info),
            new_bus_instance_guid,
            devList
        )

        # 将输出转换为Python类型
        new_guid_str = UbsBusinstanceId().from_raw(self._get_guid_string(new_bus_instance_guid))
        return result, new_guid_str, devList

    def _setup_npu_functions(self):
        """设置相关原型"""
        # 定义函数原型
        self.lib_ubse.ubs_engine_client_initialize.argtypes = [ctypes.c_char_p]
        self.lib_ubse.ubs_engine_client_initialize.restype = ctypes.c_int32

        self.lib_ubse.ubs_error_name.argtypes = [ctypes.c_int32]  # int32_t error
        self.lib_ubse.ubs_error_name.restype = ctypes.c_char_p  # const char *

        self.lib_ubse.ubs_error_string.argtypes = [ctypes.c_int32]  # int32_t error
        self.lib_ubse.ubs_error_string.restype = ctypes.c_char_p  # const char *

        # npu
        self.lib_ubse.ubs_npu_device_list_query.argtypes = [POINTER(UbsUbDevicesListT)]
        self.lib_ubse.ubs_npu_device_list_query.restype = ctypes.c_int32

        self.lib_ubse.ubs_npu_device_alloc.argtypes = [POINTER(UbsUbAllocDevicesInfoT), POINTER(ctypes.c_uint8),
                                                       POINTER(UbsUbDevicesListT)]
        self.lib_ubse.ubs_npu_device_alloc.restype = ctypes.c_int32

        self.lib_ubse.ubs_npu_device_list_free.argtypes = [POINTER(UbsUbDevicesListT)]
        self.lib_ubse.ubs_npu_device_list_free.restype = None

        self.lib_ubse.ubs_uba_tid_size_query.argtypes = [POINTER(ctypes.c_uint8), POINTER(ctypes.c_uint32),
                                                         POINTER(ctypes.c_uint32), POINTER(ctypes.c_uint32)]
        self.lib_ubse.ubs_uba_tid_size_query.restype = ctypes.c_int32
