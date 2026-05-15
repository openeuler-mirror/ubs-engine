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
测试页面交换启用配置接口

该测试演示如何使用 ubs_page_swap_enable 接口为指定进程配置NUMA页面交换策略，
优化跨NUMA节点的内存访问性能。
"""

import sys
from ubse.ffi.ubs_virt_agent_page_swap_enable import UbsVirtAgentPageSwapEnable
from ubse.models.ubs_virt_agent_model import PageSwapPairT, NumaQuotaT


def test_page_swap_enable(pid, local_numa_id, local_quota, remote_numa_id, remote_quota):
    """
    测试页面交换启用功能
    
    Args:
        pid (int): 进程ID
        local_numa_id (int): 本地NUMA ID
        local_quota (int): 本地NUMA配额百分比 (0-100)
        remote_numa_id (int): 远程NUMA ID
        remote_quota (int): 远程NUMA配额百分比 (0-100)
        
    Returns:
        int: 调用结果，0表示成功，非0表示失败
    """
    print(f"\n配置页面交换策略:")
    print(f"  进程ID: {pid}")
    print(f"  本地NUMA配置:")
    print(f"    NUMA ID: {local_numa_id}")
    print(f"    配额: {local_quota}%")
    print(f"  远程NUMA配置:")
    print(f"    NUMA ID: {remote_numa_id}")
    print(f"    配额: {remote_quota}%")
    
    page_swap_client = UbsVirtAgentPageSwapEnable()
    try:
        page_swap_client.ubs_virt_agent_initialize()
        
        swap_pair = PageSwapPairT(
            local_numa_quotas=[NumaQuotaT(numa_id=local_numa_id, quota=local_quota)],
            remote_numa_quotas=[NumaQuotaT(numa_id=remote_numa_id, quota=remote_quota)]
        )
        
        result = page_swap_client.ubs_page_swap_enable(pid, [swap_pair])
        
        if result == 0:
            print(f"\n页面交换配置成功！")
        else:
            print(f"\n页面交换配置失败，返回码: {result}")
        
        return result
        
    except Exception as e:
        msg = f"Failed to enable page swap, error: {e}"
        print(msg)
        raise RuntimeError(msg) from e
    finally:
        page_swap_client.ubs_virt_agent_finalize()


def test_page_swap_enable_multi_pair(pid):
    """
    测试多配对页面交换配置
    
    Args:
        pid (int): 进程ID
        
    Returns:
        int: 调用结果，0表示成功，非0表示失败
    """
    print(f"\n配置多配对页面交换策略:")
    print(f"  进程ID: {pid}")
    
    page_swap_client = UbsVirtAgentPageSwapEnable()
    try:
        page_swap_client.ubs_virt_agent_initialize()
        
        swap_pairs = [
            PageSwapPairT(
                local_numa_quotas=[NumaQuotaT(numa_id=0, quota=80)],
                remote_numa_quotas=[NumaQuotaT(numa_id=1, quota=20)]
            ),
            PageSwapPairT(
                local_numa_quotas=[NumaQuotaT(numa_id=1, quota=70)],
                remote_numa_quotas=[NumaQuotaT(numa_id=2, quota=30)]
            )
        ]
        
        print(f"  配置了 {len(swap_pairs)} 个交换配对:")
        for i, pair in enumerate(swap_pairs, 1):
            print(f"\n  配对 {i}:")
            for quota in pair.local_numa_quotas:
                print(f"    本地: NUMA {quota.numa_id}, 配额 {quota.quota}%")
            for quota in pair.remote_numa_quotas:
                print(f"    远程: NUMA {quota.numa_id}, 配额 {quota.quota}%")
        
        result = page_swap_client.ubs_page_swap_enable(pid, swap_pairs)
        
        if result == 0:
            print(f"\n多配对页面交换配置成功！")
        else:
            print(f"\n多配对页面交换配置失败，返回码: {result}")
        
        return result
        
    except Exception as e:
        msg = f"Failed to enable multi-pair page swap, error: {e}"
        print(msg)
        raise RuntimeError(msg) from e
    finally:
        page_swap_client.ubs_virt_agent_finalize()


if __name__ == '__main__':
    print("=" * 60)
    print("页面交换启用配置测试")
    print("=" * 60)
    
    if len(sys.argv) < 2:
        print("\n用法:")
        print("  单配对模式: python test_page_swap_enable.py <pid> [local_numa_id] [local_quota] [remote_numa_id] [remote_quota]")
        print("  多配对模式: python test_page_swap_enable.py <pid> multi")
        print("\n参数说明:")
        print("  pid            - 进程ID (例如: 12345)")
        print("  local_numa_id  - 本地NUMA ID (默认: 0)")
        print("  local_quota    - 本地NUMA配额百分比 (默认: 80)")
        print("  remote_numa_id - 远程NUMA ID (默认: 1)")
        print("  remote_quota   - 远程NUMA配额百分比 (默认: 20)")
        print("  multi          - 指定为'multi'时使用多配对模式")
        print("\n示例:")
        print("  python test_page_swap_enable.py 12345")
        print("  python test_page_swap_enable.py 12345 0 70 1 30")
        print("  python test_page_swap_enable.py 12345 multi")
        sys.exit(1)
    
    pid = int(sys.argv[1])
    
    try:
        if len(sys.argv) > 2 and sys.argv[2].lower() == 'multi':
            result = test_page_swap_enable_multi_pair(pid)
        else:
            local_numa_id = int(sys.argv[2]) if len(sys.argv) > 2 else 0
            local_quota = int(sys.argv[3]) if len(sys.argv) > 3 else 80
            remote_numa_id = int(sys.argv[4]) if len(sys.argv) > 4 else 1
            remote_quota = int(sys.argv[5]) if len(sys.argv) > 5 else 20
            
            result = test_page_swap_enable(pid, local_numa_id, local_quota, 
                                         remote_numa_id, remote_quota)
        
        print(f"\n{'='*60}")
        if result == 0:
            print("测试成功！")
        else:
            print(f"测试完成，返回码: {result}")
        
    except Exception as e:
        print(f"\n测试失败: {e}")
        sys.exit(1)
