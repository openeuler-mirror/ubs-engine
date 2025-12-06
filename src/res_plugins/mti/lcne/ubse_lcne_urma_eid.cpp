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

#include <iostream>
#include <httplib.h>
#include <cstdint>                // for uint32_t, uint8_t
#include "securec.h"              // for memcpy_s, EOK

#include "ubse_context.h"         // for UbseContext
#include "ubse_error.h"           // for UBSE_ERROR, UBSE_OK, UBSE_ERROR_NOMEM
#include "ubse_http_module.h"     // for UbseHttpModule
#include "ubse_logger.h"          // for FormatRetCode, UBSE_DEFINE_THIS_MO...
#include "ubse_logger_inner.h"    // for RM_LOG_ERROR
#include "ubse_pointer_process.h" // for SafeDeleteArray
#include "ubse_xml.h"             // for UbseXml, UbseXmlError // for UbseByteBuffer
#include "ubse_lcne_urma_eid.h"   // for Lcne_urma

namespace ubse::lcne {
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_LCNE_MID);
using namespace ubse::log;
using namespace ubse::utils;
using namespace ubse::mti;

void OutPutUrmaEidResultToLog(std::map<DevName, UbseLcneSocketInfo> &urmaEidMap)
{
    std::ostringstream oss;
    for (auto &item : urmaEidMap) {
        oss << "DevName:" << item.first.devName << ", " << "PrimaryEid:" << item.second.primaryEid << "\n";
        for (auto &portEid : item.second.portEidList) {
            oss << "portId:" << portEid.first << ", " << portEid.second.urmaEid << "\n";
        }
        oss << "\n";
    }
    auto result = oss.str();
    UBSE_LOG_INFO << "[MTI] UrmaEid Info:" << "\n" << result;
}

// 校验单个 urma-eid 字符串
bool IsValidUrmaEid(const std::string &eid)
{
    if (eid.empty()) {
        return false;
    }

    std::istringstream iss(eid);
    std::string part;
    int count = 0;

    // 使用 getline 分割冒号
    while (std::getline(iss, part, ':')) {
        // 检查是否正好4个字符
        if (part.length() != 4) {
            return false;
        }

        // 检查每个字符是否为合法十六进制
        for (char c : part) {
            if (!std::isxdigit(c)) { // isxdigit 包含 0-9, a-f, A-F
                return false;
            }
        }

        count++;
    }

    // 必须正好8组
    return count == 8;
}

// 校验整个 socketInfoMap
UbseResult ValidateAllComEid(const std::map<DevName, UbseLcneSocketInfo> &socketInfoMap)
{
    for (const auto &pair : socketInfoMap) {
        const auto &socketInfo = pair.second;

        // 校验 primaryEid
        if (!socketInfo.primaryEid.empty() && !IsValidUrmaEid(socketInfo.primaryEid)) {
            return UBSE_ERROR_INVAL;
        }

        // 校验 portEidList 中的每个 urmaEid
        for (const auto &portPair : socketInfo.portEidList) {
            const std::string &urmaEid = portPair.second.urmaEid;

            if (!urmaEid.empty() && !IsValidUrmaEid(urmaEid)) {
                return UBSE_ERROR_INVAL;
            }
        }
    }
    return UBSE_OK;
}

UbseResult UbseLcneUrmaEid::GetUrmaEid(std::map<DevName, UbseLcneSocketInfo> &allSocketComEid)
{
    UbseHttpRequest req;
    UbseHttpResponse rsp;

    req.method = "GET";
    req.path = GET_URMA_EID_URI;
    req.headers.emplace("Accept", "application/yang-data+xml");
    req.headers.emplace("Content-Type", "application/yang-data+xml");

    auto res = UbseHttpModule::HttpSend(host_, port_, req, rsp);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] Access the LCNE UrmaEid information interface via HTTP is failed. "
                     << FormatRetCode(res);
        return res;
    }
    if (rsp.status != static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK)) {
        UBSE_LOG_ERROR << "[MTI] Access the LCNE UrmaEid information interface is failed.The HTTP status code is "
                     << rsp.status;
        return UBSE_ERROR;
    }
    if (rsp.body.empty()) {
        UBSE_LOG_ERROR << "[MTI] LCNE UrmaEid response is empty.";
        return UBSE_ERROR;
    }
    res = ParseGetUrmaEidResponse(rsp.body, allSocketComEid);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] Failed to parse response body for get urma eid";
        return res;
    }
    // 解析后立即校验
    res = ValidateAllComEid(allSocketComEid);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] Validation failed after parsing URMA EID response";
        return res; // 拒绝非法数据
    }
    OutPutUrmaEidResultToLog(allSocketComEid);
    return UBSE_OK;
}

UbseResult UbseLcneUrmaEid::ParseGetUrmaEidResponse(const std::string &responseStr,
                                                    std::map<DevName, UbseLcneSocketInfo> &ss)
{
    std::map<DevName, UbseLcneSocketInfo> socketInfoMap{};
    std::shared_ptr<UbseXml> ubseXml = SafeMakeShared<UbseXml>(responseStr);
    if (ubseXml == nullptr) {
        return UBSE_ERROR_NOMEM;
    }
    const auto ret = ubseXml->Parse();
    if (ret != UbseXmlError::OK) {
        return UBSE_ERROR;
    }
    ubseXml = ubseXml->Next("static-urma-eids");
    if (ubseXml == nullptr) {
        return UBSE_ERROR;
    }
    int staticUrmaEidsIndex = 0;
    while (ubseXml->Next("static-urma-eid", staticUrmaEidsIndex) != nullptr) {
        DevName devName(ubseXml->Child("slot-id")->Text(), ubseXml->Child("ubpu-id")->Text());
        if (socketInfoMap.find(devName) != socketInfoMap.end()) {
            ubseXml->Previous();
            staticUrmaEidsIndex++;
            continue;
        }
        ubseXml = ubseXml->Next("urma-eid-infos");
        if (ubseXml == nullptr) {
            return UBSE_ERROR;
        }
        UbseLcneSocketInfo socketInfo{};
        int urmaEidInfoIndex = 0;
        while (ubseXml->Next("urma-eid-info", urmaEidInfoIndex) != nullptr) {
            if (ubseXml->Next("physical-port") != nullptr) {
                ubseXml->Previous();
                std::string port = ubseXml->Child("physical-port")->Text();
                std::string urmaEid = ubseXml->Child("urma-eid")->Text();
                socketInfo.portEidList[port].urmaEid = urmaEid;
            } else if (ubseXml->Next("port-group-id") != nullptr) {
                ubseXml->Previous();
                std::string primaryEid = ubseXml->Child("urma-eid")->Text();
                socketInfo.primaryEid = primaryEid;
            }
            ubseXml->Previous();
            urmaEidInfoIndex++;
        }
        socketInfoMap[devName] = socketInfo;
        ubseXml->Previous();
        ubseXml->Previous();
        staticUrmaEidsIndex++;
    }
    ss = socketInfoMap;
    return UBSE_OK;
}
} // namespace ubse::lcne