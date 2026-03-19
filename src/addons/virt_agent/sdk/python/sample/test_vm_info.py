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
from ubse.ffi.ubs_virt_agent_vm_info import UbsVirtAgentVmInfo


def get_node_vm_infos():
    virt_agent_query = UbsVirtAgentVmInfo()
    try:
        virt_agent_query.ubs_virt_agent_initialize()
        return virt_agent_query.ubs_vm_info_list()
    except Exception as e:
        msg = f"Failed to get_vm_numa_infos, error: {e}"
        print(msg)
        raise RuntimeError(msg) from e
    finally:
        virt_agent_query.ubs_virt_agent_finalize()


if __name__ == '__main__':
    vm_info = get_node_vm_infos()
    if not vm_info.vm_infos:
        print("VmInfo is empty!")
    else:
        print(f'{socket.gethostname()}的 vm信息如下：')
        for obj in vm_info.vm_infos:
            print(obj)
