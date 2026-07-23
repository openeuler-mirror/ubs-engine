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
#include "ubs_engine_pack_util.h"
#include "libubse_helper.h"

namespace ubs::sdk {
ubs_error_t packString(PackCtx &ctx, const char *str, uint32_t max_len)
{
    size_t strLen = str ? strlen(str) : 0;
    if (strLen > max_len) {
        return UBS_ERR_OUT_OF_RANGE;
    }
    uint32_t len = static_cast<uint32_t>(strLen);
    ubs_error_t ret = packValue(ctx, len);
    if (ret != UBS_SUCCESS) {
        return ret;
    }
    if (len > 0) {
        if (ctx.ptr == nullptr || ctx.end == nullptr || ctx.ptr + len > ctx.end) {
            return UBS_ERR_BUFFER_TOO_SMALL;
        }
        errno_t cpRet = memcpy_s(ctx.ptr, len, str, len);
        if (cpRet != EOK) {
            return ubse_map_sys_error(cpRet);
        }
        ctx.ptr += len;
    }
    return UBS_SUCCESS;
}
ubs_error_t unpackString(UnpackCtx &ctx, char *str, size_t maxLen)
{
    uint32_t len;
    ubs_error_t ret = unpackValue(ctx, len);
    if (ret != UBS_SUCCESS) {
        return ret;
    }
    if (len >= maxLen) { // 需保留 1 字节放 '\0'
        return UBS_ERR_OUT_OF_RANGE;
    }
    if (len > ctx.remaining) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    if (len > 0) {
        errno_t res = memcpy_s(str, maxLen, ctx.ptr, len);
        if (res != EOK) {
            return ubse_map_sys_error(res);
        }
    }
    str[len] = '\0';
    ctx.ptr += len;
    ctx.remaining -= len;
    return UBS_SUCCESS;
}
} // namespace ubs::sdk