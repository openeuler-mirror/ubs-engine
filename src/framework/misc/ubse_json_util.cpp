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
#include "ubse_json_util.h"
#include "ubse_logger.h"
#include "ubse_logger_module.h"

UBSE_DEFINE_THIS_MODULE("ubse");

namespace ubse::utils {
using namespace ubse::log;

UbseResult UbseJsonUtil::GetStrFromJsonPtr(const rapidjson::Value &jsonPtr, const std::string &key, std::string &value)
{
    if (!jsonPtr.IsObject() || !jsonPtr.HasMember(key.c_str()) || !jsonPtr[key.c_str()].IsString()) {
        UBSE_LOG_ERROR << "[JSON] Get string from json, the key is:" << key;
        return UBSE_ERROR_INVAL;
    }
    value = jsonPtr[key.c_str()].GetString();
    return UBSE_OK;
}

UbseResult UbseJsonUtil::GetUintFromJsonPtr(const rapidjson::Value &jsonPtr, const std::string &key, uint32_t &value)
{
    if (!jsonPtr.IsObject() || !jsonPtr.HasMember(key.c_str()) || !jsonPtr[key.c_str()].IsNumber()) {
        UBSE_LOG_ERROR << "[JSON] Get uint from json, the key is:" << key;
        return UBSE_ERROR_INVAL;
    }

    value = jsonPtr[key.c_str()].GetUint();
    return UBSE_OK;
}

UbseResult UbseJsonUtil::GetDoubleFromJsonPtr(const rapidjson::Value &jsonPtr, const std::string &key, double &value)
{
    if (!jsonPtr.IsObject() || !jsonPtr.HasMember(key.c_str()) || !jsonPtr[key.c_str()].IsNumber()) {
        UBSE_LOG_ERROR << "[JSON] Get double from json, the key is:" << key;
        return UBSE_ERROR_INVAL;
    }
    value = jsonPtr[key.c_str()].GetDouble();
    return UBSE_OK;
}

UbseResult UbseJsonUtil::GetFloatFromJsonPtr(const rapidjson::Value &jsonPtr, const std::string &key, float &value)
{
    if (!jsonPtr.IsObject() || !jsonPtr.HasMember(key.c_str()) || !jsonPtr[key.c_str()].IsNumber()) {
        UBSE_LOG_ERROR << "[JSON] Get float from json, the key is:" << key;
        return UBSE_ERROR_INVAL;
    }
    value = jsonPtr[key.c_str()].GetFloat();
    return UBSE_OK;
}

UbseResult UbseJsonUtil::GetArrayFromJsonPtr(const rapidjson::Value &jsonPtr, const std::string &key,
                                             Allocator &allocator, rapidjson::Value &value)
{
    if (!jsonPtr.IsObject() || !jsonPtr.HasMember(key.c_str()) || !jsonPtr[key.c_str()].IsArray()) {
        UBSE_LOG_ERROR << "[JSON] Get array from json, the key is:" << key;
        return UBSE_ERROR_INVAL;
    }
    const rapidjson::Value &array = jsonPtr[key.c_str()].GetArray();
    value.CopyFrom(array, allocator, true);
    return UBSE_OK;
}

UbseResult UbseJsonUtil::GetObjectFromJsonPtr(const rapidjson::Value &jsonPtr, const std::string &key,
                                              Allocator &allocator, rapidjson::Value &value)
{
    if (!jsonPtr.IsObject() || !jsonPtr.HasMember(key.c_str()) || !jsonPtr[key.c_str()].IsObject()) {
        UBSE_LOG_ERROR << "[JSON] Get object from json, the key is:" << key;
        return UBSE_ERROR_INVAL;
    }
    const rapidjson::Value &object = jsonPtr[key.c_str()].GetObject();
    value.CopyFrom(object, allocator);
    return UBSE_OK;
}

UbseResult UbseJsonUtil::PrintJsonPtr(const rapidjson::Value &jsonPtr, std::string &jsonStr)
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    if (!jsonPtr.Accept(writer)) {
        return UBSE_ERROR;
    }
    jsonStr = std::string(buffer.GetString());
    return UBSE_OK;
}

UbseResult UbseJsonUtil::GetStrFromJsonPtr(const std::string &jsonPtr, const std::string &key, std::string &value)
{
    rapidjson::Document doc;
    doc.Parse(jsonPtr.c_str());
    if (doc.HasParseError()) {
        UBSE_LOG_ERROR << "[JSON] Get string from json string, the key is:" << key;
        return UBSE_ERROR_INVAL;
    }
    return GetStrFromJsonPtr(doc, key, value);
}

UbseResult UbseJsonUtil::GetUint64FromJsonPtr(const rapidjson::Value &jsonPtr, const std::string &key, uint64_t &value)
{
    if (!jsonPtr.IsObject() || !jsonPtr.HasMember(key.c_str()) || !jsonPtr[key.c_str()].IsUint64()) {
        UBSE_LOG_ERROR << "[JSON] Get uint64 from json, the key is:" << key;
        return UBSE_ERROR_INVAL;
    }
    value = jsonPtr[key.c_str()].GetUint64();
    return UBSE_OK;
}

UbseResult UbseJsonUtil::GetUint16FromJsonPtr(const rapidjson::Value &jsonPtr, const std::string &key, uint16_t &value)
{
    if (!jsonPtr.IsObject() || !jsonPtr.HasMember(key.c_str()) || !jsonPtr[key.c_str()].IsUint() ||
        jsonPtr[key.c_str()].GetUint() > UINT16_MAX) {
        UBSE_LOG_ERROR << "[JSON] Get uint16 from json, the key is:" << key;
        return UBSE_ERROR_INVAL;
        }
    value = jsonPtr[key.c_str()].GetUint();
    return UBSE_OK;
}

UbseResult UbseJsonUtil::GetUint8FromJsonPtr(const rapidjson::Value &jsonPtr, const std::string &key, uint8_t &value)
{
    if (!jsonPtr.IsObject() || !jsonPtr.HasMember(key.c_str()) || !jsonPtr[key.c_str()].IsUint() ||
        jsonPtr[key.c_str()].GetUint() > UINT8_MAX) {
        UBSE_LOG_ERROR << "[JSON] Get uint8 from json, the key is:" << key;
        return UBSE_ERROR_INVAL;
        }
    value = jsonPtr[key.c_str()].GetUint();
    return UBSE_OK;
}

UbseResult UbseJsonUtil::GetIntFromJsonPtr(const rapidjson::Value &jsonPtr, const std::string &key, int &value)
{
    if (!jsonPtr.IsObject() || !jsonPtr.HasMember(key.c_str()) || !jsonPtr[key.c_str()].IsNumber()) {
        UBSE_LOG_ERROR << "[JSON] Get int from json, the key is:" << key;
        return UBSE_ERROR_INVAL;
    }
    value = jsonPtr[key.c_str()].GetInt();
    return UBSE_OK;
}

UbseResult UbseJsonUtil::GetBoolFromJsonPtr(const rapidjson::Value &jsonPtr, const std::string &key, bool &value)
{
    if (!jsonPtr.IsObject() || !jsonPtr.HasMember(key.c_str()) || !jsonPtr[key.c_str()].IsBool()) {
        UBSE_LOG_ERROR << "[JSON] Get bool from json, the key is:" << key;
        return UBSE_ERROR_INVAL;
    }
    value = jsonPtr[key.c_str()].GetBool();
    return UBSE_OK;
}

UbseResult UbseJsonUtil::GetStrFromJsonPtrForce(const rapidjson::Value &jsonPtr, const std::string &key,
                                                std::string &str)
{
    if (!jsonPtr.IsObject() || !jsonPtr.HasMember(key.c_str())) {
        UBSE_LOG_ERROR << "[JSON] Json ptr contains no object.";
        return UBSE_ERROR_INVAL;
    }
    const rapidjson::Value &value = jsonPtr[key.c_str()];
    str = RapidValueToString(value);
    return UBSE_OK;
}

std::string UbseJsonUtil::RapidValueToString(const rapidjson::Value &value)
{
    if (value.IsString()) {
        return value.GetString();
    }
    if (value.IsInt()) {
        return std::to_string(value.GetInt());
    }
    if (value.IsUint()) {
        return std::to_string(value.GetUint());
    }
    if (value.IsInt64()) {
        return std::to_string(value.GetInt64());
    }
    if (value.IsUint64()) {
        return std::to_string(value.GetUint64());
    }
    if (value.IsDouble()) {
        return std::to_string(value.GetDouble());
    }
    if (value.IsBool()) {
        return value.GetBool() ? "true" : "false";
    }
    if (value.IsNull()) {
        return "null";
    } else {
        // 其它类型（如数组或对象），可序列化为JSON字符串
        std::string result;
        PrintJsonPtr(value, result);
        return result;
    }
}

bool UbseJsonUtil::ConvertVector2JsonStr(const std::vector<std::string> &strVec, std::string &jsonStr)
{
    rapidjson::Document pJson(rapidjson::kArrayType);
    auto &allocator = pJson.GetAllocator();
    // 遍历字符串数组，加入到Json数组对象中
    for (const auto &item : strVec) {
        rapidjson::Document pTmpJson;
        pTmpJson.Parse(item.c_str());
        if (pTmpJson.HasParseError()) {
            rapidjson::Value pJsonItem(item.c_str(), allocator);
            pJson.PushBack(pJsonItem, allocator);
            continue;
        }
        rapidjson::Value tmpJson;
        tmpJson.CopyFrom(pTmpJson, allocator);
        pJson.PushBack(tmpJson, allocator);
    }
    // 输出字符串
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    pJson.Accept(writer);
    jsonStr = buffer.GetString();
    return true;
}

bool UbseJsonUtil::GetJsonItemStr(const rapidjson::Value &pstJson, const std::string &key, std::string &jsonStr)
{
    auto ret = pstJson.HasMember(key.c_str());
    if (!ret) {
        return false;
    }
    if (pstJson[key.c_str()].IsString()) {
        jsonStr = pstJson[key.c_str()].GetString();
        return true;
    }
    const auto &valBody = pstJson[key.c_str()];
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    valBody.Accept(writer);
    jsonStr = buffer.GetString();
    return true;
}

bool UbseJsonUtil::ConvertJsonStr2Map(const std::string &jsonStr, std::map<std::string, std::string> &strMap)
{
    rapidjson::Document pJson;
    pJson.Parse(jsonStr.c_str());
    if (pJson.HasParseError() || !pJson.IsObject()) {
        UBSE_LOG_ERROR << "Parse json error";
        return false;
    }
    const rapidjson::Value &value = pJson.GetObject();
    for (auto &it : strMap) {
        if (!GetJsonItemStr(value, it.first.c_str(), it.second)) {
            UBSE_LOG_ERROR << "GetJsonItemStr error " << it.first;
            return false;
        }
    }
    return true;
}

bool UbseJsonUtil::ConvertJsonStr2Vec(const std::string &jsonStr, std::vector<std::string> &strVec)
{
    rapidjson::Document pJson;
    auto &allocator = pJson.GetAllocator();
    pJson.Parse(jsonStr.c_str());
    if (pJson.HasParseError() || !pJson.IsArray()) {
        UBSE_LOG_ERROR << "Json Parse error";
        return false;
    }
    const rapidjson::Value &array = pJson.GetArray();
    uint32_t length = array.Size();
    // 遍历Json数组将所有字符串加入Vector
    for (int i = 0; i < length; ++i) {
        if (array[i].IsString()) {
            strVec.emplace_back(array[i].GetString());
        } else {
            const auto &valBody = array[i];
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            valBody.Accept(writer);
            strVec.emplace_back(buffer.GetString());
        }
    }
    return true;
}

bool UbseJsonUtil::ConvertMap2JsonStr(const std::map<std::string, std::string> &strMap, std::string &jsonStr)
{
    rapidjson::Document pJson(rapidjson::kObjectType);
    auto &allocator = pJson.GetAllocator();
    // 遍历Map，插入键值对到Json对象
    for (const auto &[fst, snd] : strMap) {
        rapidjson::Document pTmpJson;
        pTmpJson.Parse(snd.c_str());
        if (pTmpJson.HasParseError()) {
            pJson.AddMember(rapidjson::Value(fst.c_str(), allocator), rapidjson::Value(snd.c_str(), allocator),
                            allocator);
        } else {
            rapidjson::Value tmpJson;
            tmpJson.CopyFrom(pTmpJson, allocator);
            pJson.AddMember(rapidjson::Value(fst.c_str(), allocator), tmpJson, allocator);
        }
    }
    // 输出字符串
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    pJson.Accept(writer);
    jsonStr = buffer.GetString();
    return true;
}
} // namespace ubse::utils