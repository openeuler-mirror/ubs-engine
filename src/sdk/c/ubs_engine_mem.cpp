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

#include "ubs_engine_mem.h"

#include <securec.h>

#include "ubse_ipc_client.h"
#include "ubse_ipc_common.h"
#include "ubse_ipc_log.h"
#include "libubse_helper.h"
#include "ubs_error.h"

int32_t ubs_mem_numastat_get(uint32_t slot_id, ubs_mem_numastat_t** numa_mems, uint32_t* numa_mem_cnt)
{
    // 参数校验
    if (numa_mems == nullptr || numa_mem_cnt == nullptr) {
        IPC_LOG_ERROR << "Invalid parameters: numa_mems or numa_mem_cnt is nullptr";
        return UBS_ERR_NULL_POINTER;
    }
    // 打包
    ubse_api_buffer_t request_buffer = {.buffer = nullptr, .length = 0};
    request_buffer.buffer = static_cast<uint8_t*>(malloc(sizeof(uint32_t)));
    if (!request_buffer.buffer) {
        ubse_api_buffer_free(&request_buffer);
        return UBS_ERR_OUT_OF_MEMORY;
    }
    auto ret = memcpy_s(request_buffer.buffer, sizeof(uint32_t), &slot_id, sizeof(uint32_t));
    if (ret != EOK) {
        ubse_api_buffer_free(&request_buffer);
        return ubse_map_sys_error(ret);
    }
    request_buffer.length = sizeof(uint32_t);
    // 调用接口
    ubse_api_buffer_t response_buffer = {.buffer = nullptr, .length = 0};
    ret = ubse_invoke_call(UBSE_NODE, UBSE_NODE_NUMA_MEM_GET, &request_buffer, &response_buffer);
    ubse_api_buffer_free(&request_buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code: " << ret;
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    // 解包
    ret = ubse_node_numa_mem_list_unpack(response_buffer.buffer, response_buffer.length, numa_mems, numa_mem_cnt);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack node numa mem list, error" << ret;
    }
    ubse_api_buffer_free(&response_buffer);
    return ret;
}

int32_t ubs_mem_fd_create(const char* name, uint64_t size, const ubs_mem_fd_owner_t* owner, mode_t mode,
                          ubs_mem_distance_t distance, ubs_mem_fd_desc_t* fd_desc)
{
    // 参数校验
    auto ret = ubse_mem_create_req_is_valid(name, size);
    if (ret != UBS_SUCCESS) {
        return ret;
    }
    if (fd_desc == nullptr) {
        IPC_LOG_ERROR << "Invalid parameters: fd_desc is nullptr";
        return UBS_ERR_NULL_POINTER;
    }
    // 申请内存
    const size_t total_len = ubse_mem_fd_create_req_calc_size(name);

    ubse_api_buffer_t request_buffer;
    request_buffer.buffer = static_cast<uint8_t*>(malloc(total_len));
    if (!request_buffer.buffer) {
        ubse_api_buffer_free(&request_buffer);
        return UBS_ERR_OUT_OF_MEMORY;
    }
    request_buffer.length = total_len;
    // 打包
    ret = ubse_mem_fd_create_req_pack(name, size, owner, mode, distance, request_buffer.buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack fd create request, error" << ret;
        ubse_api_buffer_free(&request_buffer);
        return static_cast<int32_t>(ret);
    }
    // 调用接口
    ubse_api_buffer_t response_buffer = {.buffer = nullptr, .length = 0};
    ret = static_cast<ubs_error_t>(ubse_invoke_call(UBSE_MEM, UBSE_MEM_FD_CREATE, &request_buffer, &response_buffer));
    ubse_api_buffer_free(&request_buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code: " << ret;
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    // 解包
    ret = ubse_mem_fd_desc_unpack(response_buffer.buffer, response_buffer.length, fd_desc);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack fd desc, error" << ret;
    }
    ubse_api_buffer_free(&response_buffer);
    return ret;
}

int32_t ubs_mem_fd_create_with_lender(const char* name, const ubs_mem_fd_owner_t* owner, mode_t mode,
                                      const ubs_mem_lender_t* lender, uint32_t lender_cnt, ubs_mem_fd_desc_t* fd_desc)
{
    // 参数校验
    auto ret = ubse_mem_create_with_lender_req_is_valid(name, lender, lender_cnt);
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }
    if (fd_desc == nullptr) {
        IPC_LOG_ERROR << "Invalid parameters: fd_desc is nullptr";
        return UBS_ERR_NULL_POINTER;
    }
    // 申请内存
    const size_t total_len = ubse_mem_fd_create_with_lender_req_calc_size(name, lender_cnt);

    ubse_api_buffer_t request_buffer;
    request_buffer.buffer = static_cast<uint8_t*>(malloc(total_len));
    if (!request_buffer.buffer) {
        ubse_api_buffer_free(&request_buffer);
        return UBS_ERR_OUT_OF_MEMORY;
    }
    request_buffer.length = total_len;
    // 打包
    ret = ubse_mem_fd_create_with_lender_req_pack(name, owner, mode, lender, lender_cnt, request_buffer.buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack fd create with lender request, error" << ret;
        ubse_api_buffer_free(&request_buffer);
        return static_cast<int32_t>(ret);
    }
    // 调用接口
    ubse_api_buffer_t response_buffer = {.buffer = nullptr, .length = 0};
    ret = static_cast<ubs_error_t>(
        ubse_invoke_call(UBSE_MEM, UBSE_MEM_FD_WITH_LEND_INFO, &request_buffer, &response_buffer));
    ubse_api_buffer_free(&request_buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code: " << ret;
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    // 解包
    ret = ubse_mem_fd_desc_unpack(response_buffer.buffer, response_buffer.length, fd_desc);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack fd desc, error" << ret;
    }
    ubse_api_buffer_free(&response_buffer);
    return ret;
}

int32_t ubs_mem_fd_create_with_candidate(const char* name, uint64_t size, const ubs_mem_fd_owner_t* owner, mode_t mode,
                                         const uint32_t* slot_ids, uint32_t slot_cnt, ubs_mem_fd_desc_t* fd_desc)
{
    // 参数校验
    auto ret = ubse_mem_create_with_candidate_req_is_valid(name, size, slot_ids, slot_cnt);
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }
    if (fd_desc == nullptr) {
        IPC_LOG_ERROR << "Invalid parameters: fd_desc is nullptr";
        return UBS_ERR_NULL_POINTER;
    }
    // 申请内存
    const size_t total_len = ubse_mem_fd_create_with_candidate_req_calc_size(name, slot_cnt);

    ubse_api_buffer_t request_buffer;
    request_buffer.buffer = static_cast<uint8_t*>(malloc(total_len));
    if (!request_buffer.buffer) {
        ubse_api_buffer_free(&request_buffer);
        return UBS_ERR_OUT_OF_MEMORY;
    }
    request_buffer.length = total_len;
    // 打包
    ret =
        ubse_mem_fd_create_with_candidate_req_pack(name, size, owner, mode, slot_ids, slot_cnt, request_buffer.buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack fd create with candidate request, error" << ret;
        ubse_api_buffer_free(&request_buffer);
        return static_cast<int32_t>(ret);
    }
    // 调用接口
    ubse_api_buffer_t response_buffer = {.buffer = nullptr, .length = 0};
    ret = static_cast<ubs_error_t>(
        ubse_invoke_call(UBSE_MEM, UBSE_MEM_FD_CREATE_WITH_CANDIDATE, &request_buffer, &response_buffer));
    ubse_api_buffer_free(&request_buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code: " << ret;
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    // 解包
    ret = ubse_mem_fd_desc_unpack(response_buffer.buffer, response_buffer.length, fd_desc);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack fd desc, error" << ret;
    }
    ubse_api_buffer_free(&response_buffer);
    return ret;
}

int32_t ubs_mem_fd_permission(const char* name, const ubs_mem_fd_owner_t* owner, mode_t mode)
{
    // 参数校验
    auto ret = ubse_mem_name_is_valid(name);
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }
    // 申请内存
    const size_t total_len = ubse_mem_fd_permission_req_calc_size(name);

    ubse_api_buffer_t request_buffer;
    request_buffer.buffer = static_cast<uint8_t*>(malloc(total_len));
    if (!request_buffer.buffer) {
        ubse_api_buffer_free(&request_buffer);
        return UBS_ERR_OUT_OF_MEMORY;
    }
    request_buffer.length = total_len;
    // 打包
    ret = ubse_mem_fd_permission_req_pack(name, owner, mode, request_buffer.buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack fd permission request, error" << ret;
        ubse_api_buffer_free(&request_buffer);
        return static_cast<int32_t>(ret);
    }
    // 调用接口
    ubse_api_buffer_t response_buffer = {nullptr, 0};
    ret =
        static_cast<ubs_error_t>(ubse_invoke_call(UBSE_MEM, UBSE_MEM_FD_PERMISSION, &request_buffer, &response_buffer));
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code: " << ret;
    }
    ubse_api_buffer_free(&request_buffer);
    ubse_api_buffer_free(&response_buffer);
    return ubse_map_daemon_error(ret);
}

int32_t ubs_mem_fd_get(const char* name, ubs_mem_fd_desc_t* mem_desc)
{
    // 参数校验
    auto ret = ubse_mem_name_is_valid(name);
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }
    if (mem_desc == nullptr) {
        IPC_LOG_ERROR << "Invalid parameters: mem_desc is nullptr";
        return UBS_ERR_NULL_POINTER;
    }
    // 打包
    uint32_t length = ubse_string_calc_size(name, UBS_MEM_MAX_NAME_LENGTH - 1);
    ubse_api_buffer_t request_buffer;
    request_buffer.buffer = static_cast<uint8_t*>(malloc(length));
    if (!request_buffer.buffer) {
        ubse_api_buffer_free(&request_buffer);
        return UBS_ERR_OUT_OF_MEMORY;
    }
    ret = ubse_string_pack(name, UBS_MEM_MAX_NAME_LENGTH - 1, request_buffer.buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack fd get request, error" << ret;
        ubse_api_buffer_free(&request_buffer);
        return static_cast<int32_t>(ret);
    }
    request_buffer.length = length;
    // 调用接口
    ubse_api_buffer_t response_buffer = {nullptr, 0};
    ret = static_cast<ubs_error_t>(ubse_invoke_call(UBSE_MEM, UBSE_MEM_FD_GET, &request_buffer, &response_buffer));
    ubse_api_buffer_free(&request_buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code: " << ret;
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    // 解包
    ret = ubse_mem_fd_desc_unpack(response_buffer.buffer, response_buffer.length, mem_desc);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack fd desc, error" << ret;
    }
    ubse_api_buffer_free(&response_buffer);
    return ret;
}

int32_t ubs_mem_fd_list(ubs_mem_fd_desc_t** fd_descs, uint32_t* fd_desc_cnt)
{
    // 参数校验
    if (fd_descs == nullptr || fd_desc_cnt == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    // 打包
    ubse_api_buffer_t request_buffer = {nullptr, 0};
    // 调用接口
    ubse_api_buffer_t response_buffer = {nullptr, 0};
    auto ret = ubse_invoke_call(UBSE_MEM, UBSE_MEM_FD_LIST, &request_buffer, &response_buffer);
    ubse_api_buffer_free(&request_buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code: " << ret;
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    // 解包
    ret = ubse_mem_fd_desc_list_unpack(response_buffer.buffer, response_buffer.length, fd_descs, fd_desc_cnt);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack fd desc list, error" << ret;
    }
    ubse_api_buffer_free(&response_buffer);
    return static_cast<int32_t>(ret);
}
int32_t ubs_mem_fd_delete(const char* name)
{
    // 参数校验
    uint32_t ret = ubse_mem_name_is_valid(name);
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }
    // 申请内存
    uint32_t length = ubse_string_calc_size(name, UBS_MEM_MAX_NAME_LENGTH - 1);
    ubse_api_buffer_t request_buffer;
    request_buffer.buffer = static_cast<uint8_t*>(malloc(length));
    if (!request_buffer.buffer) {
        ubse_api_buffer_free(&request_buffer);
        return UBS_ERR_OUT_OF_MEMORY;
    }
    // 打包
    ret = ubse_string_pack(name, UBS_MEM_MAX_NAME_LENGTH - 1, request_buffer.buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack fd delete request, error" << ret;
        ubse_api_buffer_free(&request_buffer);
        return static_cast<int32_t>(ret);
    }
    request_buffer.length = length;
    // 调用接口
    ubse_api_buffer_t response_buffer = {nullptr, 0};
    ret = ubse_invoke_call(UBSE_MEM, UBSE_MEM_FD_DELETE, &request_buffer, &response_buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code: " << ret;
    }
    ubse_api_buffer_free(&request_buffer);
    ubse_api_buffer_free(&response_buffer);
    return ubse_map_daemon_error(ret);
}
int32_t ubs_mem_numa_create(const char* name, uint64_t size, ubs_mem_distance_t distance,
                            ubs_mem_numa_desc_t* numa_desc)
{
    // 参数校验
    auto ret = ubse_mem_create_req_is_valid(name, size);
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }
    if (numa_desc == nullptr) {
        IPC_LOG_ERROR << "Invalid parameters: numa_desc is nullptr";
        return UBS_ERR_NULL_POINTER;
    }
    // 申请内存
    const size_t total_len = ubse_mem_numa_create_req_calc_size(name);
    ubse_api_buffer_t request_buffer;
    request_buffer.buffer = static_cast<uint8_t*>(malloc(total_len));
    if (!request_buffer.buffer) {
        ubse_api_buffer_free(&request_buffer);
        return UBS_ERR_OUT_OF_MEMORY;
    }
    request_buffer.length = total_len;
    // 打包
    ret = ubse_mem_numa_create_req_pack(name, size, distance, request_buffer.buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack numa create request, error" << ret;
        ubse_api_buffer_free(&request_buffer);
        return static_cast<int32_t>(ret);
    }
    // 调用接口
    ubse_api_buffer_t response_buffer = {.buffer = nullptr, .length = 0};
    ret = static_cast<ubs_error_t>(ubse_invoke_call(UBSE_MEM, UBSE_MEM_NUMA_CREATE, &request_buffer, &response_buffer));
    ubse_api_buffer_free(&request_buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code: " << ret;
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    // 解包
    ret = ubse_mem_numa_desc_unpack(response_buffer.buffer, response_buffer.length, numa_desc);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack numa desc, error" << ret;
    }
    ubse_api_buffer_free(&response_buffer);
    return ret;
}

int32_t ubs_mem_numa_create_with_lender(const char* name, const ubs_mem_lender_t* lender, uint32_t lender_cnt,
                                        ubs_mem_numa_desc_t* numa_desc)
{
    // 参数校验
    auto ret = ubse_mem_create_with_lender_req_is_valid(name, lender, lender_cnt);
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }
    if (numa_desc == nullptr) {
        IPC_LOG_ERROR << "Invalid parameters: numa_desc is nullptr";
        return UBS_ERR_NULL_POINTER;
    }
    // 申请内存
    const size_t total_len = ubse_mem_numa_create_with_lender_req_calc_size(name, lender_cnt);
    ubse_api_buffer_t request_buffer;
    request_buffer.buffer = static_cast<uint8_t*>(malloc(total_len));
    if (!request_buffer.buffer) {
        ubse_api_buffer_free(&request_buffer);
        return UBS_ERR_OUT_OF_MEMORY;
    }
    request_buffer.length = total_len;
    // 打包
    ret = ubse_mem_numa_create_with_lender_req_pack(name, lender, lender_cnt, request_buffer.buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack numa create with lender request, error" << ret;
        ubse_api_buffer_free(&request_buffer);
        return static_cast<int32_t>(ret);
    }
    // 调用接口
    ubse_api_buffer_t response_buffer = {.buffer = nullptr, .length = 0};
    ret = static_cast<ubs_error_t>(
        ubse_invoke_call(UBSE_MEM, UBSE_MEM_NUMA_WITH_LEND_INFO, &request_buffer, &response_buffer));
    ubse_api_buffer_free(&request_buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code: " << ret;
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    // 解包
    ret = ubse_mem_numa_desc_unpack(response_buffer.buffer, response_buffer.length, numa_desc);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack numa desc, error" << ret;
    }
    ubse_api_buffer_free(&response_buffer);
    return ret;
}

int32_t ubs_mem_numa_create_with_candidate(const char* name, uint64_t size, const uint32_t* slot_ids, uint32_t slot_cnt,
                                           ubs_mem_numa_desc_t* numa_desc)
{
    // 参数校验
    auto ret = ubse_mem_create_with_candidate_req_is_valid(name, size, slot_ids, slot_cnt);
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }
    if (numa_desc == nullptr) {
        IPC_LOG_ERROR << "Invalid parameters: numa_desc is nullptr";
        return UBS_ERR_NULL_POINTER;
    }
    // 申请内存
    const size_t total_len = ubse_mem_numa_create_with_candidate_req_calc_size(name, slot_cnt);
    ubse_api_buffer_t request_buffer;
    request_buffer.buffer = static_cast<uint8_t*>(malloc(total_len));
    if (!request_buffer.buffer) {
        ubse_api_buffer_free(&request_buffer);
        return UBS_ERR_OUT_OF_MEMORY;
    }
    request_buffer.length = total_len;
    // 打包
    ret = ubse_mem_numa_create_with_candidate_req_pack(name, size, slot_ids, slot_cnt, request_buffer.buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack numa create with candidate request, error" << ret;
        ubse_api_buffer_free(&request_buffer);
        return static_cast<int32_t>(ret);
    }
    // 调用接口
    ubse_api_buffer_t response_buffer = {.buffer = nullptr, .length = 0};
    ret = static_cast<ubs_error_t>(
        ubse_invoke_call(UBSE_MEM, UBSE_MEM_NUMA_CREATE_WITH_CANDIDATE, &request_buffer, &response_buffer));
    ubse_api_buffer_free(&request_buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code: " << ret;
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    // 解包
    ret = ubse_mem_numa_desc_unpack(response_buffer.buffer, response_buffer.length, numa_desc);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack numa desc, error" << ret;
    }
    ubse_api_buffer_free(&response_buffer);
    return ret;
}

int32_t ubs_mem_numa_get(const char* name, ubs_mem_numa_desc_t* numa_desc)
{
    // 参数校验
    auto ret = ubse_mem_name_is_valid(name);
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }
    if (numa_desc == nullptr) {
        IPC_LOG_ERROR << "Invalid parameters: numa_desc is nullptr";
        return UBS_ERR_NULL_POINTER;
    }
    // 申请内存
    uint32_t length = ubse_string_calc_size(name, UBS_MEM_MAX_NAME_LENGTH - 1);
    ubse_api_buffer_t request_buffer;
    request_buffer.buffer = static_cast<uint8_t*>(malloc(length));
    if (!request_buffer.buffer) {
        ubse_api_buffer_free(&request_buffer);
        return UBS_ERR_OUT_OF_MEMORY;
    }
    // 打包
    ret = ubse_string_pack(name, UBS_MEM_MAX_NAME_LENGTH - 1, request_buffer.buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack numa get request, error" << ret;
        ubse_api_buffer_free(&request_buffer);
        return static_cast<int32_t>(ret);
    }
    request_buffer.length = length;
    // 调用接口
    ubse_api_buffer_t response_buffer = {.buffer = nullptr, .length = 0};
    ret = static_cast<ubs_error_t>(ubse_invoke_call(UBSE_MEM, UBSE_MEM_NUMA_GET, &request_buffer, &response_buffer));
    ubse_api_buffer_free(&request_buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code: " << ret;
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    // 解包
    ret = ubse_mem_numa_desc_unpack(response_buffer.buffer, response_buffer.length, numa_desc);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack numa desc, error" << ret;
    }
    ubse_api_buffer_free(&response_buffer);
    return ret;
}

int32_t ubs_mem_numa_list(ubs_mem_numa_desc_t** numa_descs, uint32_t* numa_desc_cnt)
{
    // 参数校验
    if (numa_descs == nullptr || numa_desc_cnt == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    // 打包
    ubse_api_buffer_t request_buffer = {nullptr, 0};
    // 调用接口
    ubse_api_buffer_t response_buffer = {.buffer = nullptr, .length = 0};
    auto ret = ubse_invoke_call(UBSE_MEM, UBSE_MEM_NUMA_LIST, &request_buffer, &response_buffer);
    ubse_api_buffer_free(&request_buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code: " << ret;
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    // 解包
    ret = ubse_mem_numa_desc_list_unpack(response_buffer.buffer, response_buffer.length, numa_descs, numa_desc_cnt);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack numa desc list, error" << ret;
    }
    ubse_api_buffer_free(&response_buffer);
    return static_cast<int32_t>(ret);
}

int32_t ubs_mem_numa_delete(const char* name)
{
    // 参数校验
    auto ret = ubse_mem_name_is_valid(name);
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }
    // 申请内存
    uint32_t length = ubse_string_calc_size(name, UBS_MEM_MAX_NAME_LENGTH - 1);
    ubse_api_buffer_t request_buffer;
    request_buffer.buffer = static_cast<uint8_t*>(malloc(length));
    if (!request_buffer.buffer) {
        ubse_api_buffer_free(&request_buffer);
        return UBS_ERR_OUT_OF_MEMORY;
    }
    // 打包
    ret = ubse_string_pack(name, UBS_MEM_MAX_NAME_LENGTH - 1, request_buffer.buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack numa delete request, error" << ret;
        ubse_api_buffer_free(&request_buffer);
        return static_cast<int32_t>(ret);
    }
    request_buffer.length = length;
    // 调用接口
    ubse_api_buffer_t response_buffer = {.buffer = nullptr, .length = 0};
    ret = static_cast<ubs_error_t>(ubse_invoke_call(UBSE_MEM, UBSE_MEM_NUMA_DELETE, &request_buffer, &response_buffer));
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code: " << ret;
    }
    ubse_api_buffer_free(&request_buffer);
    ubse_api_buffer_free(&response_buffer);
    return ubse_map_daemon_error(ret);
}

int32_t ubs_mem_shm_create(const char* name, uint64_t size, uint8_t usr_info[32], uint64_t flag,
                           const ubs_mem_nodes_t* region, const ubs_mem_nodes_t* provider)
{
    // 参数校验
    ubs_error_t ret = ubse_mem_shm_create_req_valid(name, size, region, provider);
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }
    // 构造buffer
    ubse_api_buffer_t request_buffer;
    ret = ubse_mem_shm_create_req_build(&request_buffer, name, region, provider);
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }
    // 打包
    ret = ubse_mem_shm_create_req_pack(name, size, usr_info, flag, region, provider, request_buffer.buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack shm create request, error" << ret;
        ubse_api_buffer_free(&request_buffer);
        return static_cast<int32_t>(ret);
    }
    // 调用接口
    ubse_api_buffer_t response_buffer = {.buffer = nullptr, .length = 0};
    uint32_t ipc_ret = ubse_invoke_call(UBSE_MEM, UBSE_MEM_SHM_CREATE, &request_buffer, &response_buffer);
    if (ipc_ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code: " << ipc_ret;
    }
    // 释放buffer
    ubse_api_buffer_free(&request_buffer);
    ubse_api_buffer_free(&response_buffer);
    return ubse_map_daemon_error(ipc_ret);
}

int32_t ubs_mem_shm_create_with_affinity(const char* name, uint64_t size, uint32_t affinity_socket_id,
                                         uint8_t usr_info[32], uint64_t flag, const ubs_mem_nodes_t* region,
                                         const ubs_mem_nodes_t* provider)
{
    // 参数校验
    ubs_error_t ret = ubse_mem_shm_create_req_valid(name, size, region, provider);
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }
    // 构造buffer
    ubse_api_buffer_t request_buffer = {.buffer = nullptr, .length = 0};
    ret = ubse_mem_shm_create_with_affinity_req_build(&request_buffer, name, region, provider);
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }
    // 打包
    ret = ubse_mem_shm_create_with_affinity_req_pack(name, size, usr_info, flag, region, provider,
                                                     request_buffer.buffer, affinity_socket_id);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack shm create with affinity request, error" << ret;
        ubse_api_buffer_free(&request_buffer);
        return static_cast<int32_t>(ret);
    }
    // 调用接口
    ubse_api_buffer_t response_buffer = {.buffer = nullptr, .length = 0};
    uint32_t ipc_ret = ubse_invoke_call(UBSE_MEM, UBSE_MEM_SHM_CREATE_WITH_AFFINITY, &request_buffer, &response_buffer);
    if (ipc_ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code: " << ipc_ret;
    }
    // 释放buffer
    ubse_api_buffer_free(&request_buffer);
    ubse_api_buffer_free(&response_buffer);
    return ubse_map_daemon_error(ipc_ret);
}

int32_t ubs_mem_shm_create_with_lender(const char* name, uint8_t usr_info[UBS_MEM_MAX_USR_INFO_LEN], uint64_t flag,
                                       const ubs_mem_nodes_t* region, const ubs_mem_lender_t* lender)
{
    // 参数校验
    ubs_error_t ret = ubse_mem_shm_create_with_lender_req_valid(name, region, lender);
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }
    // 构造buffer
    ubse_api_buffer_t request_buffer = {.buffer = nullptr, .length = 0};
    ret = ubse_mem_shm_create_with_lender_req_build(&request_buffer, name, usr_info, flag, region, lender);
    if (ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&request_buffer);
        return static_cast<int32_t>(ret);
    }

    // 调用接口
    ubse_api_buffer_t response_buffer = {.buffer = nullptr, .length = 0};
    uint32_t ipc_ret = ubse_invoke_call(UBSE_MEM, UBSE_MEM_SHM_CREATE_WITH_LENDER, &request_buffer, &response_buffer);
    ubse_api_buffer_free(&request_buffer);
    ubse_api_buffer_free(&response_buffer);
    if (ipc_ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code: " << ipc_ret;
        return static_cast<int32_t>(ipc_ret);
    }
    IPC_LOG_INFO << "ubse_invoke_call success";
    return ubse_map_daemon_error(ipc_ret);
}

int32_t ubs_mem_shm_attach(const char* name, const ubs_mem_fd_owner_t* owner, mode_t mode,
                           ubs_mem_shm_desc_t** shm_desc)
{
    // 参数校验
    ubs_error_t ret = ubse_mem_name_is_valid(name);
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }
    // 构造buffer
    ubse_api_buffer_t request_buffer;
    ret = ubse_mem_shm_attach_req_build(&request_buffer, name);
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }
    // 打包
    ret = ubse_mem_shm_attach_req_pack(name, owner, mode, request_buffer.buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack shm attach request, error" << ret;
        ubse_api_buffer_free(&request_buffer);
        return static_cast<int32_t>(ret);
    }
    // 调用接口
    ubse_api_buffer_t response_buffer{nullptr, 0};
    uint32_t ipc_ret = ubse_invoke_call(UBSE_MEM, UBSE_MEM_SHM_ATTACH, &request_buffer, &response_buffer);
    ubse_api_buffer_free(&request_buffer);
    if (ipc_ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code: " << ipc_ret;
        if (ubse_map_daemon_error(ipc_ret) == UBS_ENGINE_ERR_EXISTED) {
            ret = ubse_mem_shm_attach_resp_unpack(response_buffer.buffer, response_buffer.length, shm_desc);
            if (ret != UBS_SUCCESS) {
                IPC_LOG_ERROR << "Failed to unpack shm desc, error" << ret;
            }
        }
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ipc_ret);
    }
    // 解包
    ret = ubse_mem_shm_attach_resp_unpack(response_buffer.buffer, response_buffer.length, shm_desc);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack shm desc, error" << ret;
    }
    ubse_api_buffer_free(&response_buffer);
    return static_cast<int32_t>(ret);
}
int32_t ubs_mem_shm_get(const char* name, ubs_mem_shm_desc_t** shm_desc)
{
    // 参数校验
    ubs_error_t ret = ubse_mem_name_is_valid(name);
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }
    // 构造buffer
    ubse_api_buffer_t request_buffer;
    ret = ubse_mem_shm_get_req_build(&request_buffer, name);
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }
    // 打包
    ret = ubse_mem_shm_get_req_pack(name, request_buffer.buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack shm get request, error" << ret;
        ubse_api_buffer_free(&request_buffer);
        return static_cast<int32_t>(ret);
    }
    // 调用接口
    ubse_api_buffer_t response_buffer{nullptr, 0};
    uint32_t ipc_ret = ubse_invoke_call(UBSE_MEM, UBSE_MEM_SHM_GET, &request_buffer, &response_buffer);
    ubse_api_buffer_free(&request_buffer);
    if (ipc_ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code: " << ipc_ret;
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ipc_ret);
    }
    // 解包
    ret = ubse_mem_shm_get_resp_unpack(response_buffer.buffer, response_buffer.length, shm_desc);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack shm desc, error" << ret;
    }
    ubse_api_buffer_free(&response_buffer);
    return static_cast<int32_t>(ret);
}
int32_t ubs_mem_shm_list(ubs_mem_shm_desc_t** shm_descs, uint32_t* shm_desc_cnt)
{
    // 参数校验
    if (shm_descs == nullptr || shm_desc_cnt == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    // 打包
    ubse_api_buffer_t request_buffer;
    request_buffer.buffer = nullptr;
    request_buffer.length = 0;
    // 调用接口
    ubse_api_buffer_t response_buffer{nullptr, 0};
    uint32_t ipc_ret = ubse_invoke_call(UBSE_MEM, UBSE_MEM_SHM_LIST, &request_buffer, &response_buffer);
    ubse_api_buffer_free(&request_buffer);
    if (ipc_ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code: " << ipc_ret;
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ipc_ret);
    }
    // 解包
    ubs_error_t ret =
        ubse_mem_shm_list_resp_unpack(response_buffer.buffer, response_buffer.length, shm_descs, shm_desc_cnt);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack shm desc list, error" << ret;
    }
    ubse_api_buffer_free(&response_buffer);
    return static_cast<int32_t>(ret);
}

int32_t ubs_mem_shm_list_with_prefix(const char* name_prefix, ubs_mem_shm_desc_t** shm_descs, uint32_t* shm_desc_cnt)
{
    // 参数校验
    ubs_error_t ret = ubse_mem_name_is_valid(name_prefix);
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }
    if (shm_descs == nullptr || shm_desc_cnt == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    // 构造buffer
    ubse_api_buffer_t request_buffer;
    ret = ubse_mem_shm_get_req_build(&request_buffer, name_prefix);
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }
    // 打包
    ret = ubse_mem_shm_get_req_pack(name_prefix, request_buffer.buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack shm get request, error" << ret;
        ubse_api_buffer_free(&request_buffer);
        return static_cast<int32_t>(ret);
    }
    // 调用接口
    ubse_api_buffer_t response_buffer;
    uint32_t ipc_ret = ubse_invoke_call(UBSE_MEM, UBSE_MEM_SHM_LIST_WITH_PREFIX, &request_buffer, &response_buffer);
    ubse_api_buffer_free(&request_buffer);
    if (ipc_ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code: " << ipc_ret;
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ipc_ret);
    }
    // 解包
    ret = ubse_mem_shm_list_resp_unpack(response_buffer.buffer, response_buffer.length, shm_descs, shm_desc_cnt);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack shm desc list, error" << ret;
    }
    ubse_api_buffer_free(&response_buffer);
    return static_cast<int32_t>(ret);
}

int32_t ubs_mem_shm_detach(const char* name)
{
    // 参数校验
    ubs_error_t ret = ubse_mem_name_is_valid(name);
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }
    // 构造buffer
    ubse_api_buffer_t request_buffer;
    ret = ubse_mem_shm_detach_req_build(&request_buffer, name);
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }
    // 打包
    ret = ubse_mem_shm_detach_req_pack(name, request_buffer.buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack shm detach request, error" << ret;
        ubse_api_buffer_free(&request_buffer);
        return static_cast<int32_t>(ret);
    }
    // 调用接口
    ubse_api_buffer_t response_buffer{nullptr, 0};
    uint32_t ipc_ret = ubse_invoke_call(UBSE_MEM, UBSE_MEM_SHM_DETACH, &request_buffer, &response_buffer);
    if (ipc_ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code: " << ipc_ret;
    }
    ubse_api_buffer_free(&request_buffer);
    ubse_api_buffer_free(&response_buffer);
    return ubse_map_daemon_error(ipc_ret);
}
int32_t ubs_mem_shm_delete(const char* name)
{
    // 参数校验
    ubs_error_t ret = ubse_mem_name_is_valid(name);
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }
    // 构造buffer
    ubse_api_buffer_t request_buffer;
    ret = ubse_mem_shm_delete_req_build(&request_buffer, name);
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }
    // 打包
    ret = ubse_mem_shm_delete_req_pack(name, request_buffer.buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack shm delete request, error" << ret;
        ubse_api_buffer_free(&request_buffer);
        return static_cast<int32_t>(ret);
    }
    // 调用接口
    ubse_api_buffer_t response_buffer{nullptr, 0};
    uint32_t ipc_ret = ubse_invoke_call(UBSE_MEM, UBSE_MEM_SHM_DELETE, &request_buffer, &response_buffer);
    if (ipc_ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code: " << ipc_ret;
    }
    ubse_api_buffer_free(&request_buffer);
    ubse_api_buffer_free(&response_buffer);
    return ubse_map_daemon_error(ipc_ret);
}

int32_t ubs_mem_shm_fault_get(const char* name, ubs_mem_memids_fault_t* fault)
{
    // 参数校验
    ubs_error_t ret = ubse_mem_name_is_valid(name);
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }
    if (fault == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    // 构造buffer
    ubse_api_buffer_t request_buffer;
    ubse_mem_shm_fault_get_req_build(name, &request_buffer);

    // 打包
    ret = ubse_mem_shm_fault_get_req_pack(name, request_buffer.buffer);
    if (ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&request_buffer);
        return static_cast<int32_t>(ret);
    }
    // 调用接口
    ubse_api_buffer_t response_buffer{nullptr, 0};
    uint32_t ipc_ret = ubse_invoke_call(UBSE_MEM, UBSE_MEM_SHM_MEMID_STATUS_GET, &request_buffer, &response_buffer);
    ubse_api_buffer_free(&request_buffer);
    if (ipc_ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ipc_ret);
    }
    // 解包
    ipc_ret = ubse_mem_shm_fault_get_resp_unpack(response_buffer.buffer, response_buffer.length, fault);
    ubse_api_buffer_free(&response_buffer);
    return ubse_map_daemon_error(ipc_ret);
}

int32_t ubs_mem_shm_fault_register(ubs_mem_shm_fault_handler handler)
{
    if (handler == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    uint32_t ret = ubse_long_link_connect();
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }
    return static_cast<int32_t>(ubse_shm_fault_register(handler));
}

int32_t ubs_mem_fd_fault_register(ubs_mem_fd_fault_handler handler)
{
    if (handler == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    uint32_t ret = ubse_long_link_connect();
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }
    return static_cast<int32_t>(ubse_fd_fault_register(handler));
}

int32_t ubs_mem_numa_fault_register(ubs_mem_numa_fault_handler handler)
{
    if (handler == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    uint32_t ret = ubse_long_link_connect();
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }
    return static_cast<int32_t>(ubse_numa_fault_register(handler));
}

int32_t ubs_mem_get_memid_by_import(const char* name, const uint64_t import_memid, ubs_mem_export_memid_t* mem_info,
                                    const uint16_t op_code)
{
    // 参数校验
    auto ret = ubse_mem_name_is_valid(name);
    if (ret != UBS_SUCCESS) {
        return ret;
    }
    if (mem_info == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    // 构造buffer
    ubse_api_buffer_t request_buffer{.buffer = nullptr, .length = 0};
    ret = ubse_mem_get_memid_by_import_req_build(name, &request_buffer);
    if (ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&request_buffer);
        return ret;
    }
    // 打包
    ret = ubse_mem_get_memid_by_import_req_pack(name, import_memid, request_buffer.buffer);
    if (ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&request_buffer);
        return ret;
    }
    // 调用接口
    ubse_api_buffer_t response_buffer{.buffer = nullptr, .length = 0};
    uint32_t ipc_ret = ubse_invoke_call(UBSE_MEM, op_code, &request_buffer, &response_buffer);
    ubse_api_buffer_free(&request_buffer);
    if (ipc_ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ipc_ret);
    }
    // 解包
    ret = ubse_mem_get_memid_by_import_resp_unpack(response_buffer.buffer, response_buffer.length, mem_info);
    ubse_api_buffer_free(&response_buffer);
    return ret;
}

int32_t ubs_mem_fd_get_memid_by_import(const char* name, uint64_t import_memid, ubs_mem_export_memid_t* mem_info)
{
    return ubs_mem_get_memid_by_import(name, import_memid, mem_info, UBSE_MEM_FD_GET_MEM_ID_BY_IMPORT);
}

int32_t ubs_mem_numa_get_memid_by_import(const char* name, uint64_t import_memid, ubs_mem_export_memid_t* mem_info)
{
    return ubs_mem_get_memid_by_import(name, import_memid, mem_info, UBSE_MEM_NUMA_GET_MEM_ID_BY_IMPORT);
}

int32_t ubs_mem_shm_get_memid_by_import(const char* name, uint64_t import_memid, ubs_mem_export_memid_t* mem_info)
{
    return ubs_mem_get_memid_by_import(name, import_memid, mem_info, UBSE_MEM_SHM_GET_MEM_ID_BY_IMPORT);
}
