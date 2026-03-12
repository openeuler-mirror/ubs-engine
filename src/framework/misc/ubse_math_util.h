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
#include "ubse_logger.h"

namespace ubse::utils {
constexpr uint16_t BYTES_PER_MB = 20;

template <typename T>
constexpr T SafeAdd(T a, T b)
{
    if constexpr (std::is_signed_v<T>) {
        if (b > 0 && a > std::numeric_limits<T>::max() - b) {
            throw std::overflow_error("Signed integer overflow in addition! a=" + std::to_string(a) +
                                      ", b=" + std::to_string(b));
        }
        if (b < 0 && a < std::numeric_limits<T>::min() + (-b)) {
            throw std::underflow_error("Signed integer underflow in addition! a=" + std::to_string(a) +
                                       ", b=" + std::to_string(b));
        }
    } else if constexpr (std::is_unsigned_v<T>) {
        if (a > std::numeric_limits<T>::max() - b) {
            throw std::overflow_error("Unsigned integer overflow in addition! a=" + std::to_string(a) +
                                      ", b=" + std::to_string(b));
        }
    } else if constexpr (std::is_floating_point_v<T>) {
        T result = a + b;
        if (std::isinf(result) || std::isnan(result)) {
            throw std::overflow_error("Floating-point overflow or NaN result in addition! a=" + std::to_string(a) +
                                      ", b=" + std::to_string(b));
        }
    }
    return a + b;
}

template <typename T>
constexpr T SafeSub(T a, T b)
{
    if constexpr (std::is_signed_v<T>) {
        // 检查有符号整数的减法溢出和回绕
        if (b < 0 && a > std::numeric_limits<T>::max() + b) {
            throw std::overflow_error("Signed integer overflow in subtraction! a=" + std::to_string(a) +
                                      ", b=" + std::to_string(b));
        }
        if (b > 0 && a < std::numeric_limits<T>::min() + b) {
            throw std::underflow_error("Signed integer underflow in subtraction! a=" + std::to_string(a) +
                                       ", b=" + std::to_string(b));
        }
    } else if constexpr (std::is_unsigned_v<T>) {
        // 检查无符号整数的减法回绕
        if (a < b) {
            throw std::underflow_error("Unsigned integer underflow in subtraction! a=" + std::to_string(a) +
                                       ", b=" + std::to_string(b));
        }
    } else if constexpr (std::is_floating_point_v<T>) {
        // 检查浮点数的减法溢出或NaN结果
        T result = a - b;
        if (std::isinf(result) || std::isnan(result)) {
            throw std::overflow_error("Floating-point overflow or NaN result in subtraction! a=" + std::to_string(a) +
                                      ", b=" + std::to_string(b));
        }
    }
    return a - b;
}

template <typename T, typename... Args>
T SafeAddMulti(T first, Args... rest)
{
    T result = first;
    ((result = SafeAdd(result, rest)), ...);
    return result;
}

} // namespace ubse::utils
#endif // UBS_ENGINE_UBSE_MATH_UTIL_H
