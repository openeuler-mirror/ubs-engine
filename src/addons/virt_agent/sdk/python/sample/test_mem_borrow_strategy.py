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
from ubse.ffi.ubs_virt_agent_mem_borrow_strategy import UbsVirtAgentMemBorrowStrategy


def get_borrow_strategy(param: Dict[str, Any]):
    mem_borrow_strategy = UbsVirtAgentMemBorrowStrategy()
    try:
        mem_borrow_strategy.ubs_virt_agent_initialize()
        return mem_borrow_strategy.ubs_mem_borrow_strategy(param)
    except Exception as e:
        msg = f"Failed to get_borrow_strategy, error: {e}"
        print(msg)
        raise RuntimeError(msg) from e
    finally:
        mem_borrow_strategy.ubs_virt_agent_finalize()


if __name__ == '__main__':
    param = {
    "srcParam": {
        "srcNid": "1",
        "srcSocketId": 36,
        "srcNumaId": 0
    },
    "borrowSize": 1048576
    }
    result = get_borrow_strategy(param)
    # Print the content of result
    print("the content of result:", result)