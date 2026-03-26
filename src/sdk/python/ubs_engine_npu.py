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
from typing import List, Dict

from ubse.ffi.ubs_engine_binding_npu import UbsEngineBindingNpu

_npu_interface = UbsEngineBindingNpu()


def get_host_ub_devices() -> List[Dict]:
    return _npu_interface.ubs_device_list()


def alloc_devices(upi_bytes, bus_guid_bytes, device_list):
    return _npu_interface.ubs_device_alloc(upi_bytes, bus_guid_bytes, device_list)


def free_devices(bus_instance_guid, device_list):
    return _npu_interface.ubs_device_free(bus_instance_guid, device_list)

def query_uba_tid_size(bus_instance_guid):
    return _npu_interface.ubs_query_uba_tid_size(bus_instance_guid)
