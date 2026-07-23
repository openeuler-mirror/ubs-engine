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

#include "ubs_ssu_validate.h"

#include <cctype>
#include <cstring>

#include "ubs_engine_ssu.h"
#include "ubs_error.h"
#include "ubse_ipc_log.h"

namespace ubs {
namespace ssu {

// 检查name是否符合规范: 1-47个字符(缓冲区容量48字节, 含'\0')
uint32_t ubs_ssu_name_is_valid(const char *name)
{
    if (name == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    size_t len = strlen(name);
    if (len == 0 || len >= UBS_SSU_MAX_NAME_LENGTH) {
        return UBS_ERR_OUT_OF_RANGE;
    }
    return UBS_SUCCESS;
}

// 检查dev_name是否符合规范: 1-32个字符(缓冲区容量33字节, 含'\0')
uint32_t ubs_ssu_dev_name_is_valid(const char *dev_name)
{
    if (dev_name == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    size_t len = strlen(dev_name);
    if (len == 0 || len >= UBS_SSU_MAX_DEV_NAME_LENGTH) {
        return UBS_ERR_OUT_OF_RANGE;
    }
    return UBS_SUCCESS;
}

// 检查nqn是否符合规范: 1-68个字符(缓冲区容量69字节, 含'\0')
uint32_t ubs_ssu_nqn_is_valid(const char *nqn)
{
    if (nqn == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    size_t len = strlen(nqn);
    if (len == 0 || len >= UBS_SSU_MAX_NQN_LENGTH) {
        return UBS_ERR_OUT_OF_RANGE;
    }
    return UBS_SUCCESS;
}

// 检查eid是否符合规范: 1-16个字符(缓冲区容量17字节, 含'\0')
uint32_t ubs_ssu_eid_is_valid(const char *eid)
{
    if (eid == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    size_t len = strlen(eid);
    if (len == 0 || len >= UBS_SSU_MAX_EID_LENGTH) {
        return UBS_ERR_OUT_OF_RANGE;
    }
    return UBS_SUCCESS;
}

// 检查uuid是否符合规范: 1-36个字符(缓冲区容量37字节, 含'\0')
uint32_t ubs_ssu_uuid_is_valid(const char *uuid)
{
    if (uuid == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    size_t len = strlen(uuid);
    if (len == 0 || len >= UBS_SSU_MAX_UUID_LENGTH) {
        return UBS_ERR_OUT_OF_RANGE;
    }
    return UBS_SUCCESS;
}

// 检查tenant是否符合规范: 1-16个字符(缓冲区容量17字节, 含'\0')
uint32_t ubs_ssu_tenant_is_valid(const char *tenant)
{
    if (tenant == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    size_t len = strlen(tenant);
    if (len == 0 || len >= UBS_SSU_MAX_TENANT_LENGTH) {
        return UBS_ERR_OUT_OF_RANGE;
    }
    return UBS_SUCCESS;
}

// 校验分配空间请求参数
uint32_t ubs_ssu_alloc_space_req_validate(const ubs_ssu_alloc_space_req_t *req)
{
    if (req == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }

    uint32_t ret = ubs_ssu_name_is_valid(req->name);
    if (ret != UBS_SUCCESS) {
        return ret;
    }

    if (req->ns_size == 0) {
        return UBS_ERR_INVALID_ARG;
    }

    if (req->ns_num == 0) {
        return UBS_ERR_INVALID_ARG;
    }

    if (req->lba_format != UBS_SSU_LBA_FORMAT_512 && req->lba_format != UBS_SSU_LBA_FORMAT_4K) {
        return UBS_ERR_INVALID_ARG;
    }

    if (req->strategy != UBS_SSU_ALLOC_STRATEGY_STRIPED && req->strategy != UBS_SSU_ALLOC_STRATEGY_LINEAR) {
        return UBS_ERR_INVALID_ARG;
    }
    return UBS_SUCCESS;
}

// 校验空间操作请求参数
uint32_t ubs_ssu_space_req_validate(const ubs_ssu_space_req_t *req)
{
    if (req == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }

    uint32_t ret = ubs_ssu_name_is_valid(req->name);
    if (ret != UBS_SUCCESS) {
        return ret;
    }

    ret = ubs_ssu_nqn_is_valid(req->nqn);
    if (ret != UBS_SUCCESS) {
        return ret;
    }

    ret = ubs_ssu_eid_is_valid(req->src_eid);
    if (ret != UBS_SUCCESS) {
        return ret;
    }

    return UBS_SUCCESS;
}

// 校验线性空间操作请求参数
uint32_t ubs_ssu_linear_space_req_validate(const ubs_ssu_linear_space_req_t *req)
{
    if (req == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }

    uint32_t ret = ubs_ssu_name_is_valid(req->name);
    if (ret != UBS_SUCCESS) {
        return ret;
    }

    ret = ubs_ssu_nqn_is_valid(req->nqn);
    if (ret != UBS_SUCCESS) {
        return ret;
    }

    ret = ubs_ssu_eid_is_valid(req->src_eid);
    if (ret != UBS_SUCCESS) {
        return ret;
    }

    ret = ubs_ssu_dev_name_is_valid(req->dev_name);
    if (ret != UBS_SUCCESS) {
        return ret;
    }

    return UBS_SUCCESS;
}

// 校验条带化空间操作请求参数
uint32_t ubs_ssu_striped_space_req_validate(const ubs_ssu_striped_space_req_t *req)
{
    if (req == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }

    uint32_t ret = ubs_ssu_name_is_valid(req->name);
    if (ret != UBS_SUCCESS) {
        return ret;
    }

    ret = ubs_ssu_nqn_is_valid(req->nqn);
    if (ret != UBS_SUCCESS) {
        return ret;
    }

    ret = ubs_ssu_eid_is_valid(req->src_eid);
    if (ret != UBS_SUCCESS) {
        return ret;
    }

    ret = ubs_ssu_dev_name_is_valid(req->dev_name);
    if (ret != UBS_SUCCESS) {
        return ret;
    }

    if (req->level != UBS_SSU_RAID0 && req->level != UBS_SSU_RAID5) {
        return UBS_ERR_INVALID_ARG;
    }

    switch (req->chunk_size) {
        case UBS_SSU_CHUNK_SIZE_4K:
        case UBS_SSU_CHUNK_SIZE_16K:
        case UBS_SSU_CHUNK_SIZE_32K:
        case UBS_SSU_CHUNK_SIZE_64K:
        case UBS_SSU_CHUNK_SIZE_128K:
        case UBS_SSU_CHUNK_SIZE_256K:
        case UBS_SSU_CHUNK_SIZE_512K:
            break;
        default:
            return UBS_ERR_INVALID_ARG;
    }

    return UBS_SUCCESS;
}

// 校验VFE参数
uint32_t ubs_ssu_vfe_is_valid(const ubs_ub_vfe_t *vfe)
{
    if (vfe == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    return UBS_SUCCESS;
}

// 校验UPI参数
uint32_t ubs_ssu_upi_is_valid(const char *upi)
{
    if (upi == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    size_t len = strlen(upi);
    if (len == 0 || len >= UBS_SSU_MAX_TENANT_LENGTH) {
        return UBS_ERR_OUT_OF_RANGE;
    }
    return UBS_SUCCESS;
}

// 校验用户名参数
uint32_t ubs_ssu_user_name_is_valid(const char *user_name)
{
    if (user_name == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    size_t len = strlen(user_name);
    if (len == 0 || len >= UBS_SSU_MAX_USER_NAME_LENGTH) {
        return UBS_ERR_OUT_OF_RANGE;
    }
    return UBS_SUCCESS;
}

// 校验访问权限添加请求参数
uint32_t ubs_ssu_access_permission_add_req_validate(const char *name, const char *nqn)
{
    uint32_t ret = ubs_ssu_name_is_valid(name);
    if (ret != UBS_SUCCESS) {
        return ret;
    }
    return ubs_ssu_nqn_is_valid(nqn);
}

// 校验访问权限移除请求参数
uint32_t ubs_ssu_access_permission_remove_req_validate(const char *name, const char *nqn)
{
    uint32_t ret = ubs_ssu_name_is_valid(name);
    if (ret != UBS_SUCCESS) {
        return ret;
    }
    return ubs_ssu_nqn_is_valid(nqn);
}

// 校验空间挂载请求参数（委托通用空间校验）
uint32_t ubs_ssu_space_attach_req_validate(const ubs_ssu_space_req_t *req)
{
    return ubs_ssu_space_req_validate(req);
}

// 校验空间卸载请求参数（委托通用空间校验）
uint32_t ubs_ssu_space_detach_req_validate(const ubs_ssu_space_req_t *req)
{
    return ubs_ssu_space_req_validate(req);
}

// 校验线性空间挂载请求参数（委托通用线性空间校验）
uint32_t ubs_ssu_linear_space_attach_req_validate(const ubs_ssu_linear_space_req_t *req)
{
    return ubs_ssu_linear_space_req_validate(req);
}

// 校验线性空间卸载请求参数（委托通用线性空间校验）
uint32_t ubs_ssu_linear_space_detach_req_validate(const ubs_ssu_linear_space_req_t *req)
{
    return ubs_ssu_linear_space_req_validate(req);
}

// 校验条带化空间挂载请求参数（委托通用条带化空间校验）
uint32_t ubs_ssu_striped_space_attach_req_validate(const ubs_ssu_striped_space_req_t *req)
{
    return ubs_ssu_striped_space_req_validate(req);
}

// 校验条带化空间卸载请求参数（委托通用条带化空间校验）
uint32_t ubs_ssu_striped_space_detach_req_validate(const ubs_ssu_striped_space_req_t *req)
{
    return ubs_ssu_striped_space_req_validate(req);
}

// 校验FE设备分配请求参数
uint32_t ubs_ssu_fe_device_alloc_validate(uint32_t upi,const ubs_ub_vfe_t *vfe)
{
    if (upi == 0) {
        return UBS_ERR_INVALID_ARG;
    }
    return ubs_ssu_vfe_is_valid(vfe);
}

// 校验FE设备释放请求参数
uint32_t ubs_ssu_fe_device_free_validate(uint32_t upi,const ubs_ub_vfe_t *vfe)
{
    if (upi == 0) {
        return UBS_ERR_INVALID_ARG;
    }
    return ubs_ssu_vfe_is_valid(vfe);
}

}  // namespace ssu
}  // namespace ubs