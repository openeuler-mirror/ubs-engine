/*
* * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
* * ubs-engine is licensed under Mulan PSL v2.
* * You can use this software according to the terms and conditions of the Mulan PSL v2.
* * You may obtain a copy of Mulan PSL v2 at:
* *          http://license.coscl.org.cn/MulanPSL2
* * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
* * See the Mulan PSL v2 for more details.
*/

#include "ubse_topo_cna.h"

#include <string>
#include "securec.h"

#include "ubse_error.h"
#include "ubse_http_module.h"
#include "ubse_logger.h"
#include "ubse_pointer_process.h"
#include "ubse_str_util.h"
#include "ubse_xml.h"

namespace ubse::lcne {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
using namespace ubse::utils;
using namespace ubse::common::def;
using namespace ubse::http;
using namespace ubse::mti;

void OutPutUrmaEidResultToLog(std::vector<LcneNodeCnaInfo>& lcneNodeCnaInfos)
{
    std::ostringstream oss;
    for (auto& item : lcneNodeCnaInfos) {
        oss << "SlotId=" << item.slotId << ", ChipId=" << item.chipId << ", IODieId=" << item.cardId;
        oss << ", busNodeCna=" << item.busNodeCna << "\n";
        for (auto& portInfo : item.ports) {
            oss << " port_id=" << portInfo.portId << ", bus_port_cna=" << portInfo.portCna << "\n";
        }
        oss << "\n";
    }
    auto result = oss.str();
    UBSE_LOG_INFO << "[MTI] CNA Info:"
                  << "\n"
                  << result;
}

bool ValidateCna(std::vector<LcneNodeCnaInfo>& lcneNodeCnaInfos)
{
    for (auto& item : lcneNodeCnaInfos) {
        if (ConvertStrToUint32(item.busNodeCna, item.busNodeCnaUint32, NO_16) != UBSE_OK) {
            return false;
        }
        if (item.busNodeCnaUint32 > std::numeric_limits<uint16_t>::max()) {
            return false;
        }
        for (auto& portInfo : item.ports) {
            if (ConvertStrToUint32(portInfo.portCna, portInfo.portCnaUint32, NO_16) != UBSE_OK) {
                return false;
            }
        }
    }
    return true;
}

UbseResult UbseTopoCna::QueryTopoCna(std::vector<LcneNodeCnaInfo>& lcneNodeCnaInfos)
{
    UbseHttpRequest req;
    UbseHttpResponse rsp;

    req.method = "GET";
    req.path = QUERY_URI;
    req.headers.emplace("Accept", "application/yang-data+xml");
    req.headers.emplace("Content-Type", "application/yang-data+xml");

    auto res = UbseHttpModule::HttpSend(req, rsp);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] Access the LCNE Cna information interface via HTTP failed. " << FormatRetCode(res);
        return res;
    }
    if (rsp.status != static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK)) {
        UBSE_LOG_ERROR << "[MTI] Access the LCNE Cna information interface failed. The HTTP status code is "
                       << rsp.status;
        return UBSE_ERROR;
    }
    if (rsp.body.empty()) {
        UBSE_LOG_ERROR << "[MTI] LCNE Cna response is empty.";
        return UBSE_ERROR;
    }

    auto ret = ParseTopoCnaRsp(rsp.body, lcneNodeCnaInfos);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] Cna Info Parse XML data is failed. " << FormatRetCode(ret);
        return ret;
    }
    // 校验cna数据合法性（4位16进制），校验成功则转换为uint32
    if (!ValidateCna(lcneNodeCnaInfos)) {
        UBSE_LOG_ERROR << "[MTI] Validation failed after parsing CNA response";
        return UBSE_ERROR; // 拒绝非法数据
    }
    return UBSE_OK;
}

UbseResult UbseTopoCna::ParseTopoCnaRsp(std::string& resBody, std::vector<LcneNodeCnaInfo>& lcneNodeCnaInfos)
{
    std::shared_ptr<UbseXml> ubseXml = SafeMakeShared<UbseXml>(resBody);
    if (ubseXml == nullptr) {
        return UBSE_ERROR_NOMEM;
    }
    const auto ret = ubseXml->Parse();
    if (ret != UbseXmlError::OK || ubseXml == nullptr) {
        UBSE_LOG_ERROR << "[MTI] Topology resBody parse failed.";
        return UBSE_ERROR;
    }
    ubseXml = ubseXml->Next("addresses");
    if (ubseXml == nullptr) {
        UBSE_LOG_ERROR << "[MTI] topology xml parse addresses failed.";
        return UBSE_ERROR;
    }
    size_t index = 0;
    while (ubseXml->Next("address", index) != nullptr) {
        LcneNodeCnaInfo lcneNodeCnaInfo{};
        lcneNodeCnaInfo.slotId = ubseXml->Child("slot")->Text();
        lcneNodeCnaInfo.chipId = ubseXml->Child("ubpu")->Text();
        lcneNodeCnaInfo.cardId = ubseXml->Child("iou")->Text();
        lcneNodeCnaInfo.busNodeCna = ubseXml->Child("bus-primary-cna")->Text();
        ubseXml = ubseXml->Next("physical-ports");
        if (ubseXml == nullptr) {
            UBSE_LOG_ERROR << "[MTI] topology xml parse ports failed.";
            return UBSE_ERROR;
        }

        size_t innerIndex = 0;
        while (ubseXml->Next("physical-port", innerIndex) != nullptr) {
            LcnePortCnaInfo lcnePortCnaInfo{};
            lcnePortCnaInfo.portId = ubseXml->Child("physical-port-id")->Text();
            lcnePortCnaInfo.portCna = ubseXml->Child("bus-port-cna")->Text();
            lcneNodeCnaInfo.ports.emplace_back(lcnePortCnaInfo);
            ubseXml->Previous();
            innerIndex++;
        }
        UBSE_LOG_DEBUG << "[MTI] The current slot_id is " << lcneNodeCnaInfo.slotId << " and its port num is "
                       << innerIndex;
        index++;
        lcneNodeCnaInfos.emplace_back(lcneNodeCnaInfo);
        ubseXml->Previous();
        ubseXml->Previous();
    }
    OutPutUrmaEidResultToLog(lcneNodeCnaInfos);
    return UBSE_OK;
}

} // namespace ubse::lcne