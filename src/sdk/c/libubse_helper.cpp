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
#include <iostream>

#include "ubs_engine.h"
#include "ubse_ipc_common.h"
#include "ubse_ipc_log.h"

const uint64_t UBSE_SHM_MEM_ALIGNMENT = 2UL * 1024 * 1024;
const int UBSE_MEM_PRIV_DATA_ONE_PTH_SHIFT = 15;
const int UBSE_MEM_PRIV_DATA_WR_DELAY_COMP_SHIFT = 14;
const int UBSE_MEM_PRIV_DATA_REDUCE_DELAY_COMP_SHIFT = 13;
const int UBSE_MEM_PRIV_DATA_CMO_DELAY_COMP_SHIFT = 12;
const int UBSE_MEM_PRIV_DATA_SO_SHIFT = 11;
const int UBSE_MEM_PRIV_DATA_AD_TR_OCHIP_SHIFT = 10;
const int UBSE_MEM_PRIV_DATA_CACHEABLE_FLAG_SHIFT = 9;
const int UBSE_MEM_PRIV_DATA_MAR_ID_SHIFT = 6;
const int UBSE_MEM_PRIV_DATA_RSV0_SHIFT = 0;

// Daemon internal error code base
const uint32_t UBSE_DAEMON_INTERNAL_ERROR_BASE = 10000;
const uint32_t UBSE_DAEMON_ERROR_INVAL = 10003;
const uint32_t UBSE_DAEMON_URMA_ERROR_ACCESS_MTI_FAILED = 11100;
const uint32_t UBSE_DAEMON_URMA_ERROR_PRIO_GROUP_EXIST = 11101;
const uint32_t UBSE_DAEMON_URMA_ERROR_TEMPLATE_NOT_EXISTED = 11102;
const uint32_t UBSE_DAEMON_URMA_ERROR_TEMPLATE_NOT_APPLIED = 11103;
const uint32_t UBSE_DAEMON_URMA_ERROR_QUERY_PORTS_STATUS_FAILED = 11104;
const uint32_t UBSE_DAEMON_URMA_ERROR_GET_NODE_INFO_FAILED = 11105;
const uint32_t UBSE_DAEMON_URMA_ERROR_CREATE_DEV_FAILED = 11106;
const uint32_t UBSE_DAEMON_URMA_ERROR_DEV_NOT_INACTIVE = 11107;
const uint32_t UBSE_DAEMON_URMA_ERROR_DEV_NOT_EXIST = 11108;
const uint32_t UBSE_DAEMON_URMA_ERROR_DEV_NAME_INVALID = 11109;

// 定义解包上下文结构体
typedef struct {
    const uint8_t *ptr; // 当前指针位置
    uint32_t remaining; // 剩余缓冲区长度
} unpack_ctx_t;

// 安全读取基本类型并更新上下文
static ubs_error_t unpack_uint8(unpack_ctx_t *ctx, uint8_t *value)
{
    if (ctx->remaining < sizeof(uint8_t)) {
        IPC_LOG_ERROR << "Buffer is too small to unpack uint8_t";
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    *value = *ctx->ptr;
    ctx->ptr += sizeof(uint8_t);
    ctx->remaining -= sizeof(uint8_t);
    return UBS_SUCCESS;
}

static ubs_error_t unpack_uint32(unpack_ctx_t *ctx, uint32_t *value)
{
    if (ctx->remaining < sizeof(uint32_t)) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    *value = *reinterpret_cast<const uint32_t *>(ctx->ptr);
    ctx->ptr += sizeof(uint32_t);
    ctx->remaining -= sizeof(uint32_t);
    return UBS_SUCCESS;
}

static ubs_error_t unpack_int32(unpack_ctx_t *ctx, int32_t *value)
{
    if (ctx->remaining < sizeof(int32_t)) {
        IPC_LOG_ERROR << "Buffer is too small to unpack int32_t";
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    *value = *reinterpret_cast<const int32_t *>(ctx->ptr);
    ctx->ptr += sizeof(int32_t);
    ctx->remaining -= sizeof(int32_t);
    return UBS_SUCCESS;
}

static ubs_error_t unpack_uint64(unpack_ctx_t *ctx, uint64_t *value)
{
    if (ctx->remaining < sizeof(uint64_t)) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    *value = *reinterpret_cast<const uint64_t *>(ctx->ptr);
    ctx->ptr += sizeof(uint64_t);
    ctx->remaining -= sizeof(uint64_t);
    return UBS_SUCCESS;
}

static ubs_error_t unpack_int64(unpack_ctx_t *ctx, int64_t *value)
{
    if (ctx->remaining < sizeof(int64_t)) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    *value = *reinterpret_cast<const int64_t *>(ctx->ptr);
    ctx->ptr += sizeof(int64_t);
    ctx->remaining -= sizeof(int64_t);
    return UBS_SUCCESS;
}

// 安全读取字符串并更新上下文
static ubs_error_t unpack_string(unpack_ctx_t *ctx, char *dest, uint32_t max_len)
{
    uint32_t str_len;
    ubs_error_t ret = unpack_uint32(ctx, &str_len);
    if (ret != UBS_SUCCESS)
        return ret;

    if (str_len > max_len || str_len > ctx->remaining) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }

    errno_t err = memcpy_s(dest, max_len, ctx->ptr, str_len);
    if (err != EOK) {
        return ubse_map_sys_error(err);
    }
    dest[str_len] = '\0';

    ctx->ptr += str_len;
    ctx->remaining -= str_len;
    return UBS_SUCCESS;
}

void pack_uint8(uint8_t **ptr, uint8_t value)
{
    **ptr = value;
    *ptr += sizeof(uint8_t);
}

void pack_uint16(uint8_t **ptr, uint16_t value)
{
    if (*ptr == nullptr) {
        return;
    }
    *reinterpret_cast<uint16_t *>(*ptr) = value;
    *ptr += sizeof(uint16_t);
}

void pack_uint32(uint8_t **ptr, uint32_t value)
{
    if (*ptr == nullptr) {
        return;
    }
    *reinterpret_cast<uint32_t *>(*ptr) = value;
    *ptr += sizeof(uint32_t);
}

void pack_int32(uint8_t **ptr, int32_t value)
{
    if (*ptr == nullptr) {
        return;
    }
    *reinterpret_cast<int32_t *>(*ptr) = value;
    *ptr += sizeof(int32_t);
}

void pack_uint64(uint8_t **ptr, uint64_t value)
{
    if (*ptr == nullptr) {
        return;
    }
    *reinterpret_cast<uint64_t *>(*ptr) = value;
    *ptr += sizeof(uint64_t);
}

ubs_error_t pack_string(uint8_t **ptr, const char *str, uint32_t max_len)
{
    uint32_t len = str ? strlen(str) : 0;
    if (len > max_len) {
        len = max_len;
    }

    // 写入长度前缀
    pack_uint32(ptr, len);
    // 写入字符串内容
    if (len > 0) {
        // 正确解引用目标指针
        errno_t ret = memcpy_s(*ptr, len, str, len);
        if (ret != EOK) {
            return ubse_map_sys_error(ret);
        }
        // 移动指针位置
        *ptr += len;
    }
    return UBS_SUCCESS;
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

ubs_error_t ubse_map_daemon_error(uint32_t daemon_errno)
{
    if (daemon_errno < UBSE_DAEMON_INTERNAL_ERROR_BASE) {
        return static_cast<ubs_error_t>(daemon_errno);
    }
    // 内部错误码映射
    switch (daemon_errno) {
        case UBSE_DAEMON_ERROR_INVAL:
            return UBS_ENGINE_ERR_INVALID_PARAM;
        case UBSE_DAEMON_URMA_ERROR_ACCESS_MTI_FAILED:
            return UBS_ENGINE_ERR_URMA_ACCESS_MTI_FAILED;
        case UBSE_DAEMON_URMA_ERROR_PRIO_GROUP_EXIST:
            return UBS_ENGINE_ERR_URMA_PRIO_GROUP_EXIST;
        case UBSE_DAEMON_URMA_ERROR_TEMPLATE_NOT_EXISTED:
            return UBS_ENGINE_ERR_URMA_TEMPLATE_NOT_EXISTED;
        case UBSE_DAEMON_URMA_ERROR_TEMPLATE_NOT_APPLIED:
            return UBS_ENGINE_ERR_URMA_TEMPLATE_NOT_APPLIED;
        case UBSE_DAEMON_URMA_ERROR_QUERY_PORTS_STATUS_FAILED:
            return UBS_ENGINE_ERR_URMA_QUERY_PORTS_STATUS_FAILED;
        case UBSE_DAEMON_URMA_ERROR_GET_NODE_INFO_FAILED:
            return UBS_ENGINE_ERR_URMA_GET_NODE_INFO_FAILED;
        case UBSE_DAEMON_URMA_ERROR_CREATE_DEV_FAILED:
            return UBS_ENGINE_ERR_URMA_CREATE_DEV_FAILED;
        case UBSE_DAEMON_URMA_ERROR_DEV_NOT_INACTIVE:
            return UBS_ENGINE_ERR_URMA_DEV_NOT_INACTIVE;
        case UBSE_DAEMON_URMA_ERROR_DEV_NOT_EXIST:
            return UBS_ENGINE_ERR_URMA_DEV_NOT_EXIST;
        case UBSE_DAEMON_URMA_ERROR_DEV_NAME_INVALID:
            return UBS_ENGINE_ERR_URMA_DEV_NAME_INVALID;
        default:
            return UBS_ENGINE_ERR_INTERNAL;
    }
}

ubs_error_t ubse_mem_create_req_is_valid(const char *name, uint64_t size)
{
    // 检查size参数
    if (size < UBS_MEM_MIN_SIZE) {
        IPC_LOG_ERROR << "Size " << size << " is less than " << UBS_MEM_MIN_SIZE;
        return UBS_ENGINE_ERR_OUT_OF_RANGE;
    }

    return ubse_mem_name_is_valid(name);
}

ubs_error_t ubse_mem_create_with_lender_req_is_valid(const char *name, const ubs_mem_lender_t *lender,
                                                     uint32_t lender_cnt)
{
    // 检查lender参数
    if (lender == nullptr) {
        IPC_LOG_ERROR << "Lender pointer is null";
        return UBS_ERR_NULL_POINTER;
    }
    // 0 < lender_cnt <= 2
    if (lender_cnt > UBS_MEM_MAX_LENDER_CNT || lender_cnt == 0) {
        IPC_LOG_ERROR << "Invalid lender_cnt: " << lender_cnt;
        return UBS_ERR_INVALID_ARG;
    }
    for (uint32_t i = 0; i < lender_cnt; ++i) {
        if (lender[i].lender_size < UBS_MEM_MIN_SIZE) {
            IPC_LOG_ERROR << "Invalid lender_size at index " << i << ": " << lender[i].lender_size;
            return UBS_ENGINE_ERR_OUT_OF_RANGE;
        }
    }
    return ubse_mem_name_is_valid(name);
}

ubs_error_t ubse_mem_name_is_valid(const char *name)
{
    // 检查name参数
    if (name == nullptr) {
        IPC_LOG_ERROR << "Name pointer is null";
        return UBS_ERR_NULL_POINTER;
    }
    auto name_len = strlen(name);
    if (name_len == 0 || name_len > UBS_MEM_MAX_NAME_LENGTH - 1) {
        IPC_LOG_ERROR << "Invalid name length: " << name_len;
        return UBS_ERR_INVALID_ARG;
    }
    for (size_t i = 0; i < name_len; ++i) {
        unsigned char c = name[i];
        if (!isdigit(c) && !isalpha(c) && c != '_' && c != '-' && c != '.' && c != ':') {
            IPC_LOG_ERROR << "Invalid character in name, can only include English "
                             "letters, numbers, dots, colons, underscores, and hyphens.";
            return UBS_ERR_INVALID_ARG;
        }
    }
    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_create_with_candidate_req_is_valid(const char *name, uint64_t size, const uint32_t *slot_ids,
                                                        uint32_t slot_cnt)
{
    // size范围校验
    if (size < UBS_MEM_MIN_SIZE) {
        IPC_LOG_ERROR << "Size " << size << " is less than " << UBS_MEM_MIN_SIZE;
        return UBS_ENGINE_ERR_OUT_OF_RANGE;
    }
    // slot参数校验
    if (slot_cnt > 0 && !slot_ids) {
        IPC_LOG_ERROR << "Slot_ids pointer is null when slot_cnt is " << slot_cnt;
        return UBS_ERR_NULL_POINTER;
    }
    if (slot_cnt > UBS_MEM_MAX_SLOT_NUM) {
        IPC_LOG_ERROR << "Slot_cnt " << slot_cnt << " exceeds maximum " << UBS_MEM_MAX_SLOT_NUM;
        return UBS_ENGINE_ERR_OUT_OF_RANGE;
    }
    return ubse_mem_name_is_valid(name);
}

ubs_error_t ubse_size_is_valid(const uint64_t size)
{
    // size范围校验
    if (size < UBS_MEM_MIN_SIZE) {
        IPC_LOG_ERROR << "Size " << size << " is less than " << UBS_MEM_MIN_SIZE;
        return UBS_ENGINE_ERR_OUT_OF_RANGE;
    }
    const uint64_t alignment = UBSE_SHM_MEM_ALIGNMENT;
    // 检查size是否是2M的倍数
    if (size % alignment != 0) {
        IPC_LOG_ERROR << "Size " << size << " is not a multiple of alignment " << alignment;
        return UBS_ENGINE_ERR_OUT_OF_RANGE;
    }
    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_nodes_t_is_valid(const ubs_mem_nodes_t *region)
{
    if (region == nullptr) {
        return UBS_SUCCESS;
    }
    if (region->node_cnt > UBS_MEM_MAX_SLOT_NUM || region->node_cnt == 0) {
        IPC_LOG_ERROR << "Node count " << region->node_cnt << " is zero or exceeds maximum " << UBS_MEM_MAX_SLOT_NUM;
        return UBS_ENGINE_ERR_OUT_OF_RANGE;
    }
    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_shm_create_req_valid(const char *name, uint64_t size, const ubs_mem_nodes_t *region,
                                          const ubs_mem_nodes_t *provider)
{
    // name
    ubs_error_t ret = ubse_mem_name_is_valid(name);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Invalid name: " << name;
        return ret;
    }
    // size
    ret = ubse_size_is_valid(size);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Invalid size: " << size;
        return ret;
    }

    // region
    ret = ubse_mem_nodes_t_is_valid(region);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Invalid region pointer";
        return ret;
    }
    // provider非空需校验数据合法性
    if (provider != nullptr) {
        ret = ubse_mem_nodes_t_is_valid(provider);
        if (ret != UBS_SUCCESS) {
            IPC_LOG_ERROR << "Invalid provider pointer";
            return ret;
        }
    }
    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_shm_create_with_lender_req_valid(const char *name,
                                                      const ubs_mem_nodes_t *region, const ubs_mem_lender_t *lender)
{
    ubs_error_t ret = ubse_mem_name_is_valid(name);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Invalid name: " << name;
        return ret;
    }

    ret = ubse_mem_nodes_t_is_valid(region);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Invalid region pointer";
        return ret;
    }

    if (lender == nullptr) {
        IPC_LOG_ERROR << "lender pointer is null";
        return UBS_ENGINE_ERR_OUT_OF_RANGE;
    }

    if (lender->lender_size < UBS_MEM_MIN_SIZE) {
        IPC_LOG_ERROR << "Invalid lender_size, lender size=" << lender->lender_size;
        return UBS_ENGINE_ERR_OUT_OF_RANGE;
    }

    return UBS_SUCCESS;
}

size_t ubse_mem_fd_create_req_calc_size(const char *name)
{
    size_t len = 0;

    // 1. 计算name参数的长度
    len += ubse_string_calc_size(name, UBS_MEM_MAX_NAME_LENGTH - 1);

    // 2. 计算size参数长度
    len += sizeof(uint64_t); // size是uint64_t

    // 3. 计算owner结构体长度
    len += sizeof(uint32_t); // uid
    len += sizeof(uint32_t); // gid
    len += sizeof(uint32_t); // pid

    // 4. 计算mode参数长度
    len += sizeof(uint32_t); // mode作为uint32_t打包

    // 5. 计算distance枚举长度
    len += sizeof(uint32_t); // 枚举作为uint32_t打包
    return len;
}

ubs_error_t ubse_string_pack(const char *str, size_t max_len, uint8_t *buffer)
{
    ubs_error_t ret = pack_string(&buffer, str, max_len);
    if (ret != UBS_SUCCESS) {
        return ret;
    }
    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_fd_create_req_pack(const char *name, uint64_t size, const ubs_mem_fd_owner_t *owner, mode_t mode,
                                        ubs_mem_distance_t distance, uint8_t *buffer)
{
    uint8_t *ptr = buffer;
    // 打包name
    ubs_error_t ret = pack_string(&ptr, name, UBS_MEM_MAX_NAME_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack name. Error code: " << ret;
        return ret;
    }
    // 打包size
    pack_uint64(&ptr, size);
    // 打包owner
    pack_uint32(&ptr, owner ? owner->uid : 0);
    pack_uint32(&ptr, owner ? owner->gid : 0);
    pack_uint32(&ptr, owner ? owner->pid : 0);
    // 打包mode
    pack_uint32(&ptr, static_cast<uint32_t>(mode));
    // 打包distance
    pack_uint32(&ptr, static_cast<uint32_t>(distance));
    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_lender_pack(uint8_t *ptr, const ubs_mem_lender_t *lender, uint32_t lender_cnt)
{
    // 打包lender_cnt
    pack_uint32(&ptr, lender_cnt);
    for (uint32_t i = 0; i < lender_cnt; ++i) {
        pack_uint64(&ptr, lender[i].lender_size);
        pack_uint32(&ptr, lender[i].slot_id);
        pack_uint32(&ptr, lender[i].socket_id);
        pack_uint32(&ptr, lender[i].numa_id);
        pack_uint32(&ptr, lender[i].port_id);
    }
    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_fd_create_with_lender_req_pack(const char *name, const ubs_mem_fd_owner_t *owner, mode_t mode,
                                                    const ubs_mem_lender_t *lender, uint32_t lender_cnt,
                                                    uint8_t *buffer)
{
    uint8_t *ptr = buffer;
    // 打包name
    ubs_error_t ret = pack_string(&ptr, name, UBS_MEM_MAX_NAME_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack name. Error code: " << ret;
        return ret;
    }
    // 打包owner
    pack_uint32(&ptr, owner ? owner->uid : 0);
    pack_uint32(&ptr, owner ? owner->gid : 0);
    pack_uint32(&ptr, owner ? owner->pid : 0);
    // 打包mode
    pack_uint32(&ptr, static_cast<uint32_t>(mode));

    // 打包lender
    return ubse_mem_lender_pack(ptr, lender, lender_cnt);
}

ubs_error_t ubse_mem_fd_create_with_candidate_req_pack(const char *name, uint64_t size, const ubs_mem_fd_owner_t *owner,
                                                       mode_t mode, const uint32_t *slot_ids, uint32_t slot_cnt,
                                                       uint8_t *buffer)
{
    uint8_t *ptr = buffer;
    // 打包name
    ubs_error_t ret = pack_string(&ptr, name, UBS_MEM_MAX_NAME_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack name. Error code: " << ret;
        return ret;
    }
    // 打包size
    pack_uint64(&ptr, size);
    // 打包owner
    pack_uint32(&ptr, owner ? owner->uid : 0);
    pack_uint32(&ptr, owner ? owner->gid : 0);
    pack_uint32(&ptr, owner ? owner->pid : 0);
    // 打包mode
    pack_uint32(&ptr, static_cast<uint32_t>(mode));
    // 打包slot_cnt
    pack_uint32(&ptr, slot_cnt);
    // 打包slot_ids
    for (uint32_t i = 0; i < slot_cnt; i++) {
        pack_uint32(&ptr, slot_ids[i]);
    }
    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_fd_permission_req_pack(const char *name, const ubs_mem_fd_owner_t *owner, mode_t mode,
                                            uint8_t *buffer)
{
    uint8_t *ptr = buffer;
    // 打包name
    ubs_error_t ret = pack_string(&ptr, name, UBS_MEM_MAX_NAME_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack name. Error code: " << ret;
        return ret;
    }
    // 打包owner
    pack_uint32(&ptr, owner ? owner->uid : 0);
    pack_uint32(&ptr, owner ? owner->gid : 0);
    pack_uint32(&ptr, owner ? owner->pid : 0);
    // 打包mode
    pack_uint32(&ptr, static_cast<uint32_t>(mode));
    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_numa_create_req_pack(const char *name, uint64_t size, ubs_mem_distance_t distance, uint8_t *buffer)
{
    uint8_t *ptr = buffer;
    // 打包name
    ubs_error_t ret = pack_string(&ptr, name, UBS_MEM_MAX_NAME_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack name. Error code: " << ret;
        return ret;
    }
    // 打包size
    pack_uint64(&ptr, size);
    // 打包distance
    pack_uint32(&ptr, (uint32_t)distance);
    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_numa_create_with_lender_req_pack(const char *name, const ubs_mem_lender_t *lender,
                                                      uint32_t lender_cnt, uint8_t *buffer)
{
    uint8_t *ptr = buffer;
    // 打包name
    ubs_error_t ret = pack_string(&ptr, name, UBS_MEM_MAX_NAME_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack name. Error code: " << ret;
        return ret;
    }
    // 打包lender
    return ubse_mem_lender_pack(ptr, lender, lender_cnt);
}

ubs_error_t ubse_mem_numa_create_with_candidate_req_pack(const char *name, uint64_t size, const uint32_t *slot_ids,
                                                         uint32_t slot_cnt, uint8_t *buffer)
{
    uint8_t *ptr = buffer;
    // 打包name
    ubs_error_t ret = pack_string(&ptr, name, UBS_MEM_MAX_NAME_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack name. Error code: " << ret;
        return ret;
    }
    // 打包size
    pack_uint64(&ptr, size);
    // 打包slot_cnt
    pack_uint32(&ptr, slot_cnt);
    // 打包slot_ids
    for (uint32_t i = 0; i < slot_cnt; i++) {
        pack_uint32(&ptr, slot_ids[i]);
    }
    return UBS_SUCCESS;
}
void pack_mem_shm_region(uint8_t **ptr, const ubs_mem_nodes_t *region)
{
    if (region == nullptr) {
        pack_uint32(ptr, 0);
        return;
    }
    size_t node_cnt = region->node_cnt;

    // 打包 node_cnt
    pack_uint32(ptr, node_cnt);
    for (size_t i = 0; i < node_cnt; i++) {
        pack_uint32(ptr, region->slot_ids[i]);
    }
}

ubs_error_t ubse_mem_shm_create_req_pack(const char *name, uint64_t size, uint8_t usr_info[UBS_MEM_MAX_USR_INFO_LEN],
                                         uint64_t flag, const ubs_mem_nodes_t *region, const ubs_mem_nodes_t *provider,
                                         uint8_t *buffer)
{
    uint8_t *ptr = buffer;
    // 打包name
    ubs_error_t ret = pack_string(&ptr, name, UBS_MEM_MAX_NAME_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack name. Error code: " << ret;
        return ret;
    }
    // 打包size
    pack_uint64(&ptr, size);

    // 打包usr_info
    for (int i = 0; i < UBS_MEM_MAX_USR_INFO_LEN; i++) {
        pack_uint8(&ptr, usr_info[i]);
    }
    //  打包flag
    pack_uint64(&ptr, flag);

    // 打包region
    pack_mem_shm_region(&ptr, region);
    // 打包provier（可选）
    if (provider != nullptr) {
        pack_mem_shm_region(&ptr, provider);
    }
    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_shm_create_with_affinity_req_pack(const char *name, uint64_t size,
                                                       uint8_t usr_info[UBS_MEM_MAX_USR_INFO_LEN], uint64_t flag,
                                                       const ubs_mem_nodes_t *region, const ubs_mem_nodes_t *provider,
                                                       uint8_t *buffer, uint32_t affinity_socket_id)
{
    uint8_t *ptr = buffer;
    // 打包name
    ubs_error_t ret = pack_string(&ptr, name, UBS_MEM_MAX_NAME_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack name. Error code: " << ret;
        return ret;
    }
    // 打包size
    pack_uint64(&ptr, size);
    // 打包usr_info
    for (int i = 0; i < UBS_MEM_MAX_USR_INFO_LEN; i++) {
        pack_uint8(&ptr, usr_info[i]);
    }
    //  打包flag
    pack_uint64(&ptr, flag);
    // 打包affinity_socket_id
    pack_uint32(&ptr, affinity_socket_id);
    // 打包region
    pack_mem_shm_region(&ptr, region);
    // 打包provier
    if (provider != nullptr) {
        pack_mem_shm_region(&ptr, provider);
    }
    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_shm_attach_req_pack(const char *name, const ubs_mem_fd_owner_t *owner, mode_t mode,
                                         uint8_t *buffer)
{
    uint8_t *ptr = buffer;
    // 打包name
    ubs_error_t ret = pack_string(&ptr, name, UBS_MEM_MAX_NAME_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack name. Error code: " << ret;
        return ret;
    }
    // 打包owner
    pack_uint32(&ptr, owner ? owner->uid : 0);
    pack_uint32(&ptr, owner ? owner->gid : 0);
    pack_uint32(&ptr, owner ? owner->pid : 0);
    // 打包mode
    pack_uint32(&ptr, mode);
    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_shm_common_pack(const char *name, uint8_t *buffer)
{
    uint8_t *ptr = buffer;
    // 打包name
    ubs_error_t ret = pack_string(&ptr, name, UBS_MEM_MAX_NAME_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack name. Error code: " << ret;
        return ret;
    }
    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_shm_get_req_pack(const char *name, uint8_t *buffer)
{
    return ubse_mem_shm_common_pack(name, buffer);
}

ubs_error_t ubse_mem_shm_detach_req_pack(const char *name, uint8_t *buffer)
{
    return ubse_mem_shm_common_pack(name, buffer);
}

ubs_error_t ubse_mem_shm_delete_req_pack(const char *name, uint8_t *buffer)
{
    return ubse_mem_shm_common_pack(name, buffer);
}

ubs_error_t ubse_mem_memid_status_get_req_pack(const char *name, uint8_t *buffer)
{
    return ubse_mem_shm_common_pack(name, buffer);
}

ubs_error_t ubse_base_node_unpack_inner(unpack_ctx_t *ctx, ubs_topo_node_t *node)
{
    ubs_error_t ret;
    // 解包基本字段
    if ((ret = unpack_uint32(ctx, &node->slot_id)) != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack slot_id";
        return ret;
    }
    for (uint32_t &i : node->socket_id) {
        if ((ret = unpack_uint32(ctx, &i)) != UBS_SUCCESS) {
            IPC_LOG_ERROR << "Failed to unpack socket_id";
            return ret;
        }
    }
    for (auto &socket : node->numa_ids) {
        for (auto &numa_id : socket) {
            if ((ret = unpack_uint32(ctx, &numa_id)) != UBS_SUCCESS) {
                IPC_LOG_ERROR << "Failed to unpack numa_id";
                return ret;
            }
        }
    }
    // 解包主机名字符串
    return unpack_string(ctx, node->host_name, HOST_NAME_MAX - 1);
}

ubs_error_t ubse_node_unpack_inner(unpack_ctx_t *ctx, ubs_topo_node_t *node)
{
    ubs_error_t ret = ubse_base_node_unpack_inner(ctx, node);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack base node info";
        return ret;
    }
    for (ubs_topo_ip_address_t &ip_addr : node->ips) {
        if ((ret = unpack_int32(ctx, &ip_addr.af)) != UBS_SUCCESS) {
            IPC_LOG_ERROR << "Failed to unpack af";
            return ret;
        }
        if ((ret = unpack_uint32(ctx, &ip_addr.ipv4.s_addr)) != UBS_SUCCESS) {
            IPC_LOG_ERROR << "Failed to unpack ipv4";
            return ret;
        }
        if ((ret = unpack_uint64(ctx, reinterpret_cast<uint64_t *>(&ip_addr.ipv6.__in6_u))) != UBS_SUCCESS) {
            IPC_LOG_ERROR << "Failed to unpack ipv6";
            return ret;
        }
        if ((ret = unpack_uint64(ctx, reinterpret_cast<uint64_t *>(&ip_addr.ipv6.__in6_u) + 1)) != UBS_SUCCESS) {
            IPC_LOG_ERROR << "Failed to unpack ipv6";
            return ret;
        }
    }
    return UBS_SUCCESS;
}

inline ubs_mem_stage uint32_to_ubs_mem_stage(uint32_t value) noexcept
{
    if (value < static_cast<uint32_t>(ubs_mem_stage::UBSE_END)) {
        return static_cast<ubs_mem_stage>(value);
    }
    return ubs_mem_stage::UBSE_END; // 默认值
}

ubs_error_t ubse_state_unpack_inner(unpack_ctx_t *ctx, ubs_mem_fd_desc_t *fd_desc)
{
    uint32_t state_value;
    auto ret = unpack_uint32(ctx, &state_value);
    if (ret != UBS_SUCCESS) {
        return ret;
    }
    fd_desc->mem_stage = uint32_to_ubs_mem_stage(state_value);
    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_fd_desc_unpack_inner(unpack_ctx_t *ctx, ubs_mem_fd_desc_t *fd_desc)
{
    // 解包name
    ubs_error_t ret = unpack_string(ctx, fd_desc->name, UBS_MEM_MAX_NAME_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack name. Error code: " << ret;
        return ret;
    }
    // 解包memid_cnt
    ret = unpack_uint32(ctx, &fd_desc->memid_cnt);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack memid_cnt. Error code: " << ret;
        return ret;
    }
    // 验证memid_cnt有效性
    if (fd_desc->memid_cnt > UBS_MEM_MAX_MEMID_NUM) {
        IPC_LOG_ERROR << "Invalid memid_cnt value: " << fd_desc->memid_cnt;
        return UBS_ERR_INVALID_ARG;
    }
    // 解包memids
    for (uint32_t i = 0; i < fd_desc->memid_cnt; i++) {
        ret = unpack_uint64(ctx, &fd_desc->memids[i]);
        if (ret != UBS_SUCCESS) {
            IPC_LOG_ERROR << "Failed to unpack mem_id. Error code: " << ret;
            return ret;
        }
    }
    // 解包mem_size
    ret = unpack_uint64(ctx, &fd_desc->mem_size);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack mem_size. Error code: " << ret;
        return ret;
    }
    // 解包unit_size
    ret = unpack_uint64(ctx, &fd_desc->unit_size);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack unit_size. Error code: " << ret;
        return ret;
    }
    // 解包export_node
    ret = ubse_base_node_unpack_inner(ctx, &fd_desc->export_node);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack export_node. Error code: " << ret;
        return ret;
    }
    // 解包import_node
    ret = ubse_base_node_unpack_inner(ctx, &fd_desc->import_node);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack import_node. Error code: " << ret;
        return ret;
    }
    // 解包state
    ret = ubse_state_unpack_inner(ctx, fd_desc);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack state. Error code: " << ret;
        return ret;
    }
    return UBS_SUCCESS;
}

size_t ubse_string_calc_size(const char *str, size_t max_len)
{
    size_t len = 0;
    len += sizeof(uint32_t);
    uint32_t str_len = str ? strlen(str) : 0;
    if (str_len > max_len) {
        str_len = max_len;
    }
    len += str_len;
    return len;
}

size_t ubse_mem_fd_create_with_lender_req_calc_size(const char *name, uint32_t lender_cnt)
{
    size_t len = 0;

    // 1. 计算name参数的长度
    len += ubse_string_calc_size(name, UBS_MEM_MAX_NAME_LENGTH - 1);
    // 2. 计算owner结构体长度
    len += sizeof(uint32_t); // uid
    len += sizeof(uint32_t); // gid
    len += sizeof(uint32_t); // pid
    // 3. 计算mode参数长度
    len += sizeof(uint32_t); // mode作为uint32_t打包

    // 4. 计算lender_cnt长度
    len += sizeof(uint32_t);

    for (uint32_t i = 0; i < lender_cnt; ++i) {
        // 5. 计算ubse_mem_lender_t长度
        len += sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint32_t);
    }
    return len;
}

size_t ubse_mem_fd_create_with_candidate_req_calc_size(const char *name, uint32_t slot_cnt)
{
    size_t len = 0;

    // 1. 计算name参数的长度
    len += ubse_string_calc_size(name, UBS_MEM_MAX_NAME_LENGTH - 1);
    // 2. 计算size参数长度
    len += sizeof(uint64_t); // size是uint64_t
    // 3. 计算owner结构体长度
    len += sizeof(uint32_t); // uid
    len += sizeof(uint32_t); // gid
    len += sizeof(uint32_t); // pid
    // 4. 计算mode参数长度
    len += sizeof(uint32_t); // mode作为uint32_t打包
    // 5. 计算slot_ids长度
    len += sizeof(uint32_t) + sizeof(uint32_t) * slot_cnt;
    return len;
}

size_t ubse_mem_fd_permission_req_calc_size(const char *name)
{
    size_t len = 0;

    // 1. 计算name参数的长度
    len += ubse_string_calc_size(name, UBS_MEM_MAX_NAME_LENGTH - 1);
    // 2. 计算owner结构体长度
    len += sizeof(uint32_t); // uid
    len += sizeof(uint32_t); // gid
    len += sizeof(uint32_t); // pid
    // 3. 计算mode参数长度
    len += sizeof(uint32_t); // mode作为uint32_t打包
    return len;
}

size_t ubse_mem_numa_create_req_calc_size(const char *name)
{
    size_t len = 0;

    // 1. 计算name参数的长度
    len += ubse_string_calc_size(name, UBS_MEM_MAX_NAME_LENGTH - 1);

    // 2. 计算size参数长度
    len += sizeof(uint64_t); // size是uint64_t

    // 3. 计算distance枚举长度
    len += sizeof(uint32_t); // 枚举作为uint32_t打包
    return len;
}

size_t ubse_mem_numa_create_with_lender_req_calc_size(const char *name, uint32_t lender_cnt)
{
    size_t len = 0;

    // 1. 计算name参数的长度
    len += ubse_string_calc_size(name, UBS_MEM_MAX_NAME_LENGTH - 1);

    // 2. 计算lender_cnt 长度
    len += sizeof(uint32_t);

    // 3. 计算ubse_mem_lender_t长度
    for (uint32_t i = 0; i < lender_cnt; ++i) {
        len += sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint32_t);
    }
    return len;
}

size_t ubse_mem_numa_create_with_candidate_req_calc_size(const char *name, uint32_t slot_cnt)
{
    size_t len = 0;

    // 1. 计算name参数的长度
    len += ubse_string_calc_size(name, UBS_MEM_MAX_NAME_LENGTH - 1);
    // 2. 计算size参数长度
    len += sizeof(uint64_t); // size是uint64_t
    // 3. 计算slot_ids长度
    len += sizeof(uint32_t) + sizeof(uint32_t) * slot_cnt;
    return len;
}

size_t ubse_mem_shm_create_req_calc_size(const char *name, const ubs_mem_nodes_t *region,
                                         const ubs_mem_nodes_t *provider)
{
    size_t len = 0;
    // 1. 计算name参数的长度
    len += ubse_string_calc_size(name, UBS_MEM_MAX_NAME_LENGTH - 1);

    // 2. 计算size参数长度
    len += sizeof(uint64_t);

    // 3. 计算usr_info参数长度
    len += UBS_MEM_MAX_USR_INFO_LEN;

    // 4. 计算flag参数长度
    len += sizeof(uint64_t);

    // 5. 计算region参数长度
    len += sizeof(uint32_t);
    if (region != nullptr) {
        len += region->node_cnt * sizeof(uint32_t);
    }

    // 6. 计算provier参数长度
    if (provider != nullptr) {
        len += sizeof(uint32_t) + provider->node_cnt * sizeof(uint32_t); // provider
    }
    return len;
}

size_t ubse_mem_shm_create_with_affinity_req_calc_size(const char *name, const ubs_mem_nodes_t *region,
                                                       const ubs_mem_nodes_t *provider)
{
    size_t len = ubse_mem_shm_create_req_calc_size(name, region, provider);
    // 计算affinity_socket_id参数长度
    len += sizeof(uint32_t);
    return len;
}

/*
 * 1. 计算name参数的长度
 * 2. 计算usr_info参数长度
 * 3. 计算flag参数长度
 * 4. 计算region参数长度
 * 5. 计算ubse_mem_lender_t长度
 */
size_t ubse_mem_shm_create_with_lender_req_calc_size(const char *name, const ubs_mem_nodes_t *region,
                                                     const ubs_mem_lender_t *lender)
{
    size_t len = 0;
    len += ubse_string_calc_size(name, UBS_MEM_MAX_NAME_LENGTH - 1);
    len += UBS_MEM_MAX_USR_INFO_LEN;
    len += sizeof(uint64_t);
    len += sizeof(uint32_t);
    if (region != nullptr) {
        len += region->node_cnt * sizeof(uint32_t);
    }
    if (lender != nullptr) {
        len += sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t);
    }
    return len;
}

ubs_error_t ubse_mem_shm_create_with_lender_req_build(ubse_api_buffer_t *ptr, const char *name,
                                                      uint8_t usr_info[UBS_MEM_MAX_USR_INFO_LEN], uint64_t flag,
                                                      const ubs_mem_nodes_t *region, const ubs_mem_lender_t *lender)
{
    const size_t total_len = ubse_mem_shm_create_with_lender_req_calc_size(name, region, lender);
    ptr->buffer = static_cast<uint8_t *>(malloc(total_len));
    if (!ptr->buffer) {
        IPC_LOG_ERROR << "Failed to allocate memory for shm create with affinity request with size " << total_len;
        ubse_api_buffer_free(ptr);
        return UBS_ERR_NULL_POINTER;
    }
    ptr->length = total_len;
    // 打包
    auto ret = ubse_mem_shm_create_with_lender_req_pack(name, usr_info, flag, region, lender,
                                                        ptr->buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack shm create with lender request, error" << ret;
        ubse_api_buffer_free(ptr);
        return ret;
    }

    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_shm_create_with_lender_req_pack(const char *name, const uint8_t usr_info[UBS_MEM_MAX_USR_INFO_LEN],
                                                     uint64_t flag, const ubs_mem_nodes_t *region,
                                                     const ubs_mem_lender_t *lender,
                                                     uint8_t *buffer)
{
    uint8_t *ptr = buffer;
    ubs_error_t ret = pack_string(&ptr, name, UBS_MEM_MAX_NAME_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack name. Error code: " << ret;
        return ret;
    }
    for (size_t i = 0; i < UBS_MEM_MAX_USR_INFO_LEN; i++) {
        pack_uint8(&ptr, usr_info[i]);
    }
    pack_uint64(&ptr, flag);
    pack_mem_shm_region(&ptr, region);
    if (lender != nullptr) {
        pack_uint64(&ptr, lender->lender_size);
        pack_uint32(&ptr, lender->slot_id);
        pack_uint32(&ptr, lender->socket_id);
        pack_uint32(&ptr, lender->numa_id);
        pack_uint32(&ptr, lender->port_id);
    }
    return UBS_SUCCESS;
}

size_t ubse_mem_shm_attach_req_calc_size(const char *name)
{
    size_t len = 0;

    // 1. 计算name参数的长度
    len += ubse_string_calc_size(name, UBS_MEM_MAX_NAME_LENGTH - 1);

    // 2. 计算owner结构体长度
    len += sizeof(uint32_t); // uid
    len += sizeof(uint32_t); // gid
    len += sizeof(uint32_t); // pid

    // 3. 计算mode参数长度
    len += sizeof(uint32_t); // mode作为uint32_t打包
    return len;
}

ubs_error_t ubse_mem_shm_create_req_build(ubse_api_buffer_t *ptr, const char *name, const ubs_mem_nodes_t *region,
                                          const ubs_mem_nodes_t *provider)
{
    const size_t total_len = ubse_mem_shm_create_req_calc_size(name, region, provider);
    ptr->buffer = static_cast<uint8_t *>(malloc(total_len));
    if (!ptr->buffer) {
        IPC_LOG_ERROR << "Failed to allocate memory for shm create request with size " << total_len;
        ubse_api_buffer_free(ptr);
        return UBS_ERR_NULL_POINTER;
    }
    ptr->length = total_len;
    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_shm_create_with_affinity_req_build(ubse_api_buffer_t *ptr, const char *name,
                                                        const ubs_mem_nodes_t *region, const ubs_mem_nodes_t *provider)
{
    const size_t total_len = ubse_mem_shm_create_with_affinity_req_calc_size(name, region, provider);
    ptr->buffer = static_cast<uint8_t *>(malloc(total_len));
    if (!ptr->buffer) {
        IPC_LOG_ERROR << "Failed to allocate memory for shm create with affinity request with size " << total_len;
        ubse_api_buffer_free(ptr);
        return UBS_ERR_NULL_POINTER;
    }
    ptr->length = total_len;
    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_shm_attach_req_build(ubse_api_buffer_t *ptr, const char *name)
{
    size_t total_len = ubse_mem_shm_attach_req_calc_size(name);
    ptr->buffer = static_cast<uint8_t *>(malloc(total_len));
    if (!ptr->buffer) {
        IPC_LOG_ERROR << "Failed to allocate memory for shm attach request with size " << total_len;
        ubse_api_buffer_free(ptr);
        return UBS_ERR_NULL_POINTER;
    }
    ptr->length = total_len;
    return UBS_SUCCESS;
}
ubs_error_t ubse_mem_shm_common_req_build(ubse_api_buffer_t *ptr, const char *name)
{
    size_t total_len = ubse_string_calc_size(name, UBS_MEM_MAX_NAME_LENGTH - 1); // name
    ptr->buffer = static_cast<uint8_t *>(malloc(total_len));
    if (!ptr->buffer) {
        IPC_LOG_ERROR << "Failed to allocate memory for shm common request with size " << total_len;
        ubse_api_buffer_free(ptr);
        return UBS_ERR_NULL_POINTER;
    }
    ptr->length = total_len;
    return UBS_SUCCESS;
}
ubs_error_t ubse_mem_shm_get_req_build(ubse_api_buffer_t *ptr, const char *name)
{
    return ubse_mem_shm_common_req_build(ptr, name);
}

ubs_error_t ubse_mem_shm_detach_req_build(ubse_api_buffer_t *ptr, const char *name)
{
    return ubse_mem_shm_common_req_build(ptr, name);
}

ubs_error_t ubse_mem_shm_delete_req_build(ubse_api_buffer_t *ptr, const char *name)
{
    return ubse_mem_shm_common_req_build(ptr, name);
}

ubs_error_t ubse_mem_fd_desc_unpack(const uint8_t *buffer, uint32_t len, ubs_mem_fd_desc_t *fd_desc)
{
    unpack_ctx_t ctx = {buffer, len};
    return ubse_mem_fd_desc_unpack_inner(&ctx, fd_desc);
}

ubs_error_t ubse_mem_fd_desc_list_unpack(const uint8_t *buffer, uint32_t len, ubs_mem_fd_desc_t **fd_descs,
                                         uint32_t *fd_desc_cnt)
{
    unpack_ctx_t ctx = {buffer, len};
    *fd_descs = nullptr;
    *fd_desc_cnt = 0;

    // 解包节点数量
    ubs_error_t ret = unpack_uint32(&ctx, fd_desc_cnt);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack fd_desc_cnt. Error code: " << ret;
        return ret;
    }

    // 处理空列表情况
    if (*fd_desc_cnt == 0)
        return UBS_SUCCESS;
    // 数组超范围
    if (*fd_desc_cnt > UBS_MEM_MAX_DESC_LIST) {
        IPC_LOG_ERROR << "Invalid fd desc count value: " << *fd_desc_cnt;
        return UBS_ERR_OUT_OF_RANGE;
    }
    // 分配fd数组
    *fd_descs = (ubs_mem_fd_desc_t *)calloc(*fd_desc_cnt, sizeof(ubs_mem_fd_desc_t));
    if (!(*fd_descs)) {
        IPC_LOG_ERROR << "Failed to allocate memory for fd_desc list";
        return UBS_ERR_OUT_OF_MEMORY;
    }

    // 逐个解包fd
    for (uint32_t i = 0; i < *fd_desc_cnt; i++) {
        if ((ret = ubse_mem_fd_desc_unpack_inner(&ctx, &(*fd_descs)[i])) != UBS_SUCCESS) {
            IPC_LOG_ERROR << "Failed to unpack fd_desc at index " << i << ". Error code: " << ret;
            free(*fd_descs);
            *fd_descs = nullptr;
            return ret;
        }
    }
    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_numa_desc_unpack_inner(unpack_ctx_t *ctx, ubs_mem_numa_desc_t *numa_desc)
{
    // 解包name
    ubs_error_t ret = unpack_string(ctx, numa_desc->name, UBS_MEM_MAX_NAME_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack name. Error code: " << ret;
        return ret;
    }
    // 解包numaid
    ret = unpack_int64(ctx, &numa_desc->numaid);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack numa_id. Error code: " << ret;
        return ret;
    }
    // 解包export_node
    ret = ubse_base_node_unpack_inner(ctx, &numa_desc->export_node);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack export_node. Error code: " << ret;
        return ret;
    }
    // 解包import_node
    ret = ubse_base_node_unpack_inner(ctx, &numa_desc->import_node);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack import_node. Error code: " << ret;
        return ret;
    }
    // 解包size
    ret = unpack_uint64(ctx, &numa_desc->size);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack size. Error code: " << ret;
        return ret;
    }
    // 解包state
    uint32_t state_value;
    ret = unpack_uint32(ctx, &state_value);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack state. Error code: " << ret;
        return ret;
    }
    numa_desc->mem_stage = uint32_to_ubs_mem_stage(state_value);
    // 解包usrInfo
    for (uint8_t &i : numa_desc->usrInfo) {
        if ((ret = unpack_uint8(ctx, &i)) != UBS_SUCCESS) {
            IPC_LOG_ERROR << "Failed to unpack socket_id";
            return ret;
        }
    }
    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_numa_desc_unpack(const uint8_t *buffer, uint32_t len, ubs_mem_numa_desc_t *numa_desc)
{
    unpack_ctx_t ctx = {buffer, len};
    return ubse_mem_numa_desc_unpack_inner(&ctx, numa_desc);
}

ubs_error_t ubse_mem_numa_desc_list_unpack(const uint8_t *buffer, uint32_t len, ubs_mem_numa_desc_t **numa_descs,
                                           uint32_t *numa_desc_cnt)
{
    unpack_ctx_t ctx = {buffer, len};
    *numa_descs = nullptr;
    *numa_desc_cnt = 0;

    // 解包节点数量
    ubs_error_t ret = unpack_uint32(&ctx, numa_desc_cnt);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack numa_desc_cnt. Error code: " << ret;
        return ret;
    }

    // 处理空列表情况
    if (*numa_desc_cnt == 0)
        return UBS_SUCCESS;
    // 数组超范围
    if (*numa_desc_cnt > UBS_MEM_MAX_DESC_LIST) {
        IPC_LOG_ERROR << "Invalid numa desc count value: " << *numa_desc_cnt;
        return UBS_ERR_OUT_OF_RANGE;
    }
    // 分配fd数组
    *numa_descs = (ubs_mem_numa_desc_t *)calloc(*numa_desc_cnt, sizeof(ubs_mem_numa_desc_t));
    if (!(*numa_descs)) {
        IPC_LOG_ERROR << "Failed to allocate memory for numa_desc list";
        return UBS_ERR_OUT_OF_MEMORY;
    }

    // 逐个解包fd
    for (uint32_t i = 0; i < *numa_desc_cnt; i++) {
        if ((ret = ubse_mem_numa_desc_unpack_inner(&ctx, &(*numa_descs)[i])) != UBS_SUCCESS) {
            IPC_LOG_ERROR << "Failed to unpack numa_desc at index " << i << ". Error code: " << ret;
            free(*numa_descs);
            *numa_descs = nullptr;
            return ret;
        }
    }
    return UBS_SUCCESS;
}

ubs_error_t ubse_node_cpu_topo_unpack(unpack_ctx_t *ctx, ubs_topo_link_t *cpu_link)
{
    // 解包slot_id
    ubs_error_t ret = unpack_uint32(ctx, &cpu_link->slot_id);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack slot_id. Error code: " << ret;
        return ret;
    }
    // 解包socket_id
    ret = unpack_uint32(ctx, &cpu_link->socket_id);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack socket_id. Error code: " << ret;
        return ret;
    }
    // 解包port_id
    ret = unpack_uint32(ctx, &cpu_link->port_id);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack port_id. Error code: " << ret;
        return ret;
    }
    // 解包peer_slot_id
    ret = unpack_uint32(ctx, &cpu_link->peer_slot_id);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack peer_slot_id. Error code: " << ret;
        return ret;
    }
    // 解包peer_socket_id
    ret = unpack_uint32(ctx, &cpu_link->peer_socket_id);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack peer_socket_id. Error code: " << ret;
        return ret;
    }
    // 解包peer_port_id
    ret = unpack_uint32(ctx, &cpu_link->peer_port_id);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack peer_port_id. Error code: " << ret;
        return ret;
    }
    return UBS_SUCCESS;
}

ubs_error_t ubse_node_numa_mem_unpack(unpack_ctx_t *ctx, ubs_mem_numastat_t *numa_mem)
{
    ubs_error_t ret;
    // 解包slot_id
    if ((ret = unpack_uint32(ctx, &numa_mem->slot_id)) != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack slot_id. Error code: " << ret;
        return ret;
    }
    // 解包socket_id
    if ((ret = unpack_uint32(ctx, &numa_mem->socket_id)) != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack socket_id. Error code: " << ret;
        return ret;
    }
    // 解包numa_id
    if ((ret = unpack_uint32(ctx, &numa_mem->numa_id)) != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack numa_id. Error code: " << ret;
        return ret;
    }
    // 解包numa_type
    uint32_t temp_enum;
    if ((ret = unpack_uint32(ctx, &temp_enum)) != UBS_SUCCESS) {
        return ret;
    }
    numa_mem->numa_type = (ubs_mem_numa_type_t)temp_enum;
    // 解包mem_lend_ratio
    if ((ret = unpack_uint32(ctx, &numa_mem->mem_lend_ratio)) != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack mem_lend_ratio. Error code: " << ret;
        return ret;
    }
    // 解包mem_total
    if ((ret = unpack_uint64(ctx, &numa_mem->mem_total)) != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack mem_total. Error code: " << ret;
        return ret;
    }
    // 解包mem_free
    if ((ret = unpack_uint64(ctx, &numa_mem->mem_free)) != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack mem_free. Error code: " << ret;
        return ret;
    }
    // 解包huge_pages_2M
    if ((ret = unpack_uint32(ctx, &numa_mem->huge_pages_2M)) != UBS_SUCCESS) {
        return ret;
    }
    // 解包free_huge_pages_2M
    if ((ret = unpack_uint32(ctx, &numa_mem->free_huge_pages_2M)) != UBS_SUCCESS) {
        return ret;
    }
    // 解包huge_pages_1G
    if ((ret = unpack_uint32(ctx, &numa_mem->huge_pages_1G)) != UBS_SUCCESS) {
        return ret;
    }
    // 解包free_huge_pages_1G
    if ((ret = unpack_uint32(ctx, &numa_mem->free_huge_pages_1G)) != UBS_SUCCESS) {
        return ret;
    }
    // 解包mem_borrow
    if ((ret = unpack_uint64(ctx, &numa_mem->mem_borrow)) != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack mem_borrow. Error code: " << ret;
        return ret;
    }
    // 解包mem_lend
    return unpack_uint64(ctx, &numa_mem->mem_lend);
}

ubs_error_t ubse_node_list_unpack(const uint8_t *buffer, uint32_t len, ubs_topo_node_t **node_list, uint32_t *node_cnt)
{
    unpack_ctx_t ctx = {buffer, len};
    *node_list = nullptr;
    *node_cnt = 0;

    // 解包节点数量
    ubs_error_t ret = unpack_uint32(&ctx, node_cnt);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack node_cnt. Error code: " << ret;
        return ret;
    }

    // 处理空列表情况
    if (*node_cnt == 0)
        return UBS_SUCCESS;
    // 数组超范围
    if (*node_cnt > UBS_TOPO_MAX_NODE_NUM) {
        IPC_LOG_ERROR << "Invalid node count value: " << *node_cnt;
        return UBS_ERR_OUT_OF_RANGE;
    }
    // 分配节点数组
    *node_list = (ubs_topo_node_t *)calloc(*node_cnt, sizeof(ubs_topo_node_t));
    if (!(*node_list)) {
        IPC_LOG_ERROR << "Failed to allocate memory for node_list list";
        return UBS_ERR_OUT_OF_MEMORY;
    }

    // 逐个解包节点
    for (uint32_t i = 0; i < *node_cnt; i++) {
        if ((ret = ubse_node_unpack_inner(&ctx, &(*node_list)[i])) != UBS_SUCCESS) {
            IPC_LOG_ERROR << "Failed to unpack node_list at index " << i << ". Error code: " << ret;
            free(*node_list);
            *node_list = nullptr;
            return ret;
        }
    }
    return UBS_SUCCESS;
}

ubs_error_t ubse_node_unpack(const uint8_t *buffer, uint32_t len, ubs_topo_node_t *node)
{
    unpack_ctx_t ctx = {buffer, len};
    return ubse_node_unpack_inner(&ctx, node);
}

ubs_error_t ubse_mem_shm_import_desc_unpack(unpack_ctx_t *ctx, ubs_mem_shm_import_desc_t *import_desc)
{
    // 解包memid_cnt
    auto ret = unpack_uint32(ctx, &import_desc->memid_cnt);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack memid_cnt. Error code: " << ret;
        return ret;
    }
    // 验证memid_cnt有效性
    if (import_desc->memid_cnt > UBS_MEM_MAX_MEMID_NUM) {
        IPC_LOG_ERROR << "Invalid memid_cnt value: " << import_desc->memid_cnt;
        return UBS_ERR_INVALID_ARG;
    }
    // 解包memids
    for (uint32_t i = 0; i < import_desc->memid_cnt; i++) {
        ret = unpack_uint64(ctx, &import_desc->memids[i]);
        if (ret != UBS_SUCCESS) {
            IPC_LOG_ERROR << "Failed to unpack mem_id. Error code: " << ret;
            return ret;
        }
    }
    // 解包import_node
    ret = ubse_base_node_unpack_inner(ctx, &import_desc->import_node);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack import_node. Error code: " << ret;
        return ret;
    }
    // 解包state
    uint32_t state_value;
    ret = unpack_uint32(ctx, &state_value);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack state. Error code: " << ret;
        return ret;
    }
    import_desc->mem_stage = uint32_to_ubs_mem_stage(state_value);
    return UBS_SUCCESS;
}
ubs_error_t ubse_mem_shm_desc_export_unpack(unpack_ctx_t *ctx, ubs_mem_shm_desc_t *shm_desc)
{
    // 解包name
    ubs_error_t ret = unpack_string(ctx, shm_desc->name, UBS_MEM_MAX_NAME_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack name. Error code: " << ret;
        return ret;
    }
    // 解包mem_size
    ret = unpack_uint64(ctx, &shm_desc->mem_size);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack mem_size. Error code: " << ret;
        return ret;
    }
    // 解包unit_size
    ret = unpack_uint64(ctx, &shm_desc->unit_size);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack unit_size. Error code: " << ret;
        return ret;
    }
    // 解包export_node
    ret = ubse_base_node_unpack_inner(ctx, &shm_desc->export_node);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack export_node. Error code: " << ret;
        return ret;
    }
    // 解包 usr_info
    for (unsigned char &i : shm_desc->usr_info) {
        ret = unpack_uint8(ctx, &i);
        if (ret != UBS_SUCCESS) {
            IPC_LOG_ERROR << "Failed to unpack usr_info. Error code: " << ret;
            return ret;
        }
    }
    return UBS_SUCCESS;
}
ubs_error_t ubse_mem_shm_desc_unpack(unpack_ctx_t *ctx, ubs_mem_shm_desc_t *shm_desc)
{
    // 解包import_desc_cnt
    auto ret = unpack_uint32(ctx, &shm_desc->import_desc_cnt);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack import_desc_cnt. Error code: " << ret;
        return ret;
    }
    // 解包export部分
    ret = ubse_mem_shm_desc_export_unpack(ctx, shm_desc);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack export part of shm_desc. Error code: " << ret;
        return ret;
    }
    // 解包state
    uint32_t state_value;
    ret = unpack_uint32(ctx, &state_value);
    shm_desc->mem_stage = uint32_to_ubs_mem_stage(state_value);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack state. Error code: " << ret;
        return ret;
    }
    if (shm_desc->import_desc_cnt != 0 && shm_desc->import_desc == nullptr) {
        IPC_LOG_ERROR << "import_desc is null";
        return UBS_ERR_NULL_POINTER;
    }
    for (uint32_t i = 0; i < shm_desc->import_desc_cnt; i++) {
        ret = ubse_mem_shm_import_desc_unpack(ctx, &shm_desc->import_desc[i]);
        if (ret != UBS_SUCCESS) {
            // 处理错误，比如返回错误码或记录日志
            IPC_LOG_ERROR << "Failed to unpack import_desc[" << i << "]";
            return ret;
        }
    }
    return UBS_SUCCESS;
}

ubs_error_t ubse_node_cpu_topo_list_unpack(const uint8_t *buffer, uint32_t len, ubs_topo_link_t **cpu_links,
                                           uint32_t *cpu_link_cnt)
{
    unpack_ctx_t ctx = {buffer, len};
    *cpu_links = nullptr;
    *cpu_link_cnt = 0;

    // 解包cpu拓扑数量
    ubs_error_t ret = unpack_uint32(&ctx, cpu_link_cnt);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack cpu_link_cnt. Error code: " << ret;
        return ret;
    }
    // 验证cpu拓扑数量
    if (*cpu_link_cnt == 0) {
        return UBS_SUCCESS; // 空列表是有效的
    }
    // 数组超范围
    if (*cpu_link_cnt > UBS_TOPO_MAX_CPU_LINK_NUM) {
        IPC_LOG_ERROR << "Invalid cpu link count value: " << *cpu_link_cnt;
        return UBS_ERR_OUT_OF_RANGE;
    }
    // 分配内存
    *cpu_links = (ubs_topo_link_t *)calloc(*cpu_link_cnt, sizeof(ubs_topo_link_t));
    if (*cpu_links == nullptr) {
        IPC_LOG_ERROR << "Failed to allocate memory for cpu_link list";
        return UBS_ERR_OUT_OF_MEMORY;
    }
    // 解包每个cpu拓扑
    for (uint32_t i = 0; i < *cpu_link_cnt; i++) {
        ret = ubse_node_cpu_topo_unpack(&ctx, &(*cpu_links)[i]);
        if (ret != UBS_SUCCESS) {
            IPC_LOG_ERROR << "Failed to unpack cpu toto at index " << i << ". Error code: " << ret;
            free(*cpu_links);
            *cpu_links = nullptr;
            return ret;
        }
    }
    return UBS_SUCCESS;
}

ubs_mem_shm_desc_t *ubse_mem_shm_desc_alloc(uint8_t *buffer, uint32_t length)
{
    if (buffer == nullptr) {
        return nullptr;
    }
    unpack_ctx_t ctx = {buffer, length};
    // 读取 import_desc_cnt
    uint32_t import_desc_cnt{};
    unpack_uint32(&ctx, &import_desc_cnt);

    if (import_desc_cnt == 0) {
        return static_cast<ubs_mem_shm_desc_t *>(malloc(sizeof(ubs_mem_shm_desc_t)));
    }
    ubs_mem_shm_desc_t *shm_desc = static_cast<ubs_mem_shm_desc_t *>(
        malloc(sizeof(ubs_mem_shm_desc_t) + import_desc_cnt * sizeof(ubs_mem_shm_import_desc_t)));
    if (shm_desc) {
        shm_desc->import_desc_cnt = import_desc_cnt;
        shm_desc->import_desc = reinterpret_cast<ubs_mem_shm_import_desc_t *>(shm_desc + 1);
    }
    return shm_desc;
}

ubs_error_t ubse_mem_shm_attach_resp_unpack(uint8_t *buffer, uint32_t length, ubs_mem_shm_desc_t **shm_desc_ptr)
{
    // 分配内存
    *shm_desc_ptr = ubse_mem_shm_desc_alloc(buffer, length);
    if (!*shm_desc_ptr) {
        return UBS_ERR_OUT_OF_MEMORY;
    }
    // 解包
    unpack_ctx_t ctx = {buffer, length};
    auto ret = ubse_mem_shm_desc_unpack(&ctx, *shm_desc_ptr);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack shm desc. Error code: " << ret;
        free(*shm_desc_ptr);
        *shm_desc_ptr = nullptr;
        return ret;
    }
    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_shm_get_resp_unpack(uint8_t *buffer, uint32_t length, ubs_mem_shm_desc_t **shm_desc_ptr)
{
    // 分配内存
    *shm_desc_ptr = ubse_mem_shm_desc_alloc(buffer, length);
    if (!*shm_desc_ptr) {
        return UBS_ERR_OUT_OF_MEMORY;
    }

    // 解包
    unpack_ctx_t ctx = {buffer, length};
    auto ret = ubse_mem_shm_desc_unpack(&ctx, *shm_desc_ptr);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack shm desc. Error code: " << ret;
        free(*shm_desc_ptr);
        *shm_desc_ptr = nullptr;
        return ret;
    }
    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_shm_list_extract_counts(unpack_ctx_t *ctx, uint32_t fd_desc_cnt, uint32_t **import_desc_counts_ptr)
{
    if (fd_desc_cnt == 0) {
        *import_desc_counts_ptr = nullptr;
        return UBS_SUCCESS;
    }
    // 分配import_desc_counts数组
    const auto import_desc_counts = static_cast<uint32_t *>(malloc(fd_desc_cnt * sizeof(uint32_t)));
    if (!import_desc_counts) {
        return UBS_ERR_OUT_OF_MEMORY;
    }
    for (size_t i = 0; i < fd_desc_cnt; i++) {
        if (const ubs_error_t ret = unpack_uint32(ctx, &import_desc_counts[i]); ret != UBS_SUCCESS) {
            free(import_desc_counts);
            return ret;
        }

        // 数组超范围
        if (import_desc_counts[i] > UBS_TOPO_MAX_NODE_NUM) {
            IPC_LOG_ERROR << "Invalid import desc count value: " << import_desc_counts[i];
            free(import_desc_counts);
            return UBS_ERR_OUT_OF_RANGE;
        }
    }
    *import_desc_counts_ptr = import_desc_counts;
    return UBS_SUCCESS;
}
ubs_error_t ubse_mem_shm_list_alloc_memory(uint32_t shm_desc_cnt, const uint32_t *import_desc_counts,
                                           ubs_mem_shm_desc_t **shm_descs_ptr)
{
    if (shm_desc_cnt == 0) {
        IPC_LOG_WARN << "fd desc cnt is 0";
        *shm_descs_ptr = nullptr;
        return UBS_SUCCESS;
    }

    // 1. 统计总内存大小
    size_t total_size = 0;
    total_size += shm_desc_cnt * sizeof(ubs_mem_shm_desc_t); // 所有 desc
    for (size_t i = 0; i < shm_desc_cnt; i++) {
        total_size += import_desc_counts[i] * sizeof(ubs_mem_shm_import_desc_t);
    }

    // 2. 分配一整块内存
    uint8_t *memory_ptr = reinterpret_cast<uint8_t *>(malloc(total_size));
    if (memory_ptr == nullptr) {
        IPC_LOG_ERROR << "Failed to allocate memory for shm_descs";
        *shm_descs_ptr = nullptr;
        return UBS_ERR_OUT_OF_MEMORY;
    }

    // 3. 设置 shm_descs
    ubs_mem_shm_desc_t *shm_descs = reinterpret_cast<ubs_mem_shm_desc_t *>(memory_ptr);
    uint8_t *current_pos = memory_ptr + shm_desc_cnt * sizeof(ubs_mem_shm_desc_t);

    // 4. 初始化每个 desc 的 import_desc 指针
    for (size_t i = 0; i < shm_desc_cnt; i++) {
        if (import_desc_counts[i] != 0) {
            shm_descs[i].import_desc = reinterpret_cast<ubs_mem_shm_import_desc_t *>(current_pos);
            current_pos += import_desc_counts[i] * sizeof(ubs_mem_shm_import_desc_t);
        } else {
            shm_descs[i].import_desc = nullptr;
        }
    }

    // 5. 返回结果
    *shm_descs_ptr = shm_descs;
    return UBS_SUCCESS;
}
ubs_error_t ubse_mem_shm_list_unpack_data(unpack_ctx_t *ctx, ubs_mem_shm_desc_t *shm_descs_ptr, uint32_t shm_desc_cnt)
{
    for (size_t i = 0; i < shm_desc_cnt; i++) {
        // 解包shm_desc
        auto ret = ubse_mem_shm_desc_unpack(ctx, &shm_descs_ptr[i]);
        if (ret != UBS_SUCCESS) {
            IPC_LOG_ERROR << "Failed to unpack shm_desc at index " << i << ". Error code: " << ret;
            return ret;
        }
    }
    return UBS_SUCCESS;
}
ubs_error_t ubse_mem_shm_list_resp_unpack(uint8_t *buffer, uint32_t length,
                                          ubs_mem_shm_desc_t **shm_descs_ptr, // 改为二级指针
                                          uint32_t *shm_desc_cnt)
{
    if (!buffer || !shm_descs_ptr || !shm_desc_cnt) {
        return UBS_ERR_INVALID_ARG;
    }

    unpack_ctx_t ctx = {buffer, length};
    uint32_t *import_desc_counts = nullptr;
    ubs_mem_shm_desc_t *shm_descs = nullptr;

    // 1. 解包数组长度
    ubs_error_t ret = unpack_uint32(&ctx, shm_desc_cnt);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack fd_desc_cnt. Error code: " << ret;
        return ret;
    }

    // 验证描述符数量
    if (*shm_desc_cnt == 0) {
        *shm_descs_ptr = nullptr; // 直接解引用设置
        return UBS_SUCCESS;
    }
    // 数组超范围
    if (*shm_desc_cnt > UBS_MEM_MAX_DESC_LIST) {
        IPC_LOG_ERROR << "Invalid shm desc count value: " << *shm_desc_cnt;
        return UBS_ERR_OUT_OF_RANGE;
    }
    // 2. 提取 import_desc计数
    ret = ubse_mem_shm_list_extract_counts(&ctx, *shm_desc_cnt, &import_desc_counts);
    if (ret != UBS_SUCCESS) {
        return ret;
    }

    // 3. 分配内存
    ret = ubse_mem_shm_list_alloc_memory(*shm_desc_cnt, import_desc_counts, &shm_descs);
    if (ret != UBS_SUCCESS) {
        free(import_desc_counts);
        import_desc_counts = nullptr;
        return ret;
    }
    free(import_desc_counts);

    // 4. 解包数据
    ret = ubse_mem_shm_list_unpack_data(&ctx, shm_descs, *shm_desc_cnt);
    if (ret != UBS_SUCCESS) {
        // 修正：释放正确的指针
        free(shm_descs); // 使用专门的释放函数
        shm_descs = nullptr;
        return ret;
    }

    // 5. 设置输出参数
    *shm_descs_ptr = shm_descs; // 直接解引用设置
    return UBS_SUCCESS;
}

ubs_error_t ubse_node_numa_mem_list_unpack(const uint8_t *buffer, uint32_t len, ubs_mem_numastat_t **numa_mem_list,
                                           uint32_t *numa_mem_cnt)
{
    unpack_ctx_t ctx = {buffer, len};
    *numa_mem_list = nullptr;
    *numa_mem_cnt = 0;

    // 解包numa信息数量
    ubs_error_t ret = unpack_uint32(&ctx, numa_mem_cnt);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack numa_mem_cnt. Error code: " << ret;
        return ret;
    }

    // 验证numa信息数量
    if (*numa_mem_cnt == 0) {
        return UBS_SUCCESS; // 空列表是有效的
    }
    // 数组超范围
    if (*numa_mem_cnt > UBS_TOPO_NUMA_NUM) {
        IPC_LOG_ERROR << "Invalid numa count value: " << *numa_mem_cnt;
        return UBS_ERR_OUT_OF_RANGE;
    }
    // 分配内存
    *numa_mem_list = (ubs_mem_numastat_t *)calloc(*numa_mem_cnt, sizeof(ubs_mem_numastat_t));
    if (*numa_mem_list == nullptr) {
        IPC_LOG_ERROR << "Failed to allocate memory for numa mem list";
        return UBS_ERR_OUT_OF_MEMORY;
    }
    // 解包每个numa信息
    for (uint32_t i = 0; i < *numa_mem_cnt; i++) {
        ret = ubse_node_numa_mem_unpack(&ctx, &(*numa_mem_list)[i]);
        if (ret != UBS_SUCCESS) {
            IPC_LOG_ERROR << "Failed to unpack numa mem list at index " << i << ". Error code: " << ret;
            free(*numa_mem_list);
            *numa_mem_list = nullptr;
            return ret;
        }
    }
    return UBS_SUCCESS;
}
ubs_error_t ubse_mem_shm_fault_get_req_build(const char *name, ubse_api_buffer_t *ptr)
{
    size_t total_len = ubse_string_calc_size(name, UBS_MEM_MAX_NAME_LENGTH - 1); // name
    ptr->buffer = static_cast<uint8_t *>(malloc(total_len));
    if (!ptr->buffer) {
        ubse_api_buffer_free(ptr);
        return UBS_ERR_NULL_POINTER;
    }
    ptr->length = total_len;
    return UBS_SUCCESS;
}
ubs_error_t ubse_mem_shm_fault_get_req_pack(const char *name, uint8_t *buffer)
{
    if (buffer == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    uint8_t *ptr = buffer;
    // 打包name
    ubs_error_t ret = pack_string(&ptr, name, UBS_MEM_MAX_NAME_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to pack name. Error code: " << ret;
        return ret;
    }
    return UBS_SUCCESS;
}

ubs_error_t ubse_mem_shm_fault_get_resp_unpack(uint8_t *buffer, uint32_t len, ubs_mem_memids_fault_t *shm_status)
{
    unpack_ctx_t ctx = {buffer, len};

    // 解包cnt
    ubs_error_t ret = unpack_uint32(&ctx, &shm_status->memid_cnt);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack mem_id_cnt. Error code: " << ret;
        return ret;
    }
    // 验证memid_cnt有效性
    if (shm_status->memid_cnt > UBS_MEM_MAX_MEMID_NUM) {
        IPC_LOG_ERROR << "Invalid memid_cnt value: " << shm_status->memid_cnt;
        return UBS_ERR_INVALID_ARG;
    }
    // 解包memids
    for (uint32_t i = 0; i < shm_status->memid_cnt; i++) {
        ret = unpack_uint64(&ctx, &shm_status->memids[i]);
        if (ret != UBS_SUCCESS) {
            IPC_LOG_ERROR << "Failed to unpack mem_id. Error code: " << ret;
            return ret;
        }

        uint32_t temp_enum;
        if ((ret = unpack_uint32(&ctx, &temp_enum)) != UBS_SUCCESS) {
            IPC_LOG_ERROR << "Failed to unpack mem_id_status. Error code: " << ret;
            return ret;
        }
        shm_status->memid_status[i] = (ubs_mem_fault_type_t)temp_enum;
    }
    return UBS_SUCCESS;
}


ubs_error_t ubse_urma_subdev_unpack(unpack_ctx_t *ctx, ubs_urma_dev_t *urma_dev)
{
    ubs_error_t ret = UBS_SUCCESS;
    ret = unpack_string(ctx, urma_dev->name, UBS_URMA_NAME_MAX);
    if (ret == UBS_SUCCESS) {
        ret = unpack_uint32(ctx, &urma_dev->healthy);
    }
    if (ret == UBS_SUCCESS) {
        ret = unpack_uint64(ctx, &urma_dev->hw_res_id);
    }
    return ret;
}

ubs_error_t ubse_urma_dev_unpack(const uint8_t *buffer, uint32_t len, ubs_urma_dev_t **urma_devices, uint32_t *urma_cnt)
{
    if (buffer == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    if (len < sizeof(uint32_t)) {
        *urma_devices = NULL;
        *urma_cnt = 0;
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    unpack_ctx_t ctx = {.ptr = buffer, .remaining = len};
    unpack_uint32(&ctx, urma_cnt);
    if (*urma_cnt == 0) {
        *urma_devices = NULL;
        *urma_cnt = 0;
        return UBS_SUCCESS;
    }

    *urma_devices = (ubs_urma_dev_t *)calloc(*urma_cnt, sizeof(ubs_urma_dev_t));
    if (*urma_devices == NULL) {
        *urma_cnt = 0;
        return UBS_ERR_OUT_OF_MEMORY;
    }
    for (uint32_t i = 0; i < *urma_cnt; i++) {
        ubs_error_t ret = ubse_urma_subdev_unpack(&ctx, &(*urma_devices)[i]);
        if (ret != UBS_SUCCESS) {
            free(*urma_devices);
            *urma_devices = NULL;
            *urma_cnt = 0;
            return ret;
        }
    }

    return UBS_SUCCESS;
}

ubs_error_t ubse_urma_dev_info_unpack(const uint8_t *buffer, uint32_t len, ubs_urma_dev_info_t *dev_info)
{
    if (buffer == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    unpack_ctx_t ctx;
    ctx.ptr = buffer;
    ctx.remaining = len;
    ubs_error_t ret = UBS_SUCCESS;
    ret = unpack_string(&ctx, dev_info->bonding_path, UBS_MAX_URMA_PATH_LENGTH);
    if (ret == UBS_SUCCESS) {
        ret = unpack_string(&ctx, dev_info->vfe_path[0], UBS_MAX_URMA_PATH_LENGTH);
    }
    if (ret == UBS_SUCCESS) {
        ret = unpack_string(&ctx, dev_info->vfe_path[UBS_VFE_PATH_NUM - 1], UBS_MAX_URMA_PATH_LENGTH);
    }
    if (ret == UBS_SUCCESS) {
        ret = unpack_string(&ctx, dev_info->bonding_eid, UBS_MAX_URMA_PATH_LENGTH);
    }
    return ret;
}

size_t ubse_urma_qos_create_req_calc_size(uint32_t count)
{
    size_t len = 0;
    len += sizeof(uint32_t);
    len += count * (sizeof(uint32_t) + sizeof(uint32_t));
    return len;
}

ubs_error_t ubse_urma_qos_create_req_build(const ubs_urma_qos_config_t *configs, uint32_t count, ubse_api_buffer_t *ptr)
{
    if (ptr == nullptr || configs == nullptr) {
        IPC_LOG_ERROR << "ptr or configs is null";
        return UBS_ERR_NULL_POINTER;
    }

    size_t total_len = ubse_urma_qos_create_req_calc_size(count);
    if (total_len == 0 || total_len > UINT32_MAX) {
        IPC_LOG_ERROR << "Invalid buffer size: " << total_len;
        ubse_api_buffer_free(ptr);
        return UBS_ERR_INVALID_ARG;
    }
    ptr->buffer = static_cast<uint8_t *>(malloc(total_len));
    if (ptr->buffer == nullptr) {
        IPC_LOG_ERROR << "Failed to allocate memory for qos create request with size " << total_len;
        ubse_api_buffer_free(ptr);
        return UBS_ERR_OUT_OF_MEMORY;
    }
    ptr->length = total_len;

    uint8_t *cur = ptr->buffer;
    pack_uint32(&cur, count);
    for (uint32_t i = 0; i < count; i++) {
        pack_uint32(&cur, configs[i].priority);
        pack_uint32(&cur, configs[i].bandwidth);
    }
    return UBS_SUCCESS;
}

ubs_error_t ubse_urma_qos_get_resp_unpack(const uint8_t *buffer, uint32_t len, ubs_urma_qos_config_t **configs,
                                          uint32_t *count)
{
    if (buffer == nullptr || configs == nullptr || count == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    *configs = nullptr;
    *count = 0;
    if (len < sizeof(uint32_t)) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    unpack_ctx_t ctx = {buffer, len};
    ubs_error_t ret = unpack_uint32(&ctx, count);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack count. Error code: " << ret;
        return ret;
    }

    if (*count == 0) {
        return UBS_SUCCESS;
    }
    *configs = (ubs_urma_qos_config_t *)calloc(*count, sizeof(ubs_urma_qos_config_t));
    if (*configs == nullptr) {
        IPC_LOG_ERROR << "Failed to allocate memory for qos configs";
        *count = 0;
        return UBS_ERR_OUT_OF_MEMORY;
    }
    for (uint32_t i = 0; i < *count; i++) {
        uint32_t priorityValue;
        ret = unpack_uint32(&ctx, &priorityValue);
        if (ret != UBS_SUCCESS) {
            IPC_LOG_ERROR << "Failed to unpack priority at index " << i;
            free(*configs);
            *configs = nullptr;
            *count = 0;
            return ret;
        }
        (*configs)[i].priority = priorityValue;
        ret = unpack_uint32(&ctx, &(*configs)[i].bandwidth);
        if (ret != UBS_SUCCESS) {
            IPC_LOG_ERROR << "Failed to unpack bandwidth at index " << i;
            free(*configs);
            *configs = nullptr;
            *count = 0;
            return ret;
        }
    }
    return UBS_SUCCESS;
}