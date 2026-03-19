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
from __future__ import annotations

import ctypes

LIB_PATH = '/usr/lib64/libubse-client.so.1'


class UbsEngineBindingBase:
    def __init__(self):
        self.lib_ubse = None
        self._handle = None
        self._load_library()
        self._setup_function_prototypes()

    def ubs_engine_client_initialize(self: str, conf_path: str | None = None) -> None:
        path = conf_path.encode() if conf_path else None
        ret = self.lib_ubse.ubs_engine_client_initialize(path)
        if ret != 0:
            raise RuntimeError(f"ubs_engine_client_initialize failed, code={ret}")

    def ubs_engine_client_finalize(self) -> None:
        self.lib_ubse.ubs_engine_client_finalize()

    def _setup_function_prototypes(self):
        self.lib_ubse.ubs_engine_client_initialize.argtypes = [ctypes.c_char_p]
        self.lib_ubse.ubs_engine_client_initialize.restype = ctypes.c_int32
        self.lib_ubse.ubs_engine_client_finalize.argtypes = []
        self.lib_ubse.ubs_engine_client_finalize.restype = None

    def _load_library(self):
        self.lib_ubse = ctypes.CDLL(LIB_PATH)
