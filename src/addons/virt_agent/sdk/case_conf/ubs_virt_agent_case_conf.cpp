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

#include "ubs_virt_agent_case_conf.h"

#include <ubse_ipc_client.h>
#include <ubse_ipc_log.h>
#include "src/sdk/c/include/ubs_error.h"
#include "ubs_virt_agent_case_conf_helper.h"
#include "vm_sdk_def.h"
static const int MAX_CASE_CONF_PARAM_LENGTH = 128;
virt_agent_ret_t ubs_virt_agent_case_conf_get(case_conf_info_t *case_conf_info)
{
    if (case_conf_info == nullptr) {
        IPC_LOG_ERROR << "Invalid parameters: node_list or node_cnt is nullptr.";
        return VA_ERROR_INVALID_PARAM;
    }
    ubse_api_buffer_t request_buffer = {nullptr, 0};
    ubse_api_buffer_t response_buffer = {nullptr, 0};
    uint32_t ret = ubse_invoke_call(UBS_VA_CASE_CONF, UBS_VA_CASE_CONF_GET, &request_buffer, &response_buffer);
    ubse_api_buffer_free(&request_buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code = " << ret;
        ubse_api_buffer_free(&response_buffer);
        return VA_ERROR_BASE;
    }
    ret = ubse_case_conf_info_unpack(response_buffer.buffer, response_buffer.length, case_conf_info);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_node_info_unpack failed with error code = " << ret;
        ubse_api_buffer_free(&response_buffer);
        return VA_ERROR_DESERIALIZE_FAILED;
    }
    ubse_api_buffer_free(&response_buffer);
    IPC_LOG_DEBUG << "cur_case:" << case_conf_info->cur_case
                  << ", over_commitment_ratio:" << case_conf_info->over_commitment_ratio
                  << ", migrate_waterLine:" << case_conf_info->migrate_waterLine << ", index:" << case_conf_info->index;
    return VA_SUCCESS;
}

virt_agent_ret_t ubs_virt_agent_case_conf_set(const char *param, case_conf_set_info_t *case_conf_set_info)
{
    if (param == nullptr || case_conf_set_info == nullptr) {
        IPC_LOG_ERROR << "Invalid parameters: param or case_conf_set_info is nullptr.";
        return VA_ERROR_INVALID_PARAM;
    }

    ubse_api_buffer_t request_buffer = {nullptr, 0};
    request_buffer.length = strnlen(param, MAX_CASE_CONF_PARAM_LENGTH + 1);
    if (request_buffer.length > MAX_CASE_CONF_PARAM_LENGTH) {
        IPC_LOG_ERROR << "request_buffer.length is too large";
        return VA_ERROR_MEM_ALLOCATE_FAILED;
    }
    request_buffer.buffer = new (std::nothrow) uint8_t[request_buffer.length + 1];
    if (request_buffer.buffer == nullptr) {
        IPC_LOG_ERROR << "Failed to allocate memory for request_buffer.buffer.";
        return VA_ERROR_MEM_ALLOCATE_FAILED;
    }
    errno_t result = memcpy_s(request_buffer.buffer, request_buffer.length + 1, param, request_buffer.length);
    if (result != 0) {
        IPC_LOG_ERROR << "Failed to copy data using memcpy_s.";
        ubse_api_buffer_delete(&request_buffer);
        return VA_ERROR_MEM_COPY_FAILED;
    }
    request_buffer.buffer[request_buffer.length] = '\0';

    ubse_api_buffer_t response_buffer = {nullptr, 0};
    uint32_t ret = ubse_invoke_call(UBS_VA_CASE_CONF, UBS_VA_CASE_CONF_SET, &request_buffer, &response_buffer);
    SafeDeleteArray(request_buffer.buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code = " << ret;
        ubse_api_buffer_free(&response_buffer);
        return VA_ERROR_BASE;
    }
    ret = ubse_case_conf_set_unpack(response_buffer.buffer, response_buffer.length, case_conf_set_info);
    ubse_api_buffer_free(&response_buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_mem_borrow_execute_unpack failed with error code = " << ret;
        return VA_ERROR_DESERIALIZE_FAILED;
    }
    return VA_SUCCESS;
}
