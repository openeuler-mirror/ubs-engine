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

#include "intercom/ubse_inter_com.h"

#include "ubse_com_base.h"
#include "ubse_context.h"
#include "ubse_logger.h"

namespace ubse::com {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
using namespace ubse::context;
using namespace ubse::task_executor;

UbseResult UbseInterCom::StartQueue()
{
    auto taskExecutor = ubse::context::UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    if (taskExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get task executor failed";
        return UBSE_ERROR_CONF_INVALID;
    }

    mqExecutor_ = taskExecutor->Get("ComExecutor");
    return UBSE_OK;
}

UbseResult UbseInterCom::StopQueue()
{
    return UBSE_OK;
}

UbseMqHandler UbseInterCom::GetHandler(uint16_t moduleCode, uint16_t opCode)
{
    auto it = handlerMap_.find({moduleCode, opCode});
    if (it == handlerMap_.end() || it->second.handler == nullptr) {
        UBSE_LOG_ERROR << "module " << moduleCode << " opCode " << opCode << " handler not exists";
        return {moduleCode, opCode, nullptr};
    }
    return it->second;
}
} // namespace ubse::com