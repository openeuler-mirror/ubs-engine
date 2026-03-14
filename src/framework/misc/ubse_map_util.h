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

#ifndef UBSE_MAP_UTIL_H
#define UBSE_MAP_UTIL_H

#include <functional>
#include <unordered_map>
#include <utility>

namespace ubse::utils {

// 辅助模板：安全获取枚举的底层类型或原类型
template <typename T, bool IsEnum = std::is_enum_v<T>>
struct SafeUnderlyingType;

template <typename T>
struct SafeUnderlyingType<T, true> { // 枚举类型
    using type = std::underlying_type_t<T>;
};

template <typename T>
struct SafeUnderlyingType<T, false> { // 非枚举类型
    using type = T;
};

template <typename T>
using SafeUnderlyingTypeT = typename SafeUnderlyingType<T>::type;

template <typename T1, typename T2>
struct PairHash {
    static_assert(std::is_integral_v<T1> || std::is_enum_v<T1>, "T1 must be integral or enum");
    static_assert(std::is_integral_v<T2> || std::is_enum_v<T2>, "T2 must be integral or enum");

    using UT1 = SafeUnderlyingTypeT<T1>;
    using UT2 = SafeUnderlyingTypeT<T2>;

    size_t operator()(const std::pair<T1, T2> &p) const noexcept
    {
        size_t h1 = std::hash<UT1>{}(p.first);
        size_t h2 = std::hash<UT2>{}(p.second);
        return h1 ^ (h2 << 1);
    }
};

template <typename Key1, typename Key2, typename Value>
using PairMap = std::unordered_map<std::pair<Key1, Key2>, Value, PairHash<Key1, Key2>>;

template <typename Key, typename Val>
bool IfMapContainKey(const Key &key, const std::unordered_map<Key, Val> &map)
{
    return map.find(key) != map.end();
}
} // namespace ubse::utils

#endif // UBSE_MAP_UTIL_H