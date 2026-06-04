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

#include "ubse_lcne_fe_eid.h" // for Lcne_urma
#include "ubse_error.h"
#include "ubse_http_module.h"       // for UbseHttpModule
#include "ubse_logger.h"            // for FormatRetCode, UBSE_DEFINE_THIS_MO...
#include "ubse_mti_eid_interface.h" // for ParseBaseEid
#include "ubse_pointer_process.h"   // for SafeDeleteArray
#include "ubse_str_util.h"          // for ConvertStrToUint32
#include "ubse_xml.h"               // for UbseXml, UbseXmlError // for UbseByteBuffer
#include "adapter_plugins/mti/ubse_mti_def.h"

namespace ubse::lcne {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace common::def;
using namespace ubse::http;
using namespace ubse::adapter_plugins::mti;
using namespace ubse::log;
using namespace ubse::utils;

UbseResult CheckFeEid(std::vector<UbseMtiFeInfo>& allFeInfos)
{
    for (auto& item : allFeInfos) {
        std::string nodeId;
        if (!GetCurNodeId(item.slotId, nodeId) || nodeId.empty()) {
            UBSE_LOG_ERROR << "[MTI] Failed to convert slotId to nodeId, slotId=" << item.slotId;
            return UBSE_ERROR;
        }
        item.slotId = nodeId;
        uint32_t entityId;
        if (ConvertStrToUint32(item.entityId, entityId) != UBSE_OK) {
            UBSE_LOG_ERROR << "[MTI] Failed to convert entityId to uint32_t. entityId=" << item.entityId;
            return UBSE_ERROR;
        }
        for (auto& eidGroup : item.eidGroups) {
            if (ConvertStrToUint32(eidGroup.entityId, entityId) != UBSE_OK) {
                UBSE_LOG_ERROR << "[MTI] Failed to convert entityId to uint32_t. entityId=" << eidGroup.entityId;
                return UBSE_ERROR;
            }
        }
    }
    return UBSE_OK;
}

UbseResult UbseLcneFeEid::UpdateFeType(UbseMtiIouInfo& iouInfo, std::vector<UbseMtiFeInfo>& allFeInfos)
{
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    req.method = "GET";
    req.path = GET_FE_LIST_URI + "/mue-ue-binding-info=" + iouInfo.slotId + "," + iouInfo.ubpuId + "," + iouInfo.iouId;
    req.headers.emplace("Accept", "application/yang-data+xml");
    req.headers.emplace("Content-Type", "application/yang-data+xml");
    auto ret = UbseHttpModule::HttpSend(req, rsp);
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
    ret = ParseFeTypeListResponse(rsp.body, allFeInfos);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] Failed to parse response body for fe list.";
        return ret;
    }
    return UBSE_OK;
}

UbseResult UbseLcneFeEid::GetComUrmaEidClos(UbseMtiIouInfo& iouInfo, UbseMtiEidGroup& feInfo)
{
    std::vector<UbseMtiFeInfo> allFeInfos;
    auto ret = GetFeEid(iouInfo, allFeInfos);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] Failed to get fe eid.";
        return UBSE_ERROR;
    }

    ret = GetComEidInfo(allFeInfos, feInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] Failed to get com fe eid from all fes.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseLcneFeEid::GetFeEid(UbseMtiIouInfo& iouInfo, std::vector<UbseMtiFeInfo>& allFeInfos)
{
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    req.method = "GET";
    std::string slotId;
    req.path = GET_FE_EID_URI + "/entity-urma-communication-info=" + iouInfo.slotId + "," + iouInfo.ubpuId + "," +
               iouInfo.iouId;
    req.headers.emplace("Accept", "application/yang-data+xml");
    req.headers.emplace("Content-Type", "application/yang-data+xml");
    auto ret = UbseHttpModule::HttpSend(req, rsp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] UbseHttpModule::HttpSend failed. " << FormatRetCode(ret);
        return ret;
    }
    if (rsp.status != static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK)) {
        UBSE_LOG_ERROR << "[MTI] UpdateFeEid information is failed. The HTTP status code is " << rsp.status;
        return UBSE_ERROR;
    }
    if (rsp.body.empty()) {
        UBSE_LOG_ERROR << "[MTI] LCNE UrmaEid response is empty.";
        return UBSE_ERROR;
    }
    if (ParseGetFeEidResponse(rsp.body, allFeInfos) != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] Failed to parse response body for get fe eid.";
        return UBSE_ERROR;
    }
    if (UpdateFeType(iouInfo, allFeInfos) != UBSE_OK) {
        UBSE_LOG_WARN << "[MTI] Failed to update fe type.";
    }
    if (CheckFeEid(allFeInfos) != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] Failed to check fe eid.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseLcneFeEid::ExtractBasicInfoFromXml(const std::shared_ptr<UbseXml>& ubseXml, UbseMtiIouInfo& iouInfo)
{
    iouInfo.slotId = ubseXml->Child("slot-id")->Text();
    if (iouInfo.slotId.empty()) {
        UBSE_LOG_ERROR << "[MTI] Xml parse slot-id failed.";
        return UBSE_ERROR;
    }
    iouInfo.ubpuId = ubseXml->Child("ubpu-id")->Text();
    if (iouInfo.ubpuId.empty()) {
        UBSE_LOG_ERROR << "[MTI] Xml parse ubpu-id failed.";
        return UBSE_ERROR;
    }
    iouInfo.iouId = ubseXml->Child("iou-id")->Text();
    if (iouInfo.iouId.empty()) {
        UBSE_LOG_ERROR << "[MTI] Xml parse iou-id failed.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseLcneFeEid::ParseFeTypeListResponse(const std::string& responseStr,
                                                  std::vector<UbseMtiFeInfo>& allFeInfos)
{
    std::shared_ptr<UbseXml> ubseXml = UbseXml::Create(responseStr);
    if (ubseXml == nullptr) {
        return UBSE_ERROR_NOMEM;
    }
    const auto ret = ubseXml->Parse();
    if (ret != UbseXmlError::OK) {
        return UBSE_ERROR;
    }
    if (ubseXml->Next("mue-ue-binding-infos") == nullptr) {
        UBSE_LOG_ERROR << "[MTI] Xml parse mue-ue-binding-infos failed.";
        return UBSE_ERROR;
    }
    if (ubseXml->Next("mue-ue-binding-info") == nullptr) {
        UBSE_LOG_ERROR << "[MTI] Xml parse mue-ue-binding-infos failed.";
        return UBSE_ERROR;
    }
    UbseMtiIouInfo iouInfo{};
    if (ExtractBasicInfoFromXml(ubseXml, iouInfo) != UBSE_OK) {
        return UBSE_ERROR;
    }
    ubseXml = ubseXml->Next("mue-ue-bindings");
    if (ubseXml == nullptr) {
        UBSE_LOG_ERROR << "[MTI] Xml parse mue-ue-bindings failed.";
        return UBSE_ERROR;
    }
    uint32_t i = 0;
    while (ubseXml->Next("mue-ue-binding", i) != nullptr) {
        if (ubseXml->Next("mue-id") != nullptr) {
            std::string entityId = ubseXml->Text();
            UbseMtiFeInfo* ubseFeInfo = FindVfeInVector(iouInfo, entityId, allFeInfos);
            if (ubseFeInfo != nullptr) {
                ubseFeInfo->fetype = UbseMtiFeType::PHYSICAL_TYPE;
            }
            ubseXml->Previous();
        }
        if (ubseXml->Next("ue-id") != nullptr) {
            std::string ueIdlist = ubseXml->Text();
            std::vector<std::string> ueId = ueIdlistSplit(ueIdlist, " ");
            for (const auto& entityId : ueId) {
                UbseMtiFeInfo* ubseFeInfo = FindVfeInVector(iouInfo, entityId, allFeInfos);
                if (ubseFeInfo != nullptr) {
                    ubseFeInfo->fetype = UbseMtiFeType::VIRTUAL_TYPE;
                }
            }
            ubseXml->Previous();
        }
        ubseXml->Previous();
        i++;
    }
    return UBSE_OK;
}

UbseResult UbseLcneFeEid::ParseGetFeEidResponse(const std::string& responseStr, std::vector<UbseMtiFeInfo>& allFeInfos)
{
    std::shared_ptr<UbseXml> ubseXml = UbseXml::Create(responseStr);
    if (ubseXml == nullptr) {
        return UBSE_ERROR_NOMEM;
    }
    const auto ret = ubseXml->Parse();
    if (ret != UbseXmlError::OK) {
        return UBSE_ERROR;
    }
    if (ubseXml->Next("entity-urma-communication-infos") == nullptr) {
        UBSE_LOG_ERROR << "[MTI] Xml parse entity-urma-communication-infos failed.";
        return UBSE_ERROR;
    }
    if (ubseXml->Next("entity-urma-communication-info") == nullptr) {
        UBSE_LOG_ERROR << "[MTI] Xml parse entity-urma-communication-info failed.";
        return UBSE_ERROR;
    }
    UbseMtiIouInfo iouInfo{};
    if (ExtractBasicInfoFromXml(ubseXml, iouInfo) != UBSE_OK) {
        return UBSE_ERROR;
    }
    ubseXml = ubseXml->Next("urma-communication-entity-ids");
    if (ubseXml == nullptr) {
        return UBSE_ERROR;
    }
    uint32_t i = 0;
    while (ubseXml->Next("urma-communication-entity-id", i) != nullptr) {
        std::string entityId = ubseXml->Child("entity-id")->Text();
        UbseMtiFeInfo ubseFeInfo{iouInfo.slotId, iouInfo.ubpuId, iouInfo.iouId, entityId, UbseMtiFeType::PHYSICAL_TYPE};
        ubseXml = ubseXml->Next("urma-communication-infos");
        if (ParseFeEidXml(ubseXml, ubseFeInfo) != UBSE_OK) {
            return UBSE_ERROR;
        }
        allFeInfos.push_back(ubseFeInfo);
        i++;
        ubseXml->Previous();
        ubseXml->Previous();
    }
    return UBSE_OK;
}

UbseResult UbseLcneFeEid::ParseFeEidXml(std::shared_ptr<UbseXml> ubseEidXml, UbseMtiFeInfo& feInfo)
{
    uint32_t i = 0;
    std::map<std::string, UbseMtiEidGroup> eidGroups;
    while (ubseEidXml->Next("urma-communication-info", i) != nullptr) {
        if (ubseEidXml->Next("urma-eid") == nullptr) {
            i++;
            ubseEidXml->Previous();
            continue;
        }
        std::string eid = ubseEidXml->Text();
        ubseEidXml->Previous();
        if (ubseEidXml->Next("port-group-id") != nullptr) {
            eidGroups[GetEidGroupId(eid)].primaryEid = eid;
            ubseEidXml->Previous();
        } else if (ubseEidXml->Next("interface-name") != nullptr) {
            std::string interfaceName = ubseEidXml->Text();
            uint32_t portId;
            if (GetPortIdFromInterfaceName(interfaceName, portId) != UBSE_OK) {
                UBSE_LOG_ERROR << "[MTI] get portId from interfaceName " << interfaceName << " failed";
                return UBSE_ERROR;
            }
            eidGroups[GetEidGroupId(eid)].portEids[std::to_string(portId)] = eid;
            ubseEidXml->Previous();
        } else {
            UBSE_LOG_ERROR << "[MTI] Xml parse communication-info failed, which label is not supported";
            return UBSE_ERROR;
        }
        i++;
        ubseEidXml->Previous();
    }
    UBSE_LOG_DEBUG << "ubpuId=" << feInfo.ubpuId << ", entityId=" << feInfo.entityId
                   << ", eidGroups.size=" << eidGroups.size();
    for (auto& group : eidGroups) {
        group.second.entityId = feInfo.entityId;
        feInfo.eidGroups.push_back(group.second);
    }
    return UBSE_OK;
}

UbseResult UbseLcneFeEid::GetComEidInfo(std::vector<UbseMtiFeInfo>& allFeInfos, UbseMtiEidGroup& feInfo)
{
    // 1. 筛选出fetype为PHYSICAL_TYPE的FeInfo，然后按照entityId转换为uint32_t后的值进行降序排序，取entityId最小的一组
    std::vector<UbseMtiFeInfo> phyFeInfos;
    for (const auto& feInfo : allFeInfos) {
        if (feInfo.fetype == UbseMtiFeType::PHYSICAL_TYPE) {
            phyFeInfos.push_back(feInfo);
        }
    }
    if (phyFeInfos.empty()) {
        UBSE_LOG_ERROR << "[MTI] No PHYSICAL_TYPE FeInfo found. allFeInfos size =" << allFeInfos.size();
        return UBSE_ERROR;
    }
    std::sort(phyFeInfos.begin(), phyFeInfos.end(), [](const UbseMtiFeInfo& a, const UbseMtiFeInfo& b) {
        uint32_t idA = static_cast<uint32_t>(std::stoul(a.entityId));
        uint32_t idB = static_cast<uint32_t>(std::stoul(b.entityId));
        return idA < idB;
    });

    // 2. 遍历排序后的 allFeInfos，找到 primaryEid 最小的一组 UbseMtiEidGroup
    UbseMtiEidGroup* selectedGroup = nullptr;
    std::string minPrimaryEid;

    for (auto& eidGroup : phyFeInfos[NO_0].eidGroups) {
        if (selectedGroup == nullptr || eidGroup.primaryEid < minPrimaryEid) {
            selectedGroup = &eidGroup;
            minPrimaryEid = eidGroup.primaryEid;
        }
    }

    if (selectedGroup != nullptr) {
        feInfo.entityId = selectedGroup->entityId;
        feInfo.primaryEid = selectedGroup->primaryEid;
        feInfo.portEids = selectedGroup->portEids;
        UBSE_LOG_DEBUG << "[MTI] selectedGroup entityId= " << feInfo.entityId << ", primaryEid=" << feInfo.primaryEid;
    } else {
        // 处理没有选中任何组的情况，返回错误码
        UBSE_LOG_ERROR << "[MTI] Failed to find the minimal EidInfo.";
        return UBSE_ERROR;
    }

    return UBSE_OK;
}

UbseMtiFeInfo* UbseLcneFeEid::FindVfeInVector(UbseMtiIouInfo& iouInfo, std::string entityId,
                                              std::vector<UbseMtiFeInfo>& allFeInfos)
{
    for (auto& fe : allFeInfos) {
        if ((fe.slotId == iouInfo.slotId) && (fe.ubpuId == iouInfo.ubpuId) && (fe.iouId == iouInfo.iouId) &&
            (fe.entityId == entityId)) {
            return &fe; // 返回指针
        }
    }
    return nullptr; // 明确表示未找到
}

std::vector<std::string> UbseLcneFeEid::ueIdlistSplit(const std::string& str, const std::string& delimiter)
{
    if (str.empty() || delimiter.empty()) {
        return {};
    }
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

UbseResult UbseLcneFeEid::GetPortIdFromInterfaceName(std::string intfaceName, uint32_t& portId)
{
    // 接口名400GUB8/1/4， 返回最后一个/后的4-1=3
    size_t lastSlashPos = intfaceName.find_last_of('/'); // 查找最后一个'/'的位置
    if (lastSlashPos != std::string::npos) {
        // 提取'/'后面的部分
        std::string portStr = intfaceName.substr(lastSlashPos + 1);
        try {
            portId = std::stoul(portStr) - 1;
        } catch (const std::invalid_argument& e) {
            return UBSE_ERROR;
        } catch (const std::out_of_range& e) {
            return UBSE_ERROR;
        }
        return UBSE_OK;
    }
    return UBSE_ERROR;
}

std::string UbseLcneFeEid::GetEidGroupId(std::string eid)
{
    return eid.substr(NO_0, NO_20);
    // EID 128位bit字符串，第121位到第125位为EID组ID
    /* 当前eid算法逻辑保持原有，待LCNE更新后替换 */
}
} // namespace ubse::lcne