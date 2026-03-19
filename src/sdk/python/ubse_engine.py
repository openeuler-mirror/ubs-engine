#!/usr/bin/python3
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
from ubse.ffi.ubs_engine_binding_base import UbsEngineBindingBase

_base_interface = UbsEngineBindingBase()


def ubs_engine_client_initialize(ubs_engine_uds_path: str):
    _base_interface.ubs_engine_client_initialize(ubs_engine_uds_path)


def ubs_engine_client_finalize():
    _base_interface.ubs_engine_client_finalize()
