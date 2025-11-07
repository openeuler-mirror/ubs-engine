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

#ifndef MEM_STRATEGY_MEMORYFABRIC_NEW_ULOG_H
#define MEM_STRATEGY_MEMORYFABRIC_NEW_ULOG_H

#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <sstream>
#include <utility>
#include "ubse_logger_inner.h"

constexpr auto MEM_MANAGER_MODULE_NAME = "ubsemem_strategy";
constexpr std::uint16_t MEM_MANAGER_MODULE_CODE = 811;
#ifndef DEBUG_MEM_UT
#ifndef FROM_CLIENT_SO
#endif
#endif

#define LEVEL_DEBUG (0)
#define LEVEL_INFO (1)
#define LEVEL_WARN (2)
#define LEVEL_ERROR (3)
#define LOG_ERROR(msg)                                                                 \
    do {                                                                               \
        std::ostringstream oss;                                                        \
        oss << msg;                                                                    \
        UBSE_LOGGER_ERROR(MEM_MANAGER_MODULE_NAME, MEM_MANAGER_MODULE_CODE) << oss.str(); \
    } while (0)

#define LOG_INFO(msg)                                                                 \
    do {                                                                              \
        std::ostringstream oss;                                                       \
        oss << msg;                                                                   \
        UBSE_LOGGER_INFO(MEM_MANAGER_MODULE_NAME, MEM_MANAGER_MODULE_CODE) << oss.str(); \
    } while (0)

#define LOG_WARN(msg)                                                                 \
    do {                                                                              \
        std::ostringstream oss;                                                       \
        oss << msg;                                                                   \
        UBSE_LOGGER_WARN(MEM_MANAGER_MODULE_NAME, MEM_MANAGER_MODULE_CODE) << oss.str(); \
    } while (0)

#define LOG_DEBUG(msg)                                                                 \
    do {                                                                               \
        std::ostringstream oss;                                                        \
        oss << msg;                                                                    \
        UBSE_LOGGER_DEBUG(MEM_MANAGER_MODULE_NAME, MEM_MANAGER_MODULE_CODE) << oss.str(); \
    } while (0)
#endif