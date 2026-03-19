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
#include "libubse_helper.h"
#include "ubse_ipc_client.h"
#include "ubse_ipc_common.h"

uint32_t ubs_urma_dev_get(ubs_urma_dev_t **urma_devices, uint32_t *urma_cnt)
{
    if ((urma_devices == nullptr) || (urma_cnt == nullptr)) {
        return UBS_ERR_NULL_POINTER;
    }
    ubse_api_buffer_t response_buffer = {nullptr, 0};
    ubse_api_buffer_t request_buffer = {nullptr, 0};
    request_buffer.length = sizeof(uint32_t);
    request_buffer.buffer = static_cast<uint8_t *>(malloc(request_buffer.length));
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

uint32_t ubs_urma_dev_alloc(const char *name, ubs_urma_dev_info_t *dev_info)
{
    if ((name == nullptr) || (dev_info == nullptr)) {
        return UBS_ERR_NULL_POINTER;
    }
    uint32_t nameLen = strnlen(name, UBS_URMA_NAME_MAX);
    if (nameLen >= UBS_URMA_NAME_MAX) {
        return UBS_ENGINE_ERR_OUT_OF_RANGE;
    }
    ubse_api_buffer_t request_buffer = {(uint8_t *)name, nameLen + 1};
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

uint32_t ubs_urma_dev_free(const char *name)
{
    if (name == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    uint32_t nameLen = strnlen(name, UBS_URMA_NAME_MAX);
    if (nameLen >= UBS_URMA_NAME_MAX) {
        return UBS_ENGINE_ERR_OUT_OF_RANGE;
    }
    ubse_api_buffer_t request_buffer = {(uint8_t *)name, nameLen + 1};
    ubse_api_buffer_t response_buffer = {nullptr, 0};

    // 调用接口
    uint32_t ret = ubse_invoke_call(UBSE_URMA, UBSE_URMA_DEV_FREE, &request_buffer, &response_buffer);
    if (ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    ubse_api_buffer_free(&response_buffer);
    return UBS_SUCCESS;
}

uint32_t ubs_urma_bandwidth_set(const char* name, uint32_t minBandWidth, uint32_t maxBandWidth)
{
    // 参数校验
    if (name == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }

    size_t nameLen = strnlen(name, UBS_URMA_NAME_MAX);
    if (nameLen >= UBS_URMA_NAME_MAX || minBandWidth > maxBandWidth) {
        return UBS_ENGINE_ERR_OUT_OF_RANGE;
    }

    // 计算总长度
    const size_t totalLen = nameLen + 1 + sizeof(uint32_t) + sizeof(uint32_t);

    ubse_api_buffer_t request_buffer{};
    ubse_api_buffer_t response_buffer{};

    // 分配内存
    request_buffer.buffer = static_cast<uint8_t*>(malloc(totalLen));
    if (request_buffer.buffer == nullptr) {
        return UBS_ENGINE_ERR_INTERNAL;
    }
    request_buffer.length = static_cast<uint32_t>(totalLen);

    // 填充数据
    uint8_t* ptr = request_buffer.buffer;

    // 1. 复制字符串（包含结尾的 '\0'）
    int copy_ret = strcpy_s(reinterpret_cast<char*>(ptr), nameLen + 1, name);
    if (copy_ret != EOK) {
        ubse_api_buffer_free(&request_buffer);
        return UBS_ERR_OUT_OF_RANGE;
    }
    ptr += nameLen + 1;

    // 2. 写入 minBandWidth
    errno_t err = memcpy_s(ptr, sizeof(uint32_t), &minBandWidth, sizeof(uint32_t));
    if (err != 0) {
        ubse_api_buffer_free(&request_buffer);
        return UBS_ENGINE_ERR_INTERNAL;  // 或其他合适的错误码
    }
    ptr += sizeof(uint32_t);

    // 3. 写入 maxBandWidth
    err = memcpy_s(ptr, sizeof(uint32_t), &maxBandWidth, sizeof(uint32_t));
    if (err != 0) {
        ubse_api_buffer_free(&request_buffer);
        return UBS_ENGINE_ERR_INTERNAL;  // 或其他合适的错误码
    }

    // 调用接口
    uint32_t invoke_ret = ubse_invoke_call(UBSE_URMA, UBSE_URMA_QOS_SET,
                                           &request_buffer, &response_buffer);

    // 释放请求缓冲区（无论成功与否都要释放）
    ubse_api_buffer_free(&request_buffer);

    if (invoke_ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(invoke_ret);
    }

    // 释放响应缓冲区
    ubse_api_buffer_free(&response_buffer);

    return UBS_SUCCESS;
}

uint32_t ubs_urma_bandwidth_reset(const char *name)
{
    // 参数校验
    if (name == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    uint32_t nameLen = strnlen(name, UBS_URMA_NAME_MAX);
    if (nameLen >= UBS_URMA_NAME_MAX) {
        return UBS_ENGINE_ERR_OUT_OF_RANGE;
    }
    ubse_api_buffer_t request_buffer = {(uint8_t *)name, nameLen + 1};
    ubse_api_buffer_t response_buffer = {nullptr, 0};

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
    if ((name == nullptr) || (minBandWidth == nullptr) || (maxBandWidth == nullptr)) {
        return UBS_ERR_NULL_POINTER;
    }
    uint32_t nameLen = strnlen(name, UBS_URMA_NAME_MAX);
    if (nameLen >= UBS_URMA_NAME_MAX) {
        return UBS_ENGINE_ERR_OUT_OF_RANGE;
    }
    ubse_api_buffer_t request_buffer = {(uint8_t *)name, nameLen + 1};
    ubse_api_buffer_t response_buffer = {nullptr, 0};

    // 调用接口
    uint32_t ret = ubse_invoke_call(UBSE_URMA, UBSE_URMA_QOS_GET, &request_buffer, &response_buffer);
    if (ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    // 解包

    if (ubse_urma_qos_unpack(response_buffer.buffer, response_buffer.length, minBandWidth, maxBandWidth) !=
        UBS_SUCCESS) {
        ubse_api_buffer_free(&response_buffer);
        return UBS_ENGINE_ERR_INTERNAL;
    }
    ubse_api_buffer_free(&response_buffer);
    return UBS_SUCCESS;
}