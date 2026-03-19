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
from ubse.ubs_virt_agent_fragmentation import ubs_task_result_query


if __name__ == '__main__':
    if len(sys.argv) > 1:
        task_id = sys.argv[1]
    else:
        print("没有提供参数, 请提供任务id")
        sys.exit(1)

    try:
        result, task_info = ubs_task_result_query(task_id)
        print(f"Function returned: {result}, task_id: {task_info}")
    except Exception as e:
        print(f"Error occurred: {e}")
