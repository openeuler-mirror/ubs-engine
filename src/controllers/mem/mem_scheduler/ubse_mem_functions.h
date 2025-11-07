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

#ifndef UBSE_MEM_FUNCTIONS_H
#define UBSE_MEM_FUNCTIONS_H
#include <cstdint>
#include "ubse_mem_constants.h"
namespace ubse::mem::strategy {

constexpr uint16_t MOVE20_BIT = 20;

inline uint64_t CeilToN(const uint64_t x, const uint64_t n)
{
    return ((x + n - 1) / n) * n;
}

inline int32_t CeilToN(const int32_t x, const uint64_t n)
{
#ifdef UB_ENVIRONMENT
    const uint64_t m = n / ONE_M;
    return static_cast<int32_t>(((static_cast<uint64_t>(x) + m - 1) / m) * m);
#else
    return x;
#endif
}
inline uint64_t SizeByte2Mb(uint64_t size)
{
    return size >> MOVE20_BIT;
}

inline uint64_t SizeMb2Byte(uint64_t size)
{
    return size << MOVE20_BIT;
}

template <typename T>
bool SafeAdd(T a, T b, T &result)
{
    const auto ret = __builtin_add_overflow(a, b, &result);
    return ret;
}

template <typename T>
bool SafeSub(T a, T b, T &result)
{
    const auto ret = __builtin_sub_overflow(a, b, &result);
    return ret;
}

inline bool StrToULong(const std::string &src, uint64_t &value)
{
    char *remain = nullptr;
    errno = 0;
    value = std::strtoul(src.c_str(), &remain, 10L); // 10 is decimal digits
    if ((value == 0 && src != "0") || remain == nullptr || strlen(remain) > 0 || errno == ERANGE) {
        return false;
    }
    return true;
}

inline bool StrToULL(const std::string &src, uint64_t &value, int base = 10L)
{
    char *remain = nullptr;
    errno = 0;
    value = std::strtoull(src.c_str(), &remain, base);
    if (base == 16U && value == 0 && src == "0x0") {
        return true;
    }
    if ((value == 0 && src != "0") || remain == nullptr || strlen(remain) > 0 || errno == ERANGE) {
        return false;
    }
    return true;
}
} // namespace ubse::mem::strategy
#endif