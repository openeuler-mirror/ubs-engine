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

#include "ubse_lcne_node_info.h"
#include <cstdint>                // for uint32_t, uint8_t
#include "ubse_error.h"           // for UBSE_ERROR, UBSE_OK, UBSE_ERROR_NOMEM
#include "ubse_http_module.h"     // for UbseHttpModule
#include "ubse_logger.h"          // for FormatRetCode, UBSE_DEFINE_THIS_MO...
#include "ubse_pointer_process.h" // for SafeDeleteArray
#include "ubse_xml.h"             // for UbseXml, UbseXmlError // for UbseByteBuffer
#include "securec.h"              // for memcpy_s, EOK
#include "src/adapter_plugins/mti/ubse_lcne_topology.h"

namespace ubse::lcne {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
using namespace ubse::utils;
using namespace ubse::mti;
using namespace ubse::http;
UbseResult UbseLcneNodeInfo::QueryAllLcneIODieInfo(UbseLcneIODieInfoMap& ubseLcneIODieInfoMap)
{
    UbseHttpRequest req;
    UbseHttpResponse rsp;

    req.method = "GET";
    req.path = QUERY_ALL_URI;
    req.headers.emplace("Accept", "application/yang-data+xml");
    req.headers.emplace("Content-Type", "application/yang-data+xml");

    auto res = UbseHttpModule::HttpSend(req, rsp);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] Access the LCNE IO DIE information interface via HTTP failed." << FormatRetCode(res);
        return res;
    }
    if (rsp.status != static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK)) {
        UBSE_LOG_ERROR << "[MTI] Access the LCNE IO DIE information interface failed. The HTTP status code is "
                       << rsp.status;
        return UBSE_ERROR;
    }
    if (rsp.body.empty()) {
        UBSE_LOG_ERROR << "[MTI] LCNE IO DIE response is empty.";
        return UBSE_ERROR;
    }
    // 解析xml响应
    res = ParseIODieInfoQueryAllResponse(rsp.body, ubseLcneIODieInfoMap);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] IO DIE Info Parse XML data is failed.";
        return UBSE_ERROR;
    }
    return res;
}

void ParseIODieInfo(const std::shared_ptr<UbseXml>& ubseXml, UbseLcneIODieInfo& ubseLcneIODieInfo)
{
    ubseLcneIODieInfo.ubControllerEid = ubseXml->Child("bus-controller-eid")->Text();
    ubseLcneIODieInfo.guid = ubseXml->Child("guid")->Text();
    ubseLcneIODieInfo.upi = ubseXml->Child("upi")->Text();
    ubseLcneIODieInfo.primaryCna = ubseXml->Child("primary-cna")->Text();
    ubseLcneIODieInfo.chipTypeStr = ubseXml->Child("ubpu-type")->Text();
    ubseLcneIODieInfo.chipType = StringToUbseDevType(ubseLcneIODieInfo.chipTypeStr);
    ubseLcneIODieInfo.chipStatusStr = ubseXml->Child("iou-status")->Text();
}

std::string UbseLcneIODieInfoMapToString(const UbseLcneIODieInfoMap& devMap)
{
    std::ostringstream oss;
    for (const auto &[iouInfo, info] : devMap) {
        oss << "{" << "Device= "<< iouInfo.slotId << "-" << iouInfo.ubpuId << "-" << iouInfo.iouId
            << ", ubControllerEid= " << info.ubControllerEid
            << ", guid= " << info.guid << ", upi= " << info.upi << ", primaryCna= " << info.primaryCna
            << ", chipType= " << info.chipTypeStr << ", chipStatus= " << info.chipStatusStr << "} ";
        oss << "\n";
    }

    return oss.str();
}

UbseResult UbseLcneNodeInfo::ParseIODieInfoQueryAllResponse(const std::string& responseStr,
                                                            UbseLcneIODieInfoMap& ubseLcneIODieInfoMap)
{
    std::shared_ptr<UbseXml> ubseXml = SafeMakeShared<UbseXml>(responseStr);
    if (ubseXml == nullptr) {
        UBSE_LOG_ERROR << "[MTI] Get ubse xml failed, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }
    const auto ret = ubseXml->Parse();
    if (ret != UbseXmlError::OK) {
        UBSE_LOG_ERROR << "[MTI] Topology resBody parse failed, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    ubseXml = ubseXml->Next("iou-infos");
    if (ubseXml == nullptr) {
        UBSE_LOG_ERROR << "[MTI] Xml parse iou infos failed," << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    int index = 0;
    while (ubseXml->Next("iou-info", index++) != nullptr) {
        std::string slotId = ubseXml->Child("slot-id")->Text();
        std::string nodeId;
        if (!GetCurNodeId(slotId, nodeId)) {
            UBSE_LOG_ERROR << "[MTI] Convert slot id to node id failed, slotId: " << slotId;
            return UBSE_ERROR;
        }
        UbseMtiIouInfo iouInfo(nodeId, ubseXml->Child("ubpu-id")->Text(),
                               ubseXml->Child("iou-id")->Text());
        UbseLcneIODieInfo ubseLcneIODieInfo{};
        ParseIODieInfo(ubseXml, ubseLcneIODieInfo);
        if (ubseLcneIODieInfo.chipStatusStr != "normal") {
            UBSE_LOG_ERROR << "[MTI] iou-status is " << ubseLcneIODieInfo.chipStatusStr;
            return UBSE_ERROR;
        } else {
            ubseLcneIODieInfo.chipStatus = DevStatus::normal;
        }
        ubseLcneIODieInfoMap[iouInfo] = ubseLcneIODieInfo;
        if (ubseXml->Previous() != UbseXmlError::OK) {
            UBSE_LOG_ERROR << "[MTI] Failed to find xml previous, " << FormatRetCode(UBSE_ERROR);
            return UBSE_ERROR;
        }
    }
    UBSE_LOG_DEBUG << "[MTI] IO DIE information printing: " << "\n"
                << UbseLcneIODieInfoMapToString(ubseLcneIODieInfoMap);
    return UBSE_OK;
}
} // namespace ubse::lcne