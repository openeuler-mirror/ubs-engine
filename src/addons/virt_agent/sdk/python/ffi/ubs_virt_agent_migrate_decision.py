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
from ctypes import c_char_p, c_uint32, POINTER
from enum import Enum

from ubse.ffi.ubs_virt_agent_base import UbsVirtAgentBase


class MigrateStrategy(Enum):
    MULTI_COPY_MIGRATE = 0
    ONE_COPY_MIGRATE = 1
    HAM_MIGRATE = 2


class UbsVirtMigrateDecision(UbsVirtAgentBase):
    """Obtaining Migration Decisions"""

    def __init__(self):
        super().__init__()
        self.default_migrate_strategy = MigrateStrategy.MULTI_COPY_MIGRATE.value
        self._setup_topo_functions()

    def get_migrate_decision(self, uuid: str, vm_memory_mb: int, dest_hostname: str, dest_numa_id: int):

        if None in [vm_memory_mb, uuid, dest_hostname, dest_numa_id]:
            return self.default_migrate_strategy
        if not self.lib_ubse:
            raise ConnectionError("Native library not loaded")
        migrate_strategy = c_uint32(0)
        result = self.lib_ubse.ubs_virt_agent_make_migrate_decision(
            ctypes.c_uint32(vm_memory_mb),
            c_char_p(uuid.encode('utf-8')),
            c_char_p(dest_hostname.encode('utf-8')),
            ctypes.c_uint32(dest_numa_id),
            ctypes.byref(migrate_strategy)
        )
        return migrate_strategy.value if result == 0 else self.default_migrate_strategy

    def _setup_topo_functions(self):
        """Sets the parameter type and return value type of the C function"""
        self.lib_ubse.ubs_virt_agent_make_migrate_decision.argtypes = [
            c_uint32,
            c_char_p,
            c_char_p,
            c_uint32,
            POINTER(c_uint32)
        ]
        self.lib_ubse.ubs_virt_agent_make_migrate_decision.restype = c_uint32
