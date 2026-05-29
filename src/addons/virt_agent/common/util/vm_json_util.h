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

#ifndef VM_JSON_UTIL_H
#define VM_JSON_UTIL_H

#include <map>
#include <string>
#include <vector>

#include <rapidjson/document.h>

#include "vm_error.h"

namespace vm {
using JSON_STR = std::string;
using JSON_VEC = std::vector<std::string>;
using JSON_MAP = std::map<std::string, std::string>;
using namespace rapidjson;

class VMJsonUtil {
public:
    static VmResult GetString(const Value& pstJson, const std::string& key, std::string& value);
    static VmResult GetNumber(const Value& pstJson, const std::string& key, double& value);

    // Return the JSON string of the child node corresponding to the key in pstJson
    static bool VMGetJsonItemStr(const Value& pstJson, const std::string& key, JSON_STR& jsonStr);

    // Convert the map to a JSON string, where the values in the map can also be JSON strings
    static bool VMConvertMap2JsonStr(const JSON_MAP& strMap, JSON_STR& jsonStr);

    // Convert a JSON string of object type to a map, where the keys of the map must be pre-filled,
    // and the values can also be JSON strings
    static bool VMConvertJsonStr2Map(const JSON_STR& jsonStr, JSON_MAP& strMap);

    // Convert a vector to a JSON string, where the value of the vector can also be a JSON string
    static bool VMConvertVector2JsonStr(const JSON_VEC& strVec, JSON_STR& jsonStr);

    // Convert a JSON string of array type to a vector; the value can also be a JSON string
    static bool VMConvertJsonStr2Vec(const JSON_STR& jsonStr, JSON_VEC& strVec);

    // Get the value string corresponding to the specified key from an object of type object
    static uint32_t GetJsonString(const Value& pstJson, const std::string& key, std::string& value,
                                  bool bVerifyExist = true);

    // Encapsulated GetJsonString, returns a boolean type value, true for invalid, false for valid
    static bool GetJsonStringNotValid(const Value& jsonBody, const std::string& key, std::string& value);
};
} // namespace vm

#endif // VM_JSON_UTIL_H
