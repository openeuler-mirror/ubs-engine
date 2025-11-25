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

#include "ubse_lcne_decoder_specification.h"

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

UbseResult BuildReqStr(const UbseMamiMemDecoderSetInfo &setInfo, std::string &xmlStr)
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

    ubseXml->AddNode("ub-memory-decoder-specification");
    ubseXml->Attr("xmlns", "urn:huawei:yang:huawei-vbussw-service");
    ubseXml->AddNode("slot-id");
    ubseXml->Child("slot-id")->Text(curNodeInfo.nodeId);
    ubseXml->AddNode("ubpu-id");
    ubseXml->Child("ubpu-id")->Text(std::to_string(setInfo.ubpuId));
    ubseXml->AddNode("iou-id");
    ubseXml->Child("iou-id")->Text(std::to_string(setInfo.iouId));
    ubseXml->AddNode("mar-id");
    ubseXml->Child("mar-id")->Text(std::to_string(setInfo.marId));
    ubseXml->AddNode("decoder-num");
    ubseXml->Child("decoder-num")->Text(std::to_string(setInfo.decoderNum));

    ubseXml->AddNode("decoder-infos");
    ubseXml->Next("decoder-infos");
    for (int i = 0; i < setInfo.decoder.size(); i++) {
        const auto decoder = &setInfo.decoder[i];
        ubseXml->AddNode("decoder-info");
        ubseXml->Next("decoder-info", i);
        ubseXml->AddNode("decoder-id");
        ubseXml->Child("decoder-id")->Text(std::to_string(decoder->decoderId));
        ubseXml->AddNode("entry-offset");
        ubseXml->Child("entry-offset")->Text(std::to_string(decoder->entryOffset));
        ubseXml->AddNode("entry-size");
        ubseXml->Child("entry-size")->Text(std::to_string(decoder->entrySize));
        ubseXml->AddNode("cfg-type");
        ubseXml->Child("cfg-type")->Text(std::to_string(decoder->cfgType));
        ubseXml->Previous();
    }
    ubseXml->Printer(xmlStr);
    return UBSE_OK;
}

UbseResult ParseRspXml(const std::string &responseStr)
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
    return UBSE_OK;
}

UbseResult UbseLcneDecoderSpecification::SetDecoderSpecification(const UbseMamiMemDecoderSetInfo &setInfo)
{
    UbseHttpRequest req{};
    UbseHttpResponse rsp{};

    req.method = "POST";
    req.path = DECODER_SPECIFICATION_URL;
    req.headers.emplace("Accept", "application/yang-data+xml");
    req.headers.emplace("Content-Type", "application/yang-data+xml");

    std::string xmlStr;
    auto res = BuildReqStr(setInfo, xmlStr);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI_MEM] Build request xml failed, " << FormatRetCode(res);
        return res;
    }
    UBSE_LOG_DEBUG << "[MTI_MEM] Http require body is " << xmlStr;
    req.body = xmlStr;

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

    res = ParseRspXml(rsp.body);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI_MEM] Parse http response failed, " << FormatRetCode(res);
        return res;
    }
    UBSE_LOG_DEBUG << "[MTI_MEM] SetDecoderSpecification success ";
    return UBSE_OK;
}
}