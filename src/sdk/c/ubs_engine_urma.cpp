/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "ubs_engine_urma.h"
#include <netinet/in.h>
#include <securec.h>
#include "ubse_ipc_client.h"
#include "ubse_ipc_common.h"
#include "libubse_helper.h"

uint32_t ubs_urma_dev_get(ubs_urma_dev_t** urma_devices, uint32_t* urma_cnt)
{
    if ((urma_devices == nullptr) || (urma_cnt == nullptr)) {
        return UBS_ERR_NULL_POINTER;
    }
    ubse_api_buffer_t response_buffer = {nullptr, 0};
    ubse_api_buffer_t request_buffer = {nullptr, 0};
    request_buffer.length = sizeof(uint32_t);
    request_buffer.buffer = static_cast<uint8_t*>(malloc(request_buffer.length));
    if (request_buffer.buffer == nullptr) {
        return UBS_ENGINE_ERR_INTERNAL;
    }
    // 调用接口
    uint32_t ret = ubse_invoke_call(UBSE_URMA, UBSE_URMA_DEV_GET, &request_buffer, &response_buffer);
    ubse_api_buffer_free(&request_buffer);
    if (ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    // 解包

    if (ubse_urma_dev_unpack(response_buffer.buffer, response_buffer.length, urma_devices, urma_cnt) != UBS_SUCCESS) {
        ubse_api_buffer_free(&response_buffer);
        return UBS_ENGINE_ERR_INTERNAL;
    }
    ubse_api_buffer_free(&response_buffer);
    return UBS_SUCCESS;
}

uint32_t ubs_urma_dev_alloc(const char* name, ubs_urma_dev_info_t* dev_info)
{
    if ((name == nullptr) || (dev_info == nullptr)) {
        return UBS_ERR_NULL_POINTER;
    }
    uint32_t nameLen = strnlen(name, UBS_URMA_NAME_MAX);
    if (nameLen >= UBS_URMA_NAME_MAX) {
        return UBS_ENGINE_ERR_OUT_OF_RANGE;
    }
    ubse_api_buffer_t request_buffer = {(uint8_t*)name, nameLen + 1};
    ubse_api_buffer_t response_buffer = {nullptr, 0};

    // 调用接口
    uint32_t ret = ubse_invoke_call(UBSE_URMA, UBSE_URMA_DEV_ALLOC, &request_buffer, &response_buffer);
    if (ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    if (ubse_urma_dev_info_unpack(response_buffer.buffer, response_buffer.length, dev_info) != UBS_SUCCESS) {
        ubse_api_buffer_free(&response_buffer);
        return UBS_ENGINE_ERR_INTERNAL;
    }
    ubse_api_buffer_free(&response_buffer);
    return UBS_SUCCESS;
}

uint32_t ubs_urma_dev_free(const char* name)
{
    if (name == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    uint32_t nameLen = strnlen(name, UBS_URMA_NAME_MAX);
    if (nameLen >= UBS_URMA_NAME_MAX) {
        return UBS_ENGINE_ERR_OUT_OF_RANGE;
    }
    ubse_api_buffer_t request_buffer = {(uint8_t*)name, nameLen + 1};
    ubse_api_buffer_t response_buffer = {nullptr, 0};

    uint32_t ret = ubse_invoke_call(UBSE_URMA, UBSE_URMA_DEV_FREE, &request_buffer, &response_buffer);
    if (ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    ubse_api_buffer_free(&response_buffer);
    return UBS_SUCCESS;
}

uint32_t ubs_urma_qos_create(const ubs_urma_qos_config_t *configs, uint32_t count)
{
    if (configs == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    if (count < UBS_URMA_QOS_CONFIG_MIN_COUNT || count > UBS_URMA_QOS_CONFIG_MAX_COUNT) {
        return UBS_ENGINE_ERR_INVALID_PARAM;
    }

    for (uint32_t i = 0; i < count; i++) {
        if (configs[i].priority > UBS_URMA_QOS_PRIORITY_MAX) {
            return UBS_ENGINE_ERR_INVALID_PARAM;
        }
        if (configs[i].bandwidth == 0) {
            return UBS_ENGINE_ERR_INVALID_PARAM;
        }
    }

    for (uint32_t i = 0; i < count; i++) {
        for (uint32_t j = i + 1; j < count; j++) {
            if (configs[i].priority == configs[j].priority) {
                return UBS_ENGINE_ERR_INVALID_PARAM;
            }
        }
    }

    ubse_api_buffer_t request_buffer{};
    ubse_api_buffer_t response_buffer{};

    uint32_t ret = ubse_urma_qos_create_req_build(configs, count, &request_buffer);
    if (ret != UBS_SUCCESS) {
        return ret;
    }

    ret = ubse_invoke_call(UBSE_URMA, UBSE_URMA_QOS_CREATE, &request_buffer, &response_buffer);
    ubse_api_buffer_free(&request_buffer);
    if (ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }

    ubse_api_buffer_free(&response_buffer);
    return UBS_SUCCESS;
}

uint32_t ubs_urma_qos_delete(void)
{
    ubse_api_buffer_t request_buffer{nullptr, 0};
    ubse_api_buffer_t response_buffer{nullptr, 0};

    uint32_t ret = ubse_invoke_call(UBSE_URMA, UBSE_URMA_QOS_DELETE, &request_buffer, &response_buffer);
    if (ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }

    ubse_api_buffer_free(&response_buffer);
    return UBS_SUCCESS;
}