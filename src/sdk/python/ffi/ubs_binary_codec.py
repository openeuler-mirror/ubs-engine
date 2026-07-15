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
"""二进制编解码工具类。

参照 C++ ``UbsePackUtil``/``UbseUnpackUtil`` (ubse_pack_util.h) 的设计思路，
提供 Python 版的二进制数据打包与解包工具，消除手动偏移量管理。

设计要点:
    - ``BinaryPacker`` 封装 ``bytearray`` 并自动追踪写入位置，支持链式调用
    - ``BinaryUnpacker`` 封装 ``bytes`` 并自动推进读取位置，内置边界校验
    - 所有整数使用小端序 (little-endian)，
    - 字符串/字节数组采用长度前缀格式 ``[uint32 len][len bytes]``，
    - 解码失败时使用严格模式（``errors='strict'``），不静默丢弃非法字节；
      定义了 ``BinaryCodecError`` 异常体系，便于上层精准捕获与转换

典型用法::

    # 打包
    packed = (BinaryPacker()
              .pack_string(req.name, UBS_SSU_MAX_NAME_LENGTH)
              .pack_uint64(req.ns_size)
              .pack_uint32(req.ns_num)
              .result())

    # 解包
    unpacker = BinaryUnpacker(response)
    name = unpacker.unpack_string(UBS_SSU_MAX_NAME_LENGTH)
    strategy = unpacker.unpack_uint8()
    count = unpacker.unpack_uint32()
"""
import struct
from typing import Callable, List, TypeVar

from .ubs_engine_exceptions import (UbsInsufficientDataError, UbsDecodeError, UbsLengthExceededError)

T = TypeVar('T')

class BinaryPacker:
    """二进制数据打包器。

    封装 ``bytearray`` 并提供链式 API，自动追踪写入偏移量，

    Attributes:
        _buf: 内部缓冲区
    """

    def __init__(self) -> None:
        self._buf = bytearray()

    def pack_uint8(self, value: int) -> 'BinaryPacker':
        """写入 1 字节无符号整数。"""
        self._buf += struct.pack('<B', value & 0xFF)
        return self

    def pack_uint16(self, value: int) -> 'BinaryPacker':
        """写入 2 字节小端序无符号整数。"""
        self._buf += struct.pack('<H', value & 0xFFFF)
        return self

    def pack_uint32(self, value: int) -> 'BinaryPacker':
        """写入 4 字节小端序无符号整数。"""
        self._buf += struct.pack('<I', value & 0xFFFFFFFF)
        return self

    def pack_uint64(self, value: int) -> 'BinaryPacker':
        """写入 8 字节小端序无符号整数。"""
        self._buf += struct.pack('<Q', value & 0xFFFFFFFFFFFFFFFF)
        return self

    def pack_string(self, s, max_len: int) -> 'BinaryPacker':
        """写入长度前缀字符串

        格式: ``[uint32 len][len bytes]``

        Args:
            s: 待写入的字符串（``str`` 或 ``bytes``）
            max_len: 字符串最大长度（字节数），超过则截断
        """
        encoded = s.encode('utf-8') if isinstance(s, str) else bytes(s)
        write_len = min(len(encoded), max_len)
        self.pack_uint32(write_len)
        if len(encoded) > max_len:
            raise UbsLengthExceededError(
                f"string length {len(encoded)} exceeds maximum allowed {max_len}")
        return self

    def pack_bytes(self, data: bytes, max_len: int) -> 'BinaryPacker':
        """写入长度前缀字节数组

        格式: ``[uint32 len][len bytes]``。

        Args:
            data: 待写入的字节数据
            max_len: 字节最大长度，超过则截断
        """
        encoded = bytes(data)
        write_len = min(len(encoded), max_len)
        self.pack_uint32(write_len)
        if len(encoded) > max_len:
            raise UbsLengthExceededError(
                f"string length {len(encoded)} exceeds maximum allowed {max_len}")
        return self

    def pack_raw(self, data: bytes) -> 'BinaryPacker':
        """写入原始字节数据（不定长）。"""
        self._buf += data
        return self

    def result(self) -> bytes:
        """返回打包结果的不可变 ``bytes`` 副本。"""
        return bytes(self._buf)


class BinaryUnpacker:
    """二进制数据解包器。

    封装 ``bytes`` 并提供顺序读取 API，自动推进偏移量，
    内置边界校验防止越界读取。

    Attributes:
        _data: 原始字节数据
        _offset: 当前读取偏移量
    """

    def __init__(self, data: bytes) -> None:
        self._data = data
        self._offset = 0

    @property
    def offset(self) -> int:
        """当前偏移量。"""
        return self._offset

    @property
    def remaining(self) -> int:
        """剩余可读字节数。"""
        return len(self._data) - self._offset

    def _check_remaining(self, needed: int) -> None:
        """校验剩余数据是否足够。"""
        if self.remaining < needed:
            raise UbsInsufficientDataError(
                f"insufficient data: expected {needed} bytes, got {self.remaining}")

    def unpack_uint8(self) -> int:
        """读取 1 字节无符号整数。"""
        self._check_remaining(1)
        value = self._data[self._offset]
        self._offset += 1
        return value

    def unpack_uint16(self) -> int:
        """读取 2 字节小端序无符号整数。"""
        self._check_remaining(2)
        value = struct.unpack_from('<H', self._data, self._offset)[0]
        self._offset += 2
        return value

    def unpack_uint32(self) -> int:
        """读取 4 字节小端序无符号整数。"""
        self._check_remaining(4)
        value = struct.unpack_from('<I', self._data, self._offset)[0]
        self._offset += 4
        return value

    def unpack_uint64(self) -> int:
        """读取 8 字节小端序无符号整数。"""
        self._check_remaining(8)
        value = struct.unpack_from('<Q', self._data, self._offset)[0]
        self._offset += 8
        return value

    def unpack_string(self, max_len: int) -> str:
        """读取长度前缀字符串（

        先读取 ``uint32`` 长度前缀，再读取对应字节数的字符串内容。
        解码采用严格模式，遇到非法 UTF-8 序列时抛出 ``UbsDecodeError``，
        不会静默丢弃脏数据。

        Args:
            max_len: 字符串最大允许长度（字节数），超过则抛出异常

        Returns:
            UTF-8 解码后的字符串

        Raises:
            UbsLengthExceededError: 长度前缀超过 ``max_len``
            UbsInsufficientDataError: 剩余数据不足
            UbsDecodeError: 字节序列不是合法的 UTF-8
        """
        length = self.unpack_uint32()
        if length > max_len:
            raise UbsDecodeError(
                f"string length {length} exceeds maximum allowed {max_len}")
        if length == 0:
            return ""
        self._check_remaining(length)
        raw = self._data[self._offset:self._offset + length]
        try:
            self._offset += length
            return raw.decode('utf-8')
        except UnicodeDecodeError  as e:
            raise UbsDecodeError(
                f"failed to decode {length} bytes as UTF-8 at offset "
                f"{self._offset - length}: {e}") from e

    def unpack_bytes(self, max_len: int) -> bytes:
        """读取长度前缀字节数组

        先读取 ``uint32`` 长度前缀，再读取对应字节数的原始内容。

        Args:
            max_len: 字节最大允许长度，超过则抛出异常

        Returns:
            原始字节数据

        Raises:
            UbsLengthExceededError: 长度前缀超过 ``max_len``
            UbsInsufficientDataError: 剩余数据不足
        """
        length = self.unpack_uint32()
        if length > max_len:
            raise UbsDecodeError(
                f"bytes length {length} exceeds maximum allowed {max_len}")
        if length == 0:
            return b""
        self._check_remaining(length)
        raw = self._data[self._offset:self._offset + length]
        self._offset += length
        return bytes(raw)

    def remaining_data(self) -> bytes:
        """返回剩余未读数据（不推进偏移量）。"""
        return self._data[self._offset:]


def unpack_list(unpacker: BinaryUnpacker, max_count: int,
                item_fn: Callable[[BinaryUnpacker], T]) -> List[T]:
    """通用列表解包函数。

    从解包器中读取 ``uint32`` 计数，校验上限后循环调用 ``item_fn`` 解包每个元素。
    消除了原来 4 个 ``unpack_xxx_list`` 函数中的重复逻辑。

    Args:
        unpacker: 解包器实例
        max_count: 列表元素数量上限
        item_fn: 单个元素的解包回调函数，接收 ``BinaryUnpacker`` 并返回元素

    Returns:
        解包后的元素列表

    Raises:
        UbsLengthExceededError: 计数超过 ``max_count``
    """
    count = unpacker.unpack_uint32()
    if count > max_count:
        raise UbsDecodeError(
            f"count {count} exceeds maximum allowed {max_count}")
    return [item_fn(unpacker) for _ in range(count)]
