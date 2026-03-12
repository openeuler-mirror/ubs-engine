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

from ubse.ffi.ubs_virt_agent_base import UbsVirtAgentBase
from ubse.models.ubs_virt_agent_model import CaseConfInfoT
from ubse.ffi.ubs_virt_agent_types import CaseConfInfo


class UbsVirtAgentCaseConfGet(UbsVirtAgentBase):
    """Case configuration query interface"""

    def __init__(self):
        super().__init__()
        self._setup_topo_functions()

    def ubs_case_conf_info(self) -> CaseConfInfoT:
        if not self.lib_ubse:
            raise ConnectionError("Native library not loaded")
        case_conf_info_ptr = CaseConfInfo()
        result = self.lib_ubse.ubs_virt_agent_case_conf_get(
            ctypes.byref(case_conf_info_ptr)
        )

        self.ubs_virt_agent_handle_result(result, "ubs_case_conf_info")

        if not case_conf_info_ptr:
            return CaseConfInfoT(
                case_type="",
                over_commitment="",
                migrate_water_line="",
                index=0,
                host_id="0"
            )
        return CaseConfInfoT.from_c_struct(case_conf_info_ptr)

    def _setup_topo_functions(self):
        """Set up prototypes for log-related functions"""
        self.lib_ubse.ubs_virt_agent_case_conf_get.argtypes = [
            ctypes.POINTER(CaseConfInfo)
        ]
        self.lib_ubse.ubs_virt_agent_case_conf_get.restype = ctypes.c_int32

        # Set up the free function (if it exists)
        if hasattr(self.lib_ubse, 'free'):
            self.lib_ubse.free.argtypes = [ctypes.POINTER(CaseConfInfo)]
            self.lib_ubse.free.restype = None
