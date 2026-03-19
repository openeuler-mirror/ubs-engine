/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MP_STRING_UTIL_H
#define MP_STRING_UTIL_H

#include <cmath>
#include <string>
#include <vector>

#include "mp_error.h"

namespace mempooling {
class MpStringUtil {
public:
    static uint64_t SafeStoullOld(const std::string &str);
    static int16_t SafeStoi16Old(const std::string &str);
    static MEM_POOLING_RES SafeStopid(const std::string &str, pid_t &ret) noexcept;
    static MEM_POOLING_RES SafeStoul(const std::string &str, uint32_t &ret) noexcept;
    static MEM_POOLING_RES SafeStoull(const std::string &str, uint64_t &ret) noexcept;
    static MEM_POOLING_RES SafeStoi16(const std::string &str, int16_t &ret) noexcept;
    static MEM_POOLING_RES SafeStou16(const std::string &str, uint16_t &ret) noexcept;
    static MEM_POOLING_RES SafeStoi64(const std::string &str, int64_t &ret) noexcept;
    static MEM_POOLING_RES SafeStof(const std::string &str, float_t &ret) noexcept;
    static MEM_POOLING_RES Split(const std::string &src, const std::string &sep, std::vector<std::string> &out);
};
}

#endif // MP_STRING_UTIL_H
