/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
#include "libubse_helper.h"
#include "ubse_ipc_client.h"
#include "ubse_ipc_common.h"

uint32_t ubs_urma_dev_get(const ubs_urma_type urma_type, urma_device_t **urma_devices, uint32_t *urma_cnt)
{
    if ((urma_devices == NULL) || (urma_cnt == NULL)) {
        return UBS_ERR_NULL_POINTER;
    }
    ubse_api_buffer_t response_buffer = {NULL, 0};
    ubse_api_buffer_t request_buffer = {NULL, 0};
    uint32_t trans_urma_type = urma_type;
    request_buffer.length = sizeof(uint32_t);
    request_buffer.buffer = malloc(request_buffer.length);
    if (request_buffer.buffer == NULL) {
        return UBS_ENGINE_ERR_INTERNAL;
    }
    *request_buffer.buffer = trans_urma_type;
    // 调用接口
    auto ret = ubse_invoke_call(UBSE_URMA, UBSE_URMA_DEV_GET, &request_buffer, &response_buffer);
    ubse_api_buffer_free(&request_buffer);
    if (ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    // 解包

    if (ubse_urma_dev_get_unpack(response_buffer.buffer, response_buffer.length, urma_devices, urma_cnt) !=
        UBS_SUCCESS) {
        ubse_api_buffer_free(&response_buffer);
        return UBS_ENGINE_ERR_INTERNAL;
    }
    ubse_api_buffer_free(&response_buffer);
    return UBS_SUCCESS;
}

uint32_t ubs_urma_dev_alloc(const char *name, ubs_urma_dev_path_t *dev_info)
{
    if ((name == NULL) || (dev_info == NULL)) {
        return UBS_ERR_NULL_POINTER;
    }
    uint32_t nameLen = strnlen(name, UBS_URMA_NAME_MAX);
    if (nameLen >= UBS_URMA_NAME_MAX) {
        return UBS_ENGINE_ERR_OUT_OF_RANGE;
    }
    ubse_api_buffer_t request_buffer = {name, nameLen + 1};
    ubse_api_buffer_t response_buffer = {NULL, 0};

    // 调用接口
    uint32_t ret = ubse_invoke_call(UBSE_URMA, UBSE_URMA_DEV_ALLOC, &request_buffer, &response_buffer);
    if (ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    // 解包
    if (response_buffer.length != UBS_MAX_URMA_PATH_LENGTH * sizeof(char)) {
        ubse_api_buffer_free(&response_buffer);
        return UBS_ENGINE_ERR_INTERNAL;
    }
    if (ubse_urma_dev_alloc_unpack(response_buffer.buffer, response_buffer.length, dev_info) != UBS_SUCCESS) {
        ubse_api_buffer_free(&response_buffer);
        return UBS_ENGINE_ERR_INTERNAL;
    }
    ubse_api_buffer_free(&response_buffer);
    return UBS_SUCCESS;
}

uint32_t ubs_urma_dev_free(const char *name)
{
    if (name == NULL) {
        return UBS_ERR_NULL_POINTER;
    }
    uint32_t nameLen = strnlen(name, UBS_URMA_NAME_MAX);
    if (nameLen >= UBS_URMA_NAME_MAX) {
        return UBS_ENGINE_ERR_OUT_OF_RANGE;
    }
    ubse_api_buffer_t request_buffer = {name, nameLen + 1};
    ubse_api_buffer_t response_buffer = {NULL, 0};

    // 调用接口
    uint32_t ret = ubse_invoke_call(UBSE_URMA, UBSE_URMA_DEV_FREE, &request_buffer, &response_buffer);
    if (ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    ubse_api_buffer_free(&response_buffer);
    return UBS_SUCCESS;
}

uint32_t ubs_urma_bandwidth_set(const char *name, uint32_t minBandWidth, uint32_t maxBandWidth)
{
    // 参数校验
    if (name == NULL) {
        return UBS_ERR_NULL_POINTER;
    }
    uint32_t nameLen = strnlen(name, UBS_URMA_NAME_MAX);
    if ((nameLen >= UBS_URMA_NAME_MAX) || (minBandWidth > maxBandWidth)) {
        return UBS_ENGINE_ERR_OUT_OF_RANGE;
    }
    ubse_api_buffer_t request_buffer = {NULL, 0};
    ubse_api_buffer_t response_buffer = {NULL, 0};
    // 打包
    uint32_t totallen = nameLen + 1 + sizeof(minBandWidth) + sizeof(maxBandWidth);
    request_buffer.length = totallen;
    request_buffer.buffer = malloc(totallen);
    if (request_buffer.buffer == NULL) {
        return UBS_ENGINE_ERR_INTERNAL;
    }
    uint8_t *buffer = request_buffer.buffer;
    uint32_t ret = strcpy_s((char *)buffer, nameLen + 1, name);
    if (ret != EOK) {
        ubse_api_buffer_free(&request_buffer);
        return UBS_ERR_OUT_OF_RANGE;
    }
    buffer += nameLen + 1;
    *(uint32_t *)buffer = htonl(minBandWidth);
    buffer += sizeof(minBandWidth);
    *(uint32_t *)buffer = htonl(maxBandWidth);
    // 调用接口
    ret = ubse_invoke_call(UBSE_URMA, UBSE_URMA_QOS_SET, &request_buffer, &response_buffer);
    ubse_api_buffer_free(&request_buffer);
    if (ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    ubse_api_buffer_free(&response_buffer);
    return UBS_SUCCESS;
}

uint32_t ubs_urma_bandwidth_reset(const char *name)
{
    // 参数校验
    if (name == NULL) {
        return UBS_ERR_NULL_POINTER;
    }
    uint32_t nameLen = strnlen(name, UBS_URMA_NAME_MAX);
    if (nameLen >= UBS_URMA_NAME_MAX) {
        return UBS_ENGINE_ERR_OUT_OF_RANGE;
    }
    ubse_api_buffer_t request_buffer = {name, nameLen + 1};
    ubse_api_buffer_t response_buffer = {NULL, 0};

    // 调用接口
    uint32_t ret = ubse_invoke_call(UBSE_URMA, UBSE_URMA_QOS_RESET, &request_buffer, &response_buffer);
    if (ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    ubse_api_buffer_free(&response_buffer);
    return UBS_SUCCESS;
}

uint32_t ubs_urma_bandwidth_get(const char *name, uint32_t *minBandWidth, uint32_t *maxBandWidth)
{
    // 参数校验
    if ((name == NULL) || (minBandWidth == NULL) || (maxBandWidth == NULL)) {
        return UBS_ERR_NULL_POINTER;
    }
    uint32_t nameLen = strnlen(name, UBS_URMA_NAME_MAX);
    if (nameLen >= UBS_URMA_NAME_MAX) {
        return UBS_ENGINE_ERR_OUT_OF_RANGE;
    }
    ubse_api_buffer_t request_buffer = {name, nameLen + 1};
    ubse_api_buffer_t response_buffer = {NULL, 0};

    // 调用接口
    uint32_t ret = ubse_invoke_call(UBSE_URMA, UBSE_URMA_QOS_GET, &request_buffer, &response_buffer);
    if (ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    // 解包
    if (response_buffer.length != sizeof(uint32_t) + sizeof(uint32_t)) {
        ubse_api_buffer_free(&response_buffer);
        return UBS_ENGINE_ERR_INTERNAL;
    }
    uint8_t *buffer = response_buffer.buffer;
    *minBandWidth = ntohl(*(uint32_t *)buffer);
    buffer += sizeof(uint32_t);
    *maxBandWidth = ntohl(*(uint32_t *)buffer);
    ubse_api_buffer_free(&response_buffer);
    return UBS_SUCCESS;
}