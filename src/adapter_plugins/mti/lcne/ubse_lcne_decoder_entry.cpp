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

#include "ubse_lcne_decoder_entry.h"

#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_http_module.h"
#include "ubse_json_util.h"
#include "ubse_logger.h"
#include "ubse_str_util.h"
#include "ubse_lcne_module.h"
#include "ubse_context.h"

namespace ubse::lcne {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
using namespace ubse::utils;
using namespace rapidjson;
using namespace ubse::mti;
using namespace context;

const std::string KEY_ADD = "huawei-vbussw-service:ub-memory-decoder";
const std::string KEY_DELETE = "huawei-vbussw-service:ub-memory-decoder-delete";
const std::string KEY_INVALIDATE = "huawei-vbussw-service:ub-memory-decoder-invalid";
const std::string ADD_DECODER_ENTRY_URL = "/restconf/operations/huawei-vbussw-service:ub-memory-decoder";
const std::string DELETE_DECODER_ENTRY_URL = "/restconf/operations/huawei-vbussw-service:ub-memory-decoder-delete";
const std::string INVALIDATE_DECODER_ENTRY_URL = "/restconf/operations/huawei-vbussw-service:ub-memory-decoder-invalid";

std::string UIntToHex(const uint32_t number)
{
    std::stringstream ss;
    ss << std::hex << number;
    return "0x" + ss.str();
}

bool Convert2Uint64(const std::string &str, uint64_t &value)
{
    try {
        size_t pos = 0;
        value = std::stoull(str, &pos);
        return pos == str.length();
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "[MTI_MEM] Covert string to uint64 failed, " << e.what();
    }
    return false;
}

UbseResult BuildAddReqStr(const UbseMamiMemImportInfo &importInfo,
    const ubse::adapter_plugins::mti::UbseDecoderTrustRingData &trustRingData, std::string &body)
{
    auto module = UbseContext::GetInstance().GetModule<UbseLcneModule>();
    if (module == nullptr) {
 	    return UBSE_ERROR_MODULE_LOAD_FAILED;
 	}
    std::string slotId = module->GetCurSlotId();
    Document doc;
    doc.SetObject();
    auto &allocator = doc.GetAllocator();
    Value innerObj(kObjectType);

    try {
        innerObj.AddMember("slot-id", static_cast<uint64_t>(std::stoull(slotId)), allocator);
    } catch (...) {
        UBSE_LOG_ERROR << "Build slot id failed, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }
    innerObj.AddMember("ubpu-id", importInfo.ubpuId, allocator);
    innerObj.AddMember("iou-id", importInfo.iouId, allocator);
    innerObj.AddMember("mar-id", importInfo.marId, allocator);
    innerObj.AddMember("decoder-id", importInfo.decoderId, allocator);
    auto tmpDcna = UIntToHex(importInfo.dstCNA);
    innerObj.AddMember("destination-cna", Value(tmpDcna.c_str(), allocator).Move(), allocator);
    innerObj.AddMember("import-type", importInfo.importType, allocator);
    innerObj.AddMember("lb", importInfo.lb, allocator);
    innerObj.AddMember("token-id", importInfo.tokenId, allocator);
    innerObj.AddMember("flag", importInfo.flag, allocator);
    auto tmpUba = std::to_string(importInfo.uba);
    innerObj.AddMember("uba", Value(tmpUba.c_str(), allocator).Move(), allocator);
    innerObj.AddMember("size", importInfo.size, allocator);
    innerObj.AddMember("handle", importInfo.handle, allocator);

    if (trustRingData.isHighSafety) {
        innerObj.AddMember("mb-type", Value(trustRingData.type.c_str(), allocator).Move(), allocator);
        innerObj.AddMember("signed-data", Value(trustRingData.signedData.c_str(), allocator).Move(), allocator);
        innerObj.AddMember("trust-ring-id", Value(trustRingData.trustRingId.c_str(), allocator).Move(), allocator);
    }

    Value keyRoot(KEY_ADD.c_str(), allocator);
    doc.AddMember(keyRoot, innerObj, allocator);
    StringBuffer buffer;
    Writer writer(buffer);
    doc.Accept(writer);
    body = buffer.GetString();
    return UBSE_OK;
}

UbseResult BuildBaseReqStr(const UbseMamiMemWithdraw &drawInfo, const std::string &key, std::string &body)
{
    auto module = UbseContext::GetInstance().GetModule<UbseLcneModule>();
    if (module == nullptr) {
 	    return UBSE_ERROR_MODULE_LOAD_FAILED;
 	}
    std::string slotId = module->GetCurSlotId();

    Document doc;
    doc.SetObject();
    auto &allocator = doc.GetAllocator();
    Value innerObj(kObjectType);

    try {
        innerObj.AddMember("slot-id", static_cast<uint64_t>(std::stoull(slotId)), allocator);
    } catch (...) {
        return UBSE_ERROR;
    }
    innerObj.AddMember("ubpu-id", drawInfo.ubpuId, allocator);
    innerObj.AddMember("iou-id", drawInfo.iouId, allocator);
    innerObj.AddMember("mar-id", drawInfo.marId, allocator);
    innerObj.AddMember("decoder-id", drawInfo.decoderIdx, allocator);
    innerObj.AddMember("handle", drawInfo.handle, allocator);
    Value keyRoot(key.c_str(), allocator);
    doc.AddMember(keyRoot, innerObj, allocator);
    StringBuffer buffer;
    Writer writer(buffer);
    doc.Accept(writer);
    body = buffer.GetString();
    return UBSE_OK;
}

UbseResult BuildDeleteReqStr(const UbseMamiMemWithdraw &drawInfo, std::string &body)
{
    return BuildBaseReqStr(drawInfo, KEY_DELETE, body);
}

UbseResult BuildInvalidateReqStr(const UbseMamiMemWithdraw &drawInfo, std::string &body)
{
    return BuildBaseReqStr(drawInfo, KEY_INVALIDATE, body);
}

UbseResult ParseBaseRespBody(const std::string &responseStr, const std::string &key,
                             const std::string &operationName, Document &doc, const Value*& output)
{
    UBSE_LOG_INFO << "[MTI_MEM] Response is " << responseStr;
    if (doc.Parse(responseStr.c_str()).HasParseError()) {
        UBSE_LOG_ERROR << "[MTI_MEM] Parse response body failed, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    if (!doc.IsObject() || !doc.HasMember(key.c_str())) {
        UBSE_LOG_ERROR << "[MTI_MEM] Parse root object failed, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    const Value &decoder = doc[key.c_str()];
    if (!decoder.IsObject() || !decoder.HasMember("huawei-vbussw-service:output")) {
        UBSE_LOG_ERROR << "[MTI_MEM] Parse output object failed, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    const Value &outputValue = decoder["huawei-vbussw-service:output"];
    std::string result{};
    if (const UbseResult res = UbseJsonUtil::GetStrFromJsonPtr(outputValue, "result", result);
        res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI_MEM] Parse result failed, " << FormatRetCode(res);
        return res;
    }
    if (result != "success") {
        UBSE_LOG_ERROR << "[MTI_MEM] " << operationName << " decoder failed, the result of response is "
                       << result << ", " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    output = &outputValue;
    return UBSE_OK;
}

UbseResult ParseAddRespBody(const std::string &responseStr,
                            UbseMamiMemImportResult &importResult)
{
    Document doc{};
    const Value* output = nullptr;
    UbseResult ret = ParseBaseRespBody(responseStr, KEY_ADD, "Add", doc, output);
    if (ret != UBSE_OK) {
        return ret;
    }

    if (std::string hpa{};
        UbseJsonUtil::GetStrFromJsonPtr(*output, "hpa", hpa) != UBSE_OK ||
        ConvertStrToUint64(hpa, importResult.hpa) != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI_MEM] Parse hpa failed, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }
    if (std::string handle{};
        UbseJsonUtil::GetStrFromJsonPtr(*output, "handle", handle) != UBSE_OK ||
        ConvertStrToUint64(handle, importResult.handle) != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI_MEM] Parse handle failed, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult ParseDeleteRespBody(const std::string &responseStr)
{
    Document doc{};
    const Value* output = nullptr;
    return ParseBaseRespBody(responseStr, KEY_DELETE, "Delete", doc, output);
}

UbseResult ParseInvalidateRespBody(const std::string &responseStr)
{
    Document doc{};
    const Value* output = nullptr;
    return ParseBaseRespBody(responseStr, KEY_INVALIDATE, "Invalidate", doc, output);
}

UbseResult UbseLcneDecoderEntry::AddDecoderEntry(const UbseMamiMemImportInfo &importInfo,
    UbseMamiMemImportResult &importResult, const ubse::adapter_plugins::mti::UbseDecoderTrustRingData &trustRingData)
{
    // 1. 构建请求体
    importResult.marId = importInfo.marId;
    std::string reqJson;
    UbseResult res = BuildAddReqStr(importInfo, trustRingData, reqJson);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI_MEM] Build add decoder request failed, " << FormatRetCode(res);
        return res;
    }

    // 2. 发送HTTP请求
    std::string rspJson{};
    res = UbseHttpModule::UbseHttpPostJsonRequest(ADD_DECODER_ENTRY_URL, reqJson, rspJson);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI_MEM] HTTP request failed, " << FormatRetCode(res);
        return res;
    }

    // 3. 解析响应
    return ParseAddRespBody(rspJson, importResult);
}

UbseResult UbseLcneDecoderEntry::DeleteDecoderEntry(const UbseMamiMemWithdraw &drawInfo)
{
    // 1. 构建请求体
    std::string reqJson;
    UbseResult res = BuildDeleteReqStr(drawInfo, reqJson);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI_MEM] Build delete decoder request failed, " << FormatRetCode(res);
        return res;
    }

    // 2. 发送HTTP请求
    std::string rspJson{};
    res = UbseHttpModule::UbseHttpPostJsonRequest(DELETE_DECODER_ENTRY_URL, reqJson, rspJson);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI_MEM] HTTP request failed, " << FormatRetCode(res);
        return res;
    }

    // 3. 解析响应
    return ParseDeleteRespBody(rspJson);
}

UbseResult UbseLcneDecoderEntry::InvalidateDecoderEntry(const UbseMamiMemWithdraw &drawInfo)
{
    std::string reqJson;
    UbseResult res = BuildInvalidateReqStr(drawInfo, reqJson);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI_MEM] Build invalidate decoder request failed, " << FormatRetCode(res);
        return res;
    }

    std::string rspJson{};
    res = UbseHttpModule::UbseHttpPostJsonRequest(INVALIDATE_DECODER_ENTRY_URL, reqJson, rspJson);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI_MEM] HTTP request failed, " << FormatRetCode(res);
        return res;
    }

    return ParseInvalidateRespBody(rspJson);
}
} // namespace ubse::lcne