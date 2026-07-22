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

#include "ubse_com.h"
#include "ubse_logger.h"

#include "ucache_config.h"
#include "ucache_error.h"
#include "ucache_serialize.h"

#include "master_task_controller.h"

using namespace ubse::log;
using namespace ubse::com;
using namespace ucache::serialize;

namespace ucache::master {
void OnTaskResult(void* ctx, const UbseByteBuffer& respData, uint32_t resCode)
{
    if (ctx == nullptr) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Context pointer is null.";
        return;
    }
    if (resCode != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Rpcsend error, resCode=" << resCode << ".";
        return;
    }
    TaskResponse tResp{};
    if (!respData.data && respData.len > 0) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Invalid byteBuffer.";
        return;
    }
    if (respData.len == 0) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Empty byteBuffer.";
        return;
    }
    UCacheInStream in(respData.data, respData.len);
    in >> tResp;
    if (!in.Check()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Failed to deserialize respData to tResp.";
        return;
    }
    TaskResponse* result = static_cast<TaskResponse*>(ctx);
    *result = tResp;
}

uint32_t DispatchTask(const TaskRequest& tReq, TaskResponse& tResp, const std::string& destNode)
{
    if (!IsValidTaskType(tReq.type)) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Invalid taskType, taskType=" << static_cast<uint32_t>(tReq.type) << ".";
        return INVALID_TASK_TYPE;
    }
    UbseComEndpoint endpoint = {
        .moduleId = UCACHE_MODULE_CODE, .serviceId = ucache::OPCODE_EXECUTE_TASK, .address = destNode};
    UCacheOutStream out{};
    out << tReq;
    UbseByteBuffer reqData = {.data = out.GetBufferPointer(), .len = out.GetSize(), .freeFunc = nullptr};
    uint32_t ret = UbseRpcSend(endpoint, reqData, &tResp, OnTaskResult);
    delete[] reqData.data;
    reqData.data = nullptr;
    reqData.len = 0;
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Rpcsend error " << ret;
    }
    return ret;
}
} // namespace ucache::master