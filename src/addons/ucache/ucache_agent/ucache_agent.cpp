/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS uCache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ucache_agent.h"

#include <ubse_logger.h>

#include "ubse_com.h"
#include "ucache_config.h"
#include "ucache_error.h"
#include "agent_task_processor.h"
#include "turbo_runtime_manager.h"

using namespace ubse::log;
using namespace turbo::ucache;
namespace ucache::agent {
uint32_t Init()
{
    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "InitAgent";

    auto ret = TurboRuntimeManager::Init();
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Init TurboRuntimeManager failed, ret=" << ret;
        return ret;
    }

    // 注册任务执行接口
    auto res = InitAgentTaskProcessor();
    if (res != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Init AgentTaskProcessor failed, res=" << res;
        return res;
    }

    return UCACHE_OK;
}

void Exit()
{
    TurboRuntimeManager::Deinit();
    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "ExitAgent";
}
} // namespace ucache::agent