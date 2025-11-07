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

#include "ubse_ipc_server.h"

#include "ubse_error.h"
#include "ubse_ipc_common.h"
#include "ubse_logger_inner.h"

namespace ubse::ipc {
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_API_SERVER_MID)
UbseIpcServer::UbseIpcServer(const UbseUDSConfig &config) : udsServer(config)
{
    udsServer.RegisterHandler([this](const UbseRequestMessage &req, const UbseRequestContext &context) {
        return this->HandleRequest(req, context);
    });
}

uint32_t UbseIpcServer::Start()
{
    return udsServer.Start();
}

void UbseIpcServer::Stop()
{
    udsServer.Stop();
}

uint32_t UbseIpcServer::RegisterHandler(uint16_t moduleCode, uint16_t opCode, UbseIpcHandler handler)
{
    std::lock_guard<std::mutex> lock(handlersMutex);
    if (apiInterfaceMap.find(std::make_pair(moduleCode, opCode)) != apiInterfaceMap.end()) {
        UBSE_LOG_ERROR << "The API interface already registered, moduleCode=" << moduleCode << ", opCode=" << opCode;
        return IPC_ERROR_INVALID_HANDLE;
    }
    apiInterfaceMap[{moduleCode, opCode}] = std::move(handler);
    return IPC_SUCCESS;
}

void UbseIpcServer::HandleRequest(const UbseRequestMessage &request, const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "Start handling API request, moduleCode = " << request.header.moduleCode
                  << ", opCode = " << request.header.opCode << ", request_id= " << context.requestId;
    const auto key = std::make_pair(request.header.moduleCode, request.header.opCode);
    UbseIpcHandler handler{};
    // 获取handler
    {
        std::lock_guard<std::mutex> lock(handlersMutex);
        auto it = apiInterfaceMap.find(key);
        if (it == apiInterfaceMap.end()) {
            UBSE_LOG_ERROR << "The API interface does not exist, moduleCode= " << request.header.moduleCode
                           << ", opCode:= " << request.header.opCode << ", request_id= " << context.requestId;
            UbseResponseMessage responseMessage{{IPC_ERROR_INVALID_HANDLE, 0}, nullptr};
            auto ret = udsServer.SendResponse(context.requestId, responseMessage);
            if (ret != IPC_SUCCESS) {
                UBSE_LOG_ERROR << "The API interface Send response failed= " << request.header.moduleCode
                               << ", opCode= " << ", request_id= " << context.requestId;
            }
            return;
        }
        handler = it->second;
    }

    // 调用handler
    if (request.header.bodyLen < 0) {
        UBSE_LOG_ERROR << "Request header body len is invalid, len=" << request.header.bodyLen;
        return;
    }
    if (request.header.bodyLen > 0 && !request.body) {
        UBSE_LOG_ERROR << "Request header body len is nullptr, but request.header.bodyLen > 0";
        return;
    }
    UbseIpcMessage requestBuffer{request.body, request.header.bodyLen};
    auto handlerRet = handler(requestBuffer, context);
    UBSE_LOG_INFO << "Complete handling API request, moduleCode= " << request.header.moduleCode
                  << ", opCode= " << request.header.opCode << ", handlerRet= " << handlerRet
                  << ", request_id= " << context.requestId;
    // handler执行失败, 返回错误信息
    if (handlerRet != IPC_SUCCESS) {
        UbseResponseMessage responseMessage{{handlerRet, 0}, nullptr};
        auto ret = udsServer.SendResponse(context.requestId, responseMessage);
        if (ret != IPC_SUCCESS) {
            UBSE_LOG_ERROR << "The API interface Send response failed= " << request.header.moduleCode
                           << ", opCode= " << request.header.opCode << ", request_id= " << context.requestId;
        }
    }
}

uint32_t UbseIpcServer::SendResponse(uint32_t statusCode, uint64_t requestId, UbseIpcMessage &response)
{
    UbseResponseMessage responseMessage{{statusCode, response.length}, response.buffer};
    return udsServer.SendResponse(requestId, responseMessage);
}
} // namespace ubse::ipc