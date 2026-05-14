/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 */
#include "resource_analysis.h"

#include <cstdint>
#include <map>
#include <vector>
#include "ubse_event.h"
#include "ubse_json_util.h"
#include "ubse_logger.h"
#include "ubse_mem_configuration.h"
#include "ubse_mem_debt_info.h"
#include "ubse_mem_json_def.h"
#include "ubse_mem_meta_data.h"
#include "ubse_mem_topology_info_manager.h"
#include "ubse_node_controller.h"

namespace ubse::mem::strategy {
UBSE_DEFINE_THIS_MODULE("ubse_mem_strategy");
using namespace ubse::common::def;
using namespace ubse::utils;

void BuildNumaInfoMap(std::map<std::string, std::string>& numaInfoMap, const std::shared_ptr<MemNumaInfo> item)
{
    if (item == nullptr) {
        return;
    }
    numaInfoMap.emplace("mGlobalIndex", std::to_string(item->mGlobalIndex));
    numaInfoMap.emplace("mStatus", "true");
    numaInfoMap.emplace("mTimestamp", std::to_string(item->mTimestamp));
    numaInfoMap.emplace("mMemTotal", std::to_string(item->mMemTotal));
    numaInfoMap.emplace("mMemUsed", std::to_string(item->mMemUsed));
    numaInfoMap.emplace("mMemFree", std::to_string(item->mMemFree));
    std::string cpuList;
    for (auto item1 : item->mCpuList) {
        cpuList += std::to_string(item1) + ",";
    }
    if (!cpuList.empty()) {
        cpuList.resize(cpuList.size() - 1);
    }
    numaInfoMap.emplace("mCpuList", cpuList);
    numaInfoMap.emplace("mMemBorrowed", std::to_string(item->mMemBorrowed));
    numaInfoMap.emplace("mWaterBorrowCount", std::to_string(item->mWaterBorrowCount));
    numaInfoMap.emplace("mMemLent", std::to_string(item->mMemLent));
    numaInfoMap.emplace("mMemShared", std::to_string(item->mMemShared));
    numaInfoMap.emplace("mPercent", std::to_string(item->mPercent));
    std::string lastWarningType = "NO_WARN";
    numaInfoMap.emplace("mLastWarningType", lastWarningType);
    numaInfoMap.emplace("mLastWarningCount", std::to_string(item->mLastWarningCount));
    auto numaLoc = item->mUbseMemNumaLoc.nodeId + "/" + std::to_string(item->mUbseMemNumaLoc.socketId) + "/" +
                   std::to_string(item->mUbseMemNumaLoc.numaId);
    numaInfoMap.emplace("numaLoc", numaLoc);
}

std::string GetAllNumaJsonInfo(const NodeId& nodeId)
{
    auto list = UbseMemTopologyInfoManager::GetInstance().GetAllNumaInfo(nodeId);
    if (list.empty()) {
#ifndef DEBUG_MEM_UT
        return "";
#endif
    }
    std::vector<std::string> strVec;
    for (auto item : list) {
        std::map<std::string, std::string> numaInfoMap;
        BuildNumaInfoMap(numaInfoMap, item);
        auto hostName =
            ubse::nodeController::UbseNodeController::GetInstance().GetNodeById(item->mUbseMemNumaLoc.nodeId).hostName;
        numaInfoMap.emplace("hostName", hostName);
        std::string tmp;
        if (!UbseJsonUtil::ConvertMap2JsonStr(numaInfoMap, tmp)) {
            UBSE_LOG_ERROR << "ConvertMap2JsonStr error";
            return "";
        }
        strVec.emplace_back(tmp);
    }
    std::string json;
    if (!UbseJsonUtil::ConvertVector2JsonStr(strVec, json)) {
        UBSE_LOG_ERROR << "ConvertVector2JsonStr error.";
        return "[]";
    }
    return json;
}

static std::string ToString(const WatermarkWarningType warning)
{
    switch (warning) {
        case WatermarkWarningType::NO_WARN:
            return "NO_WARN";
        case WatermarkWarningType::LOW_WATERMARK:
            return "LOW_WATERMARK";
        case WatermarkWarningType::HIGH_WATERMARK:
            return "HIGH_WATERMARK";
    }
    return "";
}

inline uint64_t MinMemId(const std::vector<ubse::adapter_plugins::mmi::UbseMemObmmInfo>& exportMemId)
{
    if (exportMemId.empty()) {
        return 0;
    }
    std::vector<uint64_t> memIdList{};
    for (const auto& obmmInfo : exportMemId) {
        memIdList.push_back(obmmInfo.memId);
    }
    const auto minIter = std::min_element(memIdList.begin(), memIdList.end());
    return *minIter;
}

inline uint64_t MinMemId(const std::vector<ubse::adapter_plugins::mmi::UbseMemImportResult>& importMemId)
{
    if (importMemId.empty()) {
        return 0;
    }

    std::vector<uint64_t> memIdList{};
    for (const auto& importResult : importMemId) {
        memIdList.push_back(importResult.memId);
    }
    const auto minIter = std::min_element(memIdList.begin(), memIdList.end());
    return *minIter;
}

bool CheckNumaLocIsRight(const ubse::adapter_plugins::mmi::UbseMemNumaBorrowImportObj& importObj,
                         const UbseMemNumaLoc& memIdLoc)
{
    return importObj.req.importNodeId == memIdLoc.nodeId && !importObj.algoResult.importNumaInfos.empty() &&
           importObj.algoResult.importNumaInfos[0].numaId == memIdLoc.numaId &&
           importObj.algoResult.importNumaInfos[0].socketId == memIdLoc.socketId;
}

void GetAllImportItemByMap(std::vector<UbseMemEventNotifyBorrowItem>& tmpBorrow, const UbseMemNumaLoc& memIdLoc,
                           const ubse::adapter_plugins::mmi::UbseMemNumaImportObjMap& numaImportObjMap)
{
    for (const auto& [name, importObj] : numaImportObjMap) {
        if (CheckNumaLocIsRight(importObj, memIdLoc)) {
            UbseMemEventNotifyBorrowItem tmp{};
            tmp.name = name;
            tmp.exportMemId = MinMemId(importObj.exportObmmInfo);
            tmp.importMemId = MinMemId(importObj.status.importResults);
            tmp.exportLocNum = importObj.algoResult.exportNumaInfos.size();
            for (int i = 0; i < tmp.exportLocNum; ++i) {
                tmp.requestSize.emplace_back(importObj.algoResult.exportNumaInfos[i].size);
                UbseMemNumaLoc loc{};
                loc.nodeId = importObj.algoResult.exportNumaInfos[0].nodeId;
                loc.socketId = importObj.algoResult.exportNumaInfos[i].socketId;
                loc.numaId = importObj.algoResult.exportNumaInfos[i].numaId;
                tmp.exportLocInfo.emplace_back(loc);
            }
            tmpBorrow.emplace_back(tmp);
        }
    }
}

void GetAllImportItem(const UbseMemNumaLoc& memIdLoc, std::vector<UbseMemEventNotifyBorrowItem>& tmpBorrow)
{
    auto debtInfoMap = ubse::mem::controller::GetNodeMemDebtInfoMap();
    for (const auto& nodeMap : debtInfoMap) {
        GetAllImportItemByMap(tmpBorrow, memIdLoc, nodeMap.second.numaImportObjMap);
    }
}

UbseResult WaterWarningProcess(WatermarkWarningType warningType, const UbseMemNumaLoc& warningNumaLoc, bool isOom)
{
    static std::string highEventId = "RegisterMemEventNotifyFunc_high";
    static std::string lowEventId = "RegisterMemEventNotifyFunc_low";
    if (UbseMemConfiguration::GetInstance().GetManagerVmEnable()) {
        // vm场景，水线事件发生后，需要发事件给vm。我们不处理
        UbseMemEventNotifyMessage memEventNotifyMessage{};
        memEventNotifyMessage.allNumaInfo = GetAllNumaJsonInfo("");
        std::vector<UbseMemEventNotifyBorrowItem> tmpBorrow{};
        GetAllImportItem(warningNumaLoc, tmpBorrow);

        memEventNotifyMessage.notifyNumaLoc.nodeId = warningNumaLoc.nodeId;
        memEventNotifyMessage.notifyNumaLoc.socketId = warningNumaLoc.socketId;
        memEventNotifyMessage.notifyNumaLoc.numaId = warningNumaLoc.numaId;

        memEventNotifyMessage.borrowItemNum = static_cast<int>(tmpBorrow.size());
        for (int i = 0; i < memEventNotifyMessage.borrowItemNum; ++i) {
            memEventNotifyMessage.borrowItem.push_back(tmpBorrow[i]);
        }
        if (isOom) {
            memEventNotifyMessage.oomEventFlag = 1;
            UBSE_LOG_INFO << "Start to pub oom event to vm module, warning numaId=" << warningNumaLoc.numaId
                          << ", nodeId=" << warningNumaLoc.nodeId;
        }
        auto json = memEventNotifyMessage.ToJson();
        auto id = highEventId;
        if (warningType == WatermarkWarningType::LOW_WATERMARK) {
            id = lowEventId;
        }
        auto ret = ubse::event::UbsePubEvent(id, json);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Watermark warningProcess event publish failed, ret is " << ret;
            return UBSE_ERROR;
        }
        UBSE_LOG_INFO << "Watermark warningProcess is successed.";
        return UBSE_OK;
    }
    return UBSE_OK;
}
} // namespace ubse::mem::strategy