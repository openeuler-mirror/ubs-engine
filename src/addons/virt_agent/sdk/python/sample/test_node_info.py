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
from ubse.ffi.ubs_virt_agent_node_info import UbsVirtAgentNodeInfo


def get_node_numa_infos():
    virt_agent_query = UbsVirtAgentNodeInfo()
    try:
        virt_agent_query.ubs_virt_agent_initialize()
        return virt_agent_query.ubs_node_info_list()
    except Exception as e:
        msg = f"Failed to get_node_numa_infos, error: {e}"
        print(msg)
        raise RuntimeError(msg) from e
    finally:
        virt_agent_query.ubs_virt_agent_finalize()


if __name__ == '__main__':
    node_info = get_node_numa_infos()
    if not node_info.numa_infos:
        print("ERROR: NumaInfo is empty!")

    print(f'hostId = {type(node_info.numa_infos[0].host_id)}\n'
          f'hostname = {type(node_info.numa_infos[0].hostname)}\n'
          f'socketId = {type(node_info.numa_infos[0].socket_id)}\n'
          f'numaId = {type(node_info.numa_infos[0].numa_id)}\n'
          f'MemTotal = {type(node_info.numa_infos[0].mem_total)}\n'
          f'MemFree = {type(node_info.numa_infos[0].mem_free)}')

    print(f'{socket.gethostname()}的 numa信息如下：')
    for obj in node_info.numa_infos:
        print(f'    hostId={obj.host_id} {obj.hostname}-socket{obj.socket_id}-numa{obj.numa_id} '
              f'MemTotal={obj.mem_total} MemFree={obj.mem_free}')
