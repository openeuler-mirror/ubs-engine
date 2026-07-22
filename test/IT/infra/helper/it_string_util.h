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

#ifndef IT_STRING_UTIL_H
#define IT_STRING_UTIL_H

#include <string>

namespace ubse::it::infra::util {

/**
 * @brief Extract node ID from a "hostname(nodeId)" formatted string.
 *
 * If the string contains parentheses, returns the content between them.
 * Otherwise returns the original string unchanged.
 *
 * Example: "node-01(3)" -> "3"
 */
inline std::string ExtractNodeId(const std::string& nodeName)
{
    std::string::size_type openParen = nodeName.find('(');
    std::string::size_type closeParen = nodeName.find(')');
    if (openParen != std::string::npos && closeParen != std::string::npos && closeParen > openParen + 1) {
        return nodeName.substr(openParen + 1, closeParen - openParen - 1);
    }
    return nodeName;
}

} // namespace ubse::it::infra::util

#endif // IT_STRING_UTIL_H
