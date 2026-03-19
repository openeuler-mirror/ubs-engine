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
from ubse.ffi.ubs_virt_agent_types import NodeAntiDictionary
from ubse.models.ubs_virt_agent_model import BorrowStrategyT, MemMigrateStrategyT, BorrowExecuteResT, TaskInfoT

_node_anti_affinity_interface = UbsVirtAgentNodeAntiAffinity()
_mem_borrow_strategy_interface = UbsVirtAgentMemBorrowStrategy()
_mem_borrow_execute_interface = UbsVirtAgentMemBorrowExecute()
_mem_migrate_strategy_interface = UbsVirtAgentMemMigrateStrategy()
_mem_migrate_execute_interface = UbsVirtAgentMemMigrateExecute()
_mem_return_interface = UbsVirtAgentMemReturn()
_mem_rollback_interface = UbsVirtAgentMemRollback()
_task_query_interface = UbsVirtAgentTaskQuery()


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