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

#ifndef UBS_ENGINE_UBS_ENGINE_PACK_UTIL_H
#define UBS_ENGINE_UBS_ENGINE_PACK_UTIL_H
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include <securec.h>

#include "ubs_error.h"

namespace ubs::sdk {

// 定义解包上下文结构体
struct UnpackCtx {
    const uint8_t *ptr;
    size_t remaining;
};

struct PackCtx {
    uint8_t *ptr;
    uint8_t *end; // 缓冲区末尾指针, 用于边界校验
};

ubs_error_t packString(PackCtx &ctx, const char *str, uint32_t max_len);
ubs_error_t unpackString(UnpackCtx &ctx, char *str, size_t maxLen);
template <typename T>
ubs_error_t unpackValue(UnpackCtx &ctx, T &value)
{
    static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");
    if (ctx.remaining < sizeof(T)) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    auto ret = memcpy_s(&value, sizeof(T), ctx.ptr, sizeof(T));
    if (ret != EOK) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    ctx.ptr += sizeof(T);
    ctx.remaining -= sizeof(T);
    return UBS_SUCCESS;
}

template <typename T>
ubs_error_t unpackArray(UnpackCtx &ctx, T *arr, size_t count)
{
    static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");
    const size_t totalSize = sizeof(T) * count;
    if (ctx.remaining < totalSize) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    if (ctx.ptr == nullptr || arr == nullptr)
        return UBS_ERR_NULL_POINTER;
    auto ret = memcpy_s(arr, totalSize, ctx.ptr, totalSize);
    if (ret != EOK) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    ctx.ptr += totalSize;
    ctx.remaining -= totalSize;
    return UBS_SUCCESS;
}

template <typename T>
ubs_error_t packValue(PackCtx &ctx, const T &value)
{
    static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");
    if (ctx.ptr == nullptr || ctx.end == nullptr || ctx.ptr + sizeof(T) > ctx.end) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    auto ret = memcpy_s(ctx.ptr, sizeof(T), &value, sizeof(T));
    if (ret != EOK) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    ctx.ptr += sizeof(T);
    return UBS_SUCCESS;
}

template <typename T>
ubs_error_t packArray(PackCtx &ctx, const T *arr, size_t count)
{
    static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");
    if (arr == nullptr)
        return UBS_ERR_NULL_POINTER;
    const size_t totalSize = sizeof(T) * count;
    if (ctx.ptr == nullptr || ctx.end == nullptr || ctx.ptr + totalSize > ctx.end) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    auto ret = memcpy_s(ctx.ptr, totalSize, arr, totalSize);
    if (ret != EOK) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    ctx.ptr += totalSize;
    return UBS_SUCCESS;
}
}
#endif //UBS_ENGINE_UBS_ENGINE_PACK_UTIL_H
