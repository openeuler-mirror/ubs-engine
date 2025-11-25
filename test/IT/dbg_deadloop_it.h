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

#ifndef UBSE_MANAGER_DEADLOOP_IT_H
#define UBSE_MANAGER_DEADLOOP_IT_H

#include "ubse_deadloop_module.h"
#include "main_test_it.h"
#include "ubse_context.h"
#include "ubse_logger_module.h"

namespace ubse::it::deadloop {
int32_t ITestCmdDeadloopEnable(ProcessMmap *pMmap);
int32_t ITestCmdDeadloopDisable(ProcessMmap *pMmap);
void findLogsWithGrep(const std::string& filename, std::chrono::system_clock::time_point& currentMinute,
    const std::string& keyword, std::vector<std::string>& matchedLogs);
std::chrono::system_clock::time_point ParseTimestamp(const std::string& timestamp);
std::string GetLogPath();
}


#endif // UBSE_MANAGER_DEADLOOP_IT_H