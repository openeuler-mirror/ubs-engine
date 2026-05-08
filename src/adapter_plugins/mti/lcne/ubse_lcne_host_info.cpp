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

#include "ubse_lcne_host_info.h"
#include <cstdint>  // for uint32_t, uint8_t
#include "ubse_http_module.h"  // for UbseHttpModule
#include "securec.h"  // for memcpy_s, EOK
#include "ubse_context.h"  // for UbseContext
#include "ubse_error.h"  // for UBSE_ERROR, UBSE_OK, UBSE_ERROR_NOMEM
#include "ubse_logger.h"  // for FormatRetCode, UBSE_DEFINE_THIS_MO...
#include "ubse_pointer_process.h"  // for SafeDeleteArray
#include "ubse_xml.h"  // for UbseXml, UbseXmlError // for UbseByteBuffer

namespace ubse::lcne {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
using namespace ubse::utils;
using namespace ubse::http;

UbseResult UbseLcneHostInfo::QueryLcneHostInfo(UbseLcneOSInfo& ubseLcneOSInfo)
{
    UbseHttpRequest req;
    UbseHttpResponse rsp;

    req.method = "GET";
    req.path = QUERY_URI;
    req.headers.emplace("Accept", "application/yang-data+xml");
    req.headers.emplace("Content-Type", "application/yang-data+xml");

    uint32_t res = UbseHttpModule::HttpSend(req, rsp);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] Access the LCNE logic entities interface via HTTP failed. " << FormatRetCode(res);
        return res;
    }
    if (rsp.status != static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK)) {
        UBSE_LOG_ERROR << "[MTI] Access the LCNE logic entities interface failed. "
                       << "The HTTP status code is " << rsp.status;
        return UBSE_ERROR;
    }
    if (rsp.body.empty()) {
        UBSE_LOG_ERROR << "[MTI] LCNE logic entities response is empty.";
        return UBSE_ERROR;
    }
    // 解析xml响应
    res = ParseHostQueryResponse(rsp.body, ubseLcneOSInfo);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] Logic entities Info Parse XML data is failed.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

void ParseHostQuery(const std::shared_ptr<UbseXml>& ubseXml, UbseLcneOSInfo& ubseLcneOSInfo)
{
    ubseLcneOSInfo.busInstanceEid = ubseXml->Child("bus-instance-eid")->Text();
    ubseLcneOSInfo.guid = ubseXml->Child("guid")->Text();
    ubseLcneOSInfo.logicEntityType = ubseXml->Child("type")->Text() == "host" ? LogicEntityType::host :
                                                                                LogicEntityType::guest;
    ubseLcneOSInfo.upi = ubseXml->Child("upi")->Text();
    ubseLcneOSInfo.logicEntityStatus = ubseXml->Child("state")->Text() == "online" ? LogicEntityStatus::online :
                                                                                     LogicEntityStatus::offline;
}

std::string UbseLcneOSInfoToString(const UbseLcneOSInfo& info)
{
    std::ostringstream oss;
    oss << "UbseLcneOSInfo { "
        << "busInstanceEid=" << info.busInstanceEid << ", "
        << "guid=" << info.guid << ", "
        << "type=" << std::to_string(static_cast<int>(info.logicEntityType)) << ", "
        << "upi=" << info.upi << ", "
        << "status=" << std::to_string(static_cast<int>(info.logicEntityStatus)) << " }";
    return oss.str();
}

UbseResult UbseLcneHostInfo::ParseHostQueryResponse(const std::string& responseStr, UbseLcneOSInfo& ubseLcneOSInfo)
{
    std::shared_ptr<UbseXml> ubseXml = SafeMakeShared<UbseXml>(responseStr);
    if (ubseXml == nullptr) {
        UBSE_LOG_ERROR << "[MTI] Get ubse xml failed, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }
    const auto ret = ubseXml->Parse();
    if (ret != UbseXmlError::OK) {
        UBSE_LOG_ERROR << "[MTI] ResBody parse failed, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    ubseXml = ubseXml->Next("logic-entities");
    if (ubseXml == nullptr) {
        UBSE_LOG_ERROR << "[MTI] Xml parse logic entities failed," << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    int index = 0;
    while (ubseXml->Next("logic-entity", index++) != nullptr) {
        if (ubseXml->Child("type")->Text() != "host") {
            continue;
        }
        ParseHostQuery(ubseXml, ubseLcneOSInfo);
        if (ubseXml->Previous() != UbseXmlError::OK) {
            UBSE_LOG_ERROR << "[MTI] Failed to find xml previous, " << FormatRetCode(UBSE_ERROR);
            return UBSE_ERROR;
        }
    }
    UBSE_LOG_DEBUG << "[MTI] logic entities printing="
                   << "\n"
                   << UbseLcneOSInfoToString(ubseLcneOSInfo);
    return UBSE_OK;
}
}  // namespace ubse::lcne
