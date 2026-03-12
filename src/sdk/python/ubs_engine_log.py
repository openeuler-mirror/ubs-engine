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
from typing import Callable

from ubse.ffi.ubs_engine_binding_log import LogLevel, UbsEngineBindingLog

_log_interface = UbsEngineBindingLog()


def ubs_engine_log_callback_register(log_handler: Callable[[LogLevel, str], None]) -> None:
    """注册日志处理函数（委托给全局的log_interface）"""
    _log_interface.ubs_engine_log_callback_register(log_handler)
