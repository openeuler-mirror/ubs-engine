#!/usr/bin/python3
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# virtagent is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
from __future__ import annotations

import ctypes
import gc
from enum import IntEnum
from ubse.ffi.ubs_virt_agent_exceptions import UbsVirtAgentError, UbsVABaseError, UbsVAInvalidParamError, \
    UbsVANullPointerError, UbsVAMemAllocError, UbsVAMemCopyError, UbsVASerializeError, UbsVADeserializeError

LIB_PATH = '/usr/lib64/libubs-virt-agent.so'


class UbsVirtAgentErrorCode(IntEnum):
    BASE = 2000
    INVALID_PARAM = 2001
    NULL_POINTER = 2002
    MEM_ALLOCATE_FAILED = 2003
    MEM_COPY_FAILED = 2004
    SERIALIZATION_FAILED = 2005
    DESERIALIZATION_FAILED = 2006


class UbsVirtAgentBase:
    def __init__(self):
        self.lib_ubse = None
        self._handle = None
        self._load_library()
        self._setup_function_prototypes()

    @staticmethod
    def ubs_virt_agent_handle_result(result: int, operation: str):
        """
            If the error is a self-defined error, the system throws an exception.
        Otherwise, the system does not process the error.
        :param result: Ret code
        :param operation: Operation type
        :return: None
        """
        error_map = {
            UbsVirtAgentErrorCode.BASE: ("UBS_VA_ERROR_BASE", UbsVABaseError),
            UbsVirtAgentErrorCode.INVALID_PARAM: ("UBS_VA_ERROR_INVALID_PARAM", UbsVAInvalidParamError),
            UbsVirtAgentErrorCode.NULL_POINTER: ("UBS_VA_ERROR_NULL_POINTER", UbsVANullPointerError),
            UbsVirtAgentErrorCode.MEM_ALLOCATE_FAILED: ("UBS_VA_ERROR_MEM_ALLOCATE_FAILED", UbsVAMemAllocError),
            UbsVirtAgentErrorCode.MEM_COPY_FAILED: ("UBS_VA_ERROR_MEM_COPY_FAILED", UbsVAMemCopyError),
            UbsVirtAgentErrorCode.SERIALIZATION_FAILED: ("UBS_VA_ERROR_SERIALIZE_FAILED", UbsVASerializeError),
            UbsVirtAgentErrorCode.DESERIALIZATION_FAILED: ("UBS_VA_ERROR_DESERIALIZE_FAILED", UbsVADeserializeError),
            # Add more error code mappings.
        }
        if result in error_map:
            error_info = error_map.get(result, (f"error: {result}", UbsVirtAgentError))
            raise error_info[1](f"{operation} failed: {error_info[0]}")

    def ubs_virt_agent_initialize(self: str, conf_path: str | None = None) -> None:
        pass

    def ubs_virt_agent_finalize(self) -> None:
        del self.lib_ubse
        gc.collect()

    def _setup_function_prototypes(self):
        pass

    def _load_library(self):
        self.lib_ubse = ctypes.CDLL(LIB_PATH)
