/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 */
#include "resource_analysis.h"

#include <cstdint>
#include <map>
#include <vector>
#include "plugin_services/mem/ubse_mem_service.h"
#include "ubse_event.h"
#include "ubse_json_util.h"
#include "ubse_logger.h"
#include "ubse_mem_configuration.h"
#include "ubse_mem_json_def.h"
#include "ubse_node_controller.h"

namespace ubse::mem::strategy {
UBSE_DEFINE_THIS_MODULE("ubse_mem_strategy");
using namespace ubse::common::def;
using namespace ubse::utils;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::service;
using namespace ubse::service::mem;

std::string GetAllNumaJsonInfo(const std::string &nodeId)
{
    auto memService = GetMemService();
    if (memService == nullptr) {
        UBSE_LOG_ERROR << "UbseMemService is not registered";
        return "";
    }
    return memService->GetAllNumaJsonInfo(nodeId);
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

inline uint64_t MinMemId(const std::vector<ubse::adapter_plugins::mmi::UbseMemObmmInfo> &exportMemId)
{
    if (exportMemId.empty()) {
        return 0;
    }
    std::vector<uint64_t> memIdList{};
    for (const auto &obmmInfo : exportMemId) {
        memIdList.push_back(obmmInfo.memId);
    }
    const auto minIter = std::min_element(memIdList.begin(), memIdList.end());
    return *minIter;
}

inline uint64_t MinMemId(const std::vector<ubse::adapter_plugins::mmi::UbseMemImportResult> &importMemId)
{
    if (importMemId.empty()) {
        return 0;
    }

    std::vector<uint64_t> memIdList{};
    for (const auto &importResult : importMemId) {
        memIdList.push_back(importResult.memId);
    }
    const auto minIter = std::min_element(memIdList.begin(), memIdList.end());
    return *minIter;
}

bool CheckNumaLocIsRight(const ubse::adapter_plugins::mmi::UbseMemNumaBorrowImportObj &importObj,
                         const UbseMemNumaLoc &memIdLoc)
{
    return importObj.req.importNodeId == memIdLoc.nodeId && !importObj.algoResult.importNumaInfos.empty() &&
           importObj.algoResult.importNumaInfos[0].numaId == memIdLoc.numaId &&
           importObj.algoResult.importNumaInfos[0].socketId == memIdLoc.socketId;
}

void GetAllImportItemByMap(std::vector<UbseMemEventNotifyBorrowItem> &tmpBorrow, const UbseMemNumaLoc &memIdLoc,
                           const ubse::adapter_plugins::mmi::UbseMemNumaImportObjMap &numaImportObjMap)
{
    for (const auto &[name, importObj] : numaImportObjMap) {
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

void GetAllImportItem(const UbseMemNumaLoc &memIdLoc, std::vector<UbseMemEventNotifyBorrowItem> &tmpBorrow)
{
    auto memService = GetMemService();
    if (memService == nullptr) {
        UBSE_LOG_ERROR << "UbseMemService is not registered";
        return;
    }
    auto debtInfoMap = memService->UbseGetLocalMemDebtInfo();
    for (const auto &nodeMap : debtInfoMap) {
        GetAllImportItemByMap(tmpBorrow, memIdLoc, nodeMap.second.numaImportObjMap);
    }
}

UbseResult WaterWarningProcess(WatermarkWarningType warningType, const UbseMemNumaLoc &warningNumaLoc, bool isOom)
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