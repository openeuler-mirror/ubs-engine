/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS uCache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UCACHE_JSON_UTIL_H
#define UCACHE_JSON_UTIL_H
 
#include <map>
#include <string>
#include <vector>
#include <set>
#include <unordered_set>
#include <unordered_map>
 
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
 
#include "ubse_logger.h"
#include "ucache_config.h"
#include "ucache_error.h"
 
namespace ucache {
using namespace ubse::log;
 
using JSON_STR = std::string;
 
class JsonUtil {
public:
    /* 获取doc子节点中key对应的string类型value,类型检查失败返回false */
    static uint32_t GetJsonStringValue(const rapidjson::Document& doc, const char* key, std::string &output);
    /* 将value对象打印成json字符串 */
    static JSON_STR PrintJsonString(const rapidjson::Value& value);
};
} // namespace ucache
 
#endif // UCACHE_JSON_UTIL_H