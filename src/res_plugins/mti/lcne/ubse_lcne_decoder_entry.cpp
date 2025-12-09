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

#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_http_module.h"
#include "ubse_logger_inner.h"
#include "ubse_pointer_process.h"
#include "ubse_xml.h"

namespace ubse::lcne {
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_LCNE_MID);
using namespace ubse::log;
using namespace ubse::utils;

const std::string OPERATION_ADD = "add";
const std::string OPERATION_DELETE = "delete";

std::string UIntToHex(uint32_t number) {
    std::stringstream ss;
    ss << std::hex << number;
    return "0x" + ss.str();
}

UbseResult BuildReqStr(const std::string &operation, const UbseMamiMemImportInfo &importInfo, std::string &xmlStr)
{
    std::shared_ptr<UbseXml> ubseXml = SafeMakeShared<UbseXml>();
    if (ubseXml == nullptr) {
        UBSE_LOG_ERROR << "[MTI_MEM] Get xml pointer failed, " << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }

    election::UbseRoleInfo curNodeInfo{};
    auto ret = UbseGetCurrentNodeInfo(curNodeInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI_MEM] Get information of current node failed, " << FormatRetCode(ret);
        return ret;
    }

    ubseXml->AddNode(operation == OPERATION_ADD ? "ub-memory-decoder" : "ub-memory-decoder-delete");
    ubseXml->Attr("xmlns", "urn:huawei:yang:huawei-vbussw-service");
    ubseXml->AddNode("slot-id");
    ubseXml->Child("slot-id")->Text(curNodeInfo.nodeId);
    ubseXml->AddNode("ubpu-id");
    ubseXml->Child("ubpu-id")->Text(std::to_string(importInfo.ubpuId));
    ubseXml->AddNode("iou-id");
    ubseXml->Child("iou-id")->Text(std::to_string(importInfo.iouId));
    ubseXml->AddNode("mar-id");
    ubseXml->Child("mar-id")->Text(std::to_string(importInfo.marId));
    ubseXml->AddNode("decoder-id");
    ubseXml->Child("decoder-id")->Text(std::to_string(importInfo.decoderId));

    if (operation == OPERATION_ADD) {
        ubseXml->AddNode("destination-cna");
        ubseXml->Child("destination-cna")->Text(UIntToHex(importInfo.dstCNA));
        ubseXml->AddNode("import-type");
        ubseXml->Child("import-type")->Text(std::to_string(importInfo.importType));
        ubseXml->AddNode("lb");
        ubseXml->Child("lb")->Text(std::to_string(importInfo.lb));
        ubseXml->AddNode("token-id");
        ubseXml->Child("token-id")->Text(std::to_string(importInfo.tokenId));
        ubseXml->AddNode("flag");
        ubseXml->Child("flag")->Text(std::to_string(importInfo.flag));
        ubseXml->AddNode("uba");
        ubseXml->Child("uba")->Text(std::to_string(importInfo.uba));
        ubseXml->AddNode("size");
        ubseXml->Child("size")->Text(std::to_string(importInfo.size));
    }

    ubseXml->AddNode("handle");
    ubseXml->Child("handle")->Text(std::to_string(importInfo.handle));
    ubseXml->Printer(xmlStr);

    return UBSE_OK;
}

UbseResult ParseRspXml(const std::string &responseStr, UbseMamiMemImportResult *importResult = nullptr)
{
    std::shared_ptr<UbseXml> ubseXml = SafeMakeShared<UbseXml>(responseStr);
    if (ubseXml == nullptr) {
        UBSE_LOG_ERROR << "[MTI_MEM] Get xml pointer failed, " << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }

    const auto ret = ubseXml->Parse();
    if (ret != UbseXmlError::OK) {
        UBSE_LOG_ERROR << "[MTI_MEM] Parse response body failed, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    const auto result = ubseXml->Child("result")->Text();
    if (result != "success") {
        UBSE_LOG_ERROR << "[MTI_MEM] Http response failed, the result of response is " << result << ", " <<
            FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    if (importResult != nullptr) {
        try {
            importResult->hpa = std::stol(ubseXml->Child("hpa")->Text());
            UBSE_LOG_DEBUG << "[MTI_MEM] hpa is " << importResult->hpa;
            importResult->handle = std::stol(ubseXml->Child("handle")->Text());
            UBSE_LOG_DEBUG << "[MTI_MEM] handle is " << importResult->handle;
        } catch (const std::exception& e) {
            UBSE_LOG_ERROR << "[MTI_MEM] Unexpected exception: " << e.what();
            return UBSE_ERROR;
        }
    }
    return UBSE_OK;
}

UbseResult UbseLcneDecoderEntry::SendRequest(const std::string &body, UbseHttpResponse &rsp,
    const std::string &operation)
{
    UbseHttpRequest req{};
    req.method = "POST";
    req.path = operation == OPERATION_ADD ? ADD_DECODER_ENTRY_URL : DELETE_DECODER_ENTRY_URL;
    req.headers.emplace("Accept", "application/yang-data+xml");
    req.headers.emplace("Content-Type", "application/yang-data+xml");
    req.body = body;
    UBSE_LOG_DEBUG << "[MTI_MEM] request body is " << req.body;

    auto res = UbseHttpModule::HttpSend(host, port, req, rsp);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI_MEM] Http send failed, " << FormatRetCode(res);
        return res;
    }

    if (rsp.status != static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK)) {
        UBSE_LOG_ERROR << "[MTI_MEM] Http response failed, unexpected status " << rsp.status << ", " <<
            FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    if (rsp.body.empty()) {
        UBSE_LOG_ERROR << "[MTI_MEM] Http response body is empty, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    return UBSE_OK;
}

UbseResult UbseLcneDecoderEntry::AddDecoderEntry(const UbseMamiMemImportInfo &importInfo,
    UbseMamiMemImportResult &importResult)
{
    std::string xmlStr;
    auto res = BuildReqStr(OPERATION_ADD, importInfo, xmlStr);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI_MEM] Build request xml failed, " << FormatRetCode(res);
        return res;
    }

    UbseHttpResponse rsp{};
    res = SendRequest(xmlStr, rsp, OPERATION_ADD);
    if (res != UBSE_OK) {
        return res;
    }

    res = ParseRspXml(rsp.body, &importResult);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI_MEM] Parse http response failed, " << FormatRetCode(res);
        return res;
    }

    importResult.marId = importInfo.marId;
    return UBSE_OK;
}

UbseResult UbseLcneDecoderEntry::DeleteDecoderEntry(const UbseMamiMemWithdraw &drawInfo)
{
    std::string xmlStr;
    UbseMamiMemImportInfo importInfo{};
    importInfo.marId = drawInfo.marId;
    importInfo.decoderId = drawInfo.decoderIdx;
    importInfo.handle = drawInfo.handle;
    importInfo.ubpuId = drawInfo.ubpuId;
    importInfo.iouId = drawInfo.iouId;

    auto res = BuildReqStr(OPERATION_DELETE, importInfo, xmlStr);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI_MEM] Build request xml failed, " << FormatRetCode(res);
        return res;
    }

    UbseHttpResponse rsp{};
    res = SendRequest(xmlStr, rsp, OPERATION_DELETE);
    if (res != UBSE_OK) {
        return res;
    }

    res = ParseRspXml(rsp.body);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI_MEM] Parse http response failed, " << FormatRetCode(res);
        return res;
    }

    return UBSE_OK;
}
}