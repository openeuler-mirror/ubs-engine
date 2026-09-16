/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubs_virt_agent_ham_migrate.h"

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include <ubse_ipc_client.h>
#include <ubse_ipc_log.h>
#include <ubse_pointer_process.h>

#include "ham_make_decision_msg.h"
#include "src/sdk/c/include/ubs_error.h"
#include "vm_sdk_def.h"

static uint16_t g_ipctimeout = 1200;
static const uint16_t ipctimeout_max = 1200;
// 48(max numa num) * 60 (size per block) + 2048 (other)
static const uint32_t HAM_MAX_INPUT_LENGTH = 4928;
static pthread_mutex_t g_ipctimeout_mutex = PTHREAD_MUTEX_INITIALIZER;
constexpr auto MILLISECONDS_PER_SECOND = 1000;

virt_agent_ret_t ubs_virt_agent_make_migrate_decision(uint32_t vmMemoryMB, const char* uuid, const char* destHostName,
                                                      uint32_t destNumaId, uint32_t* migrateStrategy)
{
    if (migrateStrategy == nullptr || uuid == nullptr || strnlen(uuid, SDK_NO_128 + 1) > SDK_NO_128 ||
        destHostName == nullptr || strnlen(destHostName, SDK_NO_128 + 1) > SDK_NO_128 || vmMemoryMB <= 0) {
        IPC_LOG_ERROR << "param invalid";
        return VA_ERROR_INVALID_PARAM;
    }
    vm::InputParams inputParams{
        .vmMemoryMB = vmMemoryMB, .uuid = uuid, .destHostName = destHostName, .destNumaId = destNumaId};
    vm::HamMakeDecisionMsg hamMakeDecisionMsg{inputParams};
    auto ret = hamMakeDecisionMsg.Serialize();
    if (ret != VA_SUCCESS) {
        IPC_LOG_ERROR << "HamMakeDecisionMsg Serialize failed";
        return VA_ERROR_SERIALIZE_FAILED;
    }
    ubse_api_buffer_t requestBuffer = {hamMakeDecisionMsg.SerializedData(), hamMakeDecisionMsg.SerializedDataSize()};
    ubse_api_buffer_t responseBuffer = {nullptr, 0};
    ret = ubse_invoke_call(UBS_VA_VM_MIGRATE, UBS_VA_MAKE_DECISION, &requestBuffer, &responseBuffer);
    ubse_api_buffer_delete(&requestBuffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code: " << ret;
        ubse_api_buffer_free(&responseBuffer);
        return VA_ERROR_BASE;
    }
    if (responseBuffer.buffer == nullptr) {
        IPC_LOG_ERROR << "response buffer is nullptr";
        return VA_ERROR_NULL_POINTER;
    }
    errno_t memcpy_ret = memcpy_s(migrateStrategy, sizeof(uint32_t), responseBuffer.buffer, responseBuffer.length);
    if (memcpy_ret != EOK) {
        IPC_LOG_ERROR << "memcpy_s failed with error code: " << memcpy_ret;
        ubse_api_buffer_free(&responseBuffer);
        return VA_ERROR_MEM_COPY_FAILED;
    }
    ubse_api_buffer_free(&responseBuffer);
    return VA_SUCCESS;
}

virt_agent_ret_t RackStartIpcClientWithTimeout(uint16_t timeout)
{
    pthread_mutex_lock(&g_ipctimeout_mutex);
    if (timeout > ipctimeout_max) {
        g_ipctimeout = ipctimeout_max;
    } else if (timeout <= 0) {
        pthread_mutex_unlock(&g_ipctimeout_mutex);
        return VA_ERROR_BASE;
    } else {
        g_ipctimeout = timeout;
    }
    pthread_mutex_unlock(&g_ipctimeout_mutex);
    return VA_SUCCESS;
}

uint16_t GetTimeout()
{
    uint16_t timeout;
    pthread_mutex_lock(&g_ipctimeout_mutex);
    timeout = g_ipctimeout;
    pthread_mutex_unlock(&g_ipctimeout_mutex);
    return timeout;
}

int AllocateRequestBuffer(ubse_api_buffer_t* request_buffer, HamComByteBuffer* request)
{
    request_buffer->length = static_cast<uint32_t>(request->len);
    request_buffer->buffer = new (std::nothrow) uint8_t[request_buffer->length];
    if (request_buffer->buffer == nullptr) {
        IPC_LOG_ERROR << "Failed to allocate memory for request_buffer.";
        return VA_ERROR_MEM_ALLOCATE_FAILED;
    }
    errno_t copy_result =
        memcpy_s(request_buffer->buffer, request_buffer->length, request->data, request_buffer->length);
    if (copy_result != 0) {
        IPC_LOG_ERROR << "memcpy_s failed with error code = " << copy_result;
        SafeDeleteArray(request_buffer->buffer);
        return VA_ERROR_MEM_COPY_FAILED;
    }
    return VA_SUCCESS;
}

namespace {
struct TaskState {
    std::mutex mtx;
    std::condition_variable cv;
    bool done = false;
    bool abandoned = false;
    int result = VA_SUCCESS;
    ubse_api_buffer_t resp = {nullptr, 0};
};
} // namespace

int CallExternalApiWithTimeout(ubse_api_buffer_t* request_buffer, ubse_api_buffer_t* response_buffer,
                               uint16_t timeout_time)
{
    auto state = std::make_shared<TaskState>();
    // Copy request buffer pointer/length before spawning the thread to avoid
    // dereferencing request_buffer after the caller returns on timeout.
    auto reqBuffer = request_buffer->buffer;
    auto reqLength = request_buffer->length;
    auto task = [state, reqBuffer, reqLength]() {
        ubse_api_buffer_t req = {reqBuffer, reqLength};
        ubse_api_buffer_t resp = {nullptr, 0};
        uint32_t ret = ubse_invoke_call(UBS_VA_VM_MIGRATE, UBS_VA_HAM_NORTH, &req, &resp);
        std::lock_guard<std::mutex> lock(state->mtx);
        SafeDeleteArray(req.buffer);
        if (ret != UBS_SUCCESS) {
            ubse_api_buffer_free(&resp);
            state->result = VA_ERROR_BASE;
        } else {
            state->resp = resp;
        }
        state->done = true;
        if (state->abandoned && state->resp.buffer != nullptr) {
            ubse_api_buffer_free(&state->resp);
        }
        state->cv.notify_one();
    };
    try {
        std::thread th(task);
        th.detach();
    } catch (const std::exception& e) {
        IPC_LOG_ERROR << "Failed to create thread for external api call: " << e.what();
        SafeDeleteArray(request_buffer->buffer);
        return VA_ERROR_BASE;
    }
    std::chrono::milliseconds timeout(timeout_time * MILLISECONDS_PER_SECOND);
    std::unique_lock<std::mutex> lock(state->mtx);
    if (state->cv.wait_for(lock, timeout, [&]() { return state->done; })) {
        if (state->result != VA_SUCCESS) {
            return state->result;
        }
        response_buffer->buffer = state->resp.buffer;
        response_buffer->length = state->resp.length;
        return VA_SUCCESS;
    }
    state->abandoned = true;
    return VA_ERROR_TIMEOUT_FAILED;
}

int ProcessResponse(HamComByteBuffer* response, ubse_api_buffer_t* response_buffer)
{
    if (response_buffer == nullptr) {
        return VA_ERROR_BASE;
    }
    response->len = static_cast<uint32_t>(response_buffer->length);
    response->data = new (std::nothrow) uint8_t[response_buffer->length];
    if (response->data == nullptr) {
        IPC_LOG_ERROR << "Failed to allocate memory for response data.";
        return VA_ERROR_MEM_ALLOCATE_FAILED;
    }
    errno_t copy_result =
        memcpy_s(response->data, response_buffer->length, response_buffer->buffer, response_buffer->length);
    if (copy_result != 0) {
        IPC_LOG_ERROR << "memcpy_s failed with error code = " << copy_result;
        SafeDeleteArray(response->data);
        return VA_ERROR_MEM_COPY_FAILED;
    }
    return VA_SUCCESS;
}

int RackSyncSendForHam(HamComByteBuffer* request, HamComByteBuffer* response)
{
    if (request == nullptr || request->data == nullptr || request->len > HAM_MAX_INPUT_LENGTH || request->len == 0 ||
        request->data + request->len < request->data || response == nullptr) {
        return VA_ERROR_INVALID_PARAM;
    }
    uint16_t timeout = GetTimeout();

    ubse_api_buffer_t request_buffer = {nullptr, 0};
    int ret = AllocateRequestBuffer(&request_buffer, request);
    if (ret != VA_SUCCESS) {
        return ret;
    }

    ubse_api_buffer_t response_buffer = {nullptr, 0};
    ret = CallExternalApiWithTimeout(&request_buffer, &response_buffer, timeout);
    if (ret != VA_SUCCESS) {
        return ret;
    }

    ret = ProcessResponse(response, &response_buffer);
    ubse_api_buffer_free(&response_buffer);
    if (ret != VA_SUCCESS) {
        return ret;
    }

    return VA_SUCCESS;
}

void backgroundTask(std::shared_ptr<ubse_api_buffer_t> req_buf)
{
    ubse_api_buffer_t response_buffer = {nullptr, 0};
    ubse_invoke_call(UBS_VA_VM_MIGRATE, UBS_VA_HAM_NORTH, req_buf.get(), &response_buffer);
    SafeDeleteArray(req_buf->buffer);
    ubse_api_buffer_free(&response_buffer);
}

int RackAsyncSendForHam(HamComByteBuffer* request, HamComCallbackDef* callback)
{
    if (request == nullptr || request->data == nullptr || request->len > HAM_MAX_INPUT_LENGTH || request->len == 0 ||
        request->data + request->len < request->data) {
        return VA_ERROR_INVALID_PARAM;
    }
    ubse_api_buffer_t* raw_buffer = new (std::nothrow) ubse_api_buffer_t{nullptr, 0};
    if (raw_buffer == nullptr) {
        IPC_LOG_ERROR << "Failed to allocate memory for ubse_api_buffer_t.";
        return VA_ERROR_MEM_ALLOCATE_FAILED;
    }
    std::shared_ptr<ubse_api_buffer_t> request_buffer(raw_buffer);

    request_buffer->length = static_cast<uint32_t>(request->len);
    request_buffer->buffer = new (std::nothrow) uint8_t[request_buffer->length];

    if (request_buffer->buffer == nullptr) {
        IPC_LOG_ERROR << "Failed to allocate memory for request_buffer.";
        return VA_ERROR_MEM_ALLOCATE_FAILED;
    }
    errno_t copy_result =
        memcpy_s(request_buffer->buffer, request_buffer->length, request->data, request_buffer->length);
    if (copy_result != 0) {
        IPC_LOG_ERROR << "memcpy_s failed with error code = " << copy_result;
        SafeDeleteArray(request_buffer->buffer);
        return VA_ERROR_BASE;
    }

    try {
        std::thread th(backgroundTask, request_buffer);
        th.detach();
    } catch (const std::exception& e) {
        IPC_LOG_ERROR << "Failed to create thread for background task: " << e.what();
        SafeDeleteArray(request_buffer->buffer);
        return VA_ERROR_BASE;
    }

    return 0;
}