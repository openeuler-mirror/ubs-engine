#!/usr/bin/python3
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# ubs-engine is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
"""IPC通信模块，实现与UBSE守护进程的直接通信。

通过Unix域套接字与UBSE守护进程进行请求-响应式通信。

典型用法:
    response = invoke_call(module_code, op_code, request_bytes)
"""
import logging
import os
import socket
import struct
import threading
from typing import Optional, Tuple
from ubse.ffi.ubs_engine_exceptions import UbsErrInvalidArg, UbsEngineConnectionError, UbsEngineReceiveError, \
    UbsEngineTimeoutError

logger = logging.getLogger(__name__)

# IPC相关常量
UBSE_IPC_SOCKET_PATH = "/var/run/ubse/ubse.sock"
MAX_MESSAGE_SIZE = 10 * 1024 * 1024  # 10M
DEFAULT_TIMEOUT = 30  # 秒

# 状态码
UBS_SUCCESS = 0

# 消息头大小: isResp(1) + header(16) = 17字节
_HEADER_SIZE = 17
# 请求头保留字段大小 (填充0)
_RESERVED_BYTES = 8

# 套接字路径锁，保证线程安全
_socket_path_lock = threading.Lock()
# 当前套接字路径，可通过set_socket_path自定义
_socket_path: str = UBSE_IPC_SOCKET_PATH


def set_socket_path(path: str) -> None:
    """设置Unix域套接字路径。

    如果path为空，则恢复默认路径。

    Args:
        path: 套接字路径，为空时恢复默认路径
    """
    global _socket_path
    with _socket_path_lock:
        if not path:
            _socket_path = UBSE_IPC_SOCKET_PATH
        else:
            _socket_path = path


def get_socket_path() -> str:
    """获取当前Unix域套接字路径。

    Returns:
        当前套接字路径
    """
    with _socket_path_lock:
        return _socket_path


def connect_to_unix_socket() -> socket.socket:
    """连接到UBSE Unix域套接字。

    Returns:
        已连接的套接字对象

    Raises:
        UbsEngineConnectionError: 连接套接字失败
    """
    path = get_socket_path()

    try:
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.settimeout(DEFAULT_TIMEOUT)
        sock.connect(path)
    except FileNotFoundError as e:
        logger.error("ubse daemon not running: socket path %s does not exist", path)
        raise UbsEngineConnectionError(
            f"ubse daemon not running: socket path {path} does not exist") from e
    except (OSError, socket.error) as e:
        logger.error("failed to connect to ubse daemon: %s", e)
        raise UbsEngineConnectionError(f"failed to connect to ubse daemon: {e}") from e

    return sock


def send_request(sock: socket.socket, module_code: int, op_code: int,
                 request: bytes) -> None:
    """发送请求消息到UBSE守护进程。

    消息格式: isResp(1字节) + 请求头(16字节) + 请求体
    请求头: moduleCode(2) + opCode(2) + requestLen(4) + reserved(8)

    Args:
        sock: 已连接的套接字
        module_code: 模块码
        op_code: 操作码
        request: 请求体字节数据

    Raises:
        UbsErrInvalidArg: 请求体长度超出最大允许大小
        UbsEngineConnectionError: 连接UBSE服务端失败
    """
    if len(request) > MAX_MESSAGE_SIZE:
        logger.error("request body length %d exceeds maximum size %d", len(request), MAX_MESSAGE_SIZE)
        raise UbsErrInvalidArg(
            f"request body length {len(request)} exceeds maximum size {MAX_MESSAGE_SIZE}")

    # 构造消息: isResp(1) + moduleCode(2) + opCode(2) + requestLen(4) + reserved(8) + body
    header = struct.pack('<B HH I', 0, module_code, op_code, len(request))
    reserved = b'\x00' * _RESERVED_BYTES
    message = header + reserved + request

    try:
        sock.sendall(message)
    except socket.error as e:
        logger.error("failed to send request message: %s", e)
        raise UbsEngineConnectionError(f"failed to send request message: {e}") from e


def receive_response(sock: socket.socket) -> Tuple[int, bytes]:
    """接收UBSE守护进程的响应消息。

    IPC 层只做原数据透传，不根据状态码做业务判断。
    无论成功与否，都返回 ``(status_code, body)`` 给调用方自行处理。

    消息格式: isResp(1字节) + 响应头(16字节) + 响应体
    响应头: statusCode(4) + responseLen(4) + clientRequestId(8)

    Args:
        sock: 已连接的套接字

    Returns:
        (status_code, body) 元组，status_code 为 0 表示成功

    Raises:
        UbsEngineReceiveError: 响应消息接收失败
        UbsErrInvalidArg: 响应体长度超出最大允许大小
    """

    # 读取响应头 (isResp + header = 17字节)
    header_data = _recv_exactly(sock, _HEADER_SIZE)
    if header_data is None:
        logger.error("failed to read response message header: connection closed")
        raise UbsEngineReceiveError("failed to read response message header: connection closed")

    # 解析isResp标志
    is_resp = header_data[0]
    if is_resp != 1:
        logger.error("invalid response message: isResp flag is not set")
        raise UbsEngineReceiveError("invalid response message: isResp flag is not set")

    # 解析响应头
    status_code, response_len = struct.unpack_from('<I I', header_data, 1)

    if response_len > MAX_MESSAGE_SIZE:
        logger.error("response body length %d exceeds maximum size %d", response_len, MAX_MESSAGE_SIZE)
        raise UbsErrInvalidArg(
            f"response body length {response_len} exceeds maximum size {MAX_MESSAGE_SIZE}")

    # 读取响应体（无论成功与否都读取，服务端可能在错误时附带错误详情）
    body = b''
    if response_len > 0:
        body = _recv_exactly(sock, response_len)
        if body is None:
            logger.error("failed to read response body: connection closed")
            raise UbsEngineReceiveError("failed to read response body: connection closed")

    return status_code, body


def invoke_call(module_code: int, op_code: int, request: Optional[bytes] = None) -> Tuple[int, bytes]:
    """调用UBSE守护进程的IPC接口。

    IPC 层只做原数据透传，不根据状态码做业务判断。
    无论成功与否，都返回 ``(status_code, body)`` 给调用方自行处理。

    完整流程: 连接 -> 发送请求 -> 接收响应 -> 关闭连接

    Args:
        module_code: 模块码
        op_code: 操作码
        request: 请求体字节数据，可为None

    Returns:
        (status_code, body) 元组，status_code 为 0 表示成功

    Raises:
        UbsEngineConnectionError: 连接UBSE服务端失败
    """
    if request is None:
        request = b''

    sock = connect_to_unix_socket()
    try:
        send_request(sock, module_code, op_code, request)
        return receive_response(sock)
    finally:
        sock.close()


def _recv_exactly(sock: socket.socket, size: int) -> Optional[bytes]:
    """精确读取指定大小的字节数据。

    Args:
        sock: 已连接的套接字
        size: 需要读取的字节数

    Returns:
        读取到的字节数据，连接关闭时返回None
    """
    data = bytearray()
    try:
        while len(data) < size:
            chunk = sock.recv(size - len(data))
            if not chunk:
                return None
            data.extend(chunk)
    except socket.timeout as e:
        logger.error("timeout while reading response: %s", e)
        raise UbsEngineTimeoutError(f"timeout while reading response: {e}") from e
    except OSError as e:
        raise UbsEngineConnectionError(f"Connection error during recv: {e}") from e
    return bytes(data)
