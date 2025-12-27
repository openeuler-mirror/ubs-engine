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

#include "libubse_helper.h"

#include <netinet/in.h>
#include <securec.h>

#include "ubs_engine.h"
#include "ubse_ipc_common.h"
// 自定义64位网络字节序转换
uint64_t htonll_custom(uint64_t host_value)
{
    uint32_t low = htonl((uint32_t)host_value);
    uint32_t high = htonl((uint32_t)(host_value >> 32));
    return ((uint64_t)low << 32) | high;
}

uint64_t ntohll_custom(uint64_t net_value)
{
    uint32_t low = ntohl((uint32_t)net_value);
    uint32_t high = ntohl((uint32_t)(net_value >> 32));
    return ((uint64_t)low << 32) | high;
}

ubs_error_t ubse_map_sys_error(int sys_errno)
{
    switch (sys_errno) {
        case 0:
            return UBS_SUCCESS; // 成功
        case EINVAL:
            return UBS_ERR_INVALID_ARG; // 无效参数

        case ERANGE:
            return UBS_ERR_BUFFER_TOO_SMALL; // 目标缓冲区不足

        case ENOSPC:
            return UBS_ERR_RESOURCE_EXHAUSTED; // 资源不足

        case EACCES:
            return UBS_ERR_PERMISSION_DENIED; // 权限问题

        case ENOMEM:
            return UBS_ERR_OUT_OF_MEMORY; // 内存不足

        default:
            return UBS_ERR_OPERATION_FAILED; // 未知失败
    }
}

ubs_error_t ubse_map_daemon_error(int daemon_errno)
{
    if (daemon_errno >= 1000) { // 1000-1099属于ubse错误码
        return daemon_errno;
    }
    switch (daemon_errno) {
        case IPC_SUCCESS:
            return UBS_SUCCESS; // 成功

        case IPC_ERROR_CONNECTION_FAILED:
        case IPC_ERROR_SEND_FAILED:
            return UBS_ERR_IPC_CONNECTION_FAILED; // 连接失败

        case IPC_ERROR_TIMEOUT:
            return UBS_ERR_TIMED_OUT; // 超时

        case IPC_ERROR_INVALID_HANDLE:
        case IPC_ERROR_DAEMON_NOT_READY:
            return UBS_ERR_IPC_SERVICE_UNAVAILABLE; // 服务不可用
        case IPC_ERROR_INVALID_PATH_LENGTH:
            return UBS_ERR_IPC_CONNECTION_FAILED_PATH_LENGTH; // sun_path超长
        default:
            return UBS_ERR_DAEMON_INTERNEL; // 内部错误
    }
}

ubs_error_t ubse_mem_create_req_is_valid(const char *name, uint64_t size)
{
    // 检查size参数
    if (size < UBS_MEM_MIN_SIZE || size > UBS_MEM_MAX_SIZE) {
        return UBS_ERR_OUT_OF_RANGE;
    }

    return ubse_mem_name_is_valid(name);
}

ubs_error_t ubse_mem_create_with_lender_req_is_valid(const char *name, const ubs_mem_lender_t *lender)
{
    // 检查lender参数
    if (lender == NULL) {
        return UBS_ERR_NULL_POINTER;
    }
    return ubse_mem_name_is_valid(name);
}

ubs_error_t ubse_mem_name_is_valid(const char *name)
{
    // 检查name参数
    if (name == NULL) {
        return UBS_ERR_NULL_POINTER;
    }
    if (strlen(name) == 0 || strlen(name) > UBS_MEM_MAX_NAME_LENGTH - 1) {
        return UBS_ERR_INVALID_ARG;
    }
    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_fd_create_req_pack(const char *name, uint64_t size, ubs_mem_distance_t distance,
                                        const ubs_mem_fd_owner_t *owner, mode_t mode, uint8_t *buffer)
{
    uint8_t *ptr = buffer;
    // 打包name
    errno_t ret = strncpy_s((char *)ptr, UBS_MEM_MAX_NAME_LENGTH, name, UBS_MEM_MAX_NAME_LENGTH);
    if (ret != EOK) {
        return ubse_map_sys_error(ret);
    }
    ptr[UBS_MEM_MAX_NAME_LENGTH - 1] = '\0'; // 确保终止符
    ptr += UBS_MEM_MAX_NAME_LENGTH;
    // 打包size
    *(uint64_t *)ptr = htonll_custom(size);
    ptr += sizeof(uint64_t);
    // 打包uds_info
    *(uint32_t *)ptr = htonl(owner ? owner->uid : 0);
    ptr += sizeof(uint32_t);
    *(uint32_t *)ptr = htonl(owner ? owner->gid : 0);
    ptr += sizeof(uint32_t);
    *(uint32_t *)ptr = htonl(owner ? owner->pid : 0);
    ptr += sizeof(uint32_t);
    // 打包mode
    *(uint32_t *)ptr = htonl((uint32_t)mode);
    ptr += sizeof(uint32_t);
    // 打包distance
    *(uint32_t *)ptr = htonl((uint32_t)distance);
    ptr += sizeof(uint32_t);
    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_lender_pack(uint8_t *ptr, const ubs_mem_lender_t *lender)
{
    *(uint32_t *)ptr = htonl(lender->slot_id);
    ptr += sizeof(uint32_t);
    *(uint32_t *)ptr = htonl(lender->socket_id);
    ptr += sizeof(uint32_t);
    *(uint64_t *)ptr = htonll_custom(lender->numa_id);
    ptr += sizeof(uint64_t);
    *(uint64_t *)ptr = htonll_custom(lender->lender_size);
    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_fd_create_with_lender_req_pack(const char *name, const ubs_mem_fd_owner_t *owner, mode_t mode,
                                                    const ubs_mem_lender_t *lender, uint8_t *buffer)
{
    uint8_t *ptr = buffer;

    // 打包name
    errno_t ret = strncpy_s((char *)ptr, UBS_MEM_MAX_NAME_LENGTH, name, UBS_MEM_MAX_NAME_LENGTH);
    if (ret != EOK) {
        return ubse_map_sys_error(ret);
    }
    ptr[UBS_MEM_MAX_NAME_LENGTH - 1] = '\0'; // 确保终止符
    ptr += UBS_MEM_MAX_NAME_LENGTH;
    // 打包uds_info
    *(uint32_t *)ptr = htonl(owner ? owner->uid : 0);
    ptr += sizeof(uint32_t);
    *(uint32_t *)ptr = htonl(owner ? owner->gid : 0);
    ptr += sizeof(uint32_t);
    *(uint32_t *)ptr = htonl(owner ? owner->pid : 0);
    ptr += sizeof(uint32_t);
    // 打包mode
    *(uint32_t *)ptr = htonl((uint32_t)mode);
    ptr += sizeof(uint32_t);
    // 打包lender
    ret = ubse_mem_lender_pack(ptr, lender);
    if (ret != UBS_SUCCESS) {
        return ret;
    }
    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_fd_delete_req_pack(const char *name, uint8_t *buffer)
{
    uint8_t *ptr = buffer;
    // 打包name
    errno_t ret = strncpy_s((char *)ptr, UBS_MEM_MAX_NAME_LENGTH, name, UBS_MEM_MAX_NAME_LENGTH);
    if (ret != EOK) {
        return ubse_map_sys_error(ret);
    }
    ptr[UBS_MEM_MAX_NAME_LENGTH - 1] = '\0'; // 确保终止符
    ptr += UBS_MEM_MAX_NAME_LENGTH;
    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_numa_create_req_pack(const char *name, uint64_t size, ubs_mem_distance_t distance, uint8_t *buffer)
{
    uint8_t *ptr = buffer;
    // 打包name
    errno_t ret = strncpy_s((char *)ptr, UBS_MEM_MAX_NAME_LENGTH + 1, name, UBS_MEM_MAX_NAME_LENGTH);
    if (ret != EOK) {
        return ubse_map_sys_error(ret);
    }
    ptr[UBS_MEM_MAX_NAME_LENGTH - 1] = '\0'; // 确保终止符
    ptr += UBS_MEM_MAX_NAME_LENGTH;
    // 打包size
    *(uint64_t *)ptr = htonll_custom(size);
    ptr += sizeof(uint64_t);
    // 打包distance
    *(uint32_t *)ptr = htonl((uint32_t)distance);
    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_numa_create_with_lender_req_pack(const char *name, const ubs_mem_lender_t *lender, uint8_t *buffer)
{
    uint8_t *ptr = buffer;

    // 打包name
    errno_t ret = strncpy_s((char *)ptr, UBS_MEM_MAX_NAME_LENGTH, name, UBS_MEM_MAX_NAME_LENGTH);
    if (ret != EOK) {
        return ubse_map_sys_error(ret);
    }
    ptr[UBS_MEM_MAX_NAME_LENGTH - 1] = '\0'; // 确保终止符
    ptr += UBS_MEM_MAX_NAME_LENGTH;

    // 打包lender
    ret = ubse_mem_lender_pack(ptr, lender);
    if (ret != UBS_SUCCESS) {
        return ret;
    }
    return UBS_SUCCESS;
}

// 内部函数：解包单个描述符，返回消耗的字节数
static ubs_error_t mem_fd_desc_unpack(const uint8_t **buffer_ptr, const uint8_t *end, ubs_mem_fd_desc_t *desc)
{
    const uint8_t *ptr = *buffer_ptr;

    // 检查最小长度
    if ((end - ptr) < (UBS_MEM_MAX_NAME_LENGTH + sizeof(uint32_t))) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }

    // 解包name
    errno_t ret = memcpy_s(desc->name, UBS_MEM_MAX_NAME_LENGTH, ptr, UBS_MEM_MAX_NAME_LENGTH);
    if (ret != EOK) {
        return ubse_map_sys_error(ret);
    }
    desc->name[UBS_MEM_MAX_NAME_LENGTH - 1] = '\0';
    ptr += UBS_MEM_MAX_NAME_LENGTH;

    // 解包memid_cnt
    desc->memid_cnt = ntohl(*(const uint32_t *)ptr);
    ptr += sizeof(uint32_t);

    // 验证memid_cnt有效性
    if (desc->memid_cnt > UBS_MEM_MAX_MEMID_NUM) {
        return UBS_ERR_INVALID_ARG;
    }

    // 检查剩余字段空间
    const size_t remaining_len = desc->memid_cnt * sizeof(uint64_t) + sizeof(uint64_t) + // mem_size
                                 sizeof(uint64_t);                                       // unit_size

    if ((size_t)(end - ptr) < remaining_len) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }

    // 解包memids
    for (uint32_t j = 0; j < desc->memid_cnt; j++) {
        desc->memids[j] = ntohll_custom(*(const uint64_t *)ptr);
        ptr += sizeof(uint64_t);
    }

    // 解包mem_size
    desc->mem_size = ntohll_custom(*(const uint64_t *)ptr);
    ptr += sizeof(uint64_t);

    // 解包unit_size
    desc->unit_size = (size_t)ntohll_custom(*(const uint64_t *)ptr);
    ptr += sizeof(uint64_t);

    // 更新指针位置
    *buffer_ptr = ptr;
    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_fd_desc_unpack(const uint8_t *buffer, uint32_t len, ubs_mem_fd_desc_t *fd_desc)
{
    const uint8_t *ptr = buffer;
    const uint8_t *end = buffer + len;

    ubs_error_t ret = mem_fd_desc_unpack(&ptr, end, fd_desc);
    if (ret != UBS_SUCCESS) {
        return ret;
    }

    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_fd_desc_list_unpack(const uint8_t *buffer, uint32_t len, ubs_mem_fd_desc_t **fd_descs,
                                         uint32_t *fd_desc_cnt)
{
    if (len < sizeof(uint32_t)) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    const uint8_t *ptr = buffer;
    const uint8_t *end = buffer + len;

    // 解包描述符数量
    *fd_desc_cnt = ntohl(*(const uint32_t *)ptr);
    ptr += sizeof(uint32_t);

    // 验证描述符数量
    if (*fd_desc_cnt == 0) {
        *fd_descs = NULL;
        return UBS_SUCCESS; // 空列表是有效的
    }

    // 分配内存
    *fd_descs = (ubs_mem_fd_desc_t *)calloc(*fd_desc_cnt, sizeof(ubs_mem_fd_desc_t));
    if (*fd_descs == NULL) {
        return UBS_ERR_OUT_OF_MEMORY;
    }
    // 解包每个描述符
    for (uint32_t i = 0; i < *fd_desc_cnt; i++) {
        ubs_error_t ret = mem_fd_desc_unpack(&ptr, end, &(*fd_descs)[i]);
        if (ret != UBS_SUCCESS) {
            free(*fd_descs);
            *fd_descs = NULL;
            return ret;
        }
    }
    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_unma_desc_unpack(const uint8_t *buffer, uint32_t len, ubs_mem_numa_desc_t *numa_desc)
{
    const uint8_t *ptr = buffer;
    // 检查长度
    if (len < UBSE_MEM_NUMA_DESC_SIZE) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }

    // 解包name
    errno_t ret = memcpy_s(numa_desc->name, UBS_MEM_MAX_NAME_LENGTH - 1, ptr, UBS_MEM_MAX_NAME_LENGTH - 1);
    if (ret != EOK) {
        return ubse_map_sys_error(ret);
    }
    numa_desc->name[UBS_MEM_MAX_NAME_LENGTH - 1] = '\0'; // 确保终止符
    ptr += UBS_MEM_MAX_NAME_LENGTH;

    // 解包numaid
    numa_desc->numaid = (int64_t)ntohll_custom(*(uint64_t *)ptr);
    ptr += sizeof(int64_t);
    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_numa_desc_list_unpack(const uint8_t *buffer, uint32_t len, ubs_mem_numa_desc_t **numa_descs,
                                           uint32_t *numa_desc_cnt)
{
    if (len < sizeof(uint32_t)) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    const uint8_t *ptr = buffer;

    // 解包描述符数量
    *numa_desc_cnt = ntohl(*(const uint32_t *)ptr);
    ptr += sizeof(uint32_t);

    // 验证描述符数量
    if (*numa_desc_cnt == 0) {
        *numa_descs = NULL;
        return UBS_SUCCESS; // 空列表是有效的
    }

    if (len < sizeof(uint32_t) + (*numa_desc_cnt) * UBSE_MEM_NUMA_DESC_SIZE) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    // 分配内存
    *numa_descs = (ubs_mem_numa_desc_t *)calloc(*numa_desc_cnt, sizeof(ubs_mem_numa_desc_t));
    if (*numa_descs == NULL) {
        return UBS_ERR_OUT_OF_MEMORY;
    }
    // 解包每个描述符
    for (uint32_t i = 0; i < *numa_desc_cnt; i++) {
        ubs_error_t ret = ubse_mem_unma_desc_unpack(ptr, UBSE_MEM_NUMA_DESC_SIZE, &(*numa_descs)[i]);
        if (ret != UBS_SUCCESS) {
            free(*numa_descs);
            *numa_descs = NULL;
            return ret;
        }
        ptr += UBSE_MEM_NUMA_DESC_SIZE;
    }
    return UBS_SUCCESS;
}

ubs_error_t ubse_node_unpack_inner(const uint8_t *buffer, ubs_topo_node_t *node)
{
    const uint8_t *ptr = buffer;
    // 解包nodeId
    node->slot_id = ntohl(*(const uint32_t *)ptr);
    ptr += sizeof(uint32_t);
    // 解包socketId
    for (int i = 0; i < UBS_TOPO_SOCKET_NUM; i++) {
        node->socket_id[i] = ntohl(*(const uint32_t *)ptr);
        ptr += sizeof(uint32_t);
    }
    // 解包host_name
    errno_t ret = memcpy_s(node->host_name, HOST_NAME_MAX, ptr, HOST_NAME_MAX - 1);
    if (ret != EOK) {
        return ubse_map_sys_error(ret);
    }
    node->host_name[HOST_NAME_MAX - 1] = '\0';
    ptr += HOST_NAME_MAX;
    return UBS_SUCCESS;
}

ubs_error_t ubse_node_cpu_topo_unpack(const uint8_t *buffer, ubs_topo_link_t *cpu_link)
{
    const uint8_t *ptr = buffer;
    // 解包slot_id
    cpu_link->slot_id = ntohl(*(const uint32_t *)ptr);
    ptr += sizeof(uint32_t);
    cpu_link->socket_id = ntohl(*(const uint32_t *)ptr);
    ptr += sizeof(uint32_t);
    cpu_link->peer_slot_id = ntohl(*(const uint32_t *)ptr);
    ptr += sizeof(uint32_t);
    cpu_link->peer_socket_id = ntohl(*(const uint32_t *)ptr);
    return UBS_SUCCESS;
}

ubs_error_t ubse_node_numa_mem_unpack(const uint8_t *buffer, ubs_mem_numastat_t *numa_mem)
{
    const uint8_t *ptr = buffer;
    // 解包numa_loc
    numa_mem->slot_id = ntohl(*(const uint32_t *)ptr);
    ptr += sizeof(uint32_t);
    numa_mem->socket_id = ntohl(*(const uint32_t *)ptr);
    ptr += sizeof(uint32_t);
    numa_mem->numa_id = ntohl(*(const uint32_t *)ptr);
    ptr += sizeof(uint32_t);
    numa_mem->numa_type = ntohl(*(const uint32_t *)ptr);
    ptr += sizeof(uint32_t);
    // 解包mem_lend_ratio
    numa_mem->mem_lend_ratio = ntohl(*(const uint32_t *)ptr);
    ptr += sizeof(uint32_t);
    // 解包mem_total
    numa_mem->mem_total = ntohll_custom(*(const uint64_t *)ptr);
    ptr += sizeof(uint64_t);
    // 解包mem_free
    numa_mem->mem_free = ntohll_custom(*(const uint64_t *)ptr);
    ptr += sizeof(uint64_t);
    // 解包huge_pages
    numa_mem->huge_pages_2M = ntohl(*(const uint32_t *)ptr);
    ptr += sizeof(uint32_t);
    // 解包free_huge_pages
    numa_mem->free_huge_pages_2M = ntohl(*(const uint32_t *)ptr);
    return UBS_SUCCESS;
}

ubs_error_t ubse_node_list_unpack(const uint8_t *buffer, uint32_t len, ubs_topo_node_t **node_list, uint32_t *node_cnt)
{
    if (len < sizeof(uint32_t)) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    const uint8_t *ptr = buffer;

    // 解包node数量
    *node_cnt = ntohl(*(const uint32_t *)ptr);
    ptr += sizeof(uint32_t);

    // 验证node数量
    if (*node_cnt == 0) {
        *node_list = NULL;
        return UBS_SUCCESS; // 空列表是有效的
    }

    if (len < sizeof(uint32_t) + (*node_cnt) * UBSE_NODE_SIZE) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    // 分配内存
    *node_list = (ubs_topo_node_t *)calloc(*node_cnt, sizeof(ubs_topo_node_t));
    if (*node_list == NULL) {
        return UBS_ERR_OUT_OF_MEMORY;
    }
    // 解包每个node
    for (uint32_t i = 0; i < *node_cnt; i++) {
        ubs_error_t ret = ubse_node_unpack_inner(ptr, &(*node_list)[i]);
        if (ret != UBS_SUCCESS) {
            free(*node_list);
            *node_list = NULL;
            return ret;
        }
        ptr += UBSE_NODE_SIZE;
    }
    return UBS_SUCCESS;
}

ubs_error_t ubse_node_unpack(const uint8_t *buffer, uint32_t len, ubs_topo_node_t *node)
{
    if (len < UBSE_NODE_SIZE) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    const uint8_t *ptr = buffer;
    return ubse_node_unpack_inner(ptr, node);
}

ubs_error_t ubse_node_cpu_topo_list_unpack(const uint8_t *buffer, uint32_t len, ubs_topo_link_t **cpu_links,
                                           uint32_t *cpu_link_cnt)
{
    if (len < sizeof(uint32_t)) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    const uint8_t *ptr = buffer;

    // 解包cpu拓扑数量
    *cpu_link_cnt = ntohl(*(const uint32_t *)ptr);
    ptr += sizeof(uint32_t);

    // 验证cpu拓扑数量
    if (*cpu_link_cnt == 0) {
        *cpu_links = NULL;
        return UBS_SUCCESS; // 空列表是有效的
    }

    if (len < sizeof(uint32_t) + (*cpu_link_cnt) * UBSE_LINK_SIZE) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    // 分配内存
    *cpu_links = (ubs_topo_link_t *)calloc(*cpu_link_cnt, sizeof(ubs_topo_link_t));
    if (*cpu_links == NULL) {
        return UBS_ERR_OUT_OF_MEMORY;
    }
    // 解包每个cpu拓扑
    for (uint32_t i = 0; i < *cpu_link_cnt; i++) {
        ubs_error_t ret = ubse_node_cpu_topo_unpack(ptr, &(*cpu_links)[i]);
        if (ret != UBS_SUCCESS) {
            free(*cpu_links);
            *cpu_links = NULL;
            return ret;
        }
        ptr += UBSE_LINK_SIZE;
    }
    return UBS_SUCCESS;
}

ubs_error_t ubse_node_numa_mem_list_unpack(const uint8_t *buffer, uint32_t len, ubs_mem_numastat_t **numa_mem_list,
                                           uint32_t *numa_mem_cnt)
{
    if (len < sizeof(uint32_t)) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    const uint8_t *ptr = buffer;

    // 解包numa信息数量
    *numa_mem_cnt = ntohl(*(const uint32_t *)ptr);
    ptr += sizeof(uint32_t);

    // 验证numa信息数量
    if (*numa_mem_cnt == 0) {
        *numa_mem_list = NULL;
        return UBS_SUCCESS; // 空列表是有效的
    }

    const size_t ubse_node_numa_mem_size = sizeof(uint32_t) + sizeof(uint32_t) + // slot_id+socketId
                                           sizeof(uint32_t) + sizeof(uint32_t) + // numaId + numaType
                                           sizeof(uint32_t) +                    // mem_reserved_ratio
                                           sizeof(uint64_t) + sizeof(uint64_t) + // mem_total + mem_free
                                           sizeof(uint32_t) + sizeof(uint32_t);  // huge_pages+free_huge_pages
    if (len < sizeof(uint32_t) + (*numa_mem_cnt) * ubse_node_numa_mem_size) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    // 分配内存
    *numa_mem_list = (ubs_mem_numastat_t *)calloc(*numa_mem_cnt, sizeof(ubs_mem_numastat_t));
    if (*numa_mem_list == NULL) {
        return UBS_ERR_OUT_OF_MEMORY;
    }
    // 解包每个numa信息
    for (uint32_t i = 0; i < *numa_mem_cnt; i++) {
        ubs_error_t ret = ubse_node_numa_mem_unpack(ptr, &(*numa_mem_list)[i]);
        if (ret != UBS_SUCCESS) {
            free(*numa_mem_list);
            *numa_mem_list = NULL;
            return ret;
        }
        ptr += ubse_node_numa_mem_size;
    }
    return UBS_SUCCESS;
}
typedef struct {
    const uint8_t *ptr;
    uint32_t remaining;
} unpack_ctx_t;

static ubs_error_t unpack_uint32(unpack_ctx_t *ctx, uint32_t *value)
{
    if (ctx->remaining < sizeof(uint32_t)) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    *value = *(const uint32_t *)(ctx->ptr);
    ctx->ptr += sizeof(uint32_t);
    ctx->remaining -= sizeof(uint32_t);
    return UBS_SUCCESS;
}

static ubs_error_t unpack_string(unpack_ctx_t *ctx, char *dest, uint32_t max_len)
{
    uint32_t str_len = 0;
    ubs_error_t ret = unpack_uint32(ctx, &str_len);
    if (ret != UBS_SUCCESS) {
        return ret;
    }

    if ((str_len + 1) > max_len || str_len > ctx->remaining) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }

    errno_t err = memcpy_s(dest, str_len, ctx->ptr, str_len);
    if (err != EOK) {
        return ubse_map_sys_error(err);
    }
    dest[str_len] = '\0';
    ctx->ptr += str_len;
    ctx->remaining -= str_len;
    return UBS_SUCCESS;
}

ubs_error_t ubse_urma_dev_unpack(unpack_ctx_t *ctx, urma_device_t *urma_dev)
{
    unpack_string(ctx, urma_dev->name, UBS_URMA_NAME_MAX);
    unpack_uint32(ctx, &urma_dev->healthy);
    return UBS_SUCCESS;
}

ubs_error_t ubse_urma_dev_get_unpack(const uint8_t *buffer, uint32_t len, urma_device_t **urma_devices,
                                     uint32_t *urma_cnt)
{
    if (len < sizeof(uint32_t)) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    unpack_ctx_t ctx = {.ptr = buffer, .remaining = len};
    unpack_uint32(&ctx, urma_cnt);
    if (*urma_cnt == 0) {
        *urma_cnt = NULL;
        return UBS_SUCCESS;
    }

    *urma_devices = (urma_device_t *)calloc(*urma_cnt, sizeof(urma_device_t));
    if (*urma_devices == NULL) {
        return UBS_ERR_OUT_OF_MEMORY;
    }
    for (uint32_t i = 0; i < *urma_cnt; i++) {
        ubs_error_t ret = ubse_urma_dev_unpack(&ctx, &(*urma_devices)[i]);
        if (ret != UBS_SUCCESS) {
            free(*urma_devices);
            *urma_devices = NULL;
            return ret;
        }
    }

    return UBS_SUCCESS;
}

ubs_error_t ubse_urma_dev_alloc_unpack(const uint8_t *buffer, uint32_t len, ubs_urma_dev_path_t *dev_info)
{
    unpack_ctx_t ctx;
    ctx.ptr = buffer;
    ctx.remaining = len;
    unpack_string(&ctx, dev_info->bonding_path, UBSE_MAX_URMA_PATH_LENGTH);
    unpack_string(&ctx, dev_info->vfe0_path, UBSE_MAX_URMA_PATH_LENGTH);
    unpack_string(&ctx, dev_info->vfe1_path, UBSE_MAX_URMA_PATH_LENGTH);
    unpack_string(&ctx, dev_info->bonding_eid, UBSE_MAX_URMA_PATH_LENGTH);
}