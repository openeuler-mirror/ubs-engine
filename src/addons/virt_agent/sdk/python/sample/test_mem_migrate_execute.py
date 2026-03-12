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
from ubse.ffi.ubs_virt_agent_mem_migrate_execute import UbsVirtAgentMemMigrateExecute


def get_migrate_execute(param: Dict[str, Any]):
    virt_agent_query = UbsVirtAgentMemMigrateExecute()
    try:
        virt_agent_query.ubs_virt_agent_initialize()
        return virt_agent_query.ubs_mem_migrate_execute(param)
    except Exception as e:
        msg = f"Failed to get_migrate_execute, error: {e}"
        print(msg)
        raise RuntimeError(msg) from e
    finally:
        virt_agent_query.ubs_virt_agent_finalize()


if __name__ == '__main__':
    param = {
        "borrowInNode": "3",
        "borrowIds": ["3@c5e8020c666bc1457a4c892281b8c3ad", "3@2a8c549d8d287dbacbf41cf31ebe453c"],
        "vmInfoList": [
            {
                "pid": 1844014,
                "memSize": 106496,
                "destNumaId": 12
            },
            {
                "pid": 1844010,
                "memSize": 24576,
                "destNumaId": 12
            }
        ],
        "waitingTime": 10001
    }
    result = get_migrate_execute(param)
    # Print the content of result
    print("the content of result:", result)