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

from typing import Dict, Any
from ubse.ffi.ubs_virt_agent_migrate_decision import UbsVirtMigrateDecision
from ubse.ffi.ubs_virt_agent_mem_migrate import UbsVirtMemMigrate

_migrate_decision_interface = UbsVirtMigrateDecision()
_mem_migrate_interface = UbsVirtMemMigrate()


def get_migrate_decision(uuid: str, vm_memory_mb: int, dest_hostname: str, dest_numa_id: int):
    """Obtaining Migration Decision Information.(Use the global interface)"""
    return _migrate_decision_interface.get_migrate_decision(uuid, vm_memory_mb, dest_hostname, dest_numa_id)


def update_page_flow_and_status(params: Dict[str, Any]):
    """
    Update the flow status of hot and cold pages.(Use the global interface)
    :param params:
        opt: "true","false","none_migrating","none_migrate_success","none_migrate_failed"
        uuid: vm uuid
    :return: update result
    """
    return _mem_migrate_interface.update_page_flow_and_status(params)
