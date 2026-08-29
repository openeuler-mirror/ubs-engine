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

#ifndef MEMPOOLING_OVER_COMMIT_PID_FAULT_COMMON_H
#define MEMPOOLING_OVER_COMMIT_PID_FAULT_COMMON_H

#include <sstream>
#include <string>
#include <vector>

namespace mempooling {

// 调试用: 把列表拼成"[ a b c ]"形式，方便日志里一行看清集合内容
template <typename T>
static inline std::string JoinToString(const std::vector<T>& values)
{
    std::ostringstream oss;
    oss << "[";
    for (const auto& v : values) {
        oss << " " << v;
    }
    oss << " ]";
    return oss.str();
}

} // namespace mempooling

#endif // MEMPOOLING_OVER_COMMIT_PID_FAULT_COMMON_H
