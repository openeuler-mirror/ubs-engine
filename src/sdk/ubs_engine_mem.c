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

#include "ubs_engine_mem.h"

#include <securec.h>

#include "libubse_helper.h"
#include "ubse_ipc_client.h"
#include "ubse_ipc_common.h"

int32_t ubs_mem_numastat_get(uint32_t slot_id, ubs_mem_numastat_t **numa_mems, uint32_t *numa_mem_cnt)
{
    // 参数校验
    if (numa_mems == NULL || numa_mem_cnt == NULL) {
        return UBS_ERR_NULL_POINTER;
    }
    // 打包
    ubse_api_buffer_t request_buffer = {
        .buffer = NULL,
        .length = 0
    };
    request_buffer.buffer = malloc(sizeof(uint32_t));
    if (!request_buffer.buffer) {
        ubse_api_buffer_free(&request_buffer);
        return UBS_ERR_OUT_OF_MEMORY;
    }
    ubs_error_t ret = memcpy_s(request_buffer.buffer, sizeof(uint32_t), &slot_id, sizeof(uint32_t));
    if (ret != EOK) {
        ubse_api_buffer_free(&request_buffer);
        return ubse_map_sys_error(ret);
    }
    uint32_t slotId = 0;
    ret = memcpy_s(&slotId, sizeof(uint32_t), request_buffer.buffer, sizeof(uint32_t));
    if (ret != EOK) {
        return ret;
    }
    request_buffer.length = sizeof(uint32_t);
    // 调用接口
    ubse_api_buffer_t response_buffer = {
        .buffer = NULL,
        .length = 0
    };
    ret = ubse_invoke_call(UBSE_NODE, UBSE_NODE_NUMA_MEM_GET, &request_buffer, &response_buffer);
    ubse_api_buffer_free(&request_buffer);
    if (ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    // 解包
    ret = ubse_node_numa_mem_list_unpack(response_buffer.buffer, response_buffer.length, numa_mems, numa_mem_cnt);
    ubse_api_buffer_free(&response_buffer);
    return ret;
}

int32_t ubs_mem_fd_create(const char *name, uint64_t size, const ubs_mem_fd_owner_t *owner, mode_t mode,
    ubs_mem_distance_t distance, ubs_mem_fd_desc_t *fd_desc)
{
    // 参数校验
    ubs_error_t ret = ubse_mem_create_req_is_valid(name, size);
    if (ret != UBS_SUCCESS) {
        return ret;
    }
    if (fd_desc == NULL) {
        return UBS_ERR_NULL_POINTER;
    }
    // 申请内存
    const size_t total_len = UBS_MEM_MAX_NAME_LENGTH +  // name
        sizeof(uint64_t) +                              // size
        sizeof(uid_t) + sizeof(gid_t) + sizeof(pid_t) + // owner
        sizeof(mode) +                                  // mode
        sizeof(uint32_t);                               // distance

    ubse_api_buffer_t request_buffer = {
        .buffer = NULL,
        .length = 0
    };
    request_buffer.buffer = malloc(total_len);
    if (!request_buffer.buffer) {
        ubse_api_buffer_free(&request_buffer);
        return UBS_ERR_OUT_OF_MEMORY;
    }
    request_buffer.length = total_len;
    // 打包
    ret = ubse_mem_fd_create_req_pack(name, size, distance, owner, mode, request_buffer.buffer);
    if (ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&request_buffer);
        return ret;
    }
    // 调用接口
    ubse_api_buffer_t response_buffer = {
        .buffer = NULL,
        .length = 0
    };
    ret = ubse_invoke_call(UBSE_MEM, UBSE_MEM_FD_CREATE, &request_buffer, &response_buffer);
    ubse_api_buffer_free(&request_buffer);
    if (ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    // 解包
    ret = ubse_mem_fd_desc_unpack(response_buffer.buffer, response_buffer.length, fd_desc);
    ubse_api_buffer_free(&response_buffer);
    return ret;
}

int32_t ubs_mem_fd_create_with_lender(const char *name, const ubs_mem_fd_owner_t *owner, mode_t mode,
    const ubs_mem_lender_t *lender, uint32_t lender_cnt, ubs_mem_fd_desc_t *fd_desc)
{
    // 参数校验
    ubs_error_t ret = ubse_mem_create_with_lender_req_is_valid(name, lender);
    if (ret != UBS_SUCCESS) {
        return ret;
    }
    if (fd_desc == NULL) {
        return UBS_ERR_NULL_POINTER;
    }
    // 申请内存
    const size_t total_len = UBS_MEM_MAX_NAME_LENGTH +  // 名称字段
        sizeof(uid_t) + sizeof(gid_t) + sizeof(pid_t) + // owner
        sizeof(mode) +                                  // mode
        sizeof(ubs_mem_lender_t);                       // 出借信息数组

    ubse_api_buffer_t request_buffer = {
        .buffer = NULL,
        .length = 0
    };
    request_buffer.buffer = malloc(total_len);
    if (!request_buffer.buffer) {
        ubse_api_buffer_free(&request_buffer);
        return UBS_ERR_OUT_OF_MEMORY;
    }
    request_buffer.length = total_len;
    // 打包
    ret = ubse_mem_fd_create_with_lender_req_pack(name, owner, mode, lender, request_buffer.buffer);
    if (ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&request_buffer);
        return ret;
    }
    // 调用接口
    ubse_api_buffer_t response_buffer = {
        .length = 0,
        .buffer = NULL
    };
    ret = ubse_invoke_call(UBSE_MEM, UBSE_MEM_FD_WITH_LEND_INFO, &request_buffer, &response_buffer);
    ubse_api_buffer_free(&request_buffer);
    if (ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    // 解包
    ret = ubse_mem_fd_desc_unpack(response_buffer.buffer, response_buffer.length, fd_desc);
    ubse_api_buffer_free(&response_buffer);
    return ret;
}

int32_t ubs_mem_fd_get(const char *name, ubs_mem_fd_desc_t *mem_desc)
{
    // 参数校验
    ubs_error_t ret = ubse_mem_name_is_valid(name);
    if (ret != UBS_SUCCESS) {
        return ret;
    }
    if (mem_desc == NULL) {
        return UBS_ERR_NULL_POINTER;
    }
    // 打包
    ubse_api_buffer_t request_buffer = {
        .buffer = NULL,
        .length = 0
    };
    request_buffer.buffer = malloc(UBS_MEM_MAX_NAME_LENGTH);
    if (!request_buffer.buffer) {
        ubse_api_buffer_free(&request_buffer);
        return UBS_ERR_OUT_OF_MEMORY;
    }
    ret = strncpy_s((char *)(request_buffer.buffer), UBS_MEM_MAX_NAME_LENGTH, name, UBS_MEM_MAX_NAME_LENGTH);
    if (ret != EOK) {
        ubse_api_buffer_free(&request_buffer);
        return ubse_map_sys_error(ret);
    }
    request_buffer.buffer[UBS_MEM_MAX_NAME_LENGTH - 1] = '\0'; // 确保终止符
    request_buffer.length = UBS_MEM_MAX_NAME_LENGTH;
    // 调用接口
    ubse_api_buffer_t response_buffer = {
        .buffer = NULL,
        .length = 0
    };
    ret = ubse_invoke_call(UBSE_MEM, UBSE_MEM_FD_GET, &request_buffer, &response_buffer);
    ubse_api_buffer_free(&request_buffer);
    if (ret != IPC_SUCCESS) {
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    // 解包
    ret = ubse_mem_fd_desc_unpack(response_buffer.buffer, response_buffer.length, mem_desc);
    ubse_api_buffer_free(&response_buffer);
    return ret;
}

int32_t ubs_mem_fd_list(ubs_mem_fd_desc_t **fd_descs, uint32_t *fd_desc_cnt)
{
    // 参数校验
    if (fd_descs == NULL || fd_desc_cnt == NULL) {
        return UBS_ERR_NULL_POINTER;
    }

    // 调用接口
    ubse_api_buffer_t request_buffer = {
        .buffer = NULL,
        .length = 0
    };
    ubse_api_buffer_t response_buffer = {
        .buffer = NULL,
        .length = 0
    };
    ubs_error_t ret = ubse_invoke_call(UBSE_MEM, UBSE_MEM_FD_LIST, &request_buffer, &response_buffer);
    ubse_api_buffer_free(&request_buffer);
    if (ret != IPC_SUCCESS) {
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    // 解包
    ret = ubse_mem_fd_desc_list_unpack(response_buffer.buffer, response_buffer.length, fd_descs, fd_desc_cnt);
    ubse_api_buffer_free(&response_buffer);
    return ret;
}
int32_t ubs_mem_fd_delete(const char *name)
{
    // 参数校验
    ubs_error_t ret = ubse_mem_name_is_valid(name);
    if (ret != UBS_SUCCESS) {
        return ret;
    }
    // 申请内存
    const size_t total_len = UBS_MEM_MAX_NAME_LENGTH; // 名称字段
    ubse_api_buffer_t request_buffer = {
        .buffer = NULL,
        .length = 0
    };
    request_buffer.buffer = malloc(total_len);
    if (!request_buffer.buffer) {
        ubse_api_buffer_free(&request_buffer);
        return UBS_ERR_OUT_OF_MEMORY;
    }
    request_buffer.length = total_len;
    // 打包
    ret = ubse_mem_fd_delete_req_pack(name, request_buffer.buffer);
    if (ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&request_buffer);
        return ret;
    }
    // 调用接口
    ubse_api_buffer_t response_buffer = {
        .buffer = NULL,
        .length = 0
    };
    ret = ubse_invoke_call(UBSE_MEM, UBSE_MEM_FD_DELETE, &request_buffer, &response_buffer);
    ubse_api_buffer_free(&request_buffer);
    ubse_api_buffer_free(&response_buffer);
    return ubse_map_daemon_error(ret);
}
int32_t ubs_mem_numa_create(const char *name, uint64_t size, ubs_mem_distance_t distance,
    ubs_mem_numa_desc_t *numa_desc)
{
    // 参数校验
    ubs_error_t ret = ubse_mem_create_req_is_valid(name, size);
    if (ret != UBS_SUCCESS) {
        return ret;
    }
    if (numa_desc == NULL) {
        return UBS_ERR_NULL_POINTER;
    }
    // 申请内存
    const size_t total_len = UBS_MEM_MAX_NAME_LENGTH + // name
        sizeof(uint64_t) +                             // size
        sizeof(uint32_t);                              // distance

    ubse_api_buffer_t request_buffer = {
        .buffer = NULL,
        .length = 0
    };
    request_buffer.buffer = malloc(total_len);
    if (!request_buffer.buffer) {
        ubse_api_buffer_free(&request_buffer);
        return UBS_ERR_OUT_OF_MEMORY;
    }
    request_buffer.length = total_len;
    // 打包
    ret = ubse_mem_numa_create_req_pack(name, size, distance, request_buffer.buffer);
    if (ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&request_buffer);
        return ret;
    }
    // 调用接口
    ubse_api_buffer_t response_buffer = {
        .buffer = NULL,
        .length = 0
    };
    ret = ubse_invoke_call(UBSE_MEM, UBSE_MEM_NUMA_CREATE, &request_buffer, &response_buffer);
    ubse_api_buffer_free(&request_buffer);
    if (ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    // 解包
    ret = ubse_mem_unma_desc_unpack(response_buffer.buffer, response_buffer.length, numa_desc);
    ubse_api_buffer_free(&response_buffer);
    return ret;
}

int32_t ubs_mem_numa_create_with_lender(const char *name, const ubs_mem_lender_t *lender, uint32_t lender_cnt,
    ubs_mem_numa_desc_t *numa_desc)
{
    // 参数校验
    ubs_error_t ret = ubse_mem_create_with_lender_req_is_valid(name, lender);
    if (ret != UBS_SUCCESS) {
        return ret;
    }
    if (numa_desc == NULL) {
        return UBS_ERR_NULL_POINTER;
    }
    // 申请内存
    const size_t total_len = UBS_MEM_MAX_NAME_LENGTH + // name
        UBSE_MEM_LENDER_SIZE;                          // lender
    ubse_api_buffer_t request_buffer = {
        .buffer = NULL,
        .length = 0
    };
    request_buffer.buffer = malloc(total_len);
    if (!request_buffer.buffer) {
        ubse_api_buffer_free(&request_buffer);
        return UBS_ERR_OUT_OF_MEMORY;
    }
    request_buffer.length = total_len;
    // 打包
    ret = ubse_mem_numa_create_with_lender_req_pack(name, lender, request_buffer.buffer);
    if (ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&request_buffer);
        return ret;
    }
    // 调用接口
    ubse_api_buffer_t response_buffer = {
        .buffer = NULL,
        .length = 0
    };
    ret = ubse_invoke_call(UBSE_MEM, UBSE_MEM_NUMA_WITH_LEND_INFO, &request_buffer, &response_buffer);
    ubse_api_buffer_free(&request_buffer);
    if (ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    // 解包
    ret = ubse_mem_unma_desc_unpack(response_buffer.buffer, response_buffer.length, numa_desc);
    ubse_api_buffer_free(&response_buffer);
    return ret;
}

int32_t ubs_mem_numa_get(const char *name, ubs_mem_numa_desc_t *numa_desc)
{
    // 参数校验
    ubs_error_t ret = ubse_mem_name_is_valid(name);
    if (ret != UBS_SUCCESS) {
        return ret;
    }
    if (numa_desc == NULL) {
        return UBS_ERR_NULL_POINTER;
    }
    // 打包
    ubse_api_buffer_t request_buffer = {
        .buffer = NULL,
        .length = 0
    };
    request_buffer.buffer = malloc(UBS_MEM_MAX_NAME_LENGTH);
    if (!request_buffer.buffer) {
        ubse_api_buffer_free(&request_buffer);
        return UBS_ERR_OUT_OF_MEMORY;
    }
    ret = strncpy_s((char *)(request_buffer.buffer), UBS_MEM_MAX_NAME_LENGTH, name, UBS_MEM_MAX_NAME_LENGTH);
    if (ret != EOK) {
        ubse_api_buffer_free(&request_buffer);
        return ubse_map_sys_error(ret);
    }
    request_buffer.buffer[UBS_MEM_MAX_NAME_LENGTH - 1] = '\0'; // 确保终止符
    request_buffer.length = UBS_MEM_MAX_NAME_LENGTH;
    // 调用接口
    ubse_api_buffer_t response_buffer = {
        .buffer = NULL,
        .length = 0
    };
    ret = ubse_invoke_call(UBSE_MEM, UBSE_MEM_NUMA_GET, &request_buffer, &response_buffer);
    ubse_api_buffer_free(&request_buffer);
    if (ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    // 解包
    ret = ubse_mem_unma_desc_unpack(response_buffer.buffer, response_buffer.length, numa_desc);
    ubse_api_buffer_free(&response_buffer);
    return ret;
}

int32_t ubs_mem_numa_list(ubs_mem_numa_desc_t **numa_descs, uint32_t *numa_desc_cnt)
{
    // 参数校验
    if (numa_descs == NULL || numa_desc_cnt == NULL) {
        return UBS_ERR_NULL_POINTER;
    }
    // 调用接口
    ubse_api_buffer_t request_buffer = {
        .buffer = NULL,
        .length = 0
    };
    ubse_api_buffer_t response_buffer = {
        .buffer = NULL,
        .length = 0
    };
    ubs_error_t ret = ubse_invoke_call(UBSE_MEM, UBSE_MEM_NUMA_LIST, &request_buffer, &response_buffer);
    ubse_api_buffer_free(&request_buffer);
    if (ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    // 解包
    ret = ubse_mem_numa_desc_list_unpack(response_buffer.buffer, response_buffer.length, numa_descs, numa_desc_cnt);
    ubse_api_buffer_free(&response_buffer);
    return ret;
}

int32_t ubs_mem_numa_delete(const char *name)
{
    // 参数校验
    ubs_error_t ret = ubse_mem_name_is_valid(name);
    if (ret != UBS_SUCCESS) {
        return ret;
    }
    // 打包
    ubse_api_buffer_t request_buffer = {
        .buffer = NULL,
        .length = 0
    };
    request_buffer.buffer = malloc(UBS_MEM_MAX_NAME_LENGTH);
    if (!request_buffer.buffer) {
        ubse_api_buffer_free(&request_buffer);
        return UBS_ERR_OUT_OF_MEMORY;
    }
    ret = strncpy_s((char *)(request_buffer.buffer), UBS_MEM_MAX_NAME_LENGTH, name, UBS_MEM_MAX_NAME_LENGTH);
    if (ret != EOK) {
        ubse_api_buffer_free(&request_buffer);
        return ubse_map_sys_error(ret);
    }
    request_buffer.buffer[UBS_MEM_MAX_NAME_LENGTH - 1] = '\0'; // 确保终止符
    request_buffer.length = UBS_MEM_MAX_NAME_LENGTH;
    // 调用接口
    ubse_api_buffer_t response_buffer = {
        .buffer = NULL,
        .length = 0
    };
    ret = ubse_invoke_call(UBSE_MEM, UBSE_MEM_NUMA_DELETE, &request_buffer, &response_buffer);
    ubse_api_buffer_free(&request_buffer);
    ubse_api_buffer_free(&response_buffer);
    return ubse_map_daemon_error(ret);
}
