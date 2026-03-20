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
import ctypes
from enum import IntEnum
from typing import Optional, Callable, Union
from ubse.ffi.ubs_virt_agent_base import UbsVirtAgentBase


class LogLevel(IntEnum):
    """Log level enumeration"""
    DEBUG = 0
    INFO = 1
    WARN = 2
    ERROR = 3
    CRIT = 4

    @classmethod
    def from_int(cls, value: int) -> 'LogLevel':
        """Create a LogLevel enum from an integer value"""
        try:
            return cls(value)
        except ValueError:
            # Handle unknown log levels
            return cls.DEBUG


class UbsVirtAgentLog(UbsVirtAgentBase):
    def __init__(self):
        super().__init__()
        self._log_callback = None
        self._setup_log_prototypes()

    def ubs_virt_agent_log_callback_register(
            self,
            log_handler: Callable[[Union[LogLevel, int], str], None]
    ):
        """Register a log handler function, supporting both enum and integer log levels"""

        def log_callback_wrapper(c_level, c_message):
            level_int = c_level
            message = c_message.decode('utf-8', errors='ignore') if c_message else ""

            try:
                # Check whether the handler accepts an enum type
                import inspect
                sig = inspect.signature(log_handler)
                params = list(sig.parameters.values())

                if len(params) > 0 and hasattr(params[0].annotation, '__args__'):
                    # If the parameter annotation is a Union type, check whether it contains LogLevel
                    if LogLevel in params[0].annotation.__args__:
                        log_handler(LogLevel.from_int(level_int), message)
                    else:
                        log_handler(level_int, message)
                else:
                    # Default behavior: try passing the enum first, fall back to integer on failure
                    try:
                        log_handler(LogLevel.from_int(level_int), message)
                    except TypeError:
                        log_handler(level_int, message)

            except Exception as e:
                print(f"Log callback error: {e}")

        self._log_callback = self.log_callback(log_callback_wrapper)
        self.lib_ubse.ubs_virt_agent_log_callback_register(self._log_callback)

    def _setup_log_prototypes(self):
        """Set up prototypes for log-related functions"""
        self.log_callback = ctypes.CFUNCTYPE(None, ctypes.c_uint32, ctypes.c_char_p)
        self.lib_ubse.ubs_virt_agent_log_callback_register.argtypes = [self.log_callback]
        self.lib_ubse.ubs_virt_agent_log_callback_register.restype = None
