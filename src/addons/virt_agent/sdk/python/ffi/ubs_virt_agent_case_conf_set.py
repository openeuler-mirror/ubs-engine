# !/usr/bin/python3
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
import ctypes
import json
from typing import Dict, Any

from ubse.ffi.ubs_virt_agent_base import UbsVirtAgentBase
from ubse.ffi.ubs_virt_agent_types import CaseConfSetInfo
from ubse.ffi.ubs_virt_agent_exceptions import UbsVANullPointerError


class UbsVirtAgentCaseConfSet(UbsVirtAgentBase):
    """Case configuration set interface"""

    def __init__(self):
        super().__init__()
        self._setup_topo_functions()

    def ubs_case_conf_set(self, param: Dict[str, Any]) -> int:
        if not self.lib_ubse:
            raise ConnectionError("Native library not loaded")
        if len(param) == 0 or len(param) > 10:
            raise UbsVAInvalidParamError("Too many or too few keys.")
        param_str = json.dumps(param)
        param_bytes = param_str.encode('utf-8')
        param_ptr = ctypes.create_string_buffer(param_bytes)
        case_conf_set_ptr = CaseConfSetInfo()

        result = self.lib_ubse.ubs_virt_agent_case_conf_set(
            param_ptr,
            ctypes.byref(case_conf_set_ptr),
        )

        self.ubs_virt_agent_handle_result(result, "ubs_case_conf_set")

        if not case_conf_set_ptr:
            raise UbsVANullPointerError("Output is null pointer.")
        return case_conf_set_ptr.ret

    def _setup_topo_functions(self):
        """Set up prototypes for log-related functions"""
        self.lib_ubse.ubs_virt_agent_case_conf_set.argtypes = [
            ctypes.c_char_p,
            ctypes.POINTER(CaseConfSetInfo)
        ]
        self.lib_ubse.ubs_virt_agent_case_conf_set.restype = ctypes.c_int32
