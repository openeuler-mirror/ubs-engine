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
from typing import Callable

from ubse.ffi.ubs_virt_agent_log import LogLevel, UbsVirtAgentLog

_log_interface = UbsVirtAgentLog()


def ubs_virt_agent_log_callback_register(log_handler: Callable[[LogLevel, str], None]) -> None:
    """Register the log processing function.（Use the global interface）"""
    _log_interface.ubs_virt_agent_log_callback_register(log_handler)
