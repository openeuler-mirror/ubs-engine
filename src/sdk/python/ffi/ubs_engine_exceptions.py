#!/usr/bin/python3
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MatrixEngine is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
class UbsError(Exception):
    """UBS引擎基础异常"""
    pass


class UbsEngineError(UbsError):
    """引擎运行时异常"""
    pass


class UbsMemError(UbsError):
    """内存操作异常"""
    pass


class UbsTopologyError(UbsError):
    """拓扑查询异常"""
    pass


class UbsNodeError(UbsError):
    """节点查询异常"""
    pass


class UbsConnectionFailedError(UbsError):
    """连接异常"""
    pass


class UbsAuthFailedError(UbsError):
    """鉴权异常"""
    pass


class UbsConfigError(UbsError):
    """配置异常"""
    pass


class UbsNativeLibraryError(UbsError):
    """本地库加载异常"""
    pass


class UbsErrNullPointer(UbsError):
    pass


class UbsEngineConnectionError(UbsError):
    """连接UBSE服务端失败"""
    pass


class UbsEngineReceiveError(UbsError):
    """接收UBSE服务端响应失败"""
    pass


class UbsEngineAuthError(UbsError):
    """UBSE服务端鉴权不通过"""
    pass


class UbsEngineTimeoutError(UbsError):
    """UBSE服务端处理超时"""
    pass


class UbsEngineInternalError(UbsError):
    """UBSE服务端内部错误"""
    pass


class UbsEngineOutOfRangeError(UbsError):
    """name参数超出范围"""
    pass


class UbsEngineNotExistError(UbsError):
    """借用关系不存在"""
    pass


class UbsErrInvalidArg(UbsError):
    """无效参数"""
    pass


class UbsEngineExistedError(UbsError):
    """实例已存在"""
    pass


class UbsEngineAllocateError(UbsError):
    """算法分配失败"""
    pass


class UbsBinaryCodecError(UbsError):
    """二进制编解码异常基类。

    所有编解码过程中的错误均派生自此异常，便于上层统一捕获与转换。
    """
    pass


class UbsLengthExceededError(UbsBinaryCodecError):
    """长度超过允许的最大值。"""
    pass


class UbsInsufficientDataError(UbsBinaryCodecError):
    """剩余数据不足以完成读取。"""
    pass


class UbsDecodeError(UbsBinaryCodecError):
    """字节序列解码失败（如非法 UTF-8 序列）。"""
    pass
