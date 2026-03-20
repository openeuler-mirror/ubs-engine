/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MXE_MEM_TYPES_H
#define MXE_MEM_TYPES_H
#include <regex.h>
#include <securec.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <string>
#include "ubse_mem_constants.h"

namespace ubse::mem::strategy {
using BResult = uint32_t;
using NodeId = std::string; /* 节点真实ID，agent的nodeid，不算manager */
using SocketId = int;
using NumaId = int64_t;

using NodeIndex = int16_t;
using NumaIndex = int16_t;       // 节点内编号
using SocketIndex = int16_t;     // 节点内编号
using GlobalNumaIndex = int32_t; // 全局编号
using IpAddress = std::pair<std::string, uint16_t>;
using UbseResult = uint32_t;
using mem_id = uint64_t;

#ifdef UB_ENVIRONMENT
#define INVALID_MEM_ID 0
#endif
} // namespace ubse::mem::strategy

#endif