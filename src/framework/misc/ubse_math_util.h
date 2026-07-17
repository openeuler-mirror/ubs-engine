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

#ifndef UBS_ENGINE_UBSE_MATH_UTIL_H
#define UBS_ENGINE_UBSE_MATH_UTIL_H

#include <cmath>
#include <cstdint>
#include <limits>
#include "ubse_logger.h"

namespace ubse::utils {
constexpr uint16_t BYTES_PER_KB = 10;
constexpr uint16_t BYTES_PER_MB = 20;
constexpr uint16_t BYTES_PER_GB = 30;

template <typename T>
constexpr bool SafeAdd(T a, T b, T& result)
{
    if constexpr (std::is_floating_point_v<T>) {
        result = a + b;
        return !std::isinf(result) && !std::isnan(result);
    } else {
        return !__builtin_add_overflow(a, b, &result);
    }
}

template <typename T>
constexpr bool SafeSub(T a, T b, T& result)
{
    if constexpr (std::is_floating_point_v<T>) {
        result = a - b;
        return !std::isinf(result) && !std::isnan(result);
    } else {
        return !__builtin_sub_overflow(a, b, &result);
    }
}

inline uint64_t SizeByte2Mb(uint64_t size)
{
    return size >> BYTES_PER_MB;
}

inline bool SizeMb2Byte(uint64_t size, uint64_t& result)
{
    return !__builtin_mul_overflow(size, static_cast<uint64_t>(1) << BYTES_PER_MB, &result);
}

inline bool SizeKb2Byte(uint64_t size, uint64_t& result)
{
    return !__builtin_mul_overflow(size, static_cast<uint64_t>(1) << BYTES_PER_KB, &result);
}

inline bool SizeGb2Byte(uint64_t size, uint64_t& result)
{
    return !__builtin_mul_overflow(size, static_cast<uint64_t>(1) << BYTES_PER_GB, &result);
}

} // namespace ubse::utils
#endif // UBS_ENGINE_UBSE_MATH_UTIL_H
