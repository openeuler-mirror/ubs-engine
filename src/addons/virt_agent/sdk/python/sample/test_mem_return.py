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
from ubse.ubs_virt_agent_fragmentation import ubs_mem_return


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
    try:
        result, task_id = ubs_mem_return(is_async)
        print(f"Function returned: {result}, task_id: {task_id}")
    except Exception as e:
        print(f"Error occurred: {e}")
