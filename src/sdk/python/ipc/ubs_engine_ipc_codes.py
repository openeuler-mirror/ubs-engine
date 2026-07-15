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
"""SSU 模块码和操作码定义。

须与ubse服务端定义的模块码和操作码保持一致。
"""

# ====================== 模块码 ======================

UBSE_MODULE_CODE = 0x000D

# ====================== 操作码 ======================

OP_ALLOC_REQ = 0x0001
OP_FREE_REQ = 0x0004
OP_LIST_ALLOC_INFO_REQ = 0x0006
OP_GET_ALLOC_INFO_BY_NAME_REQ = 0x0007
OP_GET_NS_STATS_REQ = 0x0008
OP_GET_CONNECT_INFO_REQ = 0x0009
OP_ADD_ACCESS_PERMISSION_REQ = 0x000A
OP_REMOVE_ACCESS_PERMISSION_REQ = 0x000B
OP_ATTACH_SPACE_REQ = 0x000C
OP_DETACH_SPACE_REQ = 0x000D
OP_ATTACH_LINEAR_SPACE_REQ = 0x000E
OP_DETACH_LINEAR_SPACE_REQ = 0x000F
OP_ATTACH_STRIPED_SPACE_REQ = 0x0010
OP_DETACH_STRIPED_SPACE_REQ = 0x0011
OP_GET_FE_DEVICE_LIST_REQ = 0x0012
OP_FE_DEVICE_ALLOC_REQ = 0x0013
OP_FE_DEVICE_FREE_REQ = 0x0014
