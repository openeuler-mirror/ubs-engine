#!/usr/bin/python3
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# ubs-engine is licensed under Mulan PSL v2.
"""UBSE python sdk错误码注册表。

服务端返回的错误码到 Python 异常类的扁平映射，IPC 层根据状态码查表抛出对应的异常。
所有错误码统一在 ``_ERROR_MAP`` 中定义，与 ``src/include/ubse_error.h`` 对齐，无模块区分。

设计要点:
    - 所有错误码集中在一张扁平映射表，与 C 端 ubse_error.h 一致
    - 未命中时：>=10000 内部错误码统一映射为 ``UbsEngineInternalError``，其余兜底
"""
import logging
from typing import Dict, Tuple, Type

from .ubs_engine_exceptions import (
    UbsErrInvalidArg, UbsEngineConnectionError, UbsEngineReceiveError,
    UbsEngineAuthError, UbsEngineTimeoutError, UbsEngineInternalError,
    UbsEngineOutOfRangeError, UbsEngineExistedError, UbsEngineNotExistError,
    UbsEngineAllocateError, UbsEngineStateError, UbsEngineStrategyMismatchError,
)

logger = logging.getLogger(__name__)

# 错误码映射项: (错误码名称, 异常类)
_ErrorEntry = Tuple[str, Type[Exception]]

# 错误码映射表，与 ubse_error.h 的对外错误码对齐，扁平结构无模块区分。
_ERROR_MAP: Dict[int, _ErrorEntry] = {
    # 参数错误 (1-9)
    1: ("UBSE_ERR_INVALID_ARG", UbsErrInvalidArg),
    4: ("UBSE_ERR_BUFFER_TOO_SMALL", UbsEngineReceiveError),
    # 资源错误 (10-19)
    10: ("UBSE_ERR_OUT_OF_MEMORY", UbsEngineInternalError),
    11: ("UBSE_ERR_RESOURCE_BUSY", UbsEngineInternalError),
    12: ("UBSE_ERR_RESOURCE_EXHAUSTED", UbsEngineInternalError),
    13: ("UBSE_ERR_QUOTA_EXCEEDED", UbsEngineInternalError),
    # IPC通信错误 (20-29)
    20: ("UBSE_ERR_IPC_CONNECTION_FAILED", UbsEngineConnectionError),
    21: ("UBSE_ERR_IPC_TIMEOUT", UbsEngineTimeoutError),
    22: ("UBSE_ERR_IPC_SERVICE_UNAVAILABLE", UbsEngineConnectionError),
    23: ("UBSE_ERR_IPC_CONNECTION_FAILED_PATH_LENGTH", UbsEngineConnectionError),
    # 权限错误 (30-39)
    30: ("UBSE_ERR_PERMISSION_DENIED", UbsEngineAuthError),
    31: ("UBSE_ERR_AUTHENTICATION_FAILED", UbsEngineAuthError),
    32: ("UBSE_ERR_ACCESS_DENIED", UbsEngineAuthError),
    # 操作错误 (40-49)
    40: ("UBSE_ERR_NOT_IMPLEMENTED", UbsEngineInternalError),
    41: ("UBSE_ERR_NOT_SUPPORTED", UbsEngineInternalError),
    42: ("UBSE_ERR_OPERATION_FAILED", UbsEngineInternalError),
    43: ("UBSE_ERR_TIMED_OUT", UbsEngineTimeoutError),
    # 守护进程错误 (50-59)
    50: ("UBSE_ERR_DAEMON_UNREACHABLE", UbsEngineConnectionError),
    51: ("UBSE_ERR_DAEMON_BUSY", UbsEngineInternalError),
    52: ("UBSE_ERR_DAEMON_CRASHED", UbsEngineInternalError),
    53: ("UBSE_ERR_DAEMON_INTERNAL", UbsEngineInternalError),
    # UBSE错误码 (1000-1099)
    1000: ("UBSE_ERR_OUT_OF_RANGE", UbsEngineOutOfRangeError),
    1001: ("UBSE_ERR_RESOURCE", UbsEngineInternalError),
    1002: ("UBSE_ERR_CONNECTION_FAILED", UbsEngineConnectionError),
    1003: ("UBSE_ERR_AUTH_FAILED", UbsEngineAuthError),
    1004: ("UBSE_ERR_TIMEOUT", UbsEngineTimeoutError),
    1005: ("UBSE_ERR_INTERNAL", UbsEngineInternalError),
    1006: ("UBSE_ERR_EXISTED", UbsEngineExistedError),
    1007: ("UBSE_ERR_NOT_EXIST", UbsEngineNotExistError),
    1008: ("UBSE_ERR_UDSINFO_MISMATCH", UbsEngineInternalError),
    1009: ("UBSE_ERR_IMPORT_ABSENT", UbsEngineInternalError),
    1010: ("UBSE_ERR_CREATING", UbsEngineInternalError),
    1011: ("UBSE_ERR_DELETING", UbsEngineInternalError),
    1012: ("UBSE_ERR_UNIMPORT_SUCCESS", UbsEngineInternalError),
    1013: ("UBSE_ERR_ALLOCATE", UbsEngineAllocateError),
    1014: ("UBSE_ERR_SHM_NO_CREATE", UbsEngineInternalError),
    1015: ("UBSE_ERR_SHM_NO_ATTACH", UbsEngineInternalError),
    1016: ("UBSE_ERR_SHM_ATTACHING", UbsEngineInternalError),
    1017: ("UBSE_ERR_SHM_DETACHING", UbsEngineInternalError),
    1018: ("UBSE_ERR_LINK_NOT_ALLOWED", UbsEngineInternalError),
    1019: ("UBSE_ERR_LINK_NOT_EXIST", UbsEngineInternalError),
    1020: ("UBSE_ERR_SHM_NODE_EMPTY", UbsEngineInternalError),
    1021: ("UBSE_ERR_COM_FAILED", UbsEngineInternalError),
    1022: ("UBSE_ERR_FIND_SRC_NUMA", UbsEngineInternalError),
    1023: ("UBSE_ERR_SHM_DESTROYED", UbsEngineInternalError),
    1024: ("UBSE_ERR_SHM_ATTACH_USING", UbsEngineInternalError),
    1025: ("UBSE_ERR_SHM_AFFINITY_PARAMS_ABNORMAL", UbsEngineInternalError),
    1026: ("UBSE_ERR_NUMA_ID_IS_NOT_IN_SOCKET", UbsEngineInternalError),
    1027: ("UBSE_ERR_NODE_NOT_EXIST", UbsEngineInternalError),
    1028: ("UBSE_ERR_NODE_FAULT", UbsEngineInternalError),
    1029: ("UBSE_ENGINE_ERR_EXPORT_LEDGERING", UbsEngineInternalError),
    1041: ("UBSE_ENGINE_ERR_IMPORT_LEDGERING", UbsEngineInternalError),
    # Node Controller错误 (1100-1199)
    1100: ("UBSE_ERR_NODE_NOT_FOUND", UbsEngineInternalError),
    1101: ("UBSE_ERR_NODE_UNREACHABLE", UbsEngineInternalError),
    1102: ("UBSE_ERR_NODE_NOT_ACTIVE", UbsEngineInternalError),
    1103: ("UBSE_ERR_NODE_NOT_RESPONDING", UbsEngineInternalError),
    # SSU Controller对外错误码 (1200-1299)
    1200: ("UBSE_SSU_ERROR_SPACE_NOT_FOUND", UbsEngineNotExistError),
    1201: ("UBSE_SSU_ERROR_NEED_DETACH_BEFORE_FREE", UbsEngineStateError),
    1202: ("UBSE_SSU_ERROR_STRATEGY_MISMATCH", UbsEngineStrategyMismatchError),
    # 重复操作错误 (2000-2099)
    2000: ("UBSE_ERR_ALREADY_ALLOCATED", UbsEngineExistedError),
    2001: ("UBSE_ERR_ALREADY_ATTACHED", UbsEngineExistedError),
    2002: ("UBSE_ERR_NO_NEED_FREE", UbsEngineNotExistError),
    2003: ("UBSE_ERR_NO_NEED_DETACH", UbsEngineNotExistError),
}


def resolve_error(status_code: int) -> _ErrorEntry:
    """解析错误码，返回 (错误码名称, 异常类)。

    Args:
        status_code: 服务端返回的状态码

    Returns:
        (错误码名称, 异常类) 元组
    """
    if status_code in _ERROR_MAP:
        return _ERROR_MAP[status_code]
    # 内部错误码 (>=10000) 统一映射为 UbsEngineInternalError
    if status_code >= 10000:
        return f"UBSE_INTERNAL_ERROR", UbsEngineInternalError
    return f"UBSE_UNKNOWN_ERROR_{status_code}", UbsEngineInternalError


def raise_for_status(status_code: int) -> None:
    """状态码非0时抛出对应异常。

    供业务层调用。

    Args:
        status_code: 服务端返回的状态码

    Raises:
        根据错误码映射抛出对应的 ``UbsError`` 子类异常
    """
    if status_code == 0:
        return
    logger.error("ubse daemon returned status_code: %d", status_code)
    name, exc_cls = resolve_error(status_code)
    raise exc_cls(f"{name} ({status_code})")
