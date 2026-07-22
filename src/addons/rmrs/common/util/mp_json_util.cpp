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

#include "mp_json_util.h"

#include <map>
#include <string>
#include <vector>

#include "ubse_logger.h"
#include "mp_configuration.h"

namespace mempooling {
/* 获取doc子节点中key对应value,并转为字符串,不进行类型检查 */
bool JsonUtil::GetJsonStringValueWithoutCheck(const rapidjson::Value& value, const char* key, std::string& output)
{
    if (key == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Input key is nullptr.";
        return false;
    }
    if (!value.HasMember(key)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Json have no member named" << key << ".";
        return false;
    }
    // json string去掉引号,非字符类型直接转成字符串输出
    output = value[key].IsString() ? value[key].GetString() : PrintJsonString(value[key]);
    return true;
}

/* 获取doc子节点中key对应的string类型value,类型检查失败返回false */
bool JsonUtil::GetJsonStringValue(const rapidjson::Document& doc, const char* key, std::string& output)
{
    if (key == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Input key is nullptr.";
        return false;
    }
    if (!doc.HasMember(key)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Json have no member named" << key << ".";
        return false;
    }
    if (!doc[key].IsString()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "Json value is not string, value=" << PrintJsonString(doc[key]) << ".";
        return false;
    }
    output = doc[key].GetString();
    return true;
}

bool JsonUtil::GetJsonStringValue(const rapidjson::Value& value, const char* key, std::string& output)
{
    if (key == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Input key is nullptr.";
        return false;
    }
    if (!value.HasMember(key)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Json have no member named" << key << ".";
        return false;
    }
    if (!value[key].IsString()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "Json value is not string, value=" << PrintJsonString(value[key]) << ".";
        return false;
    }
    output = value[key].GetString();
    return true;
}

/* 获取doc子节点中key对应的UInt64类型value,类型检查失败返回false */
bool JsonUtil::GetJsonUint64Value(const rapidjson::Document& doc, const char* key, uint64_t& output)
{
    if (key == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Input key is nullptr.";
        return false;
    }
    if (!doc.HasMember(key)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Json have no member named" << key << ".";
        return false;
    }
    if (!doc[key].IsUint64()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "Json value is not UInt64, value=" << PrintJsonString(doc[key]) << ".";
        return false;
    }
    output = doc[key].GetUint64();
    return true;
}

bool JsonUtil::GetJsonUint64Value(const rapidjson::Value& value, const char* key, uint64_t& output)
{
    if (key == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Input key is nullptr.";
        return false;
    }
    if (!value.HasMember(key)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Json have no member named" << key << ".";
        return false;
    }
    if (!value[key].IsUint64()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "Json value is not UInt64, value=" << PrintJsonString(value[key]) << ".";
        return false;
    }
    output = value[key].GetUint64();
    return true;
}

/* 获取value子节点中key对应的UInt16类型value,类型检查失败返回false */
bool JsonUtil::GetJsonUint16Value(const rapidjson::Value& value, const char* key, uint16_t& output)
{
    if (key == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Input key is nullptr.";
        return false;
    }
    if (!value.HasMember(key)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Json have no member named" << key << ".";
        return false;
    }
    if (!value[key].IsUint()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "Json value is not UInt16, value=" << PrintJsonString(value[key]) << ".";
        return false;
    }
    if (value[key].GetUint() > UINT16_MAX) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "Json value is not UInt16, value=" << PrintJsonString(value[key]) << ".";
        return false;
    }
    output = value[key].GetUint();
    return true;
}

/* 获取value子节点中key对应的int类型value,类型检查失败返回false */
bool JsonUtil::GetJsonIntValue(const rapidjson::Value& value, const char* key, int& output)
{
    if (key == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Input key is nullptr.";
        return false;
    }
    if (!value.HasMember(key)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Json have no member named" << key << ".";
        return false;
    }
    if (!value[key].IsInt()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "Json value is not Int, value=" << PrintJsonString(value[key]) << ".";
        return false;
    }
    output = value[key].GetInt();
    return true;
}

/* 获取value子节点中key对应的Int16类型value,类型检查失败返回false */
bool JsonUtil::GetJsonInt16Value(const rapidjson::Value& value, const char* key, int16_t& output)
{
    if (key == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Input key is nullptr.";
        return false;
    }
    if (!value.HasMember(key)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Json have no member named" << key << ".";
        return false;
    }
    if (!value[key].IsInt()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "Json value is not Int16, value=" << PrintJsonString(value[key]) << ".";
        return false;
    }
    if (value[key].GetInt() > INT16_MAX || value[key].GetInt() < INT16_MIN) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "Json value is not Int16, value=" << PrintJsonString(value[key]) << ".";
        return false;
    }
    output = value[key].GetInt();
    return true;
}

/* 将document对象打印成json字符串 */
JSON_STR JsonUtil::PrintJsonString(const rapidjson::Document& doc)
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    JSON_STR jsonStr = buffer.GetString();
    return jsonStr;
}

/* 将Value对象打印成json字符串 */
JSON_STR JsonUtil::PrintJsonString(const rapidjson::Value& value)
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    value.Accept(writer);
    JSON_STR jsonStr = buffer.GetString();
    return jsonStr;
}

/* 返回doc对象中key对应的子节点的json字符串,其中的json string类型会带引号 */
bool JsonUtil::RackMemGetJsonItemStr(const rapidjson::Document& doc, const char* key, JSON_STR& jsonStr)
{
    if (key == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Input key is nullptr.";
        return false;
    }
    if (!doc.HasMember(key)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Json item get error, dont have member " << key << ".";
        return false;
    }

    const rapidjson::Value& child = doc[key];
    jsonStr = PrintJsonString(child);
    return true;
}

bool JsonUtil::RackMemGetJsonItemStr(const rapidjson::Value& value, const char* key, JSON_STR& jsonStr)
{
    if (key == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Input key is nullptr.";
        return false;
    }
    if (!value.HasMember(key)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Json item get error, dont have member " << key << ".";
        return false;
    }

    const rapidjson::Value& child = value[key];
    jsonStr = PrintJsonString(child);
    return true;
}

/* 将map转为json字符串，其中map的value可以是json字符串,也可以是不加引号的string字符串 */
bool JsonUtil::RackMemConvertMap2JsonStr(const JSON_MAP& strMap, JSON_STR& jsonStr)
{
    rapidjson::Document doc;
    doc.SetObject();
    for (const auto& item : strMap) {
        const JSON_STR& key = item.first;
        const JSON_STR& value = item.second;
        // 此处保证临时对象与doc使用相同的allocator
        rapidjson::Document tmpDoc(&doc.GetAllocator());
        tmpDoc.Parse(value.c_str());
        if (!tmpDoc.HasParseError()) {
            // value是合法的json字符串,为doc添加一个子对象
            doc.AddMember(rapidjson::StringRef(key.c_str()), tmpDoc, doc.GetAllocator());
        } else {
            // value是不加引号的string,为doc添加一个字符串成员
            doc.AddMember(rapidjson::StringRef(key.c_str()), rapidjson::Value(value.c_str(), doc.GetAllocator()),
                          doc.GetAllocator());
        }
    }
    jsonStr = PrintJsonString(doc);
    return true;
}

/* 将object类型的json字符串转为map，其中map的key须提前填充，value也可以是json字符串 */
bool JsonUtil::RackMemConvertJsonStr2Map(const JSON_STR& jsonStr, JSON_MAP& strMap)
{
    Document pJson;
    pJson.Parse(jsonStr.c_str());
    if (pJson.HasParseError()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Parse json error.";
        return false;
    }
    if (!pJson.IsObject()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Json string is not object.";
        return false;
    }
    for (auto& [fst, snd] : strMap) {
        if (!pJson.HasMember(fst.c_str())) {
            return false;
        }
        if (pJson[fst.c_str()].IsString()) {
            snd = pJson[fst.c_str()].GetString();
        } else {
            const auto& pcVal = pJson[fst.c_str()];
            StringBuffer buffer;
            PrettyWriter<StringBuffer> writer(buffer);
            pcVal.Accept(writer);
            snd = buffer.GetString();
        }
    }
    return true;
}

/* 将vector转为json字符串，其中vector的value可以是json字符串,也可以是不加引号的string字符串 */
bool JsonUtil::RackMemConvertVector2JsonStr(const JSON_VEC& strVec, JSON_STR& jsonStr)
{
    rapidjson::Document doc;
    doc.SetArray();
    for (const auto& item : strVec) {
        // 此处保证临时对象与doc使用相同的allocator
        rapidjson::Document tmpDoc(&doc.GetAllocator());
        tmpDoc.Parse(item.c_str());
        if (!tmpDoc.HasParseError()) {
            // value是合法的json字符串,为doc添加一个子对象
            doc.PushBack(tmpDoc, doc.GetAllocator());
        } else {
            // value是不加引号的string,为doc添加一个字符串成员
            doc.PushBack(rapidjson::Value(item.c_str(), doc.GetAllocator()), doc.GetAllocator());
        }
    }
    jsonStr = PrintJsonString(doc);
    return true;
}

/* 将array类型的json字符串转为vec,value也可以是json字符串 */
bool JsonUtil::RackMemConvertJsonStr2Vec(const JSON_STR& jsonStr, JSON_VEC& strVec)
{
    rapidjson::Document pJson;
    auto& allocator = pJson.GetAllocator();
    pJson.Parse(jsonStr.c_str());
    if (pJson.HasParseError() || !pJson.IsArray()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Json Parse error, input json str=" << jsonStr << ".";
        return false;
    }
    const rapidjson::Value& array = pJson.GetArray();
    size_t length = array.Size();
    // 遍历Json数组将所有字符串加入Vector
    for (size_t i = 0; i < length; ++i) {
        if (array[i].IsString()) {
            (void)strVec.emplace_back(array[i].GetString());
        } else {
            std::string element = PrintJsonString(array[i]);
            UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
                << "Json element is not a string, element=" << element << ".";
            (void)strVec.emplace_back(element);
        }
    }
    return true;
}

std::string JsonUtil::CreateRackDeleteAttr(const bool isForceDelete)
{
    JSON_MAP strMap;
    (void)strMap.emplace("isForceDelete", isForceDelete ? "true" : "false");
    JSON_STR res;
    if (!JsonUtil::RackMemConvertMap2JsonStr(strMap, res)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "RackMemConvertMap2JsonStr error.";
        return "";
    }
    return res;
}

} // namespace mempooling