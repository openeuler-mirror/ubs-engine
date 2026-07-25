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
static ubs_error_t unpack_host_nqn_list(UnpackCtx *ctx, ubs_ssu_namespace_info_t *ns);
static void free_namespace_info(ubs_ssu_namespace_info_t *ns);
static ubs_error_t unpack_namespace_info(UnpackCtx *ctx, ubs_ssu_namespace_info_t *ns);
static ubs_error_t unpack_namespace_info_list(UnpackCtx *ctx, ubs_ssu_namespace_info_t **ns_list, uint32_t ns_count);
static ubs_error_t unpack_alloc_result(UnpackCtx *ctx, ubs_ssu_alloc_result_t *result);
static ubs_error_t unpack_ns_stats(UnpackCtx *ctx, ubs_ssu_ns_stats_t *ns);
static ubs_error_t unpack_ns_stats_list(UnpackCtx *ctx, ubs_ssu_ns_stats_t **ns_stats_list, uint32_t *ns_stats_cnt);
static ubs_error_t unpack_connect_info(UnpackCtx *ctx, ubs_ssu_connect_info_t *info);
static ubs_error_t unpack_connect_info_list(UnpackCtx *ctx, ubs_ssu_connect_info_t **connect_info_list,
                                            uint32_t *connect_info_cnt);
static ubs_error_t unpack_vfe(UnpackCtx *ctx, ubs_ub_vfe_t *vfe);
static ubs_error_t unpack_vfe_list(UnpackCtx *ctx, ubs_ub_vfe_t **vfe_list, uint32_t vfe_count);
static ubs_error_t unpack_fe_device(UnpackCtx *ctx, ubs_ub_fe_t *fe);
static ubs_error_t unpack_fe_device_list(UnpackCtx *ctx, ubs_ub_fe_t **fe_list, uint32_t *fe_cnt);
static ubs_error_t pack_vfe(PackCtx &ctx, const ubs_ub_vfe_t *vfe);
static void free_buffer(ubse_api_buffer_t &buffer);

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
 * @param max_len 字符串内容最大长度(不含'\0'), 调用方传 MAX_LENGTH - 1
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
    total += UBS_SSU_GUID_LENGTH; // vfe_guid (定长字节数组)
    total += UBS_SSU_GUID_LENGTH; // bind_bus_instance_guid (定长字节数组)
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
static size_t calc_fe_device_alloc_req_size(const ubs_ub_vfe_t *vfe, const uint8_t *bus_instance_guid)
{
    size_t total = sizeof(uint32_t);
    // VfePack
    total += calc_vfe_pack_size(vfe);
    // bus_instance_guid: 定长字节数组
    total += UBS_SSU_GUID_LENGTH;
    return total;
}
static void free_buffer(ubse_api_buffer_t &buffer)
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
static ubs_error_t pack_vfe(PackCtx &ctx, const ubs_ub_vfe_t *vfe)
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
        ret = packArray(ctx, vfe->vfe_guid, UBS_SSU_GUID_LENGTH);
        if (ret != UBS_SUCCESS) {
            IPC_LOG_ERROR << "pack failed, attr: vfe_guid, ret: " << ret;
            return ret;
        }
        ret = packArray(ctx, vfe->bind_bus_instance_guid, UBS_SSU_GUID_LENGTH);
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
 * @param bus_instance_guid 设备实例GUID, 定长字节数组
 * @param buffer 输出缓冲区
 * @return ubs_error_t 错误码
 */
ubs_error_t ubs_ssu_fe_device_alloc_pack(uint32_t upi, const ubs_ub_vfe_t *vfe, const uint8_t *bus_instance_guid,
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

    ret = packArray(ctx, bus_instance_guid, UBS_SSU_GUID_LENGTH);
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

// ============================================================================
// 解包函数（内部辅助函数）
// ============================================================================

/**
 * 解包单个 ubs_ssu_namespace_info_t 结构体
 * 对应服务端 NameSpaceInfoPack: tgtEid(string) + tgtNqn(string) + nsUuid(string) + namespaceId(uint32) +
 * nsDevPath(string) + nsSize(uint64) + lbaFormat(uint32) + nqnCount(uint32) + [hostNqn(string)]*nqnCount
 * @param ctx 解包上下文，函数内部会推进指针位置
 * @param ns  已分配并零初始化的命名空间信息结构体指针，调用方负责其生命周期
 * @return UBS_SUCCESS 成功，其他值为错误码
 */
static ubs_error_t unpack_namespace_info(UnpackCtx *ctx, ubs_ssu_namespace_info_t *ns)
{
    ubs_error_t ret = unpackString(*ctx, ns->tgt_eid, UBS_SSU_MAX_EID_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack tgt_eid";
        return ret;
    }
    ret = unpackString(*ctx, ns->tgt_nqn, UBS_SSU_MAX_NQN_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack tgt_nqn";
        return ret;
    }
    ret = unpackString(*ctx, ns->ns_uuid, UBS_SSU_MAX_UUID_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack ns_uuid";
        return ret;
    }
    ret = unpackValue(*ctx, ns->ns_id);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack namespaceId";
        return ret;
    }
    ret = unpackString(*ctx, ns->ns_dev_path, UBS_SSU_MAX_DEV_PATH_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack nsDevPath";
        return ret;
    }
    ret = unpackValue(*ctx, ns->ns_size);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack nsSize";
        return ret;
    }
    uint32_t lba_format = 0;
    ret = unpackValue(*ctx, lba_format);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack lbaFormat";
        return ret;
    }
    ns->lba_format = static_cast<ubs_ssu_lba_format_t>(lba_format);

    // 解包 hostNqn 列表
    return unpack_host_nqn_list(ctx, ns);
}

/**
 * 释放 ubs_ssu_namespace_info_t 内部分配的 hostNqn 列表
 * @param ns 命名空间信息结构体指针
 */
static void free_namespace_info(ubs_ssu_namespace_info_t *ns)
{
    if (ns->host_nqns != nullptr) {
        for (uint32_t i = 0; i < ns->nqn_count; i++) {
            if (ns->host_nqns[i] != nullptr) {
                delete[] ns->host_nqns[i];
            }
        }
        delete[] ns->host_nqns;
        ns->host_nqns = nullptr;
    }
    ns->nqn_count = 0;
}
/**
 * 释放 hostNqn 数组(指针数组 + 每个元素字符串)
 * @param nqns   指针数组,可为 nullptr
 * @param count  数组元素个数(仅释放前 count 个非空槽位)
 */
static void free_host_nqn_array(char **nqns, uint32_t count)
{
    if (nqns == nullptr) {
        return;
    }
    for (uint32_t i = 0; i < count; i++) {
        delete[] nqns[i];
        nqns[i] = nullptr;
    }
    delete[] nqns;
}

/**
 * 分配 hostNqn 数组:char*[count] + 每个 char[UBS_SSU_MAX_NQN_LENGTH],并零初始化
 * @param count 元素个数,必须 > 0
 * @return 成功返回数组首地址,失败返回 nullptr
 */
static char **alloc_host_nqn_array(uint32_t count)
{
    if (count == 0 || count > UBS_SSU_MAX_LIST_COUNT) {
        IPC_LOG_ERROR << "Invalid hostNqn count: " << count;
        return nullptr;
    }
    char **nqns = new (std::nothrow) char *[count];
    if (nqns == nullptr) {
        return nullptr;
    }

    for (uint32_t i = 0; i < count; i++) {
        nqns[i] = new (std::nothrow) char[UBS_SSU_MAX_NQN_LENGTH];
        if (nqns[i] == nullptr) {
            IPC_LOG_ERROR << "Failed to allocate memory for hostNqn[" << i << "]";
            free_host_nqn_array(nqns, count);
            return nullptr;
        }
    }
    return nqns;
}

/**
 * 解包 hostNqn 列表
 * 对应服务端 NameSpaceInfoPack: nqnCount(uint32) + [hostNqn(string)]*nqnCount
 * 服务端打包: for (const auto &hostNqn : info.allowHostNqnList) { StringPack(hostNqn, MAX_NQN_LEN); }
 * @param ctx 解包上下文
 * @param ns  命名空间信息结构体指针，nqn_count 和 host_nqns 字段将被填充
 * @return UBS_SUCCESS 成功，其他值为错误码
 */
static ubs_error_t unpack_host_nqn_list(UnpackCtx *ctx, ubs_ssu_namespace_info_t *ns)
{
    uint32_t nqn_count = 0;
    ubs_error_t ret = unpackValue(*ctx, nqn_count);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack nqnCount";
        return ret;
    }

    // 先初始化为空状态,确保后续任何失败路径下 ns 都是可安全释放的干净状态
    ns->nqn_count = 0;
    ns->host_nqns = nullptr;
    if (nqn_count == 0) {
        return UBS_SUCCESS;
    }
    if (check_list_count(nqn_count) != UBS_SUCCESS) {
        return UBS_ERR_OUT_OF_RANGE;
    }

    // 分配与解包分离:alloc 只管内存,unpack 只管数据
    char **nqns = alloc_host_nqn_array(nqn_count);
    if (nqns == nullptr) {
        IPC_LOG_ERROR << "Failed to allocate memory for hostNqn list";
        return UBS_ERR_OUT_OF_MEMORY;
    }

    // 逐个解包;失败时统一用 free_host_nqn_array 清理,无需区分已解包/未解包
    for (uint32_t i = 0; i < nqn_count; i++) {
        ret = unpackString(*ctx, nqns[i], UBS_SSU_MAX_NQN_LENGTH - 1);
        if (ret != UBS_SUCCESS) {
            IPC_LOG_ERROR << "Failed to unpack hostNqn[" << i << "]";
            free_host_nqn_array(nqns, nqn_count);
            return ret;
        }
    }

    // 全部成功,移交所有权
    ns->host_nqns = nqns;
    ns->nqn_count = nqn_count;
    return UBS_SUCCESS;
}

/**
 * 解包 namespace 信息列表
 * 对应服务端 [NameSpaceInfo]*nsListSize
 * @param ctx     解包上下文
 * @param ns_list 成功时指向新分配的命名空间信息数组，调用方负责释放
 *                失败时为 nullptr，已分配内存已内部清理
 * @param ns_count 命名空间数量
 * @return UBS_SUCCESS 成功，其他值为错误码
 */
static ubs_error_t unpack_namespace_info_list(UnpackCtx *ctx, ubs_ssu_namespace_info_t **ns_list, uint32_t ns_count)
{
    if (ns_count == 0) {
        IPC_LOG_DEBUG << "namespace count is 0";
        *ns_list = nullptr;
        return UBS_SUCCESS;
    }
    if (check_list_count(ns_count) != UBS_SUCCESS) {
        return UBS_ERR_OUT_OF_RANGE;
    }
    *ns_list = new (std::nothrow) ubs_ssu_namespace_info_t[ns_count];
    if (*ns_list == nullptr) {
        IPC_LOG_ERROR << "Failed to allocate memory for namespace info list";
        return UBS_ERR_OUT_OF_MEMORY;
    }
    for (uint32_t i = 0; i < ns_count; i++) {
        ubs_error_t ret = unpack_namespace_info(ctx, &(*ns_list)[i]);
        if (ret != UBS_SUCCESS) {
            // 清理已解包的 namespace 信息（包括 hostNqn 列表）
            for (uint32_t j = 0; j < i; j++) {
                free_namespace_info(&(*ns_list)[j]);
            }
            delete[] *ns_list;
            *ns_list = nullptr;
            IPC_LOG_ERROR << "Failed to unpack namespace info";
            return ret;
        }
    }
    return UBS_SUCCESS;
}

/**
 * 解包单个 ubs_ssu_alloc_result_t 结构体
 * 对应服务端 AllocResultPack: name(string) + strategy(uint8) + nsListSize(uint32) + [NameSpaceInfo]*nsListSize
 * @param ctx 解包上下文
 * @param result 已分配并零初始化的分配结果结构体指针
 * @note 成功时 result->namespaces 指向新分配的数组，调用方负责释放
 *       失败时 result->namespaces 已内部清理（设为 nullptr）
 * @return UBS_SUCCESS 成功，其他值为错误码
 */
static ubs_error_t unpack_alloc_result(UnpackCtx *ctx, ubs_ssu_alloc_result_t *result)
{
    ubs_error_t ret = unpackString(*ctx, result->name, UBS_SSU_MAX_NAME_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack name";
        return ret;
    }
    uint8_t strategy = 0;
    ret = unpackValue(*ctx, strategy);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack strategy";
        return ret;
    }
    result->strategy = static_cast<ubs_ssu_alloc_strategy_t>(strategy);
    ret = unpackValue(*ctx, result->namespace_cnt);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack namespace count";
        return ret;
    }

    return unpack_namespace_info_list(ctx, &result->namespaces, result->namespace_cnt);
}

// ============================================================================
// 解包函数（公开接口）
// ============================================================================

/**
 * @brief 分配空间结果解包
 * 对应服务端 SsuAllocSpacePack: AllocResultPack(result)
 * @param buffer 输入缓冲区
 * @param result 成功时指向新分配的分配结果结构体指针，调用方负责释放
 * @return UBS_SUCCESS 成功，其他值为错误码
 */
ubs_error_t ubs_ssu_space_alloc_unpack(ubse_api_buffer_t &buffer, ubs_ssu_alloc_result_t **result)
{
    if (buffer.buffer == nullptr || buffer.length == 0) {
        IPC_LOG_ERROR << "Invalid buffer";
        return UBS_ERR_NULL_POINTER;
    }
    if (result == nullptr) {
        IPC_LOG_ERROR << "Invalid result pointer";
        return UBS_ERR_NULL_POINTER;
    }
    UnpackCtx ctx = {buffer.buffer, buffer.length};
    *result = new (std::nothrow) ubs_ssu_alloc_result_t();
    if (*result == nullptr) {
        IPC_LOG_ERROR << "Failed to allocate memory for alloc result";
        return UBS_ERR_OUT_OF_MEMORY;
    }
    ubs_error_t ret = unpack_alloc_result(&ctx, *result);
    if (ret != UBS_SUCCESS) {
        delete *result;
        *result = nullptr;
        IPC_LOG_ERROR << "Failed to unpack alloc result";
        return ret;
    }
    return UBS_SUCCESS;
}

/**
 * @brief 解包命名空间设备路径列表（内部辅助函数）
 * 对应服务端: nsDevPathsCount(uint32) + [nsDevPath(string)]*nsDevPathsCount
 * @param ctx 解包上下文
 * @param ns_dev_paths 命名空间设备路径列表，调用方负责释放
 * @param ns_dev_path_cnt 命名空间设备路径数量
 * @return UBS_SUCCESS 成功，其他值为错误码
 */
static ubs_error_t unpack_ns_dev_paths_list(UnpackCtx *ctx, char ***ns_dev_paths, uint32_t *ns_dev_path_cnt)
{
    ubs_error_t ret = unpackValue(*ctx, *ns_dev_path_cnt);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack ns_dev_path_cnt";
        return ret;
    }
    if (*ns_dev_path_cnt == 0) {
        *ns_dev_paths = nullptr;
        return UBS_SUCCESS;
    }
    if (check_list_count(*ns_dev_path_cnt) != UBS_SUCCESS) {
        return UBS_ERR_OUT_OF_RANGE;
    }
    char **paths = new (std::nothrow) char *[*ns_dev_path_cnt];
    if (paths == nullptr) {
        return UBS_ERR_OUT_OF_MEMORY;
    }
    for (uint32_t i = 0; i < *ns_dev_path_cnt; i++) {
        paths[i] = new (std::nothrow) char[UBS_SSU_MAX_DEV_PATH_LENGTH];
        if (paths[i] == nullptr) {
            for (uint32_t j = 0; j < i; j++) {
                delete[] paths[j];
            }
            delete[] paths;
            return UBS_ERR_OUT_OF_MEMORY;
        }
        ret = unpackString(*ctx, paths[i], UBS_SSU_MAX_DEV_PATH_LENGTH - 1);
        if (ret != UBS_SUCCESS) {
            IPC_LOG_ERROR << "Failed to unpack ns_dev_paths[" << i << "]";
            for (uint32_t j = 0; j <= i; j++) {
                delete[] paths[j];
            }
            delete[] paths;
            return ret;
        }
    }
    *ns_dev_paths = paths;
    return UBS_SUCCESS;
}

/**
 * @brief 挂载空间结果解包
 * 对应服务端 SsuAttachSpacePack: nsDevPathsCount(uint32) + [nsDevPath(string)]*nsDevPathsCount
 * @param buffer 输入缓冲区
 * @param ns_dev_paths 命名空间设备路径列表，调用方负责释放
 * @param ns_dev_path_cnt 命名空间设备路径数量
 * @return UBS_SUCCESS 成功，其他值为错误码
 */
ubs_error_t ubs_ssu_space_attach_unpack(ubse_api_buffer_t &buffer, char ***ns_dev_paths,
                                        uint32_t *ns_dev_path_cnt)
{
    if (buffer.buffer == nullptr || buffer.length == 0) {
        IPC_LOG_ERROR << "Invalid buffer";
        return UBS_ERR_NULL_POINTER;
    }
    if (ns_dev_paths == nullptr || ns_dev_path_cnt == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    UnpackCtx ctx = {buffer.buffer, buffer.length};
    return unpack_ns_dev_paths_list(&ctx, ns_dev_paths, ns_dev_path_cnt);
}

/**
 * @brief 线性挂载空间结果解包
 * 对应服务端 SsuAttachLinearSpacePack: nsDevPathsCount(uint32) + [nsDevPath(string)]*nsDevPathsCount + devPath(string)
 * @param buffer 输入缓冲区
 * @param ns_dev_paths 命名空间设备路径列表，调用方负责释放
 * @param ns_dev_path_cnt 命名空间设备路径数量
 * @param dev_path 挂载路径，调用方需分配不小于UBS_SSU_MAX_DEV_PATH_LENGTH字节的缓冲区
 * @return UBS_SUCCESS 成功，其他值为错误码
 */
ubs_error_t ubs_ssu_linear_space_attach_unpack(ubse_api_buffer_t &buffer, char ***ns_dev_paths,
                                               uint32_t *ns_dev_path_cnt, char *dev_path)
{
    if (buffer.buffer == nullptr || buffer.length == 0) {
        IPC_LOG_ERROR << "Invalid buffer";
        return UBS_ERR_NULL_POINTER;
    }
    if (ns_dev_paths == nullptr || ns_dev_path_cnt == nullptr || dev_path == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    UnpackCtx ctx = {buffer.buffer, buffer.length};
    ubs_error_t ret = unpack_ns_dev_paths_list(&ctx, ns_dev_paths, ns_dev_path_cnt);
    if (ret != UBS_SUCCESS) {
        return ret;
    }
    return unpackString(ctx, dev_path, UBS_SSU_MAX_DEV_PATH_LENGTH - 1);
}

/**
 * @brief 条带化挂载空间结果解包
 * 对应服务端 SsuAttachStripedSpacePack: nsDevPathsCount(uint32) + [nsDevPath(string)]*nsDevPathsCount + devPath(string)
 * @param buffer 输入缓冲区
 * @param ns_dev_paths 命名空间设备路径列表，调用方负责释放
 * @param ns_dev_path_cnt 命名空间设备路径数量
 * @param dev_path 挂载路径，调用方需分配不小于UBS_SSU_MAX_DEV_PATH_LENGTH字节的缓冲区
 * @return UBS_SUCCESS 成功，其他值为错误码
 */
ubs_error_t ubs_ssu_striped_space_attach_unpack(ubse_api_buffer_t &buffer, char ***ns_dev_paths,
                                                uint32_t *ns_dev_path_cnt, char *dev_path)
{
    return ubs_ssu_linear_space_attach_unpack(buffer, ns_dev_paths, ns_dev_path_cnt, dev_path);
}

/**
 * 解包单个 ubs_ssu_ns_stats_t 结构体
 * 对应服务端: nsUuid(string) + nsId(uint32) + totalSize(uint64) + usedSize(uint64)
 * @param ctx 解包上下文
 * @param ns  已分配并零初始化的 ns_stats 结构体指针
 * @return UBS_SUCCESS 成功，其他值为错误码
 */
static ubs_error_t unpack_ns_stats(UnpackCtx *ctx, ubs_ssu_ns_stats_t *ns)
{
    ubs_error_t ret = unpackString(*ctx, ns->ns_uuid, UBS_SSU_MAX_UUID_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack nsUuid";
        return ret;
    }
    ret = unpackValue(*ctx, ns->ns_id);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack nsId";
        return ret;
    }
    ret = unpackValue(*ctx, ns->total_size);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack totalSize";
        return ret;
    }
    ret = unpackValue(*ctx, ns->used_size);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack usedSize";
        return ret;
    }
    return UBS_SUCCESS;
}

/**
 * 解包 ns_stats 列表
 * 对应服务端: listSize(uint32) + [NsStats]*listSize
 * @param ctx          解包上下文
 * @param ns_stats_list 成功时指向新分配的 ns_stats 数组，调用方负责释放
 *                      失败时为 nullptr，已分配内存已内部清理
 * @param ns_stats_cnt  命名空间统计数量
 * @return UBS_SUCCESS 成功，其他值为错误码
 */
static ubs_error_t unpack_ns_stats_list(UnpackCtx *ctx, ubs_ssu_ns_stats_t **ns_stats_list, uint32_t *ns_stats_cnt)
{
    uint32_t list_size = 0;
    ubs_error_t ret = unpackValue(*ctx, list_size);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack listSize";
        return ret;
    }
    *ns_stats_cnt = list_size;
    if (list_size == 0) {
        *ns_stats_list = nullptr;
        return UBS_SUCCESS;
    }
    if (check_list_count(list_size) != UBS_SUCCESS) {
        return UBS_ERR_OUT_OF_RANGE;
    }
    *ns_stats_list = new (std::nothrow) ubs_ssu_ns_stats_t[list_size];
    if (*ns_stats_list == nullptr) {
        IPC_LOG_ERROR << "Failed to allocate memory for ns_stats_list";
        return UBS_ERR_OUT_OF_MEMORY;
    }
    for (uint32_t i = 0; i < list_size; i++) {
        ret = unpack_ns_stats(ctx, &(*ns_stats_list)[i]);
        if (ret != UBS_SUCCESS) {
            delete[] *ns_stats_list;
            *ns_stats_list = nullptr;
            *ns_stats_cnt = 0;
            IPC_LOG_ERROR << "Failed to unpack NsStats";
            return ret;
        }
    }
    return UBS_SUCCESS;
}

/**
 * 解包命名空间状态信息结果
 * @param buffer  输入缓冲区
 * @param ns_stats_list 成功时指向新分配的 ns_stats 数组，调用方负责释放
 *                      失败时为 nullptr，已分配内存已内部清理
 * @param ns_stats_cnt  命名空间统计数量
 * @return UBS_SUCCESS 成功，其他值为错误码
 */
ubs_error_t ubs_ssu_ns_stats_get_unpack(ubse_api_buffer_t &buffer, ubs_ssu_ns_stats_t **ns_stats_list,
                                        uint32_t *ns_stats_cnt)
{
    if (buffer.buffer == nullptr || buffer.length == 0) {
        IPC_LOG_ERROR << "Invalid buffer";
        return UBS_ERR_NULL_POINTER;
    }
    if (ns_stats_list == nullptr || ns_stats_cnt == nullptr) {
        IPC_LOG_ERROR << "Invalid ns_stats_list or ns_stats_cnt";
        return UBS_ERR_NULL_POINTER;
    }
    UnpackCtx ctx = {buffer.buffer, buffer.length};
    return unpack_ns_stats_list(&ctx, ns_stats_list, ns_stats_cnt);
}

/**
 * 解包单个 ubs_ssu_connect_info_t 结构体
 * 对应服务端: srcEid(string) + tgtEid(string) + tgtNqn(string) + hostNqn(string) + nsUuid(string) + nsId(uint32)
 * @param ctx  解包上下文
 * @param info 已分配并零初始化的 connect_info 结构体指针
 * @return UBS_SUCCESS 成功，其他值为错误码
 */
static ubs_error_t unpack_connect_info(UnpackCtx *ctx, ubs_ssu_connect_info_t *info)
{
    ubs_error_t ret = unpackString(*ctx, info->src_eid, UBS_SSU_MAX_EID_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack srcEid";
        return ret;
    }
    ret = unpackString(*ctx, info->tgt_eid, UBS_SSU_MAX_EID_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack tgtEid";
        return ret;
    }
    ret = unpackString(*ctx, info->tgt_nqn, UBS_SSU_MAX_NQN_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack tgtNqn";
        return ret;
    }
    ret = unpackString(*ctx, info->host_nqn, UBS_SSU_MAX_NQN_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack hostNqn";
        return ret;
    }
    ret = unpackString(*ctx, info->ns_uuid, UBS_SSU_MAX_UUID_LENGTH - 1);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack nsUuid";
        return ret;
    }
    ret = unpackValue(*ctx, info->ns_id);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack nsId";
        return ret;
    }
    return UBS_SUCCESS;
}

/**
 * 解包 connect_info 列表
 * 对应服务端: listSize(uint32) + [ConnectInfo]*listSize
 * @param ctx               解包上下文
 * @param connect_info_list 成功时指向新分配的 connect_info 数组，调用方负责释放
 *                          失败时为 nullptr，已分配内存已内部清理
 * @param connect_info_cnt  连接信息数量
 * @return UBS_SUCCESS 成功，其他值为错误码
 */
static ubs_error_t unpack_connect_info_list(UnpackCtx *ctx, ubs_ssu_connect_info_t **connect_info_list,
                                            uint32_t *connect_info_cnt)
{
    uint32_t list_size = 0;
    ubs_error_t ret = unpackValue(*ctx, list_size);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack listSize";
        return ret;
    }
    *connect_info_cnt = list_size;
    if (list_size == 0) {
        *connect_info_list = nullptr;
        IPC_LOG_DEBUG << "connectInfoListSize is 0";
        return UBS_SUCCESS;
    }
    if (check_list_count(list_size) != UBS_SUCCESS) {
        return UBS_ERR_OUT_OF_RANGE;
    }
    *connect_info_list = new (std::nothrow) ubs_ssu_connect_info_t[list_size];
    if (*connect_info_list == nullptr) {
        IPC_LOG_ERROR << "Failed to allocate memory for connect_info_list";
        return UBS_ERR_OUT_OF_MEMORY;
    }
    for (uint32_t i = 0; i < list_size; i++) {
        ret = unpack_connect_info(ctx, &(*connect_info_list)[i]);
        if (ret != UBS_SUCCESS) {
            delete[] *connect_info_list;
            *connect_info_list = nullptr;
            *connect_info_cnt = 0;
            IPC_LOG_ERROR << "Failed to unpack ConnectInfo";
            return ret;
        }
    }
    return UBS_SUCCESS;
}
/**
 * @brief 获取连接信息结果解包
 * @param buffer 输入缓冲区
 * @param connect_info_list 成功时指向新分配的 connect_info 数组，调用方负责释放
 *                          失败时为 nullptr，已分配内存已内部清理
 * @param connect_info_cnt  连接信息数量
 * @return UBS_SUCCESS 成功，其他值为错误码
 */
ubs_error_t ubs_ssu_connect_info_get_unpack(ubse_api_buffer_t &buffer, ubs_ssu_connect_info_t **connect_info_list,
                                            uint32_t *connect_info_cnt)
{
    if (buffer.buffer == nullptr || buffer.length == 0) {
        IPC_LOG_ERROR << "Invalid buffer";
        return UBS_ERR_NULL_POINTER;
    }
    if (connect_info_list == nullptr || connect_info_cnt == nullptr) {
        IPC_LOG_ERROR << "Invalid connect_info_list or connect_info_cnt";
        return UBS_ERR_NULL_POINTER;
    }
    UnpackCtx ctx = {buffer.buffer, buffer.length};
    return unpack_connect_info_list(&ctx, connect_info_list, connect_info_cnt);
}

/**
 * 释放单个 ubs_ssu_alloc_result_t 内部分配的 namespaces 列表
 * @param r 分配结果结构体指针,r->namespaces 与 r->namespace_cnt 将被清零
 */
static void free_alloc_result(ubs_ssu_alloc_result_t *r)
{
    if (r->namespaces != nullptr) {
        for (uint32_t i = 0; i < r->namespace_cnt; i++) {
            free_namespace_info(&r->namespaces[i]);
        }
        delete[] r->namespaces;
        r->namespaces = nullptr;
    }
    r->namespace_cnt = 0;
}

/**
 * 释放 ubs_ssu_alloc_result_t 数组及其每个元素内部分配的 namespaces 列表
 * @param results 待释放的数组指针,可为 nullptr
 * @param count   数组元素个数
 */
static void free_alloc_result_list(ubs_ssu_alloc_result_t *results, uint32_t count)
{
    if (results == nullptr) {
        return;
    }
    for (uint32_t i = 0; i < count; i++) {
        free_alloc_result(&results[i]);
    }
    delete[] results;
}

/**
 * @brief 已分配空间信息列表解包
 * 对应服务端 SsuAllocInfoListPack: allocResultListSize(uint32) + [AllocResultPack]*allocResultListSize
 * @param buffer 输入缓冲区
 * @param results 成功时指向新分配的分配结果数组，调用方负责释放
 * @param result_cnt 分配结果数量
 * @return UBS_SUCCESS 成功，其他值为错误码
 */
ubs_error_t ubs_ssu_alloc_info_list_unpack(ubse_api_buffer_t &buffer, ubs_ssu_alloc_result_t **results,
                                           uint32_t *result_cnt)
{
    if (buffer.buffer == nullptr || buffer.length == 0) {
        IPC_LOG_ERROR << "Invalid buffer";
        return UBS_ERR_NULL_POINTER;
    }
    if (results == nullptr || result_cnt == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    UnpackCtx ctx = {buffer.buffer, buffer.length};
    uint32_t list_size = 0;
    ubs_error_t ret = unpackValue(ctx, list_size);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "unpack failed,attr: allocResultListSize ret = " << ret;
        return ret;
    }
    *result_cnt = list_size;
    if (list_size == 0) {
        IPC_LOG_DEBUG << "No allocated resources";
        *results = nullptr;
        return UBS_SUCCESS;
    }
    if (check_list_count(list_size) != UBS_SUCCESS) {
        return UBS_ERR_OUT_OF_RANGE;
    }

    *results = new (std::nothrow) ubs_ssu_alloc_result_t[list_size];
    if (*results == nullptr) {
        return UBS_ERR_OUT_OF_MEMORY;
    }
    for (uint32_t i = 0; i < list_size; i++) {
        ret = unpack_alloc_result(&ctx, &(*results)[i]);
        if (ret != UBS_SUCCESS) {
            IPC_LOG_ERROR << "unpack failed,attr: allocResultPack, index = " << i << ", ret = " << ret;
            free_alloc_result_list(*results, i);
            *results = nullptr;
            *result_cnt = 0;
            return ret;
        }
    }
    return UBS_SUCCESS;
}

/**
 * 解包单个 ubs_ub_vfe_t 结构体
 * 对应服务端 VfePack: slotId(uint8) + chipId(uint8) + dieId(uint8) + pfeId(uint16)
 *                     + vfeId(uint16) + vfeGuid(string) + bindBusInstanceGuid(string)
 * @param ctx 解包上下文
 * @param vfe 已分配并零初始化的 VFE 结构体指针
 * @return UBS_SUCCESS 成功，其他值为错误码
 */
static ubs_error_t unpack_vfe(UnpackCtx *ctx, ubs_ub_vfe_t *vfe)
{
    ubs_error_t ret = unpackValue(*ctx, vfe->slot_id);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack vfe slotId";
        return ret;
    }
    ret = unpackValue(*ctx, vfe->chip_id);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack vfe chipId";
        return ret;
    }
    ret = unpackValue(*ctx, vfe->die_id);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack vfe dieId";
        return ret;
    }
    ret = unpackValue(*ctx, vfe->pfe_id);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack vfe pfeId";
        return ret;
    }
    ret = unpackValue(*ctx, vfe->vfe_id);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack vfe vfeId";
        return ret;
    }
    ret = unpackArray(*ctx, vfe->vfe_guid, UBS_SSU_GUID_LENGTH);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack vfe vfeGuid";
        return ret;
    }
    ret = unpackArray(*ctx, vfe->bind_bus_instance_guid, UBS_SSU_GUID_LENGTH);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack vfe bindBusInstanceGuid";
        return ret;
    }
    return UBS_SUCCESS;
}

/**
 * 解包 VFE 列表
 * 对应服务端: vfeCount(uint32) + [VfePack]*vfeCount
 * @param ctx      解包上下文
 * @param vfe_list 成功时指向新分配的 VFE 数组，调用方负责释放
 *                 失败时为 nullptr，已分配内存已内部清理
 * @param vfe_count VFE 数量
 * @return UBS_SUCCESS 成功，其他值为错误码
 */
static ubs_error_t unpack_vfe_list(UnpackCtx *ctx, ubs_ub_vfe_t **vfe_list, uint32_t vfe_count)
{
    if (vfe_count == 0) {
        *vfe_list = nullptr;
        return UBS_SUCCESS;
    }
    if (check_list_count(vfe_count) != UBS_SUCCESS) {
        return UBS_ERR_OUT_OF_RANGE;
    }
    *vfe_list = new (std::nothrow) ubs_ub_vfe_t[vfe_count];
    if (*vfe_list == nullptr) {
        IPC_LOG_ERROR << "Failed to allocate memory for vfe list";
        return UBS_ERR_OUT_OF_MEMORY;
    }
    for (uint32_t i = 0; i < vfe_count; i++) {
        ubs_error_t ret = unpack_vfe(ctx, &(*vfe_list)[i]);
        if (ret != UBS_SUCCESS) {
            delete[] *vfe_list;
            *vfe_list = nullptr;
            return ret;
        }
    }
    return UBS_SUCCESS;
}

/**
 * 解包单个 ubs_ub_fe_t 结构体
 * 对应服务端 FePack: slotId(uint8) + chipId(uint8) + dieId(uint8) + pfeId(uint16)
 *                   + pfeGuid(string) + vfeCount(uint32) + [VfePack]*vfeCount
 * @param ctx 解包上下文
 * @param fe  已分配并零初始化的 FE 结构体指针
 *            失败时 fe->vfe_list 已内部清理（设为 nullptr）
 * @return UBS_SUCCESS 成功，其他值为错误码
 */
static ubs_error_t unpack_fe_device(UnpackCtx *ctx, ubs_ub_fe_t *fe)
{
    ubs_error_t ret = unpackValue(*ctx, fe->slot_id);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack fe slotId";
        return ret;
    }
    ret = unpackValue(*ctx, fe->chip_id);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack fe chipId";
        return ret;
    }
    ret = unpackValue(*ctx, fe->die_id);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack fe dieId";
        return ret;
    }
    ret = unpackValue(*ctx, fe->pfe_id);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack fe pfeId";
        return ret;
    }
    ret = unpackArray(*ctx, fe->pfe_guid, UBS_SSU_GUID_LENGTH);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack fe pfeGuid";
        return ret;
    }
    uint32_t vfe_count = 0;
    ret = unpackValue(*ctx, vfe_count);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack vfeCount";
        return ret;
    }
    if (vfe_count > UBS_SSU_MAX_LIST_COUNT) {
        IPC_LOG_ERROR << "List count too large: " << vfe_count << ", max: " << UBS_SSU_MAX_LIST_COUNT;
        return UBS_ERR_OUT_OF_RANGE;
    }
    fe->vfe_cnt = vfe_count;

    ret = unpack_vfe_list(ctx, &fe->vfe_list, vfe_count);
    if (ret != UBS_SUCCESS) {
        fe->vfe_cnt = 0;
        return ret;
    }
    return UBS_SUCCESS;
}

/**
 * 解包 FE 设备列表
 * 对应服务端: feListSize(uint32) + [FePack]*feListSize
 * @param ctx     解包上下文
 * @param fe_list 成功时指向新分配的 FE 数组，调用方负责释放
 *                失败时为 nullptr，已分配内存（包括 VFE 列表）已内部清理
 * @param fe_cnt  FE 设备数量
 * @return UBS_SUCCESS 成功，其他值为错误码
 */
static ubs_error_t unpack_fe_device_list(UnpackCtx *ctx, ubs_ub_fe_t **fe_list, uint32_t *fe_cnt)
{
    uint32_t list_size = 0;
    ubs_error_t ret = unpackValue(*ctx, list_size);
    if (ret != UBS_SUCCESS) {
        return ret;
    }
    *fe_cnt = list_size;
    if (list_size == 0) {
        *fe_list = nullptr;
        return UBS_SUCCESS;
    }
    if (check_list_count(list_size) != UBS_SUCCESS) {
        return UBS_ERR_OUT_OF_RANGE;
    }
    *fe_list = new (std::nothrow) ubs_ub_fe_t[list_size];
    if (*fe_list == nullptr) {
        return UBS_ERR_OUT_OF_MEMORY;
    }
    for (uint32_t i = 0; i < list_size; i++) {
        ret = unpack_fe_device(ctx, &(*fe_list)[i]);
        if (ret != UBS_SUCCESS) {
            // 清理已解包的 FE 设备（包括 VFE 列表）
            for (uint32_t j = 0; j < i; j++) {
                if ((*fe_list)[j].vfe_list != nullptr) {
                    delete[] (*fe_list)[j].vfe_list;
                }
            }
            delete[] *fe_list;
            *fe_list = nullptr;
            *fe_cnt = 0;
            return ret;
        }
    }
    return UBS_SUCCESS;
}

ubs_error_t ubs_ssu_fe_device_list_unpack(ubse_api_buffer_t &buffer, ubs_ub_fe_t **fe_list, uint32_t *fe_cnt)
{
    if (buffer.buffer == nullptr || buffer.length == 0) {
        IPC_LOG_ERROR << "Invalid buffer";
        return UBS_ERR_NULL_POINTER;
    }
    if (fe_list == nullptr || fe_cnt == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    UnpackCtx ctx = {buffer.buffer, buffer.length};
    return unpack_fe_device_list(&ctx, fe_list, fe_cnt);
}

/**
 * @brief FE设备分配结果解包
 * 对应服务端 SsuFeDeviceAllocPack: busInstanceGuid(string)
 * @param buffer 输入缓冲区
 * @param bus_instance_guid [OUT] 分配的总线实例 GUID, 定长字节数组, 调用方需保证缓冲区至少 UBS_SSU_GUID_LENGTH 字节
 * @return UBS_SUCCESS 成功，其他值为错误码
 */
ubs_error_t ubs_ssu_fe_device_alloc_unpack(ubse_api_buffer_t &buffer, uint8_t *bus_instance_guid)
{
    if (buffer.buffer == nullptr || buffer.length == 0) {
        IPC_LOG_ERROR << "Invalid buffer";
        return UBS_ERR_NULL_POINTER;
    }
    if (bus_instance_guid == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    UnpackCtx ctx = {buffer.buffer, buffer.length};
    ubs_error_t ret = unpackArray(ctx, bus_instance_guid, UBS_SSU_GUID_LENGTH);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack busInstanceGuid";
        return ret;
    }
    return UBS_SUCCESS;
}

} // namespace ubs::ssu
