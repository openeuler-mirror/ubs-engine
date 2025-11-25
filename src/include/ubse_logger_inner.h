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

#ifndef UBSE_LOGGER_INNER_H
#define UBSE_LOGGER_INNER_H
#include "ubse_logger.h"

// UBSE_LOGGING_OFF 用于需要关闭日志输出文件场景，如 C SDK 场景
#ifdef UBSE_LOGGING_OFF
#include <fstream>

#define RM_LOG_CRIT std::ofstream("/dev/null")
#define RM_LOG_ERROR std::ofstream("/dev/null")
#define RM_LOG_WARN std::ofstream("/dev/null")
#define RM_LOG_INFO std::ofstream("/dev/null")
#define RM_LOG_DEBUG std::ofstream("/dev/null")

#else

namespace ubse::log {
#define UBSE_LOG_CRIT UBSE_LOGGER_CRIT(gModuleName, gModuleId)
#define UBSE_LOG_ERROR UBSE_LOGGER_ERROR(gModuleName, gModuleId)
#define UBSE_LOG_ERROR UBSE_LOGGER_ERROR(gModuleName, gModuleId)
#define UBSE_LOG_WARN UBSE_LOGGER_WARN(gModuleName, gModuleId)
#define UBSE_LOG_INFO UBSE_LOGGER_INFO(gModuleName, gModuleId)
#define UBSE_LOG_DEBUG UBSE_LOGGER_DEBUG(gModuleName, gModuleId)
}

#endif
#endif // UBSE_LOGGER_INNER_H
