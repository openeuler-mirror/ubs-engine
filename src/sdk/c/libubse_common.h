/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * UbsEngine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef LIBUBSE_COMMON_H
#define LIBUBSE_COMMON_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include <securec.h>

#include "ubs_error.h"

struct UnpackCtx {
    const uint8_t *ptr;
    size_t remaining;
};

struct PackCtx {
    uint8_t *ptr;
};

namespace ubse {

template<typename T>
inline ubs_error_t UnpackValue(UnpackCtx &ctx, T &value)
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

template<typename T>
inline ubs_error_t UnpackArray(UnpackCtx &ctx, T *arr, size_t count)
{
    static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");
    const size_t totalSize = sizeof(T) * count;
    if (ctx.remaining < totalSize) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    auto ret = memcpy_s(arr, totalSize, ctx.ptr, totalSize);
    if (ret != EOK) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    ctx.ptr += totalSize;
    ctx.remaining -= totalSize;
    return UBS_SUCCESS;
}

template<typename T>
inline ubs_error_t PackValue(PackCtx &ctx, const T &value)
{
    static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");
    auto ret = memcpy_s(ctx.ptr, sizeof(T), &value, sizeof(T));
    if (ret != EOK) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    ctx.ptr += sizeof(T);
    return UBS_SUCCESS;
}

template<typename T>
inline ubs_error_t PackArray(PackCtx &ctx, const T *arr, size_t count)
{
    static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");
    const size_t totalSize = sizeof(T) * count;
    auto ret = memcpy_s(ctx.ptr, totalSize, arr, totalSize);
    if (ret != EOK) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    ctx.ptr += totalSize;
    return UBS_SUCCESS;
}

} // namespace ubse

using ubse::PackArray;
using ubse::PackValue;
using ubse::UnpackArray;
using ubse::UnpackValue;

#endif // LIBUBSE_COMMON_H
