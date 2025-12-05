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
#include "ubse_logger_inner.h"
#include "ubse_pointer_process.h"
#include "ubse_xml.h"

namespace ubse::lcne {
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_LCNE_MID);
using namespace ubse::log;
using namespace ubse::utils;

const int HEX_BASE = 16;

UbseResult BuildReqStr(const UbseMamiMemHandleQueryInfo &queryInfo, std::string &xmlStr)
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

    ubseXml->AddNode("ub-memory-handle");
    ubseXml->Attr("xmlns", "urn:huawei:yang:huawei-vbussw-service");
    ubseXml->AddNode("slot-id");
    ubseXml->Child("slot-id")->Text(curNodeInfo.nodeId);
    ubseXml->AddNode("ubpu-id");
    ubseXml->Child("ubpu-id")->Text(std::to_string(queryInfo.ubpuId));
    ubseXml->AddNode("iou-id");
    ubseXml->Child("iou-id")->Text(std::to_string(queryInfo.iouId));
    ubseXml->AddNode("mar-id");
    ubseXml->Child("mar-id")->Text(std::to_string(queryInfo.marId));
    ubseXml->AddNode("decoder-id");
    ubseXml->Child("decoder-id")->Text(std::to_string(queryInfo.decoderId));
    ubseXml->AddNode("type");
    ubseXml->Child("type")->Text(std::to_string(queryInfo.type));

    ubseXml->Printer(xmlStr);
    return UBSE_OK;
}

UbseResult ParseRspXml(const std::string &responseStr, std::vector<UbseMamiMemHandleValue> &handleValues)
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
        UBSE_LOG_ERROR << "[MTI_MEM] Add decoder failed, the result of response is " << result << ", " <<
            FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    ubseXml = ubseXml->Next("ub-memory-handles");
    if (ubseXml == nullptr) {
        UBSE_LOG_ERROR << "[MTI] Xml parse handles failed, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    size_t index = 0;
    while (ubseXml->Next("ub-memory-handle", index++) != nullptr) {
        UbseMamiMemHandleValue handleValue{};
        handleValue.hpa = std::stol(ubseXml->Child("hpa")->Text(), nullptr, HEX_BASE);
        UBSE_LOG_DEBUG << "[MTI_MEM] hpa is " << handleValue.hpa;
        handleValue.handle = std::stol(ubseXml->Child("handle")->Text(), nullptr, HEX_BASE);
        UBSE_LOG_DEBUG << "[MTI_MEM] handle is " << handleValue.handle;
        handleValue.size = std::stol(ubseXml->Child("size")->Text());
        UBSE_LOG_DEBUG << "[MTI_MEM] size is " << handleValue.size;
        try {
            handleValue.entryStartIdx = std::stoi(ubseXml->Child("entry-start-idx")->Text());
            handleValue.entryEndIdx = std::stoi(ubseXml->Child("entry-end-idx")->Text());
            handleValue.blockStartIdx = std::stoi(ubseXml->Child("block-start-idx")->Text());
            handleValue.blockEndIdx = std::stoi(ubseXml->Child("block-end-idx")->Text());
        } catch (const std::exception& e) {
            UBSE_LOG_ERROR << "[MTI_MEM] Unexpected exception: " << e.what();
            return UBSE_ERROR;
        }
        handleValue.type = std::stoi(ubseXml->Child("type")->Text());
        UBSE_LOG_DEBUG << "[MTI_MEM] type is " << handleValue.type;
        handleValues.emplace_back(handleValue);
        if (ubseXml->Previous() != UbseXmlError::OK) {
            UBSE_LOG_ERROR << "[MTI_MEM] Find previous xml pointer failed, " << FormatRetCode(UBSE_ERROR);
            return UBSE_ERROR;
        }
    }
    return UBSE_OK;
}

UbseResult UbseLcneDecoderHandle::GetAllMemHandles(const UbseMamiMemHandleQueryInfo &queryInfo,
    std::vector<UbseMamiMemHandleValue> &handleValues)
{
    UbseHttpRequest req{};
    UbseHttpResponse rsp{};

    req.method = "POST";
    req.path = DECODER_HANDLE_URL;
    req.headers.emplace("Accept", "application/yang-data+xml");
    req.headers.emplace("Content-Type", "application/yang-data+xml");

    std::string xmlStr;
    auto res = BuildReqStr(queryInfo, xmlStr);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI_MEM] Build request xml failed, " << FormatRetCode(res);
        return res;
    }
    req.body = xmlStr;
    UBSE_LOG_DEBUG << "[MTI_MEM] Http request body is " << req.body;
    res = UbseHttpModule::HttpSend(host, port, req, rsp);
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

    res = ParseRspXml(rsp.body, handleValues);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI_MEM] Parse http response failed, " << FormatRetCode(res);
        return res;
    }

    return UBSE_OK;
}
}