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

#include "ubse_lcne_vfe_eid.h" // for Lcne_urma
#include "ubse_error.h"
#include "ubse_logger.h"          // for FormatRetCode, UBSE_DEFINE_THIS_MO...
#include "ubse_logger_inner.h"    // for RM_LOG_ERROR
#include "ubse_pointer_process.h" // for SafeDeleteArray
#include "ubse_urma.h"
#include "ubse_xml.h" // for UbseXml, UbseXmlError // for UbseByteBuffer
#include "ubse_http_module.h" // for UbseHttpModule

namespace ubse::lcne {
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_LCNE_MID);
using namespace common::def;
using namespace ubse::http;
using namespace ubse::mti;
using namespace ubse::urma;
using namespace ubse::log;

UbseResult UbseLcneVfeEid::GetVfeEid(UbseLcneIouInfo iouInfo, std::vector<UbseFeInfo> &allFeInfos)
{
    /* 第一步先下发消息查询消息获取所有Vfe列表 */
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    req.method = "GET";
    req.path = GET_VFE_LIST_URI + "/mue-ue-binding-info=" + iouInfo.slotId + "," + iouInfo.ubpuId + "," + iouInfo.iouId;
    req.headers.emplace("Accept", "application/yang-data+xml");
    req.headers.emplace("Content-Type", "application/yang-data+xml");
    auto ret = UbseHttpModule::HttpSend(host, port, req, rsp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] UbseHttpModule::HttpSend failed. " << FormatRetCode(ret);
        return ret;
    }
    if (rsp.status != static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK)) {
        UBSE_LOG_ERROR << "[MTI] GetVfeEid List information interface is failed.The HTTP status code is " << rsp.status;
        return UBSE_ERROR;
    }
    if (rsp.body.empty()) {
        UBSE_LOG_ERROR << "[MTI] LCNE UrmaEid response is empty.";
        return UBSE_ERROR;
    }
    ret = ParseGetFeListResponse(rsp.body, allFeInfos);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] ParseGetFeListResponse fail.";
        return ret;
    }
    /* 然后再下发消息更新vfe中的emid数据 */
    return UpdateVfeEid(iouInfo, allFeInfos);
}
UbseResult UbseLcneVfeEid::UpdateVfeEid(UbseLcneIouInfo iouInfo, std::vector<UbseFeInfo> &allFeInfos)
{
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    req.method = "GET";
    req.path = GET_VFE_EID_URI + "/entity-urma-communication-info=" + iouInfo.slotId + "," + iouInfo.ubpuId + "," +
               iouInfo.iouId;
    req.headers.emplace("Accept", "application/yang-data+xml");
    req.headers.emplace("Content-Type", "application/yang-data+xml");
    auto ret = UbseHttpModule::HttpSend(host, port, req, rsp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] UbseHttpModule::HttpSend failed. " << FormatRetCode(ret);
        return ret;
    }
    if (rsp.status != static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK)) {
        UBSE_LOG_ERROR << "[MTI] UpdateVfeEid information is failed.The HTTP status code is " << rsp.status;
        return UBSE_ERROR;
    }
    if (rsp.body.empty()) {
        UBSE_LOG_ERROR << "[MTI] LCNE UrmaEid response is empty.";
        return UBSE_ERROR;
    }
    return ParseGetFeEidResponse(rsp.body, allFeInfos);
}
UbseResult UbseLcneVfeEid::ParseGetFeListResponse(const std::string &responseStr, std::vector<UbseFeInfo> &allFeInfos)
{
    std::shared_ptr<UbseXml> ubseXml = SafeMakeShared<UbseXml>(responseStr);
    if (ubseXml == nullptr) {
        return UBSE_ERROR_NOMEM;
    }
    const auto ret = ubseXml->Parse();
    if (ret != UbseXmlError::OK) {
        return UBSE_ERROR;
    }
    std::string slotId = ubseXml->Child("slot-id")->Text();
    std::string ubpuId = ubseXml->Child("ubpu-id")->Text();
    std::string iouId = ubseXml->Child("iou-id")->Text();
    ubseXml = ubseXml->Next("mue-ue-bindings");
    if (ubseXml == nullptr) {
        return UBSE_ERROR;
    }
    while (ubseXml->Next("mue-ue-binding") != nullptr) {
        std::string ueIdlist = ubseXml->Child("ue-id")->Text();
        std::vector<std::string> ueId = ueIdlistSplit(ueIdlist, " ");
        for (const auto &i : ueId) {
            UbseFeInfo ubseFeInfo;
            ubseFeInfo.slotId = slotId;
            ubseFeInfo.ubpuId = ubpuId;
            ubseFeInfo.iouId = iouId;
            ubseFeInfo.entityId = i;
            ubseFeInfo.fetype = FeType::VIRTUAL_TYPE;
            allFeInfos.emplace_back(ubseFeInfo);
        }
    }
    return UBSE_OK;
}

std::vector<std::string> UbseLcneVfeEid::ueIdlistSplit(const std::string &str, const std::string &delimiter)
{
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = 0;
    while ((end = str.find(delimiter, start)) != std::string::npos) {
        tokens.push_back(str.substr(start, end - start));
        start = end + delimiter.length();
    }
    // 添加最后一个token
    tokens.push_back(str.substr(start));
    return tokens;
}

UbseResult UbseLcneVfeEid::ParseGetFeEidResponse(const std::string &responseStr, std::vector<UbseFeInfo> &allFeInfos)
{
    std::shared_ptr<UbseXml> ubseXml = SafeMakeShared<UbseXml>(responseStr);
    if (ubseXml == nullptr) {
        return UBSE_ERROR_NOMEM;
    }
    const auto ret = ubseXml->Parse();
    if (ret != UbseXmlError::OK) {
        return UBSE_ERROR;
    }
    std::string slotId = ubseXml->Child("slot-id")->Text();
    std::string ubpuId = ubseXml->Child("ubpu-id")->Text();
    std::string iouId = ubseXml->Child("iou-id")->Text();
    ubseXml = ubseXml->Next("urma-communication-entity-ids");
    if (ubseXml == nullptr) {
        return UBSE_ERROR;
    }
    while (ubseXml->Next("urma-communication-entity-id") != nullptr) {
        std::string entityId = ubseXml->Child("entity-id")->Text();
        UbseFeInfo *ubseFeInfo = FindVfeInVector(slotId, ubpuId, iouId, entityId, allFeInfos);
        if (ubseFeInfo == nullptr) {
            continue;
        }
        std::shared_ptr<UbseXml> ubseEidXml = ubseXml->Child("urma-communication-infos");
        uint32_t res = ParseFeEidXml(ubseEidXml, *ubseFeInfo);
        if (res != UBSE_OK) {
            return res;
        }
    }
    return UBSE_OK;
}

UbseResult UbseLcneVfeEid::ParseFeEidXml(std::shared_ptr<UbseXml> ubseEidXml, UbseFeInfo &FeInfo)
{
    while (ubseEidXml->Next("urma-communication-info") != nullptr) {
        std::string eid = ubseEidXml->Child("urma-eid")->Text();
        if (ubseEidXml->Child("port-group-id") != nullptr) {
            uint32_t portId = std::stoul(ubseEidXml->Child("port-group-id")->Text());
            FeInfo.primaryEid.insert({portId, eid});
        } else if (ubseEidXml->Child("interface-name") != nullptr) {
            std::string interfaceName = ubseEidXml->Child("interface-name")->Text();
            uint32_t portId;
            auto ret = GetPortIdFromInterfaceName(interfaceName, portId);
            if (ret != UBSE_OK) {
                return ret;
            }
            FeInfo.portEidInfos.insert({portId, eid});
        } else {
            return UBSE_ERROR;
        }
    }
    return UBSE_OK;
}

UbseFeInfo *UbseLcneVfeEid::FindVfeInVector(std::string slotId, std::string ubpuId, std::string iouId,
                                            std::string entityId, std::vector<UbseFeInfo> &allFeInfos)
{
    for (auto &fe : allFeInfos) {
        if ((fe.slotId == slotId) && (fe.ubpuId == ubpuId) && (fe.iouId == iouId) && (fe.entityId == entityId)) {
            return &fe; // 返回指针
        }
    }
    return nullptr; // 明确表示未找到
}

UbseResult UbseLcneVfeEid::GetPortIdFromInterfaceName(std::string intfaceName, uint32_t &portId)
{
    // 查找最后一个'/'的位置
    size_t lastSlashPos = intfaceName.find_last_of('/');
    if (lastSlashPos != std::string::npos) {
        // 提取'/'后面的部分
        std::string portStr = intfaceName.substr(lastSlashPos + 1);
        portId = std::stoul(portStr) - 1;
        return UBSE_OK;
    }
    return UBSE_ERROR;
}

} // namespace ubse::lcne