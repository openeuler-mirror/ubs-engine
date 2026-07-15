#!/usr/bin/python3
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# ubs-engine is licensed under Mulan PSL v2.
"""UBSE python sdk错误码注册表。

按模块注册服务端返回的错误码到 Python 异常类的映射，IPC 层根据
``module_code`` 查表抛出对应的异常。

设计要点:
    - 公共错误码（所有模块共享）集中在 ``_COMMON_ERROR_MAP``
    - 模块专属错误码通过 :func:`register_module_errors` 注册
    - 解析时模块映射优先于公共映射，未命中则回退到 ``UbsEngineInternalError``

典型用法::

    # 业务模块导入时注册专属错误码抛出自定义异常
    from ubse.ffi.ubs_error_registry import register_module_errors
    register_module_errors(MODULE_SSU, {
        1006: ("UBSE_ERR_EXISTED", UbsEngineExistedError),
        1013: ("UBSE_ERR_ALLOCATE", UbsEngineAllocateError),
    })
"""
import threading
from typing import Dict, Tuple, Type

from .ubs_engine_exceptions import (
    UbsErrInvalidArg, UbsEngineConnectionError, UbsEngineReceiveError,
    UbsEngineAuthError, UbsEngineTimeoutError, UbsEngineInternalError,
    UbsEngineOutOfRangeError, UbsEngineExistedError, )

# 错误码映射项: (错误码名称, 异常类)
_ErrorEntry = Tuple[str, Type[Exception]]

# 公共错误码（所有模块共享），与 ubse_error.h 的对外错误码对齐
_COMMON_ERROR_MAP: Dict[int, _ErrorEntry] = {
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
}

# 按模块注册的专属错误码: {module_code: {err_code: (name, exc_cls)}}
_module_error_maps: Dict[int, Dict[int, _ErrorEntry]] = {}
_module_error_lock = threading.Lock()


def register_module_errors(module_code: int, error_map: Dict[int, _ErrorEntry]) -> None:
    """注册模块专属错误码映射。

    模块映射在解析时优先于公共映射，允许模块对同一错误码定义更具体的异常类型。

    Args:
        module_code: 模块码（与 ``invoke_call`` 的 module_code 一致）
        error_map: 错误码到映射项的字典，形如 ``{1006: ("UBSE_ERR_EXISTED", UbsEngineExistedError)}``
    """
    with _module_error_lock:
        _module_error_maps[module_code] = dict(error_map)


def resolve_error(module_code: int, status_code: int) -> _ErrorEntry:
    """解析错误码，返回 (错误码名称, 异常类)。

    解析优先级: 模块专属映射 > 公共映射 > 兜底 ``UbsEngineInternalError``。

    Args:
        module_code: 模块码
        status_code: 服务端返回的状态码

    Returns:
        (错误码名称, 异常类) 元组
    """
    with _module_error_lock:
        module_map = _module_error_maps.get(module_code, {})
        if status_code in module_map:
            return module_map[status_code]
        if status_code in _COMMON_ERROR_MAP:
            return _COMMON_ERROR_MAP[status_code]
    return f"UBSE_UNKNOWN_ERROR_{status_code}", UbsEngineInternalError


def raise_for_status(module_code: int, status_code: int, op_code: int = -1) -> None:
    """状态码非0时抛出对应异常。

    供 IPC 层 ``receive_response`` 调用，根据模块码和状态码路由到注册的异常类。
    ``status_code == 0`` 时直接返回，不抛异常。

    Args:
        module_code: 模块码，用于查找模块专属映射
        status_code: 服务端返回的状态码
        op_code: 操作码，仅用于异常消息定位（默认 -1 表示未知）

    Raises:
        根据错误码映射抛出对应的 ``UbsError`` 子类异常
    """
    if status_code == 0:
        return
    name, exc_cls = resolve_error(module_code, status_code)
    raise exc_cls(f"[module={module_code} op={op_code}] {name} ({status_code})")


def clear_registry() -> None:
    """清空所有已注册的模块错误码映射（仅供测试使用）。"""
    with _module_error_lock:
        _module_error_maps.clear()
