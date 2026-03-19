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
class UbsVirtAgentError(Exception):
    """Base exception for the UBS VirtAgent engine"""
    pass


class UbsVABaseError(UbsVirtAgentError):
    """Generic base error"""
    pass


class UbsVAInvalidParamError(UbsVirtAgentError):
    """Invalid parameter error"""
    pass


class UbsVANullPointerError(UbsVirtAgentError):
    """Null pointer error"""
    pass


class UbsVAMemAllocError(UbsVirtAgentError):
    """Memory allocation error"""
    pass


class UbsVAMemCopyError(UbsVirtAgentError):
    """Memory copy error"""
    pass


class UbsVASerializeError(UbsVirtAgentError):
    """Serialization error"""
    pass


class UbsVADeserializeError(UbsVirtAgentError):
    """Deserialization error"""
    pass
