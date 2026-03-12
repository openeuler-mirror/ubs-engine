/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_lcne_decoder_handle.h"

#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_http_module.h"
#include "ubse_json_util.h"
#include "ubse_logger.h"

namespace ubse::lcne {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
using namespace ubse::utils;
using namespace rapidjson;
const std::string KEY_QUERY = "huawei-vbussw-service:ub-memory-handle";
const std::string DECODER_HANDLE_URL = "/restconf/operations/huawei-vbussw-service:ub-memory-handle";

UbseResult BuildReqStr(const UbseMamiMemHandleQueryInfo &queryInfo, std::string &body)
{
    election::UbseRoleInfo curNodeInfo{};
    if (const auto ret = UbseGetCurrentNodeInfo(curNodeInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI_MEM] Get information of current node failed, " << FormatRetCode(ret);
        return ret;
    }

    Document doc;
    doc.SetObject();
    auto& allocator = doc.GetAllocator();
    Value innerObj(kObjectType);

    try {
        innerObj.AddMember("slot-id", static_cast<uint64_t>(std::stoull(curNodeInfo.nodeId)), allocator);
    } catch (...) {
        return UBSE_ERROR;
    }
    innerObj.AddMember("ubpu-id", queryInfo.ubpuId, allocator);
    innerObj.AddMember("iou-id", queryInfo.iouId, allocator);
    innerObj.AddMember("mar-id", queryInfo.marId, allocator);
    innerObj.AddMember("decoder-id", queryInfo.decoderId, allocator);
    innerObj.AddMember("type", queryInfo.type, allocator);

    Value keyRoot(KEY_QUERY.c_str(), allocator);
    doc.AddMember(keyRoot, innerObj, allocator);
    StringBuffer buffer;
    Writer writer(buffer);
    doc.Accept(writer);
    body = buffer.GetString();
    return UBSE_OK;
}

bool ConvertToUint64(const std::string& str, uint64_t& value)
{
    try {
        size_t pos = 0;
        value = std::stoull(str, &pos);
        return pos == str.length();
    } catch (const std::exception& e) {
        UBSE_LOG_ERROR << "[MTI_MEM] Covert string to uint64 failed, " << e.what();
    }
    return false;
}

UbseResult ParseHandleValueArray(const Value &handleArray, std::vector<UbseMamiMemHandleValue> &handleValuse)
{
    for (SizeType i = 0; i < handleArray.Size(); i++) {
        UbseMamiMemHandleValue memHandleValue{};
        const auto &handleValue = handleArray[i];
        if (std::string handle{}; UbseJsonUtil::GetStrFromJsonPtr(handleValue, "handle", handle) != UBSE_OK ||
            !ConvertToUint64(handle, memHandleValue.handle)) {
            UBSE_LOG_ERROR << "[MTI_MEM] Parse handle failed, " << FormatRetCode(UBSE_ERROR);
            return UBSE_ERROR;
        }

        if (std::string hpa{}; UbseJsonUtil::GetStrFromJsonPtr(handleValue, "hpa", hpa) != UBSE_OK ||
            !ConvertToUint64(hpa, memHandleValue.hpa)) {
            UBSE_LOG_ERROR << "[MTI_MEM] Parse hpa failed, " << FormatRetCode(UBSE_ERROR);
            return UBSE_ERROR;
        }

        if (std::string size{}; UbseJsonUtil::GetStrFromJsonPtr(handleValue, "size", size) != UBSE_OK ||
            !ConvertToUint64(size, memHandleValue.size)) {
            UBSE_LOG_ERROR << "[MTI_MEM] Parse size failed, " << FormatRetCode(UBSE_ERROR);
            return UBSE_ERROR;
            }

        if (UbseJsonUtil::GetUint16FromJsonPtr(handleValue, "entry-start-index", memHandleValue.entryStartIdx) !=
            UBSE_OK) {
            UBSE_LOG_ERROR << "[MTI_MEM] Parse entryStartIdx failed, " << FormatRetCode(UBSE_ERROR);
            return UBSE_ERROR;
        }

        if (UbseJsonUtil::GetUint16FromJsonPtr(handleValue, "entry-end-index", memHandleValue.entryEndIdx) != UBSE_OK) {
            UBSE_LOG_ERROR << "[MTI_MEM] Parse entryEndIdx failed, " << FormatRetCode(UBSE_ERROR);
            return UBSE_ERROR;
        }

        if (UbseJsonUtil::GetUint16FromJsonPtr(handleValue, "block-start-index", memHandleValue.blockStartIdx) !=
            UBSE_OK) {
            UBSE_LOG_ERROR << "[MTI_MEM] Parse blockStartIdx failed, " << FormatRetCode(UBSE_ERROR);
            return UBSE_ERROR;
        }

        if (UbseJsonUtil::GetUint16FromJsonPtr(handleValue, "block-end-index", memHandleValue.blockEndIdx) != UBSE_OK) {
            UBSE_LOG_ERROR << "[MTI_MEM] Parse blockEndIdx failed, " << FormatRetCode(UBSE_ERROR);
            return UBSE_ERROR;
        }

        if (UbseJsonUtil::GetUint8FromJsonPtr(handleValue, "type", memHandleValue.type) != UBSE_OK) {
            UBSE_LOG_ERROR << "[MTI_MEM] Parse type failed, " << FormatRetCode(UBSE_ERROR);
            return UBSE_ERROR;
        }

        handleValuse.emplace_back(memHandleValue);
    }
    return UBSE_OK;
}

UbseResult ParseRespBody(const std::string &responseStr, std::vector<UbseMamiMemHandleValue> &handleValuse)
{
    UBSE_LOG_INFO << "[MTI_MEM] Response is " << responseStr;

    Document doc{};
    if (doc.Parse(responseStr.c_str()).HasParseError()) {
        UBSE_LOG_ERROR << "[MTI_MEM] Parse response body failed, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    if (!doc.IsObject() || !doc.HasMember("huawei-vbussw-service:ub-memory-handle")) {
        UBSE_LOG_ERROR << "[MTI_MEM] Parse root object failed, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    const Value &root = doc["huawei-vbussw-service:ub-memory-handle"];
    if (!root.IsObject() || !root.HasMember("huawei-vbussw-service:output")) {
        UBSE_LOG_ERROR << "[MTI_MEM] Parse output object failed, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    const Value &output = root["huawei-vbussw-service:output"];
    if (!output.IsObject() || !output.HasMember("ub-memory-handles")) {
        UBSE_LOG_ERROR << "[MTI_MEM] Parse handles object failed, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    const Value &handles = output["ub-memory-handles"];

    if (handles.IsObject() && handles.MemberCount() == 0) {
        return UBSE_OK;
    }

    if (!handles.IsObject() || !handles.HasMember("ub-memory-handle") || !handles["ub-memory-handle"].IsArray()) {
        UBSE_LOG_ERROR << "[MTI_MEM] Parse handle array failed, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    const Value &handleArray = handles["ub-memory-handle"];
    return ParseHandleValueArray(handleArray, handleValuse);
}

UbseResult UbseLcneDecoderHandle::GetAllMemHandles(const UbseMamiMemHandleQueryInfo &queryInfo,
                                                   std::vector<UbseMamiMemHandleValue> &handleValues) const
{
    // // 1. 构建请求体
    std::string reqJson{};
    auto res = BuildReqStr(queryInfo, reqJson);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI_MEM] Build request body failed, " << FormatRetCode(res);
        return res;
    }

    // 2. 发送HTTP请求
    std::string rspJson{};
    res = UbseHttpModule::UbseHttpPostJsonRequest(DECODER_HANDLE_URL, reqJson, rspJson);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI_MEM] Http send failed, " << FormatRetCode(res);
        return res;
    }

    // 3. 解析响应体
    res = ParseRespBody(rspJson, handleValues);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI_MEM] Parse http response failed, " << FormatRetCode(res);
        return res;
    }
    return UBSE_OK;
}
}