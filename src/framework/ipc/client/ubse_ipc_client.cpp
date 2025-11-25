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

#include "ubse_common_def.h"
#include "ubse_ipc_common.h"
#include "ubse_uds_client.h"

static std::string ubseSocketPath = ubse::common::def::UBSE_UDS_SOCKET_PATH;

static uint32_t CopyResponseBody(const UbseResponseMessage &src, ubse_api_buffer_t *dest)
{
    dest->buffer = nullptr;
    dest->length = 0;
    if (src.header.bodyLen == 0 || !src.body) {
        return IPC_SUCCESS;
    }
    dest->buffer = static_cast<uint8_t *>(malloc(src.header.bodyLen));
    if (!dest->buffer) {
        return IPC_ERROR_DESERIALIZATION_FAILED;
    }
    dest->length = src.header.bodyLen;

    if (memcpy_s(dest->buffer, src.header.bodyLen, src.body, src.header.bodyLen) != EOK) {
        free(dest->buffer);
        dest->buffer = nullptr;
        dest->length = 0;
        return IPC_ERROR_DESERIALIZATION_FAILED;
    }

    return IPC_SUCCESS;
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
        return IPC_ERROR_INVALID_ARGUMENT;
    }
    response_data->buffer = nullptr;
    response_data->length = 0;
    // Establish connection
    ubse::ipc::UbseUDSClient udsClient(ubseSocketPath);
    auto ret = udsClient.Connect();
    if (ret != IPC_SUCCESS) {
        return ret;
    }
    // Pack request data
    UbseRequestMessage requestMessage{{module_code, op_code, request_data->length}, request_data->buffer};
    UbseResponseMessage responseMessage{};
    // Send request
    ret = udsClient.Send(requestMessage, responseMessage);
    if (ret != IPC_SUCCESS) {
        udsClient.Disconnect();
        return ret;
    }
    ret = CopyResponseBody(responseMessage, response_data);
    if (responseMessage.freeFunc && responseMessage.body) {
        responseMessage.freeFunc(responseMessage.body);
        responseMessage.body = nullptr;
    }
    if (ret != IPC_SUCCESS) {
        udsClient.Disconnect();
        return ret;
    }
    udsClient.Disconnect();
    return responseMessage.header.statusCode;
}

void ubse_api_buffer_free(ubse_api_buffer_t *apiBuffer)
{
    if (apiBuffer->buffer != nullptr) {
        free(apiBuffer->buffer);
        apiBuffer->buffer = nullptr;
        apiBuffer->length = 0;
    }
}