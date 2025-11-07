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

#include "ubse_http_common.h"

#include <unordered_map>
#include "ubse_error.h"
#include "ubse_logger_module.h"

namespace ubse::http {
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_HTTP_MID)

std::string UbseHttpMethodToString(UbseHttpMethod method)
{
    static const std::unordered_map<UbseHttpMethod, std::string> methodEnumToStringMap = {
        {UbseHttpMethod::UBSE_HTTP_METHOD_GET, "GET"},
        {UbseHttpMethod::UBSE_HTTP_METHOD_HEAD, "HEAD"},
        {UbseHttpMethod::UBSE_HTTP_METHOD_POST, "POST"},
        {UbseHttpMethod::UBSE_HTTP_METHOD_PUT, "PUT"},
        {UbseHttpMethod::UBSE_HTTP_METHOD_DELETE, "DELETE"},
        {UbseHttpMethod::UBSE_HTTP_METHOD_CONNECT, "CONNECT"},
        {UbseHttpMethod::UBSE_HTTP_METHOD_TRACE, "TRACE"},
        {UbseHttpMethod::UBSE_HTTP_METHOD_PATCH, "PATCH"},
    };
    auto it = methodEnumToStringMap.find(method);
    if (it != methodEnumToStringMap.end()) {
        return it->second;
    }
    return "INVALID";
}

UbseHttpMethod StringToUbseHttpMethod(std::string method)
{
    static const std::unordered_map<std::string, UbseHttpMethod> methodStringToEnumMap = {
        {"GET", UbseHttpMethod::UBSE_HTTP_METHOD_GET},
        {"HEAD", UbseHttpMethod::UBSE_HTTP_METHOD_HEAD},
        {"POST", UbseHttpMethod::UBSE_HTTP_METHOD_POST},
        {"PUT", UbseHttpMethod::UBSE_HTTP_METHOD_PUT},
        {"DELETE", UbseHttpMethod::UBSE_HTTP_METHOD_DELETE},
        {"CONNECT", UbseHttpMethod::UBSE_HTTP_METHOD_CONNECT},
        {"TRACE", UbseHttpMethod::UBSE_HTTP_METHOD_TRACE},
        {"PATCH", UbseHttpMethod::UBSE_HTTP_METHOD_PATCH},
    };
    auto it = methodStringToEnumMap.find(method);
    if (it != methodStringToEnumMap.end()) {
        return it->second;
    }
    return UbseHttpMethod::UBSE_HTTP_METHOD_INVALID;
}
} // namespace ubse::http
