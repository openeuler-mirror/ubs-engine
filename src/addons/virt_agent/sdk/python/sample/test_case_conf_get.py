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

import socket
from ubse.ffi.ubs_virt_agent_case_conf_get import UbsVirtAgentCaseConfGet


def get_case_conf_info():
    virt_agent_query = UbsVirtAgentCaseConfGet()
    try:
        virt_agent_query.ubs_virt_agent_initialize()
        return virt_agent_query.ubs_case_conf_info()
    except Exception as e:
        msg = f"Failed to get_vm_numa_infos, error: {e}"
        print(msg)
        raise RuntimeError(msg) from e
    finally:
        virt_agent_query.ubs_virt_agent_finalize()


if __name__ == '__main__':
    case_conf_info = get_case_conf_info()
    if not case_conf_info:
        print("Case conf info is empty!")
    else:
        print(f'{socket.gethostname()}的 case conf信息如下：')
        print(f'cur_case = {type(case_conf_info.case_type)}\n'
              f'over_commitment_ratio = {type(case_conf_info.over_commitment)}\n'
              f'index = {type(case_conf_info.index)}\n'
              f'migrate_waterLine = {type(case_conf_info.migrate_water_line)}\n'
              f'host_id = {type(case_conf_info.host_id)}\n')

        print(case_conf_info.__dict__)
