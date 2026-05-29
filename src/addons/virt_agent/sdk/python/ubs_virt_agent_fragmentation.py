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

from typing import Dict, List, Union, Any, Tuple
from ubse.ffi.ubs_virt_agent_node_anti_affinity import UbsVirtAgentNodeAntiAffinity
from ubse.ffi.ubs_virt_agent_mem_borrow_strategy import UbsVirtAgentMemBorrowStrategy
from ubse.ffi.ubs_virt_agent_mem_borrow_execute import UbsVirtAgentMemBorrowExecute
from ubse.ffi.ubs_virt_agent_mem_migrate_strategy import UbsVirtAgentMemMigrateStrategy
from ubse.ffi.ubs_virt_agent_mem_migrate_execute import UbsVirtAgentMemMigrateExecute
from ubse.ffi.ubs_virt_agent_mem_return import UbsVirtAgentMemReturn
from ubse.ffi.ubs_virt_agent_mem_rollback import UbsVirtAgentMemRollback
from ubse.ffi.ubs_virt_agent_task_query import UbsVirtAgentTaskQuery
from ubse.ffi.ubs_virt_agent_node_info_list import UbsVirtAgentNodeInfoList
from ubse.ffi.ubs_virt_agent_mem_borrow import UbsVirtAgentMemBorrow
from ubse.ffi.ubs_virt_agent_page_swap_enable import UbsVirtAgentPageSwapEnable
from ubse.ffi.ubs_virt_agent_types import NodeAntiDictionary
from ubse.models.ubs_virt_agent_model import BorrowStrategyT, BorrowExecuteResT, MemMigrateStrategyT, TaskInfoT, \
    NodeInfoT, BorrowParamT, MemBorrowResultT, PageSwapPairT

_node_anti_affinity_interface = UbsVirtAgentNodeAntiAffinity()
_mem_borrow_strategy_interface = UbsVirtAgentMemBorrowStrategy()
_mem_borrow_execute_interface = UbsVirtAgentMemBorrowExecute()
_mem_migrate_strategy_interface = UbsVirtAgentMemMigrateStrategy()
_mem_migrate_execute_interface = UbsVirtAgentMemMigrateExecute()
_mem_return_interface = UbsVirtAgentMemReturn()
_mem_rollback_interface = UbsVirtAgentMemRollback()
_task_query_interface = UbsVirtAgentTaskQuery()
_node_info_list_interface = UbsVirtAgentNodeInfoList()
_mem_borrow_interface = UbsVirtAgentMemBorrow()
_page_swap_enable_interface = UbsVirtAgentPageSwapEnable()


def ubs_node_anti_affinity(node_anti_dict: Union[NodeAntiDictionary, Dict[str, List[str]]]) -> int:
    """Setting mode anti-affinity（Use the global interface）"""
    return _node_anti_affinity_interface.ubs_node_anti_affinity(node_anti_dict)


def ubs_mem_borrow_strategy(param: Dict[str, Any]) -> BorrowStrategyT:
    """Get memory borrow strategy（Use the global interface）"""
    return _mem_borrow_strategy_interface.ubs_mem_borrow_strategy(param)


def ubs_mem_borrow_execute(param: Dict[str, Any], is_async: bool = False) -> Tuple[BorrowExecuteResT, str]:
    """Execute memory borrow strategy（Use the global interface）"""
    return _mem_borrow_execute_interface.ubs_mem_borrow_execute(param, is_async)


def ubs_mem_migrate_strategy(param: Dict[str, Any]) -> MemMigrateStrategyT:
    """Get memory migrate strategy（Use the global interface）"""
    return _mem_migrate_strategy_interface.ubs_mem_migrate_strategy(param)


def ubs_mem_migrate_execute(param: Dict[str, Any]) -> int:
    """Execute memory migrate strategy（Use the global interface）"""
    return _mem_migrate_execute_interface.ubs_mem_migrate_execute(param)


def ubs_mem_return(is_async: bool = False) -> Tuple[int, str]:
    """Memory return（Use the global interface）"""
    return _mem_return_interface.ubs_mem_return(is_async)


def ubs_mem_rollback(node_id: str, borrow_id_list: List[str]) -> int:
    """Memory rollback（Use the global interface）"""
    return _mem_rollback_interface.ubs_mem_rollback(node_id, borrow_id_list)


def ubs_task_result_query(task_id: str) -> Tuple[int, TaskInfoT]:
    """
    Interface for querying asynchronous tasks of memory borrowing and memory return（Use the global interface）
    Args:
        task_id (str): Task ID returned by invoking the ubs_mem_borrow_execute or ubs_mem_return interface.

    Returns:
        int: Call result, 0 indicates success, non-zero indicates failure
        TaskInfoT: Task info.
    """

    return _task_query_interface.task_query(task_id)


def ubs_mem_fragmentation_node_info_list() -> List['NodeInfoT']:
    """
    Query memory fragmentation node information list (using global interface)

    Get NUMA topology information and memory status of all nodes in the system, including node ID, hostname,
    NUMA node details (total memory, free memory, huge page information, etc.), and identify the current node.

    Returns:
        List[NodeInfoT]: Node information list, each element contains:
            - node_id: Node ID
            - numa_infos: NUMA information list
            - is_current: Whether it is the current node
    """
    return _node_info_list_interface.ubs_mem_fragmentation_node_info_list()


def ubs_mem_borrow(param: 'BorrowParamT', is_async: bool = False) -> List['MemBorrowResultT']:
    """
    Execute memory borrow operation (using global interface)

    Borrow memory from target node according to specified borrow parameters, supporting both synchronous and asynchronous modes.
    In asynchronous mode, a task ID will be returned, which can be queried using ubs_task_result_query.

    Args:
        param (BorrowParamT): Memory borrow parameters, containing:
            - node_id: Source node ID
            - numa_meta_infos: NUMA meta information list (socket_id, numa_id)
            - borrow_size: Borrow size (MB)
        is_async (bool): Whether to execute asynchronously, default is False (synchronous)

    Returns:
        List[MemBorrowResultT]: Memory borrow result list, each element contains:
            - borrow_ids: Borrow ID list
            - present_numa_ids: Current NUMA ID list
            - task_id: Task ID (valid in asynchronous mode)

    Raises:
        Exception: Throws exception when borrow operation fails

    """
    return _mem_borrow_interface.ubs_mem_borrow(param, is_async)


def ubs_page_swap_enable(pid: int, page_swap_enable: List['PageSwapPairT']) -> int:
    """
    Enable page swap functionality (using global interface)

    Configure NUMA page swap policy for specified process, setting quotas for local and remote NUMA nodes,
    optimizing cross-NUMA node memory access performance.

    Args:
        pid (int): Process ID
        page_swap_enable (List[PageSwapPairT]): Page swap pair list, each element contains:
            - local_numa_quotas: Local NUMA quota list (numa_id, quota)
            - remote_numa_quotas: Remote NUMA quota list (numa_id, quota)

    Returns:
        int: Call result, 0 indicates success, non-zero indicates failure

    Raises:
        Exception: Throws exception when configuration fails

    """
    return _page_swap_enable_interface.ubs_page_swap_enable(pid, page_swap_enable)
