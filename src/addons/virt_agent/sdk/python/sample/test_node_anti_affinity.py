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

from ubse.ffi.ubs_virt_agent_node_anti_affinity import UbsVirtAgentNodeAntiAffinity


if __name__ == '__main__':
    anti_affinity = UbsVirtAgentNodeAntiAffinity()
    node_anti_affinity_map = {
        "1": ["2"],
        "2": ["3"],
        "3": ["4"],
        "4": ["1"]
    }

    try:
        result = anti_affinity.ubs_node_anti_affinity(node_anti_affinity_map)
        print("Function returned:", result)
    except Exception as e:
        print("Error occurred:", e)
