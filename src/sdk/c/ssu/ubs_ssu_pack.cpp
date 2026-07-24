/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubs_ssu_pack.h"

#include <securec.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>

#include "util/ubs_engine_pack_util.h"
#include "ubs_engine_ssu.h"
#include "ubs_error.h"
#include "ubse_ipc_client.h"
#include "ubse_ipc_log.h"

namespace ubs::ssu {

using namespace ubs::sdk;
ubs_error_t unpack_host_nqn_list(UnpackCtx *ctx, ubs_ssu_namespace_info_t *ns);
void free_namespace_info(ubs_ssu_namespace_info_t *ns);
ubs_error_t unpack_namespace_info(UnpackCtx *ctx, ubs_ssu_namespace_info_t *ns);
ubs_error_t unpack_namespace_info_list(UnpackCtx *ctx, ubs_ssu_namespace_info_t **ns_list, uint32_t ns_count);
ubs_error_t unpack_alloc_result(UnpackCtx *ctx, ubs_ssu_alloc_result_t *result);
ubs_error_t unpack_ns_stats(UnpackCtx *ctx, ubs_ssu_ns_stats_t *ns);
ubs_error_t unpack_ns_stats_list(UnpackCtx *ctx, ubs_ssu_ns_stats_t **ns_stats_list, uint32_t *ns_stats_cnt);
ubs_error_t unpack_connect_info(UnpackCtx *ctx, ubs_ssu_connect_info_t *info);
ubs_error_t unpack_connect_info_list(UnpackCtx *ctx, ubs_ssu_connect_info_t **connect_info_list,
                                     uint32_t *connect_info_cnt);
ubs_error_t unpack_vfe(UnpackCtx *ctx, ubs_ub_vfe_t *vfe);
ubs_error_t unpack_vfe_list(UnpackCtx *ctx, ubs_ub_vfe_t **vfe_list, uint32_t vfe_count);
ubs_error_t unpack_fe_device(UnpackCtx *ctx, ubs_ub_fe_t *fe);
ubs_error_t unpack_fe_device_list(UnpackCtx *ctx, ubs_ub_fe_t **fe_list, uint32_t *fe_cnt);
ubs_error_t pack_vfe(PackCtx &ctx, const ubs_ub_vfe_t *vfe);
void free_buffer(ubse_api_buffer_t &buffer);

// ============================================================================
// 内存分配大小校验(G.RES.02-CPP: malloc/new 前必须校验申请大小合法性)
// ============================================================================

// IPC 单次 buffer 合理上限 16MB,超过视为异常/恶意输入
static constexpr size_t UBS_SSU_MAX_ALLOC_SIZE = 16UL * 1024 * 1024;
// 解包列表元素数上限,防止恶意回包塞入超大 count 导致 OOM
static constexpr uint32_t UBS_SSU_MAX_LIST_COUNT = 100000;

/**
 * 校验 malloc 申请大小的合法性
 * @param size 申请字节数
 * @return UBS_SUCCESS 合法;否则错误码
 */
static ubs_error_t check_alloc_size(size_t size)
{
    if (size == 0) {
        IPC_LOG_ERROR << "Invalid alloc size: 0";
        return UBS_ERR_OUT_OF_RANGE;
    }
    if (size > UBS_SSU_MAX_ALLOC_SIZE) {
        IPC_LOG_ERROR << "Alloc size too large: " << size << ", max: " << UBS_SSU_MAX_ALLOC_SIZE;
        return UBS_ERR_OUT_OF_RANGE;
    }
    return UBS_SUCCESS;
}

/**
 * 校验解包列表元素数的合法性
 * @param count 列表元素数(来自外部回包,不可信)
 * @return UBS_SUCCESS 合法;否则错误码
 */
static ubs_error_t check_list_count(uint32_t count)
{
    if (count == 0) {
        IPC_LOG_ERROR << "Invalid list count: 0";
        return UBS_ERR_OUT_OF_RANGE;
    }
    if (count > UBS_SSU_MAX_LIST_COUNT) {
        IPC_LOG_ERROR << "List count too large: " << count << ", max: " << UBS_SSU_MAX_LIST_COUNT;
        return UBS_ERR_OUT_OF_RANGE;
    }
    return UBS_SUCCESS;
}
/**
 * @brief 计算字符串的打包大小
 * @param str 字符串指针
 * @param max_len 最大字符串长度
 * @return 字符串的打包大小
 */
static size_t ubse_string_calc_size(const char *str, size_t max_len)
{
    size_t len = 0;
    len += sizeof(uint32_t);
    size_t str_len = str ? strlen(str) : 0;
    if (str_len > max_len) {
        str_len = max_len;
    }
    len += str_len;
    return len;
}
/**
 * @brief 计算分配空间请求的打包大小
 * 对应服务端 SsuAllocSpaceUnpack: name(string) + nsSize(uint64) + nsNum(uint32) + lbaFormat(uint32) + strategy(uint8) + tenant(string)
 * @param req 分配空间请求指针
 * @return 分配空间请求的打包大小
 */
static size_t calc_alloc_space_req_size(const ubs_ssu_alloc_space_req_t *req)
{
    size_t total = 0;
    // name: uint32(len) + len bytes
    total += ubse_string_calc_size(req->name, UBS_SSU_MAX_NAME_LENGTH - 1);
    // ns_size: uint64
    total += sizeof(uint64_t);
    // ns_num: uint32
    total += sizeof(uint32_t);
    // lba_format: uint32
    total += sizeof(uint32_t);
    // strategy: uint8
    total += sizeof(uint8_t);
    // tenant: uint32(len) + len bytes
    total += ubse_string_calc_size(req->tenant, UBS_SSU_MAX_TENANT_LENGTH - 1);
    return total;
}

/**
 * @brief 计算挂载/卸载空间请求的打包大小
 * 对应服务端 SsuAttachSpaceUnpack / SsuDetachSpaceUnpack: name(string) + hostNqn(string) + srcEid(string)
 * @param req 挂载/卸载空间请求指针
 * @return 挂载/卸载空间请求的打包大小
 */
static size_t calc_space_req_size(const ubs_ssu_space_req_t *req)
{
    size_t total = 0;
    total += ubse_string_calc_size(req->name, UBS_SSU_MAX_NAME_LENGTH - 1);
    total += ubse_string_calc_size(req->nqn, UBS_SSU_MAX_NQN_LENGTH - 1);
    total += ubse_string_calc_size(req->src_eid, UBS_SSU_MAX_EID_LENGTH - 1);
    return total;
}

/**
 * @brief 计算线性空间操作请求的打包大小
 * 对应服务端 SsuLinearAttachSpaceUnpack / SsuLinearDetachSpaceUnpack: name(string) + hostNqn(string) + srcEid(string) + devName(string)
 * @param req 线性空间操作请求指针
 * @return 线性空间操作请求的打包大小
 */
static size_t calc_linear_space_req_size(const ubs_ssu_linear_space_req_t *req)
{
    size_t total = 0;
    total += ubse_string_calc_size(req->name, UBS_SSU_MAX_NAME_LENGTH - 1);
    total += ubse_string_calc_size(req->nqn, UBS_SSU_MAX_NQN_LENGTH - 1);
    total += ubse_string_calc_size(req->src_eid, UBS_SSU_MAX_EID_LENGTH - 1);
    total += ubse_string_calc_size(req->dev_name, UBS_SSU_MAX_DEV_NAME_LENGTH - 1);
    return total;
}

/**
 * @brief 计算条带化空间操作请求的打包大小
 * 对应服务端 SsuStripedAttachSpaceUnpack / SsuStripedDetachSpaceUnpack: name(string) + hostNqn(string)
 *      + srcEid(string) + devName(string) + level(uint32) + chunkSize(uint32)
 * @param req 条带化空间操作请求指针
 * @return 条带化空间操作请求的打包大小
 */
static size_t calc_striped_space_req_size(const ubs_ssu_striped_space_req_t *req)
{
    size_t total = 0;
    total += ubse_string_calc_size(req->name, UBS_SSU_MAX_NAME_LENGTH - 1);

    total += ubse_string_calc_size(req->nqn, UBS_SSU_MAX_NQN_LENGTH - 1);
    total += ubse_string_calc_size(req->src_eid, UBS_SSU_MAX_EID_LENGTH - 1);
    total += ubse_string_calc_size(req->dev_name, UBS_SSU_MAX_DEV_NAME_LENGTH - 1);
    total += sizeof(uint32_t); // level
    total += sizeof(uint32_t); // chunk_size
    return total;
}

/**
 * @brief 计算空间释放请求的打包大小
 * 对应服务端 SsuFreeSpaceUnpack: name(string)
 * @param name 空间名称
 * @return 空间释放请求的打包大小
 */
static size_t calc_free_space_req_size(const char *name)
{
    return ubse_string_calc_size(name, UBS_SSU_MAX_NAME_LENGTH - 1);
}

/**
 * @brief 计算添加/移除权限请求的打包大小
 * 对应服务端 SsuAccessPermissionAddUnpack / SsuAccessPermissionRemoveUnpack: name(string) + hostNqn(string)
 * @param name 空间名称
 * @param nqn NQN
 * @return 添加/移除权限请求的打包大小
 */
static size_t calc_access_permission_req_size(const char *name, const char *nqn)
{
    size_t total = ubse_string_calc_size(name, UBS_SSU_MAX_NAME_LENGTH - 1);
    total += ubse_string_calc_size(nqn, UBS_SSU_MAX_NQN_LENGTH - 1);
    return total;
}

/**
 * @brief 计算获取命名空间统计请求的打包大小
 * 对应服务端 SsuGetNsStatsUnpack: name(string)
 * @param name 空间名称
 * @return 获取命名空间统计请求的打包大小
 */
static size_t calc_ns_stats_req_size(const char *name)
{
    return ubse_string_calc_size(name, UBS_SSU_MAX_NAME_LENGTH - 1);
}
static size_t calc_vfe_pack_size(const ubs_ub_vfe_t *vfe)
{
    if (vfe == nullptr) {
        return 0;
    }
    size_t total = 0;
    total += sizeof(uint8_t);  // slot_id
    total += sizeof(uint8_t);  // chip_id
    total += sizeof(uint8_t);  // die_id
    total += sizeof(uint16_t); // pfe_id
    total += sizeof(uint16_t); // vfe_id
    total += ubse_string_calc_size(vfe->vfe_guid, UBS_SSU_GUID_LENGTH); // vfe_guid
    total += ubse_string_calc_size(vfe->bind_bus_instance_guid, UBS_SSU_GUID_LENGTH); // bind_bus_instance_guid
    return total;
}
/**
 * @brief 计算获取连接信息请求的打包大小
 * 对应服务端 SsuGetConnectInfoUnpack: name(string) + hasVfe(uint8) [+ vfe(VfePack)]
 * VfePack: slotId(uint8) + chipId(uint8) + dieId(uint8) + pfeId(uint16) + vfeId(uint16) + vfeGuid(string) + bindBusInstanceGuid(string)
 */
static size_t calc_connect_info_req_size(const char *name, const ubs_ub_vfe_t *vfe)
{
    size_t total = ubse_string_calc_size(name, UBS_SSU_MAX_NAME_LENGTH - 1);
    uint8_t hasVfe = (vfe != nullptr) ? 1 : 0;
    total += sizeof(uint8_t); // hasVfe
    if (hasVfe != 0) {
        total += calc_vfe_pack_size(vfe);
    }
    return total;
}

/**
 * @brief 计算FE设备分配请求的打包大小
 * 对应服务端 SsuFeDeviceAllocUnpack: upi(uint32_t) + vfe(VfePack) + busInstanceGuid(string)
 * @return FE设备分配请求的打包大小
 */
static size_t calc_fe_device_alloc_req_size(const ubs_ub_vfe_t *vfe, const char *bus_instance_guid)
{
    size_t total = sizeof(uint32_t);
    // VfePack
    total += calc_vfe_pack_size(vfe);
    // bus_instance_guid: 字符串打包格式(uint32 长度 + 内容)
    total += ubse_string_calc_size(bus_instance_guid, UBS_SSU_GUID_LENGTH);
    return total;
}
void free_buffer(ubse_api_buffer_t &buffer)
{
    if (buffer.buffer != nullptr) {
        free(buffer.buffer);
        buffer.buffer = nullptr;
        buffer.length = 0;
    }
}
// ============================================================================
// 打包函数
// ============================================================================

/**
 * @brief 分配空间请求打包
 * 对应服务端 SsuAllocSpaceUnpack: name(string) + nsSize(uint64) + nsNum(uint32)
 * + lbaFormat(uint32) + strategy(uint8) + tenant(string)
 * @param req 分配空间请求参数
 * @param buffer 输出缓冲区
 * @return ubs_error_t 错误码
 */
ubs_error_t ubs_ssu_space_alloc_pack(const ubs_ssu_alloc_space_req_t *req, ubse_api_buffer_t &buffer)
{
    if (req == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    size_t total_len = calc_alloc_space_req_size(req);
    if (check_alloc_size(total_len) != UBS_SUCCESS) {
        return UBS_ERR_OUT_OF_RANGE;
    }
    buffer.buffer = static_cast<uint8_t *>(malloc(total_len));
    if (buffer.buffer == nullptr) {
        return UBS_ERR_OUT_OF_MEMORY;
    }
    buffer.length = static_cast<uint32_t>(total_len);
    PackCtx ctx = {buffer.buffer, buffer.buffer + total_len};
    ubs_error_t ret = packString(ctx, req->name, UBS_SSU_MAX_NAME_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        free_buffer(buffer);
        IPC_LOG_ERROR << "pack failed, attr: name, ret: " << ret;
        return ret;
    }
    ret = packValue(ctx, req->ns_size);
    if (ret != UBS_SUCCESS) {
        free_buffer(buffer);
        IPC_LOG_ERROR << "pack failed, attr: ns_size, ret: " << ret;
        return ret;
    }
    ret = packValue(ctx, req->ns_num);
    if (ret != UBS_SUCCESS) {
        free_buffer(buffer);
        IPC_LOG_ERROR << "pack failed, attr: ns_num, ret: " << ret;
        return ret;
    }
    ret = packValue(ctx, static_cast<uint32_t>(req->lba_format));
    if (ret != UBS_SUCCESS) {
        free_buffer(buffer);
        IPC_LOG_ERROR << "pack failed, attr: lba_format, ret: " << ret;
        return ret;
    }
    ret = packValue(ctx, static_cast<uint8_t>(req->strategy));
    if (ret != UBS_SUCCESS) {
        free_buffer(buffer);
        IPC_LOG_ERROR << "pack failed, attr: strategy, ret: " << ret;
        return ret;
    }
    ret = packString(ctx, req->tenant, UBS_SSU_MAX_TENANT_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        free_buffer(buffer);
        IPC_LOG_ERROR << "pack failed, attr: tenant, ret: " << ret;
        return ret;
    }
    return UBS_SUCCESS;
}

/**
 * @brief 挂载空间请求打包
 * 对应服务端 SsuAttachSpaceUnpack: name(string) + hostNqn(string) + srcEid(string)
 * @param req 挂载空间请求参数
 * @param buffer 输出缓冲区
 * @return ubs_error_t 错误码
 */
ubs_error_t ubs_ssu_space_attach_pack(const ubs_ssu_space_req_t *req, ubse_api_buffer_t &buffer)
{
    if (req == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }

    size_t total_len = calc_space_req_size(req);
    if (check_alloc_size(total_len) != UBS_SUCCESS) {
        return UBS_ERR_OUT_OF_RANGE;
    }
    buffer.buffer = static_cast<uint8_t *>(malloc(total_len));
    if (buffer.buffer == nullptr) {
        return UBS_ERR_OUT_OF_MEMORY;
    }
    buffer.length = static_cast<uint32_t>(total_len);

    PackCtx ctx = {buffer.buffer, buffer.buffer + total_len};
    ubs_error_t ret = packString(ctx, req->name, UBS_SSU_MAX_NAME_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        free_buffer(buffer);
        IPC_LOG_ERROR << "pack failed, attr: name, ret: " << ret;
        return ret;
    }
    ret = packString(ctx, req->nqn, UBS_SSU_MAX_NQN_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        free_buffer(buffer);
        IPC_LOG_ERROR << "pack failed, attr: nqn, ret: " << ret;
        return ret;
    }
    ret = packString(ctx, req->src_eid, UBS_SSU_MAX_EID_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        free_buffer(buffer);
        IPC_LOG_ERROR << "pack failed, attr: src_eid, ret: " << ret;
        return ret;
    }
    return UBS_SUCCESS;
}

/**
 * @brief 卸载空间请求打包
 * 对应服务端 SsuDetachSpaceUnpack: name(string) + hostNqn(string) + srcEid(string)
 * @param req 卸载空间请求参数
 * @param buffer 输出缓冲区
 * @return ubs_error_t 错误码
 */
ubs_error_t ubs_ssu_space_detach_pack(const ubs_ssu_space_req_t *req, ubse_api_buffer_t &buffer)
{
    return ubs_ssu_space_attach_pack(req, buffer);
}

/**
 * @brief 线性挂载空间请求打包
 * 对应服务端 SsuLinearAttachSpaceUnpack: name(string) + hostNqn(string) + srcEid(string) + devName(string)
 * @param req 线性挂载空间请求参数
 * @param buffer 输出缓冲区
 * @return ubs_error_t 错误码
 */
ubs_error_t ubs_ssu_linear_space_attach_pack(const ubs_ssu_linear_space_req_t *req, ubse_api_buffer_t &buffer)
{
    if (req == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }

    size_t total_len = calc_linear_space_req_size(req);
    if (check_alloc_size(total_len) != UBS_SUCCESS) {
        return UBS_ERR_OUT_OF_RANGE;
    }
    buffer.buffer = static_cast<uint8_t *>(malloc(total_len));
    if (buffer.buffer == nullptr) {
        return UBS_ERR_OUT_OF_MEMORY;
    }
    buffer.length = static_cast<uint32_t>(total_len);

    PackCtx ctx = {buffer.buffer, buffer.buffer + total_len};
    ubs_error_t ret = packString(ctx, req->name, UBS_SSU_MAX_NAME_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        free_buffer(buffer);
        IPC_LOG_ERROR << "pack failed, attr: name, ret: " << ret;
        return ret;
    }
    ret = packString(ctx, req->nqn, UBS_SSU_MAX_NQN_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        free_buffer(buffer);
        IPC_LOG_ERROR << "pack failed, attr: nqn, ret: " << ret;
        return ret;
    }
    ret = packString(ctx, req->src_eid, UBS_SSU_MAX_EID_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        free_buffer(buffer);
        IPC_LOG_ERROR << "pack failed, attr: src_eid, ret: " << ret;
        return ret;
    }
    ret = packString(ctx, req->dev_name, UBS_SSU_MAX_DEV_NAME_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        free_buffer(buffer);
        IPC_LOG_ERROR << "pack failed, attr: dev_name, ret: " << ret;
        return ret;
    }
    return UBS_SUCCESS;
}

/**
 * @brief 线性卸载空间请求打包
 * 对应服务端 SsuLinearDetachSpaceUnpack: name(string) + hostNqn(string) + srcEid(string) + devName(string)
 * @param req 线性卸载空间请求参数
 * @param buffer 输出缓冲区
 * @return ubs_error_t 错误码
 */
ubs_error_t ubs_ssu_linear_space_detach_pack(const ubs_ssu_linear_space_req_t *req, ubse_api_buffer_t &buffer)
{
    return ubs_ssu_linear_space_attach_pack(req, buffer);
}

/**
 * @brief 条带化挂载空间请求打包
 * 对应服务端 SsuStripedAttachSpaceUnpack: name(string) + hostNqn(string) + srcEid(string)
 *  + devName(string) + level(uint32) + chunkSize(uint32)
 * @param req 条带化挂载空间请求参数
 * @param buffer 输出缓冲区
 * @return ubs_error_t 错误码
 */
ubs_error_t ubs_ssu_striped_space_attach_pack(const ubs_ssu_striped_space_req_t *req, ubse_api_buffer_t &buffer)
{
    if (req == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }

    size_t total_len = calc_striped_space_req_size(req);
    if (check_alloc_size(total_len) != UBS_SUCCESS) {
        return UBS_ERR_OUT_OF_RANGE;
    }
    buffer.buffer = static_cast<uint8_t *>(malloc(total_len));
    if (buffer.buffer == nullptr) {
        return UBS_ERR_OUT_OF_MEMORY;
    }
    buffer.length = static_cast<uint32_t>(total_len);

    PackCtx ctx = {buffer.buffer, buffer.buffer + total_len};
    ubs_error_t ret = packString(ctx, req->name, UBS_SSU_MAX_NAME_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        free_buffer(buffer);
        IPC_LOG_ERROR << "pack failed, attr: name, ret: " << ret;
        return ret;
    }
    ret = packString(ctx, req->nqn, UBS_SSU_MAX_NQN_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        free_buffer(buffer);
        IPC_LOG_ERROR << "pack failed, attr: nqn, ret: " << ret;
        return ret;
    }
    ret = packString(ctx, req->src_eid, UBS_SSU_MAX_EID_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        free_buffer(buffer);
        IPC_LOG_ERROR << "pack failed, attr: src_eid, ret: " << ret;
        return ret;
    }
    ret = packString(ctx, req->dev_name, UBS_SSU_MAX_DEV_NAME_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        free_buffer(buffer);
        IPC_LOG_ERROR << "pack failed, attr: dev_name, ret: " << ret;
        return ret;
    }
    ret = packValue(ctx, static_cast<uint32_t>(req->level));
    if (ret != UBS_SUCCESS) {
        free_buffer(buffer);
        IPC_LOG_ERROR << "pack failed, attr: level, ret: " << ret;
        return ret;
    }
    ret = packValue(ctx, static_cast<uint32_t>(req->chunk_size));
    if (ret != UBS_SUCCESS) {
        free_buffer(buffer);
        IPC_LOG_ERROR << "pack failed, attr: chunk_size, ret: " << ret;
        return ret;
    }
    return UBS_SUCCESS;
}

/**
 * @brief 条带化卸载空间请求打包
 * 对应服务端 SsuStripedDetachSpaceUnpack: name(string) + hostNqn(string) + srcEid(string) + devName(string) + level(uint32) + chunkSize(uint32)
 * @param req 条带化卸载空间请求参数
 * @param buffer 输出缓冲区
 * @return ubs_error_t 错误码
 */
ubs_error_t ubs_ssu_striped_space_detach_pack(const ubs_ssu_striped_space_req_t *req, ubse_api_buffer_t &buffer)
{
    return ubs_ssu_striped_space_attach_pack(req, buffer);
}

/**
 * @brief 释放空间请求打包
 * 对应服务端 SsuFreeSpaceUnpack: name(string)
 * @param name 空间名称
 * @param buffer 输出缓冲区
 * @return ubs_error_t 错误码
 */
ubs_error_t ubs_ssu_space_free_pack(const char *name, ubse_api_buffer_t &buffer)
{
    if (name == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }

    size_t total_len = calc_free_space_req_size(name);
    if (check_alloc_size(total_len) != UBS_SUCCESS) {
        return UBS_ERR_OUT_OF_RANGE;
    }
    buffer.buffer = static_cast<uint8_t *>(malloc(total_len));
    if (buffer.buffer == nullptr) {
        return UBS_ERR_OUT_OF_MEMORY;
    }
    buffer.length = static_cast<uint32_t>(total_len);

    PackCtx ctx = {buffer.buffer, buffer.buffer + total_len};
    ubs_error_t ret = packString(ctx, name, UBS_SSU_MAX_NAME_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        free_buffer(buffer);
        IPC_LOG_ERROR << "pack failed, attr: name, ret: " << ret;
        return ret;
    }
    return UBS_SUCCESS;
}

/**
 * @brief 访问权限请求打包
 * 对应服务端 SsuAccessPermissionUnpack: name(string) + hostNqn(string)
 * @param name 空间名称
 * @param nqn NVMeoF目标NQN
 * @param buffer 输出缓冲区
 * @return ubs_error_t 错误码
 */
ubs_error_t ubs_ssu_access_permission_pack(const char *name, const char *nqn, ubse_api_buffer_t &buffer)
{
    if (name == nullptr || nqn == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }

    size_t total_len = calc_access_permission_req_size(name, nqn);
    if (check_alloc_size(total_len) != UBS_SUCCESS) {
        return UBS_ERR_OUT_OF_RANGE;
    }
    buffer.buffer = static_cast<uint8_t *>(malloc(total_len));
    if (buffer.buffer == nullptr) {
        return UBS_ERR_OUT_OF_MEMORY;
    }
    buffer.length = static_cast<uint32_t>(total_len);

    PackCtx ctx = {buffer.buffer, buffer.buffer + total_len};
    ubs_error_t ret = packString(ctx, name, UBS_SSU_MAX_NAME_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        free_buffer(buffer);
        IPC_LOG_ERROR << "pack failed, attr: name, ret: " << ret;
        return ret;
    }
    ret = packString(ctx, nqn, UBS_SSU_MAX_NQN_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        free_buffer(buffer);
        IPC_LOG_ERROR << "pack failed, attr: nqn, ret: " << ret;
        return ret;
    }
    return UBS_SUCCESS;
}
/**
 * @brief 添加访问权限请求打包
 * 对应服务端 SsuAccessPermissionAddUnpack: name(string) + hostNqn(string)
 * @param name 空间名称
 * @param nqn NVMeoF目标NQN
 * @param buffer 输出缓冲区
 * @return ubs_error_t 错误码
 */
ubs_error_t ubs_ssu_access_permission_add_pack(const char *name, const char *nqn, ubse_api_buffer_t &buffer)
{
    return ubs_ssu_access_permission_pack(name, nqn, buffer);
}

/**
 * @brief 移除访问权限请求打包
 * 对应服务端 SsuAccessPermissionRemoveUnpack: name(string) + hostNqn(string)
 * @param name 空间名称
 * @param nqn NVMeoF目标NQN
 * @param buffer 输出缓冲区
 * @return ubs_error_t 错误码
 */
ubs_error_t ubs_ssu_access_permission_remove_pack(const char *name, const char *nqn, ubse_api_buffer_t &buffer)
{
    return ubs_ssu_access_permission_pack(name, nqn, buffer);
}

/**
 * @brief 获取命名空间统计请求打包
 * 对应服务端 SsuGetNsStatsUnpack: name(string)
 * @param name 空间名称
 * @param buffer 输出缓冲区
 * @return ubs_error_t 错误码
 */
ubs_error_t ubs_ssu_ns_stats_get_pack(const char *name, ubse_api_buffer_t &buffer)
{
    if (name == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }

    size_t total_len = calc_ns_stats_req_size(name);
    if (check_alloc_size(total_len) != UBS_SUCCESS) {
        return UBS_ERR_OUT_OF_RANGE;
    }
    buffer.buffer = static_cast<uint8_t *>(malloc(total_len));
    if (buffer.buffer == nullptr) {
        return UBS_ERR_OUT_OF_MEMORY;
    }
    buffer.length = static_cast<uint32_t>(total_len);

    PackCtx ctx = {buffer.buffer, buffer.buffer + total_len};
    ubs_error_t ret = packString(ctx, name, UBS_SSU_MAX_NAME_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        free_buffer(buffer);
        IPC_LOG_ERROR << "pack failed, attr: name, ret: " << ret;
        return ret;
    }
    return UBS_SUCCESS;
}
ubs_error_t pack_vfe(PackCtx &ctx, const ubs_ub_vfe_t *vfe)
{
    if (vfe != nullptr) {
        auto ret = packValue(ctx, vfe->slot_id);
        if (ret != UBS_SUCCESS) {
            IPC_LOG_ERROR << "pack failed, attr: slot_id, ret: " << ret;
            return ret;
        }
        ret = packValue(ctx, vfe->chip_id);
        if (ret != UBS_SUCCESS) {
            IPC_LOG_ERROR << "pack failed, attr: chip_id, ret: " << ret;
            return ret;
        }
        ret = packValue(ctx, vfe->die_id);
        if (ret != UBS_SUCCESS) {
            IPC_LOG_ERROR << "pack failed, attr: die_id, ret: " << ret;
            return ret;
        }
        ret = packValue(ctx, vfe->pfe_id);
        if (ret != UBS_SUCCESS) {
            IPC_LOG_ERROR << "pack failed, attr: pfe_id, ret: " << ret;
            return ret;
        }
        ret = packValue(ctx, vfe->vfe_id);
        if (ret != UBS_SUCCESS) {
            IPC_LOG_ERROR << "pack failed, attr: vfe_id, ret: " << ret;
            return ret;
        }
        ret = packString(ctx, vfe->vfe_guid, UBS_SSU_GUID_LENGTH);
        if (ret != UBS_SUCCESS) {
            IPC_LOG_ERROR << "pack failed, attr: vfe_guid, ret: " << ret;
            return ret;
        }
        ret = packString(ctx, vfe->bind_bus_instance_guid, UBS_SSU_GUID_LENGTH);
        if (ret != UBS_SUCCESS) {
            IPC_LOG_ERROR << "pack failed, attr: bind_bus_instance_guid, ret: " << ret;
            return ret;
        }
    }
    return UBS_SUCCESS;
}
/**
 * @brief 获取连接信息请求打包
 * 对应服务端 SsuGetConnectInfoUnpack: name(string) + hasVfe(uint8) + vfe(VfePack)
 * @param name 空间名称
 * @param vfe VFE信息
 * @param buffer 输出缓冲区
 * @return ubs_error_t 错误码
 */
ubs_error_t ubs_ssu_connect_info_get_pack(const char *name, const ubs_ub_vfe_t *vfe, ubse_api_buffer_t &buffer)
{
    if (name == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }

    size_t total_len = calc_connect_info_req_size(name, vfe);
    if (check_alloc_size(total_len) != UBS_SUCCESS) {
        return UBS_ERR_OUT_OF_RANGE;
    }
    buffer.buffer = static_cast<uint8_t *>(malloc(total_len));
    if (buffer.buffer == nullptr) {
        return UBS_ERR_OUT_OF_MEMORY;
    }
    buffer.length = static_cast<uint32_t>(total_len);

    PackCtx ctx = {buffer.buffer, buffer.buffer + total_len};
    ubs_error_t ret = packString(ctx, name, UBS_SSU_MAX_NAME_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        free_buffer(buffer);
        IPC_LOG_ERROR << "pack failed, attr: name, ret: " << ret;
        return ret;
    }

    const uint8_t hasVfe = (vfe != nullptr) ? 1 : 0;
    ret = packValue(ctx, hasVfe);
    if (ret != UBS_SUCCESS) {
        free_buffer(buffer);
        IPC_LOG_ERROR << "pack failed, attr: hasVfe, ret: " << ret;
        return ret;
    }
    ret = pack_vfe(ctx, vfe);
    if (ret != UBS_SUCCESS) {
        free_buffer(buffer);
        IPC_LOG_ERROR << "pack failed, attr: vfe, ret: " << ret;
        return ret;
    }
    return UBS_SUCCESS;
}

/**
 * @brief FE设备分配请求打包
 * 对应服务端 SsuFeDeviceAllocUnpack: upi(uint32_t) + vfe(VfePack) + busInstanceGuid(string)
 * @param upi UPID
 * @param vfe VFE信息
 * @param bus_instance_guid 设备实例GUID, 以'\0'结尾的字符串
 * @param buffer 输出缓冲区
 * @return ubs_error_t 错误码
 */
ubs_error_t ubs_ssu_fe_device_alloc_pack(uint32_t upi, const ubs_ub_vfe_t *vfe, const char *bus_instance_guid,
                                         ubse_api_buffer_t &buffer)
{
    if (vfe == nullptr || bus_instance_guid == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }

    size_t total_len = calc_fe_device_alloc_req_size(vfe, bus_instance_guid);
    if (check_alloc_size(total_len) != UBS_SUCCESS) {
        return UBS_ERR_OUT_OF_RANGE;
    }
    buffer.buffer = static_cast<uint8_t *>(malloc(total_len));
    if (buffer.buffer == nullptr) {
        return UBS_ERR_OUT_OF_MEMORY;
    }
    buffer.length = static_cast<uint32_t>(total_len);

    PackCtx ctx = {buffer.buffer, buffer.buffer + total_len};
    ubs_error_t ret = packValue(ctx, upi);
    if (ret != UBS_SUCCESS) {
        free_buffer(buffer);
        IPC_LOG_ERROR << "pack failed, attr: upi, ret: " << ret;
        return ret;
    }

    // VfePack
    ret = pack_vfe(ctx, vfe);
    if (ret != UBS_SUCCESS) {
        free_buffer(buffer);
        IPC_LOG_ERROR << "pack failed, attr: vfe, ret: " << ret;
        return ret;
    }

    ret = packString(ctx, bus_instance_guid, UBS_SSU_GUID_LENGTH);
    if (ret != UBS_SUCCESS) {
        free_buffer(buffer);
        IPC_LOG_ERROR << "pack failed, attr: bus_instance_guid, ret: " << ret;
        return ret;
    }
    return UBS_SUCCESS;
}

/**
 * @brief FE设备释放请求打包
 * 对应服务端 SsuFeDeviceFreeUnpack: upi(uint32_t) + vfe(VfePack)
 * @param upi UPID
 * @param vfe VFE信息
 * @param buffer 输出缓冲区
 * @return ubs_error_t 错误码
 */
ubs_error_t ubs_ssu_fe_device_free_pack(uint32_t upi, const ubs_ub_vfe_t *vfe, ubse_api_buffer_t &buffer)
{
    if (vfe == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }

    size_t total_len = sizeof(uint32_t) + calc_vfe_pack_size(vfe);
    if (check_alloc_size(total_len) != UBS_SUCCESS) {
        return UBS_ERR_OUT_OF_RANGE;
    }
    buffer.buffer = static_cast<uint8_t *>(malloc(total_len));
    if (buffer.buffer == nullptr) {
        return UBS_ERR_OUT_OF_MEMORY;
    }
    buffer.length = static_cast<uint32_t>(total_len);

    PackCtx ctx = {buffer.buffer, buffer.buffer + total_len};
    ubs_error_t ret = packValue(ctx, upi);
    if (ret != UBS_SUCCESS) {
        free_buffer(buffer);
        IPC_LOG_ERROR << "pack failed, attr: upi, ret: " << ret;
        return ret;
    }

    ret = pack_vfe(ctx, vfe);
    if (ret != UBS_SUCCESS) {
        free_buffer(buffer);
        IPC_LOG_ERROR << "pack failed, attr: vfe, ret: " << ret;
        return ret;
    }
    return UBS_SUCCESS;
}
} // namespace ubs::ssu
