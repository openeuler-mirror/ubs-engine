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

import sys
import time
from typing import Dict, Any
from ubse.ubs_virt_agent_fragmentation import ubs_mem_borrow_execute


def do_borrow_execute(param: Dict[str, Any], is_async: bool = False):
    try:
        return ubs_mem_borrow_execute(param, is_async)
    except Exception as e:
        msg = f"Failed to do_borrow_execute, error: {e}"
        print(msg)
        raise RuntimeError(msg) from e


if __name__ == '__main__':
    is_async = False
    if len(sys.argv) > 1:
        if sys.argv[1].lower() == 'true':
            is_async = True
        elif sys.argv[1].lower() == 'false':
            is_async = False
        else:
            print("请输入参数为True或者False，表示是否异步调用")
    else:
        print("没有提供参数, 默认同步调用")

    param1 = {
        "srcParam": {
            "srcNid": "1",
            "srcSocketId": 36,
            "srcNumaId": 0
        },
        "borrowSize": 1048576,
        "destParam": [
            {
                "destNid": "2",
                "destSocketId": 36,
                "destNumaNum": 1,
                "destNumaId": [0],
                "memSize": [1048576]
            }
        ]
    }
    param2 = {
        "srcParam": {
            "srcNid": "1",
            "srcSocketId": 36,
            "srcNumaId": 0
        },
        "borrowSize": 1048576,
        "destParam": [
            {
                "destNid": "2",
                "destSocketId": 36,
                "destNumaNum": 1,
                "destNumaId": [0],
                "memSize": [131072]
            },
            {
                "destNid": "2",
                "destSocketId": 36,
                "destNumaNum": 1,
                "destNumaId": [0],
                "memSize": [131072]
            }
        ]
    }
    result, task_id = do_borrow_execute(param1, is_async)
    # Print the content of result
    print("the task id:", task_id, ", the content of result:", result)
    time.sleep(3)
    result, task_id = do_borrow_execute(param2)
    # Print the content of result
    print("the task id:", task_id, ", the content of result:", result)