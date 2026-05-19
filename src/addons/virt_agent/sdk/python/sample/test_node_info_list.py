#  Copyright (c) Huawei Technologies Co., Ltd. $tody.year. All rights reserved.
#
#  virtagent is licensed under Mulan PSL v2.
#  You can use this software according to the terms and conditions of the Mulan PSL v2.
#  You may obtain a copy of Mulan PSL v2 at:
#           http://license.coscl.org.cn/MulanPSL2
#  THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
#  EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
#  MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
#  See the Mulan PSL v2 for more details.
#
#  virtagent is licensed under Mulan PSL v2.
#  You can use this software according to the terms and conditions of the Mulan PSL v2.
#  You may obtain a copy of Mulan PSL v2 at:
#           http://license.coscl.org.cn/MulanPSL2
#  THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
#  EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
#  MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
#  See the Mulan PSL v2 for more details.

#!/usr/bin/python3
"""
测试节点信息列表查询接口

该测试演示如何使用 ubs_mem_fragmentation_node_info_list 接口查询系统中所有节点的
NUMA拓扑信息和内存状态。
"""

from ubse.ffi.ubs_virt_agent_node_info_list import UbsVirtAgentNodeInfoList


def test_node_info_list():
    """
    测试节点信息列表查询功能
    
    Returns:
        list: 节点信息列表，每个元素包含节点ID、NUMA信息等
    """
    node_info_list_client = UbsVirtAgentNodeInfoList()
    try:
        node_info_list_client.ubs_virt_agent_initialize()
        return node_info_list_client.ubs_mem_fragmentation_node_info_list()
    except Exception as e:
        msg = f"Failed to query node info list, error: {e}"
        print(msg)
        raise RuntimeError(msg) from e
    finally:
        node_info_list_client.ubs_virt_agent_finalize()


def print_node_info(node_info):
    """
    打印单个节点的详细信息
    
    Args:
        node_info: NodeInfoT 对象，包含节点信息
    """
    print(f"\n{'='*60}")
    print(f"节点ID: {node_info.node_id}")
    print(f"是否为当前节点: {'是' if node_info.is_current else '否'}")
    print(f"NUMA节点数量: {len(node_info.numa_infos)}")
    
    for i, numa_info in enumerate(node_info.numa_infos, 1):
        print(f"\n  NUMA节点 {i}:")
        print(f"    NUMA ID: {numa_info.numa_id}")
        print(f"    Socket ID: {numa_info.socket_id}")
        print(f"    是否本地: {'是' if numa_info.is_local else '否'}")
        print(f"    总内存: {numa_info.mem_total} KB")
        print(f"    空闲内存: {numa_info.mem_free} KB")
        print(f"    已用内存: {numa_info.mem_total - numa_info.mem_free} KB")
        
        if numa_info.numa_huge_page_info:
            print(f"    大页信息:")
            for page_size, huge_page_data in numa_info.numa_huge_page_info.items():
                print(f"      页大小: {page_size} KB")
                print(f"        总数: {huge_page_data.huge_page_total}")
                print(f"        空闲: {huge_page_data.huge_page_free}")


if __name__ == '__main__':
    print("开始测试节点信息列表查询...")
    print("=" * 60)
    
    try:
        node_list = test_node_info_list()
        
        if not node_list:
            print("错误: 未获取到节点信息！")
            exit(1)
        
        print(f"\n共查询到 {len(node_list)} 个节点")
        
        for node_info in node_list:
            print_node_info(node_info)
        
        print(f"\n{'='*60}")
        print("测试完成！")
        
    except Exception as e:
        print(f"\n测试失败: {e}")
        exit(1)
