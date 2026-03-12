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
from ubse.ffi.ubs_virt_agent_mem_migrate import UbsVirtMemMigrate


def call_update_page_flow_and_status(params: Dict[str, Any]):
    virt_agent_mem_migrate = UbsVirtMemMigrate()
    try:
        res = virt_agent_mem_migrate.update_page_flow_and_status(params)
        print(f"update page flow and status result is {res}")
        return res
    except Exception as e:
        msg = f"Failed to update page flow and status, error: {e}"
        print(msg)
        raise RuntimeError(msg) from e
    finally:
        virt_agent_mem_migrate.ubs_virt_agent_finalize()


if __name__ == '__main__':
    print("Start to update page flow and status")
    params = {
        'opt': "true",
        'uuid': "xxxxx"
    }
    result = call_update_page_flow_and_status(params)
