/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef VM_STRING_UTIL_H
#define VM_STRING_UTIL_H

#include <cmath>
#include <string>
#include <vector>
#include <unordered_map>

namespace vm {
class VmStringUtil {
public:
    static uint32_t SafeStoul(const std::string &str);
    static uint64_t SafeStoull(const std::string &str);
    static int16_t SafeStoi16(const std::string &str);
    static int32_t SafeStoi32(const std::string &str);
    static uint16_t SafeStou16(const std::string &str);
    static int64_t SafeStoi64(const std::string &str);
    static float_t SafeStof(const std::string &str);
    static std::string GenerateUUID();
    static pid_t SafeStopid(const std::string &str);
    static uint64_t ValToByte(uint64_t val, const std::string &unit);
    static uint16_t SafeNotEmptyStou16(const std::string &str);
    static uint64_t SafeNotEmptyStoull(const std::string &str);
    static pid_t SafeNotEmptyStopid(const std::string &str);
    static void StrSplit(const std::string &src, const std::string &sep, std::vector<std::string> &out);
private:
    static std::unordered_map<std::string, uint64_t> unitMap;
};
}

#endif // VM_STRING_UTIL_H
