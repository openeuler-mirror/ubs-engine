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

from ubse.ffi.ubs_virt_agent_mem_rollback import UbsVirtAgentMemRollback


if __name__ == '__main__':
    mem_rollback = UbsVirtAgentMemRollback()

    node_id = "1"
    borrow_id_list = ["borrow1", "borrow2", "borrow3"]

    try:
        result = mem_rollback.ubs_mem_rollback(node_id, borrow_id_list)
        print(f"Function returned: {result}")
    except Exception as e:
        print(f"Error occurred: {e}")