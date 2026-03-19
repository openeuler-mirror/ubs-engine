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

from ubse.ffi.ubs_virt_agent_migrate_decision import UbsVirtMigrateDecision, MigrateStrategy


def make_migrate_strategy_decision(uuid: str, vm_memory_mb: int, dest_hostname: str, dest_numa_id: int):
    """
    Adaptive live migration strategy decision.
    Input parameters:
        uuid: Virtual machine UUID
        vm_memory_mb: Virtual machine memory size (MB)
        dest_hostname: Destination node hostname
        dest_numa_id: Destination node NUMA ID

    Returns:
        MigrateStrategy: The migration strategy

    Raises:
        RuntimeError: If the underlying C interface returns a non-zero value
    """
    virt_agent_migrate_decision = UbsVirtMigrateDecision()
    try:
        res = virt_agent_migrate_decision.get_migrate_decision(uuid, vm_memory_mb, dest_hostname, dest_numa_id)
        print(f"get migrate decision result is {res}")
        return res
    except Exception as e:
        msg = f"Failed to get migrate strategy, error: {e}"
        print(msg)
        raise RuntimeError(msg) from e
    finally:
        virt_agent_migrate_decision.ubs_virt_agent_finalize()


if __name__ == '__main__':

    print("Start to get migrate strategy")
    unit_gb = 1024
    result = make_migrate_strategy_decision("1863ec26-3fda-4f6f-97b2-fcaf9139ac7d", 8 * unit_gb, "computer02", 0)
    print(f"When all conditions required by ham migrate are met, result is : {result}")
    result = make_migrate_strategy_decision("1863ec26-3fda-4f6f-97b2-fcaf9139ac7d", 4 * unit_gb, "computer02", 0)
    print(f"When memory of vm less then 8G, result is : {result}")
    result = make_migrate_strategy_decision("1863ec26-3fda-4f6f-97b2-fcaf9139ac7d", 8 * unit_gb, "computer02", 1)
    print(f"When node0 - node1, result is : {result}")
