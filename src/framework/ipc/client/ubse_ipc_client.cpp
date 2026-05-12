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

#include "ubse_ipc_client.h"

#include <securec.h>
#include <functional>
#include <iostream>
#include <mutex>

#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_ipc_common.h"
#include "ubse_ipc_common_def.h"
#include "ubse_ipc_log.h"
#include "ubse_ipc_message.h"
#include "ubse_ipc_utils.h"
#include "ubse_uds_client.h"

using namespace ubse::ipc;
using UbseClientIpcHandler = std::function<uint32_t(const UbseRequestMessage &)>;

struct PairHash {
    size_t operator()(const std::pair<uint16_t, uint16_t> &p) const
    {
        // 将两个 uint16_t 组合成一个 size_t 作为哈希值
        return (static_cast<size_t>(p.first) << 16) | p.second; // // 移动16位
    }
};

using UbseClientIpcHandlerMap = std::unordered_map<std::pair<uint16_t, uint16_t>, UbseClientIpcHandler, PairHash>;
std::mutex clientIpcHandlerMutex; // mutex锁
static UbseClientIpcHandlerMap clientIpcHandlerMap{};
static std::string ubseSocketPath = ubse::common::def::UBSE_UDS_SOCKET_PATH;

static uint32_t CopyResponseBody(const UbseResponseMessage &src, ubse_api_buffer_t *dest)
{
    dest->buffer = nullptr;
    dest->length = 0;
    if (src.header.bodyLen == 0 || !src.body) {
        return UBSE_OK;
    }
    dest->buffer = static_cast<uint8_t *>(malloc(src.header.bodyLen));
    if (!dest->buffer) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    dest->length = src.header.bodyLen;

    if (memcpy_s(dest->buffer, src.header.bodyLen, src.body, src.header.bodyLen) != EOK) {
        free(dest->buffer);
        dest->buffer = nullptr;
        dest->length = 0;
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }

    return UBSE_OK;
}

void ubse_socket_path_set(const char *socket_path)
{
    if (socket_path == nullptr || strlen(socket_path) == 0) {
        ubseSocketPath = ubse::common::def::UBSE_UDS_SOCKET_PATH;
        return;
    }

    ubseSocketPath = std::string(socket_path);
}

uint32_t ubse_invoke_call(uint16_t module_code, uint16_t op_code, const ubse_api_buffer_t *request_data,
                          ubse_api_buffer_t *response_data)
{
    if (request_data == nullptr || response_data == nullptr) {
        IPC_LOG_ERROR << "Request_data or response_data pointer is null";
        return UBSE_ERROR_INVAL;
    }
    response_data->buffer = nullptr;
    response_data->length = 0;
    // Establish connection
    ubse::ipc::UbseUDSClient udsClient(ubseSocketPath);
    auto ret = udsClient.Connect();
    if (ret != UBSE_OK) {
        IPC_LOG_ERROR << "Failed to connect, error code: " << ret;
        return ret;
    }
    // Pack request data
    UbseRequestMessage requestMessage{{module_code, op_code, request_data->length}, request_data->buffer};
    UbseResponseMessage responseMessage{};
    // Send request
    IPC_LOG_INFO << "Sending request, module_code=" << module_code << ", op_code=" << op_code;
    ret = udsClient.Send(requestMessage, responseMessage);
    if (ret != UBSE_OK) {
        IPC_LOG_ERROR << "Failed to send request, error code: " << ret;
        udsClient.Disconnect();
        return ret;
    }
    IPC_LOG_INFO << "Sending request successfully, module_code=" << module_code << ", op_code=" << op_code;
    ret = CopyResponseBody(responseMessage, response_data);
    if (responseMessage.freeFunc && responseMessage.body) {
        responseMessage.freeFunc(responseMessage.body);
        responseMessage.body = nullptr;
    }
    if (ret != UBSE_OK) {
        IPC_LOG_ERROR << "Failed to copy response body, error code: " << ret;
        udsClient.Disconnect();
        return ret;
    }
    udsClient.Disconnect();
    return responseMessage.header.statusCode;
}

void ubse_api_buffer_free(ubse_api_buffer_t *apiBuffer)
{
    if (apiBuffer == nullptr) {
        return;
    }
    if (apiBuffer->buffer != nullptr) {
        free(apiBuffer->buffer);
        apiBuffer->buffer = nullptr;
        apiBuffer->length = 0;
    }
}

void ubse_api_buffer_delete(ubse_api_buffer_t *apiBuffer)
{
    if (apiBuffer == nullptr) {
        return;
    }
    if (apiBuffer->buffer != nullptr) {
        delete[] apiBuffer->buffer;
        apiBuffer->buffer = nullptr;
        apiBuffer->length = 0;
    }
}

uint32_t ubse_long_link_connect(void)
{
    ubse::ipc::UbseUDSClient::GetInstance().SetSocketPath(ubseSocketPath);
    auto ret = ubse::ipc::UbseUDSClient::GetInstance().PerSistentConnect();
    if (ret != UBSE_OK) {
        IPC_LOG_ERROR << "long link failed, err=" << ret;
        ubse::ipc::UbseUDSClient::GetInstance().Stop();
        return ret;
    } else {
        IPC_LOG_INFO << "long link success.";
    }
    return ret;
}

void HandleRequest(const UbseRequestMessage &request, UbseResponseMessage &resp)
{
    IPC_LOG_INFO << "Start handling API request, moduleCode = " << request.header.moduleCode
                 << ", opCode = " << request.header.opCode << " reqId=" << request.header.clientRequestId;
    const auto key = std::make_pair(request.header.moduleCode, request.header.opCode);
    UbseClientIpcHandler handler;
    // 获取handler
    {
        std::lock_guard<std::mutex> lock(clientIpcHandlerMutex);
        auto it = clientIpcHandlerMap.find(key);
        if (it == clientIpcHandlerMap.end()) {
            IPC_LOG_ERROR << "The API interface does not exist, moduleCode= " << request.header.moduleCode
                          << ", opCode:= " << request.header.opCode << " reqId=" << request.header.clientRequestId;
            resp = {{UBSE_ERR_DAEMON_UNREACHABLE, 0, request.header.clientRequestId}, nullptr};
            return;
        }
        handler = it->second;
    }

    // 调用handler
    auto handlerRet = handler(request);
    IPC_LOG_INFO << "Complete handling API request, moduleCode= " << request.header.moduleCode
                 << ", opCode= " << request.header.opCode << ", handlerRet= " << handlerRet
                 << " reqId=" << request.header.clientRequestId;
    resp = {{handlerRet, 0, request.header.clientRequestId}, nullptr};
}

void register_handler()
{
    ubse::ipc::UbseUDSClient::GetInstance().RegisterClientRequestHandler(HandleRequest);
}

uint32_t ubse_register_listen_event(uint16_t moduleCode, uint16_t opCode)
{
    // 向服务端注册监听的事件(长连接服务端需要知道事件发给哪个客户端)
    auto ret = ubse::ipc::UbseUDSClient::GetInstance().RegisterLongLinkNotify(moduleCode, opCode);
    if (ret != UBSE_OK) {
        IPC_LOG_ERROR << "register long link listen failed, moduleCode=" << moduleCode << ", opCode=" << opCode
                      << " ret=" << ret;
    }
    register_handler();
    return ret;
}

uint32_t ubse_shm_fault_register(ubs_mem_shm_fault_handler handler)
{
    // 向服务端注册故障监听事件
    uint32_t ret = ubse_register_listen_event(UBSE_LONG_LINK_REGISTER, UBSE_LONGLINK_FAULT_SHM);
    if (ret != UBSE_OK) {
        ubse::ipc::UbseUDSClient::GetInstance().Stop();
        return ret;
    }
    std::lock_guard<std::mutex> lock(clientIpcHandlerMutex);
    UbseClientIpcHandler ipcHandler = [handler](const UbseRequestMessage &msg) -> uint32_t {
        UbseMemFault fault{};
        auto ret = DeSerializeMemFault(fault, msg.body, size_t(msg.header.bodyLen));
        if (ret != UBSE_OK) {
            IPC_LOG_ERROR << "deserialize fault info failed.";
            return ret;
        }
        IPC_LOG_INFO << "shm fault name=" << fault.memName;
        return handler(fault.memName.c_str(), fault.handleId, static_cast<ubs_mem_fault_type_t>(fault.type));
    };
    clientIpcHandlerMap[{UBSE_LONG_LINK_REGISTER, UBSE_LONGLINK_FAULT_SHM}] = ipcHandler;
    return UBSE_OK;
}