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

#include "vm_json_util.h"

#include <string>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <ubse_logger.h>

namespace vm {
UBSE_DEFINE_THIS_MODULE("vm_plugin");
VmResult VMJsonUtil::GetString(const Value &pstJson, const std::string &key, std::string &value)
{
    if (!pstJson.HasMember(key.c_str()) || !pstJson[key.c_str()].IsString()) {
        return VM_ERROR;
    }
    value = pstJson[key.c_str()].GetString();
    return VM_OK;
}

VmResult VMJsonUtil::GetNumber(const Value &pstJson, const std::string &key, double &value)
{
    auto ret = pstJson.HasMember(key.c_str()) && pstJson[key.c_str()].IsNumber();
    if (!ret) {
        return VM_ERROR;
    }
    value = pstJson[key.c_str()].GetDouble();
    return VM_OK;
}

bool VMJsonUtil::VMGetJsonItemStr(const Value &pstJson, const std::string &key, JSON_STR &jsonStr)
{
    auto ret = pstJson.HasMember(key.c_str());
    if (!ret) {
        UBSE_LOG_ERROR << "Key " << key << " not found in JSON object.";
        return false;
    }
    const auto &valBody = pstJson[key.c_str()];
    StringBuffer buffer;
    PrettyWriter<StringBuffer> writer(buffer);
    valBody.Accept(writer);
    jsonStr = buffer.GetString();
    return true;
}

bool VMJsonUtil::VMConvertMap2JsonStr(const JSON_MAP &strMap, JSON_STR &jsonStr)
{
    Document pJson;
    pJson.SetObject();
    auto &allocator = pJson.GetAllocator();
    for (const auto &[fst, snd] : strMap) {
        Document pTmpJson;
        pTmpJson.Parse(snd.c_str());
        if (pTmpJson.HasParseError()) {
            pJson.AddMember(Value(fst.c_str(), allocator), Value(snd.c_str(), allocator), allocator);
        } else {
            Value tmpJson;
            tmpJson.CopyFrom(pTmpJson, allocator);
            pJson.AddMember(Value(fst.c_str(), allocator), tmpJson, allocator);
        }
    }
    StringBuffer buffer;
    PrettyWriter<StringBuffer> writer(buffer);
    pJson.Accept(writer);
    jsonStr = buffer.GetString();
    return true;
}

bool VMJsonUtil::VMConvertJsonStr2Map(const JSON_STR &jsonStr, JSON_MAP &strMap)
{
    Document pJson;
    pJson.Parse(jsonStr.c_str());
    if (pJson.HasParseError()) {
        UBSE_LOG_ERROR << "Parse json error.";
        return false;
    }
    if (!pJson.IsObject()) {
        UBSE_LOG_ERROR << "Json string is not object.";
        return false;
    }
    for (auto &[fst, snd] : strMap) {
        if (!pJson.HasMember(fst.c_str())) {
            return false;
        }
        if (pJson[fst.c_str()].IsString()) {
            snd = pJson[fst.c_str()].GetString();
        } else {
            const auto &pcVal = pJson[fst.c_str()];
            StringBuffer buffer;
            PrettyWriter<StringBuffer> writer(buffer);
            pcVal.Accept(writer);
            snd = buffer.GetString();
        }
    }
    return true;
}

bool VMJsonUtil::VMConvertVector2JsonStr(const JSON_VEC &strVec, JSON_STR &jsonStr)
{
    Document pJson;
    pJson.SetArray();
    auto &allocator = pJson.GetAllocator();
    for (const auto &item : strVec) {
        Document pTmpJson;
        pTmpJson.Parse(item.c_str());
        if (pTmpJson.HasParseError()) {
            Value pJsonItem(item.c_str(), allocator);
            pJson.PushBack(pJsonItem, allocator);
            continue;
        }
        Value tmpJson;
        tmpJson.CopyFrom(pTmpJson, allocator);
        pJson.PushBack(tmpJson, allocator);
    }
    StringBuffer buffer;
    PrettyWriter<StringBuffer> writer(buffer);
    pJson.Accept(writer);
    jsonStr = buffer.GetString();
    return true;
}

bool VMJsonUtil::VMConvertJsonStr2Vec(const JSON_STR &jsonStr, JSON_VEC &strVec)
{
    Document pJson;
    pJson.Parse(jsonStr.c_str());
    if (pJson.HasParseError()) {
        UBSE_LOG_ERROR << "Parse json error.";
        return false;
    }
    if (!pJson.IsArray()) {
        UBSE_LOG_ERROR << "json str is not array.";
        return false;
    }
    for (auto &item : pJson.GetArray()) {
        if (item.IsString()) {
            strVec.emplace_back(item.GetString());
            continue;
        }
        StringBuffer buffer;
        PrettyWriter<StringBuffer> writer(buffer);
        item.Accept(writer);
        strVec.emplace_back(buffer.GetString());
    }
    return true;
}

VmResult VMJsonUtil::GetJsonString(const Value &pstJson, const std::string &key, std::string &value, bool bVerifyExist)
{
    if (!pstJson.HasMember(key.c_str())) {
        return bVerifyExist ? VM_ERROR : VM_OK;
    }
    const auto &valBody = pstJson[key.c_str()];
    if (valBody.IsString()) {
        value = valBody.GetString();
    } else {
        StringBuffer buffer;
        PrettyWriter<StringBuffer> writer(buffer);
        valBody.Accept(writer);
        value = buffer.GetString();
    }
    return VM_OK;
}

bool VMJsonUtil::GetJsonStringNotValid(const Value &jsonBody, const std::string &key, std::string &value)
{
    bool ret = false;
    if (GetJsonString(jsonBody, key, value, true) != VM_OK) {
        UBSE_LOG_ERROR << "Key " << std::string(key) << " not exist.";
        ret = true;
    }
    if (value.empty()) {
        UBSE_LOG_ERROR << "Key " << std::string(key) << " value is empty.";
        ret = true;
    }
    return ret;
}
} // namespace vm