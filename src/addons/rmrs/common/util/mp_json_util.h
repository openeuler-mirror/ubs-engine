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

#ifndef MP_JSON_UTIL_H
#define MP_JSON_UTIL_H

#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include "ubse_logger.h"
#include "mp_configuration.h"

namespace mempooling {
using namespace ubse::log;
using namespace rapidjson;

using JSON_STR = std::string;
using JSON_VEC = std::vector<std::string>;
using JSON_MAP = std::map<std::string, std::string>;
using JSON_SET = std::set<std::string>;

class JsonUtil {
public:
    /* 获取doc子节点中key对应value,并转为字符串,不进行类型检查 */
    static bool GetJsonStringValueWithoutCheck(const rapidjson::Value& value, const char* key, std::string& output);
    /* 获取doc子节点中key对应的string类型value,类型检查失败返回false */
    static bool GetJsonStringValue(const rapidjson::Document& doc, const char* key, std::string& output);
    static bool GetJsonStringValue(const rapidjson::Value& value, const char* key, std::string& output);

    /* 获取doc子节点中key对应的UInt64类型value,类型检查失败返回false */
    static bool GetJsonUint64Value(const rapidjson::Document& doc, const char* key, uint64_t& output);
    static bool GetJsonUint64Value(const rapidjson::Value& value, const char* key, uint64_t& output);

    /* 获取value子节点中key对应的UInt16类型value,类型检查失败返回false */
    static bool GetJsonUint16Value(const rapidjson::Value& value, const char* key, uint16_t& output);

    /* 获取value子节点中key对应的Int16类型value,类型检查失败返回false */
    static bool GetJsonInt16Value(const rapidjson::Value& value, const char* key, int16_t& output);

    /* 获取value子节点中key对应的int类型value,类型检查失败返回false */
    static bool GetJsonIntValue(const rapidjson::Value& value, const char* key, int& output);

    /* 将document对象打印成json字符串 */
    static JSON_STR PrintJsonString(const rapidjson::Document& doc);

    /* 将value对象打印成json字符串 */
    static JSON_STR PrintJsonString(const rapidjson::Value& value);

    /* 返回doc对象中key对应的子节点的json字符串,其中的json string类型会带引号 */
    static bool RackMemGetJsonItemStr(const rapidjson::Document& doc, const char* key, JSON_STR& jsonStr);
    static bool RackMemGetJsonItemStr(const rapidjson::Value& value, const char* key, JSON_STR& jsonStr);

    /* 将map转为json字符串，其中map的value可以是json字符串,也可以是不加引号的string字符串 */
    static bool RackMemConvertMap2JsonStr(const JSON_MAP& strMap, JSON_STR& jsonStr);

    /* 将object类型的json字符串转为map，其中map的key须提前填充，value也可以是json字符串 */
    static bool RackMemConvertJsonStr2Map(const JSON_STR& jsonStr, JSON_MAP& strMap);

    /* 将vector转为json字符串，其中vector的value可以是json字符串,也可以是不加引号的string字符串 */
    static bool RackMemConvertVector2JsonStr(const JSON_VEC& strVec, JSON_STR& jsonStr);

    /* 将array类型的json字符串转为vec,value也可以是json字符串 */
    static bool RackMemConvertJsonStr2Vec(const JSON_STR& jsonStr, JSON_VEC& strVec);

    static std::string CreateRackDeleteAttr(const bool isForceDelete);
};
} // namespace mempooling

#endif // MP_JSON_UTIL_H