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

#ifndef UBSE_ENV_UTIL_H
#define UBSE_ENV_UTIL_H

#include <algorithm> // for transform
#include <cctype>    // for tolower
#include <climits>   // for SHRT_MAX, SHRT_MIN
#include <cstdint>   // for int16_t
#include <cstdlib>   // for size_t, getenv
#include <iterator>  // for back_insert_iterator, back_inserter
#include <string>    // for basic_string, operator==, allocator, char_traits

namespace ubse::utils {

template <typename T>
class UbseEnvUtil {
public:
    static bool Parse(const std::string &value, T &out) = delete;
};

// 模板特化：std::string
template <>
class UbseEnvUtil<std::string> {
public:
    static bool Parse(const std::string &value, std::string &out)
    {
        // 限制长度
        static constexpr size_t MAX_LENGTH = 1024;
        if (value.length() > MAX_LENGTH) {
            return false;
        }
        out = value;
        return true;
    }
};

// 模板特化：bool
template <>
class UbseEnvUtil<bool> {
public:
    static bool Parse(const std::string &value, bool &out)
    {
        std::string lower;
        lower.reserve(value.size());
        std::transform(value.begin(), value.end(), std::back_inserter(lower), [](char c) { return std::tolower(c); });
        if (lower == "true" || lower == "1" || lower == "yes") {
            out = true;
            return true;
        }
        if (lower == "false" || lower == "0" || lower == "no") {
            out = false;
            return true;
        }
        return false;
    }
};

// 模板特化：int16_t
template <>
class UbseEnvUtil<int16_t> {
public:
    static bool Parse(const std::string &value, int16_t &out)
    {
        try {
            size_t end = 0;
            const long parsed = std::stol(value, &end, 10); // 10 表示按十进制转换

            // 检查是否整个字符串都被解析
            const bool hasExtraChars = (end != value.size());
            // 检查是否存在前导空格
            bool hasLeadingWhitespace = false;
            if (!value.empty()) {
                const size_t firstNonSpace = value.find_first_not_of(" \t");
                hasLeadingWhitespace = (firstNonSpace != 0 && end > 0);
            }

            const bool isInvalid = (end == 0) ||           // 无数字被转换
                                   hasExtraChars ||        // 存在额外字符
                                   hasLeadingWhitespace || // 禁止前导空格
                                   (parsed < SHRT_MIN) ||  // 下限检查
                                   (parsed > SHRT_MAX);    // 上限检查
            if (!isInvalid) {
                out = static_cast<int16_t>(parsed);
                return true;
            }
        } catch (...) {
            // 输入无法解析为数字
            // 超出 long 范围
            return false;
        }
        return false;
    }
};

/**
 * 安全获取环境变量，解析失败时使用默认值。
 * @tparam T 目标类型，需提供UbseEnvUtil特化
 * @param name 环境变量名称
 * @param defaultVal 默认值
 * @return T 解析结果，解析失败时使用默认值。
 */
template <typename T>
T GetEnv(const char *name, const T &defaultVal = T{})
{
    if (const auto envValue = std::getenv(name)) {
        T parsed;
        if (UbseEnvUtil<T>::Parse(envValue, parsed)) {
            return parsed;
        }
    }
    return defaultVal;
}

} // namespace ubse::utils
#endif // UBSE_ENV_UTIL_H
