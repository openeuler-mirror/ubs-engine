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
测试内存借用接口（大内存虚机）

该测试演示如何使用 ubs_mem_borrow 接口执行内存借用操作，支持同步和异步两种模式。
"""

import sys
from ubse.ffi.ubs_virt_agent_mem_borrow import UbsVirtAgentMemBorrow
from ubse.models.ubs_virt_agent_model import BorrowParamT, NumaMetaInfoT


def test_mem_borrow_sync(node_id, socket_id, numa_id, borrow_size_mb):
    """
    测试同步内存借用功能
    
    Args:
        node_id (str): 源节点ID
        socket_id (int): Socket ID
        numa_id (int): NUMA ID
        borrow_size_mb (int): 借用大小（MB）
        
    Returns:
        list: 内存借用结果列表
    """
    print(f"\n执行同步内存借用:")
    print(f"  节点ID: {node_id}")
    print(f"  Socket ID: {socket_id}")
    print(f"  NUMA ID: {numa_id}")
    print(f"  借用大小: {borrow_size_mb} MB")
    
    mem_borrow_client = UbsVirtAgentMemBorrow()
    try:
        mem_borrow_client.ubs_virt_agent_initialize()
        
        param = BorrowParamT(
            node_id=node_id,
            numa_meta_infos=[NumaMetaInfoT(socket_id=socket_id, numa_id=numa_id)],
            borrow_size=borrow_size_mb
        )
        
        results = mem_borrow_client.ubs_mem_borrow(param, is_async=False)
        
        print(f"\n借用成功！返回 {len(results)} 个结果")
        for i, result in enumerate(results, 1):
            print(f"\n结果 {i}:")
            print(f"  任务ID: {result.task_id}")
            print(f"  借用ID数量: {len(result.borrow_ids)}")
            print(f"  NUMA ID数量: {len(result.present_numa_ids)}")
            
            if result.borrow_ids:
                print(f"  前5个借用ID: {result.borrow_ids[:5]}")
            if result.present_numa_ids:
                print(f"  NUMA IDs: {result.present_numa_ids}")
        
        return results
        
    except Exception as e:
        msg = f"Failed to execute memory borrow, error: {e}"
        print(msg)
        raise RuntimeError(msg) from e
    finally:
        mem_borrow_client.ubs_virt_agent_finalize()


def test_mem_borrow_async(node_id, socket_id, numa_id, borrow_size_mb):
    """
    测试异步内存借用功能
    
    Args:
        node_id (str): 源节点ID
        socket_id (int): Socket ID
        numa_id (int): NUMA ID
        borrow_size_mb (int): 借用大小（MB）
        
    Returns:
        str: 任务ID，用于后续查询任务状态
    """
    print(f"\n执行异步内存借用:")
    print(f"  节点ID: {node_id}")
    print(f"  Socket ID: {socket_id}")
    print(f"  NUMA ID: {numa_id}")
    print(f"  借用大小: {borrow_size_mb} MB")
    
    mem_borrow_client = UbsVirtAgentMemBorrow()
    try:
        mem_borrow_client.ubs_virt_agent_initialize()
        
        param = BorrowParamT(
            node_id=node_id,
            numa_meta_infos=[NumaMetaInfoT(socket_id=socket_id, numa_id=numa_id)],
            borrow_size=borrow_size_mb
        )
        
        results = mem_borrow_client.ubs_mem_borrow(param, is_async=True)
        
        print(f"\n异步借用已提交！返回 {len(results)} 个结果")
        task_ids = []
        for i, result in enumerate(results, 1):
            print(f"\n结果 {i}:")
            print(f"  任务ID: {result.task_id}")
            print(f"  借用ID数量: {len(result.borrow_ids)}")
            task_ids.append(result.task_id)
        
        return task_ids
        
    except Exception as e:
        msg = f"Failed to execute async memory borrow, error: {e}"
        print(msg)
        raise RuntimeError(msg) from e
    finally:
        mem_borrow_client.ubs_virt_agent_finalize()


if __name__ == '__main__':
    print("=" * 60)
    print("内存借用接口测试")
    print("=" * 60)
    
    if len(sys.argv) < 5:
        print("\n用法: python test_mem_borrow.py <node_id> <socket_id> <numa_id> <borrow_size_mb> [async]")
        print("\n参数说明:")
        print("  node_id       - 源节点ID (例如: node-001)")
        print("  socket_id     - Socket ID (例如: 0)")
        print("  numa_id       - NUMA ID (例如: 0)")
        print("  borrow_size_mb - 借用大小，单位MB (例如: 1024)")
        print("  async         - 可选，指定为'async'时使用异步模式")
        print("\n示例:")
        print("  python test_mem_borrow.py node-001 0 0 1024")
        print("  python test_mem_borrow.py node-001 0 0 1024 async")
        sys.exit(1)
    
    node_id = sys.argv[1]
    socket_id = int(sys.argv[2])
    numa_id = int(sys.argv[3])
    borrow_size_mb = int(sys.argv[4])
    is_async = len(sys.argv) > 5 and sys.argv[5].lower() == 'async'
    
    try:
        if is_async:
            task_ids = test_mem_borrow_async(node_id, socket_id, numa_id, borrow_size_mb)
            print(f"\n{'='*60}")
            print(f"异步任务已提交，任务ID列表: {task_ids}")
            print(f"可使用以下命令查询任务状态:")
            for task_id in task_ids:
                print(f"  python test_task_query.py {task_id}")
        else:
            results = test_mem_borrow_sync(node_id, socket_id, numa_id, borrow_size_mb)
            print(f"\n{'='*60}")
            print("同步借用完成！")
        
        print("\n测试成功！")
        
    except Exception as e:
        print(f"\n测试失败: {e}")
        sys.exit(1)
