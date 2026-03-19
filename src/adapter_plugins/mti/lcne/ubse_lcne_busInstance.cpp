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
#include "ubse_lcne_busInstance.h"

#include <cstdint>   // for uint32_t, uint8_t // for nothrow
#include "securec.h" // for memcpy_s, EOK

#include "ubse_error.h"           // for UBSE_ERROR, UBSE_OK, UBSE_ERROR_NOMEM
#include "ubse_http_module.h"     // for UbseHttpModule
#include "ubse_logger.h"          // for FormatRetCode, UBSE_DEFINE_THIS_MO...
#include "ubse_pointer_process.h" // for SafeDeleteArray
#include "ubse_xml.h"             // for UbseXml, UbseXmlError // for UbseByteBuffer
#include "ubse_str_util.h"

namespace ubse::lcne {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
using namespace ubse::utils;

std::unique_ptr<UbseLcneBusInstance> UbseLcneBusInstance::instance = nullptr;

// 校验获取到的slotId是否为uint32
UbseResult ValidateSlotId(const UbseLcneBusInstanceInfo &busInstanceInfo)
{
    uint32_t v{};
    return ConvertStrToUint32(busInstanceInfo.localNodeId, v);
}

UbseResult UbseLcneBusInstance::QueryBusinstance(UbseLcneBusInstanceInfo &busInstanceInfo)
{
    UbseHttpRequest req;
    UbseHttpResponse rsp;

    req.method = "GET";
    req.path = "/restconf/data/huawei-vbussw-inventory:vbussw-inventory/logic-entity-mappings";
    req.headers.emplace("Accept", "application/yang-data+xml");
    req.headers.emplace("Content-Type", "application/yang-data+xml");

    auto res = UbseHttpModule::HttpSend(req, rsp);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] Access the LCNE BusInstance information interface via HTTP failed. "
                       << FormatRetCode(res);
        return res;
    }
    if (rsp.status != static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK)) {
        UBSE_LOG_ERROR << "[MTI] Access the LCNE BusInstance information interface failed. The HTTP status code is "
                       << rsp.status;
        return UBSE_ERROR;
    }
    if (rsp.body.empty()) {
        UBSE_LOG_ERROR << "[MTI] LCNE BusInstance response is empty.";
        return UBSE_ERROR;
    }

    res = ParseQueryBusinstanceResponse(rsp.body, busInstanceInfo);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] Failed to parse response body for query eid and guid.";
        return res;
    }

    // 解析后立即校验
    res = ValidateSlotId(busInstanceInfo);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] Validation failed after parsing busInstanceInfo";
        return res; // 拒绝非法数据
    }
    return UBSE_OK;
}

UbseResult UbseLcneBusInstance::ParseQueryBusinstanceResponse(const std::string &responseStr,
                                                              UbseLcneBusInstanceInfo &busInstanceInfo)
{
    // 解析报文,获取eid,guid
    std::shared_ptr<UbseXml> ubseXml = SafeMakeShared<UbseXml>(responseStr);
    if (ubseXml == nullptr) {
        return UBSE_ERROR_NOMEM;
    }
    auto ret = ubseXml->Parse();
    if (ret != UbseXmlError::OK) {
        UBSE_LOG_ERROR << "ParseQueryBusinstanceResponse parse failed.";
        return UBSE_ERROR;
    }
    ubseXml = ubseXml->Next("logic-entity-mappings");
    if (ubseXml == nullptr) {
        return UBSE_ERROR;
    }
    ubseXml = ubseXml->Next("logic-entity-mapping");
    if (ubseXml == nullptr) {
        return UBSE_ERROR;
    }
    busInstanceInfo.hostBusinstanceEid = ubseXml->Child("host-bus-instance-eid")->Text();
    ubseXml = ubseXml->Next("physical-entity-mappings");
    if (ubseXml == nullptr) {
        return UBSE_ERROR;
    }
    ubseXml = ubseXml->Next("physical-entity-mapping");
    if (ubseXml == nullptr) {
        return UBSE_ERROR;
    }
    busInstanceInfo.localNodeId = ubseXml->Child("slot-id")->Text();
    UBSE_LOG_DEBUG << "[MTI] " << "BusInstanceInfo.hostBusinstanceEid=" << busInstanceInfo.hostBusinstanceEid << ", "
                   << "BusInstanceInfo.localNodeId=" << busInstanceInfo.localNodeId;
    return UBSE_OK;
}

} // namespace ubse::lcne