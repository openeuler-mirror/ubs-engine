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

#ifndef UBSE_MANAGER_UBSE_JSON_UTIL_H
#define UBSE_MANAGER_UBSE_JSON_UTIL_H

#include <vector>
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

namespace ubse::utils {
using ubse::common::def::UbseResult;
using Allocator = rapidjson::Document::AllocatorType;

class UbseJsonUtil {
public:
    UbseJsonUtil() = delete;

    ~UbseJsonUtil() = delete;

    static UbseResult GetStrFromJsonPtr(const rapidjson::Value& jsonPtr, const std::string& key, std::string& value);

    static UbseResult GetStrFromJsonPtr(const std::string& jsonPtr, const std::string& key, std::string& value);

    static UbseResult GetUintFromJsonPtr(const rapidjson::Value& jsonPtr, const std::string& key, uint32_t& value);

    static UbseResult GetIntFromJsonPtr(const rapidjson::Value& jsonPtr, const std::string& key, int& value);

    static UbseResult GetBoolFromJsonPtr(const rapidjson::Value& jsonPtr, const std::string& key, bool& value);

    static UbseResult GetUint64FromJsonPtr(const rapidjson::Value& jsonPtr, const std::string& key, uint64_t& value);

    static UbseResult GetUint16FromJsonPtr(const rapidjson::Value& jsonPtr, const std::string& key, uint16_t& value);

    static UbseResult GetUint8FromJsonPtr(const rapidjson::Value& jsonPtr, const std::string& key, uint8_t& value);

    static UbseResult GetDoubleFromJsonPtr(const rapidjson::Value& jsonPtr, const std::string& key, double& value);

    static UbseResult GetFloatFromJsonPtr(const rapidjson::Value& jsonPtr, const std::string& key, float& value);

    static UbseResult GetArrayFromJsonPtr(const rapidjson::Value& jsonPtr, const std::string& key, Allocator& allocator,
                                          rapidjson::Value& value);

    static UbseResult GetObjectFromJsonPtr(const rapidjson::Value& jsonPtr, const std::string& key,
                                           Allocator& allocator, rapidjson::Value& value);

    static UbseResult GetStrFromJsonPtrForce(const rapidjson::Value& jsonPtr, const std::string& key,
                                             std::string& value);

    static UbseResult PrintJsonPtr(const rapidjson::Value& jsonPtr, std::string& jsonStr);

    static std::string RapidValueToString(const rapidjson::Value& value);

    static bool ConvertVector2JsonStr(const std::vector<std::string>& strVec, std::string& jsonStr);

    static bool ConvertJsonStr2Map(const std::string& jsonStr, std::map<std::string, std::string>& strMap);

    static bool GetJsonItemStr(const rapidjson::Value& pstJson, const std::string& key, std::string& jsonStr);

    static bool ConvertJsonStr2Vec(const std::string& jsonStr, std::vector<std::string>& strVec);

    static bool ConvertMap2JsonStr(const std::map<std::string, std::string>& strMap, std::string& jsonStr);
};
} // namespace ubse::utils

#endif // UBSE_MANAGER_UBSE_JSON_UTIL_H
