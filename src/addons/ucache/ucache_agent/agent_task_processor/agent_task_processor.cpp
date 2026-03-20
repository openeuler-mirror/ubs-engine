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

#include "agent_task_processor.h"

#include <securec.h>
#include <unistd.h>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>
#include "turbo_runtime_manager.h"
#include "turbo_ucache_interface.h"
#include "ucache_config.h"
#include "ucache_error.h"
#include "ucache_serialize.h"

namespace ucache::agent {
using namespace ubse::com;
using namespace ubse::log;
using namespace ucache;
using namespace ucache::serialize;
using namespace turbo::ucache;

constexpr size_t MAX_SAFE_SIZE = 1 * 1024 * 1024;
uint32_t InitAgentTaskProcessor()
{
    UbseComEndpoint endpoint = {.moduleId = UCACHE_MODULE_CODE, .serviceId = ucache::OPCODE_EXECUTE_TASK};
    uint32_t ret = UbseRegRpcService(endpoint, ProcessTask);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "UbseRegRpcService for ProcessTask failed, ret=" << ret;
        return ret;
    }
    return UCACHE_OK;
}

void FreeMemory(uint8_t *data)
{
    delete[] data;
}

void FillErrorResp(UbseByteBuffer &resp, uint32_t errCode)
{
    TaskResponse taskRes{};
    taskRes.resCode = errCode;
    UCacheOutStream out;
    out << taskRes;
    // resp由RPC框架负责释放
    resp.data = out.GetBufferPointer();
    resp.len = out.GetSize();
    resp.freeFunc = FreeMemory;
}

void ProcessTask(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    if ((!req.data && req.len > 0) || req.len > MAX_SAFE_SIZE) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Invalid byteBuffer.";
        FillErrorResp(resp, INVALID_BUFFER);
        return;
    }
    if (req.len == 0) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Empty byteBuffer.";
        FillErrorResp(resp, INVALID_BUFFER);
        return;
    }
    TaskRequest header;
    {
        UCacheInStream builderIn(req.data, req.len);
        builderIn >> header;
        if (!builderIn.Check()) {
            UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "Failed to deserialize reqBuffer to TaskRequest.";
            FillErrorResp(resp, INVALID_BUFFER);
            return;
        }
    }
    if (!IsValidTaskType(header.type)) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Invalid taskType, taskType=" << TaskTypeToString(header.type) << ".";
        FillErrorResp(resp, INVALID_TASK_TYPE);
        return;
    }
    UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
        << "Agent ProcessTask start, type=" << TaskTypeToString(header.type) << ".";
    TaskResponse taskRes{};
    uint32_t ipcRet = TurboRuntimeManager::ucacheExecuteTask(header, taskRes);
    if (ipcRet != IPC_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "ucacheExecuteTask ipc send error, ret=" << ipcRet << ".";
        FillErrorResp(resp, TURBO_EXECUTE_ERROR);
        return;
    }

    UCacheOutStream builderOut;
    builderOut << taskRes;
    resp.data = builderOut.GetBufferPointer();
    resp.len = builderOut.GetSize();
    resp.freeFunc = FreeMemory;

    UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
        << "Agent ProcessTask success, taskType=" << TaskTypeToString(header.type) << ".";
}
} // namespace ucache::agent