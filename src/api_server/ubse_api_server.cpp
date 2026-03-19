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
#include "ubse_api_server.h"

#include "ubse_api_server_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"

UBSE_DEFINE_THIS_MODULE("ubse");
namespace api::server {
using namespace ubse::context;
using namespace ubse::log;

uint32_t RegisterIpcHandler(uint16_t moduleCode, uint16_t opCode, UbseIpcHandler handler,
                            const std::string &object)
{
    auto apiServerModule = ubse::context::UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get ubse api server module failed. ";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    auto ret = apiServerModule->RegisterIpcHandler(moduleCode, opCode, handler, object);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Register ipc handler failed, " << FormatRetCode(ret);
    }
    return ret;
}

uint32_t SendResponse(uint32_t statusCode, uint64_t requestId, UbseIpcMessage &response)
{
    auto apiServerModule = ubse::context::UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get ubse api server module failed. ";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    auto ret = apiServerModule->SendResponse(statusCode, requestId, response);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Send response failed, " << FormatRetCode(ret);
    }
    return ret;
}
} // namespace api::server