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
from typing import Dict, Any

from ubse.ffi.ubs_virt_agent_base import UbsVirtAgentBase


class UbsVirtMemMigrate(UbsVirtAgentBase):
    """Mem Migrate"""

    def __init__(self):
        super().__init__()
        self._setup_topo_functions()

    def update_page_flow_and_status(self, params: Dict[str, Any]):
        """
        update page flow and status
        :param params:
            opt: "true"\"false"\"none_migrating"\"none_migrate_success"\"none_migrate_failed"
            uuid: vm uuid
        :return: update result
        """
        if not self.lib_ubse:
            raise ConnectionError("Native library not loaded")
        # pares params
        opt = params.get('opt', '')
        uuid = params.get('uuid', '')
        if not isinstance(opt, str):
            opt = str(opt)
        if not isinstance(uuid, str):
            uuid = str(uuid)

        result = self.lib_ubse.update_page_flow_and_status(
            opt.encode('utf-8'),
            uuid.encode('utf-8')
        )
        self.ubs_virt_agent_handle_result(result, "update_page_flow_and_status")
        return result

    def _setup_topo_functions(self):
        """Sets the parameter type and return value type of the C function"""
        self.lib_ubse.update_page_flow_and_status.argtypes = [
            ctypes.c_char_p,
            ctypes.c_char_p,
        ]
        self.lib_ubse.update_page_flow_and_status.restype = ctypes.c_int32
