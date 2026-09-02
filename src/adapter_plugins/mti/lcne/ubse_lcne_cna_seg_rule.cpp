/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_lcne_cna_seg_rule.h"

#include <cstdint>
#include <iostream>
#include <tuple>
#include <vector>

#include "ubse_error.h"
#include "ubse_http_module.h"
#include "ubse_logger.h"
#include "ubse_str_util.h"
#include "ubse_xml.h"
#include "adapter_plugins/mti/ubse_mti_def.h"

namespace ubse::lcne {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
using namespace ubse::utils;
using namespace ubse::http;
using namespace ubse::adapter_plugins::mti;

UbseResult ValidCnaBitRange(const std::vector<CnaBitRange>& cnaRules)
{
    if (cnaRules.empty()) {
        UBSE_LOG_ERROR << "[MTI] Address segment rule response parse failed, cnaRules is empty.";
        return UBSE_ERROR;
    }
    std::ostringstream oss;
    oss << "[MTI] Address segment rule response parse: ";
    for (const auto& cnaRange : cnaRules) {
        auto startBit = std::get<0>(cnaRange);
        auto endBit = std::get<1>(cnaRange);
        auto baseVal = std::get<2>(cnaRange);
        oss << "{" << static_cast<uint32_t>(startBit) << ", " << static_cast<uint32_t>(endBit) << ", "
            << static_cast<uint32_t>(baseVal) << "} ";
    }
    UBSE_LOG_INFO << oss.str();
    return UBSE_OK;
}

UbseResult UbseLcneCnaSegRule::QueryCnaRule(std::vector<CnaBitRange>& cnaRules)
{
    UbseHttpRequest req;
    UbseHttpResponse rsp;

    req.method = "GET";
    req.path = QUERY_URI;
    req.headers.emplace("Accept", "application/yang-data+xml");
    req.headers.emplace("Content-Type", "application/yang-data+xml");

    auto res = UbseHttpModule::HttpSend(req, rsp);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] Access the LCNE address segment rule interface via HTTP failed. "
                       << FormatRetCode(res);
        return res;
    }
    if (rsp.status != static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK)) {
        UBSE_LOG_ERROR << "[MTI] Access the LCNE address segment rule interface failed. The HTTP status code is "
                       << rsp.status;
        return UBSE_ERROR;
    }
    if (rsp.body.empty()) {
        UBSE_LOG_ERROR << "[MTI] LCNE address segment rule response is empty.";
        return UBSE_ERROR;
    }
    res = ParseCnaRule(rsp.body, cnaRules);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] Address segment rule parse XML data is failed.";
        return UBSE_ERROR;
    }
    res = ValidCnaBitRange(cnaRules);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] Address segment rule response validate failed.";
        return UBSE_ERROR;
    }

    return UBSE_OK;
}

UbseResult UbseLcneCnaSegRule::ParseCnaRule(const std::string& responseStr, std::vector<CnaBitRange>& cnaRules)
{
    // 接口返回xml格式，详见test/UT/adapter_plugins/mti/test_ubse_lcne_cna_seg_rule.cpp:32-76
    std::shared_ptr<UbseXml> ubseXml = UbseXml::Create(responseStr);
    if (ubseXml == nullptr) {
        UBSE_LOG_ERROR << "[MTI] Create ubse xml failed, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }
    const auto ret = ubseXml->Parse();
    if (ret != UbseXmlError::OK) {
        UBSE_LOG_ERROR << "[MTI] Address segment rule response parse failed, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }
    ubseXml = ubseXml->Next("address-segment-rule");
    if (ubseXml == nullptr) {
        UBSE_LOG_ERROR << "[MTI] Xml parse address-segment-rule failed, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    cnaRules.clear();

    // server-index位段（必选），基础值偏移为0
    std::shared_ptr<UbseXml> serverIndex = ubseXml->Next("server-index");
    if (serverIndex == nullptr) {
        UBSE_LOG_ERROR << "[MTI] Xml parse server-index failed, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }
    uint16_t serverStart = 0;
    uint16_t serverEnd = 0;
    if (ConvertStrToUint16(serverIndex->Child("start-bit")->Text(), serverStart) != UBSE_OK ||
        ConvertStrToUint16(serverIndex->Child("end-bit")->Text(), serverEnd) != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] Parse server-index bit range failed, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }
    cnaRules.emplace_back(static_cast<uint8_t>(serverStart), static_cast<uint8_t>(serverEnd), static_cast<uint8_t>(0));
    ubseXml->Previous();

    // group-id位段（可选），基础值偏移为1
    std::shared_ptr<UbseXml> groupId = ubseXml->Next("group-id");
    if (groupId != nullptr) {
        uint16_t groupStart = 0;
        uint16_t groupEnd = 0;
        if (ConvertStrToUint16(groupId->Child("start-bit")->Text(), groupStart) != UBSE_OK ||
            ConvertStrToUint16(groupId->Child("end-bit")->Text(), groupEnd) != UBSE_OK) {
            UBSE_LOG_ERROR << "[MTI] Parse group-id bit range failed, " << FormatRetCode(UBSE_ERROR);
            return UBSE_ERROR;
        }
        cnaRules.emplace_back(static_cast<uint8_t>(groupStart), static_cast<uint8_t>(groupEnd),
                              static_cast<uint8_t>(1));
        ubseXml->Previous();
    }
    return UBSE_OK;
}
} // namespace ubse::lcne
