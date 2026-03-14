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
import ctypes
from enum import IntEnum
from typing import Optional, Callable, Union
from ubse.ffi.ubs_engine_binding_base import UbsEngineBindingBase


class LogLevel(IntEnum):
    """日志级别枚举"""
    DEBUG = 0
    INFO = 1
    WARN = 2
    ERROR = 3
    CRIT = 4

    @classmethod
    def from_int(cls, value: int) -> 'LogLevel':
        """从整数值创建枚举"""
        try:
            return cls(value)
        except ValueError:
            # 处理未知的日志级别
            return cls.DEBUG


class UbsEngineBindingLog(UbsEngineBindingBase):
    def __init__(self):
        super().__init__()
        self._log_callback = None
        self._setup_log_prototypes()

    def ubs_engine_log_callback_register(
            self,
            log_handler: Callable[[Union[LogLevel, int], str], None]
    ):
        """注册日志处理函数，支持枚举和整数"""

        def log_callback_wrapper(c_level, c_message):
            level_int = c_level
            message = c_message.decode('utf-8', errors='ignore') if c_message else ""

            try:
                # 检查handler是否接受枚举类型
                import inspect
                sig = inspect.signature(log_handler)
                params = list(sig.parameters.values())

                if len(params) > 0 and hasattr(params[0].annotation, '__args__'):
                    # 如果参数注解是Union类型，检查是否包含LogLevel
                    if LogLevel in params[0].annotation.__args__:
                        log_handler(LogLevel.from_int(level_int), message)
                    else:
                        log_handler(level_int, message)
                else:
                    # 默认行为：尝试传递枚举，如果失败则传递整数
                    try:
                        log_handler(LogLevel.from_int(level_int), message)
                    except TypeError:
                        log_handler(level_int, message)

            except Exception as e:
                print(f"Log callback error: {e}")

        self._log_callback = self.log_callback(log_callback_wrapper)
        self.lib_ubse.ubs_engine_log_callback_register(self._log_callback)

    def _setup_log_prototypes(self):
        """设置日志相关函数的原型"""
        self.log_callback = ctypes.CFUNCTYPE(None, ctypes.c_uint32, ctypes.c_char_p)
        self.lib_ubse.ubs_engine_log_callback_register.argtypes = [self.log_callback]
        self.lib_ubse.ubs_engine_log_callback_register.restype = None
