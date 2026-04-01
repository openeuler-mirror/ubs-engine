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
#include "adapter_plugins/mti/ubse_mti_interface.h"
#include "ubse_mem_decoder_utils.h"

#include "src/controllers/mem/mem_controller/debt/ubse_mem_debt_info.h"
#include "src/controllers/mem/mem_controller/ubse_mem_controller_api.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_prehandle_manager.h"
#include "ubse_mmi_interface.h"
#include "ubse_node_controller.h"
namespace ubse::mem::decoder::utils {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
std::unordered_map<uint32_t, uint32_t> MemDecoderUtils::portToPortSet{{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 1},
                                                                      {5, 1}, {6, 1}, {7, 1}, {8, 2}};

UbseResult MemDecoderUtils::GetChipAndDieId(const uint32_t socketId, std::pair<uint32_t, uint32_t> &chipDiePair)
{
    bool isSocketIdExit = false;
    auto nodeInfo = nodeController::UbseNodeController::GetInstance().GetCurNode();
    for (const auto &cpuInfo : nodeInfo.cpuInfos) {
        if (cpuInfo.second.socketId == socketId) {
            isSocketIdExit = true;
            try {
                chipDiePair.first = stoi(cpuInfo.second.chipId);
                chipDiePair.second = stoi(cpuInfo.second.cardId);
            } catch (...) {
                UBSE_LOG_ERROR << "stoi throw a exception, chipId is " << cpuInfo.second.chipId << " cardId is "
                               << cpuInfo.second.cardId;
                return UBSE_ERROR;
            }
            break;
        }
    }

    if (!isSocketIdExit) {
        UBSE_LOG_ERROR << "socketId=" << socketId << " doesn't exit, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult GetAllHandlesFromAllDecoderId(UbseMamiMemHandleQueryInfo &queryInfo,
                                         DecoderLocTohandleValueMap &handleValues)
{
    std::vector<uint32_t> decoderIdSet{0, 1}; // 0 是cc表，1 是nc表
    for (const auto &decoderId : decoderIdSet) {
        std::vector<UbseMamiMemHandleValue> tempHandleValues{};
        queryInfo.decoderId = decoderId;
        DecoderEntryLoc loc{.ubpuId = queryInfo.ubpuId,
                            .iouId = queryInfo.iouId,
                            .marId = queryInfo.marId,
                            .decoderId = queryInfo.decoderId};
        auto ret = adapter_plugins::mti::UbseMtiInterface::GetInstance().GetAllMemHandles(queryInfo, tempHandleValues);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "[MTI_MEM] get handles of decoderId=" << decoderId << ", failed, "
                           << FormatRetCode(UBSE_ERROR);
            return ret;
        }
        handleValues[loc] = tempHandleValues;
    }
    return UBSE_OK;
}

UbseResult GetAllHandlesFromAllMarId(UbseMamiMemHandleQueryInfo &queryInfo, DecoderLocTohandleValueMap &handleValues)
{
    std::vector<uint32_t> marIdSet{0, 1, 2}; // 0、1、2 是1650芯片的portSet
    for (const auto &marId : marIdSet) {
        queryInfo.marId = marId;
        auto ret = GetAllHandlesFromAllDecoderId(queryInfo, handleValues);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "[MTI_MEM] get handles of marid=" << marId << ", failed, " << FormatRetCode(UBSE_ERROR);
            return ret;
        }
    }
    return UBSE_OK;
}

UbseResult MemDecoderUtils::GetAllHandles(uint8_t type, DecoderLocTohandleValueMap &handleValues)
{
    handleValues.clear();
    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> outSocketInfo;
    auto res = GetCurNodeSocketInfo(outSocketInfo);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI_MEM] no socketId found, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    UbseMamiMemHandleQueryInfo queryInfo{};
    queryInfo.type = type;
    for (const auto &[cpuLoc, valPair] : outSocketInfo) {
        queryInfo.ubpuId = valPair.first;
        queryInfo.iouId = valPair.second;
        auto ret = GetAllHandlesFromAllMarId(queryInfo, handleValues);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "[MTI_MEM] get handles of socketId=" << valPair.first << ", failed, "
                           << FormatRetCode(UBSE_ERROR);
            return ret;
        }
    }
    return UBSE_OK;
}

UbseResult MemDecoderUtils::SetParamMarId(uint32_t slotId, uint32_t remoteSlotId, uint32_t chipId,
                                          uint32_t remoteChipId, ImportDecoderParam &importParam)
{
    auto allLinkInfo = nodeController::UbseNodeController::GetInstance().UbseGetDirConnectInfo();
    std::unordered_set<uint32_t> portIds;
    for (const auto &[key, linkInfo] : allLinkInfo) {
        if (linkInfo.slotId == slotId && linkInfo.chipId == chipId) {
            if (linkInfo.peerChipId == remoteChipId && linkInfo.peerSlotId == remoteSlotId) {
                portIds.insert(linkInfo.portId);
                continue;
            }
        }

        if (linkInfo.slotId == remoteSlotId && linkInfo.chipId == remoteChipId) {
            if (linkInfo.peerChipId == chipId && linkInfo.peerSlotId == slotId) {
                portIds.insert(linkInfo.peerPortId);
                continue;
            }
        }
    }
    if (portIds.empty()) {
        UBSE_LOG_ERROR << "None linkInfo from chipId " << chipId << " to remoteChipId " << remoteChipId;
        return UBSE_ERROR;
    }
    auto portId = *portIds.begin();
    if (portToPortSet.find(portId) == portToPortSet.end()) {
        UBSE_LOG_ERROR << "error portId is " << portId;
        return UBSE_ERROR;
    }
    importParam.portSet = portToPortSet[portId];
    UBSE_LOG_INFO << "portId is " << portId << ", portSet is " << importParam.portSet;
    return UBSE_OK;
}

UbseResult MemDecoderUtils::GetCurNodeSocketInfo(
    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> &outSocketInfo)
{
    auto nodeInfo = nodeController::UbseNodeController::GetInstance().GetCurNode();
    for (const auto &[cpuLoc, socketInfo] : nodeInfo.cpuInfos) {
        try {
            outSocketInfo[socketInfo.socketId] =
                std::make_pair(std::stoi(socketInfo.chipId), std::stoi(socketInfo.cardId));
        } catch (...) {
            UBSE_LOG_ERROR << "stoi throw one exception";
            return UBSE_ERROR;
        }
    }

    return UBSE_OK;
}

void FillHandleMap(controller::UbseMemNumaBorrowImportObj numaImportObj, const uint8_t &decoderId,
                   const std::pair<uint32_t, uint32_t> &chipDie, DecoderLocTohandleDcnaMap &handleMap)
{
    for (const auto &importResult : numaImportObj.status.decoderResult) {
        DecoderEntryLoc loc{
            .ubpuId = chipDie.first, .iouId = chipDie.second, .marId = importResult.marId, .decoderId = decoderId};
        handleMap[loc].emplace_back(std::make_pair(numaImportObj.exportObmmInfo[0].desc.dcna, importResult.handle));
    }
}

void FillHandleMap(const std::vector<UbseMamiMemImportResult> &decoderResult, const uint8_t &decoderId,
                   const std::pair<uint32_t, uint32_t> &chipDie, DecoderLocTohandleMap &handleMap)
{
    for (const auto &importResult : decoderResult) {
        DecoderEntryLoc loc{
            .ubpuId = chipDie.first, .iouId = chipDie.second, .marId = importResult.marId, .decoderId = decoderId};
        handleMap[loc].insert(importResult.handle);
    }
}

void GetHandleFromDebtInfo(const std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> &socketIdToChipDie,
                           const ubse::adapter_plugins::mmi::NodeMemDebtInfo &memDebtInfo,
                           DecoderLocTohandleMap &handleMap)
{
    for (const auto &[name, fdImportObj] : memDebtInfo.fdImportObjMap) {
        if (fdImportObj.status.state == ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED) {
            continue;
        }
        UBSE_LOG_INFO << "ImportObj state is " << fdImportObj.status.state;
        uint8_t decoderId = 0; // 0是cc表
        if (socketIdToChipDie.find(fdImportObj.algoResult.attachSocketId) == socketIdToChipDie.end()) {
            continue;
        }
        FillHandleMap(fdImportObj.status.decoderResult, decoderId,
                      socketIdToChipDie.at(fdImportObj.algoResult.attachSocketId), handleMap);
    }

    for (const auto &[name, numaImportObj] : memDebtInfo.numaImportObjMap) {
        if (numaImportObj.status.state == ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED) {
            continue;
        }
        UBSE_LOG_INFO << "ImportObj state is " << numaImportObj.status.state;
        uint8_t decoderId = 0; // 0是cc表
        if (socketIdToChipDie.find(numaImportObj.algoResult.attachSocketId) == socketIdToChipDie.end()) {
            continue;
        }
        FillHandleMap(numaImportObj.status.decoderResult, decoderId,
                      socketIdToChipDie.at(numaImportObj.algoResult.attachSocketId), handleMap);
    }

    for (const auto &[name, shareImportObj] : memDebtInfo.shareImportObjMap) {
        if (shareImportObj.status.state == ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED) {
            continue;
        }
        UBSE_LOG_INFO << "ImportObj state is " << shareImportObj.status.state;
        uint8_t decoderId = shareImportObj.req.ubseMemPrivData.cacheableFlag == 1 ? 0 : 1;
        if (socketIdToChipDie.find(shareImportObj.algoResult.attachSocketId) == socketIdToChipDie.end()) {
            continue;
        }
        FillHandleMap(shareImportObj.status.decoderResult, decoderId,
                      socketIdToChipDie.at(shareImportObj.algoResult.attachSocketId), handleMap);
    }

    for (const auto &[name, addImportObj] : memDebtInfo.addrImportObjMap) {
        if (addImportObj.status.state == ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED) {
            continue;
        }
        UBSE_LOG_INFO << "ImportObj state is " << addImportObj.status.state;
        uint8_t decoderId = 0;
        if (socketIdToChipDie.find(addImportObj.algoResult.attachSocketId) == socketIdToChipDie.end()) {
            continue;
        }
        FillHandleMap(addImportObj.status.decoderResult, decoderId,
                      socketIdToChipDie.at(addImportObj.algoResult.attachSocketId), handleMap);
    }
}

void GetHandleFromNumaDebtInfo(const std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> &socketIdToChipDie,
                               const ubse::adapter_plugins::mmi::NodeMemDebtInfo &memDebtInfo,
                               DecoderLocTohandleDcnaMap &handleMap)
{
    for (const auto &[name, numaImportObj] : memDebtInfo.numaImportObjMap) {
        if (numaImportObj.status.state == ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED) {
            continue;
        }
        UBSE_LOG_INFO << "ImportObj state is " << numaImportObj.status.state;
        uint8_t decoderId = 0; // 0是cc表
        if (socketIdToChipDie.find(numaImportObj.algoResult.attachSocketId) == socketIdToChipDie.end()) {
            continue;
        }
        FillHandleMap(numaImportObj, decoderId, socketIdToChipDie.at(numaImportObj.algoResult.attachSocketId),
                      handleMap);
    }
}

UbseResult MemDecoderUtils::GetAllHandleFromImportObj(DecoderLocTohandleMap &handleMap)
{
    ubse::adapter_plugins::mmi::NodeMemDebtInfo memDebtInfo{};
    auto curNode = nodeController::UbseNodeController::GetInstance().GetCurrentNodeId();
    if (curNode.empty()) {
        UBSE_LOG_ERROR << "CurrentNodeId is empty";
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "CurrentNodeId is " << curNode;
    memDebtInfo = controller::GetNodeMemDebtInfoById(curNode);

    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> socketIdToChipDie{};
    auto res = GetCurNodeSocketInfo(socketIdToChipDie);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "GetCurNodeSocketInfo failed";
        return res;
    }
    GetHandleFromDebtInfo(socketIdToChipDie, memDebtInfo, handleMap);
    return UBSE_OK;
}

UbseResult MemDecoderUtils::GetAllHandleFromNumaImportObj(DecoderLocTohandleDcnaMap &handleMap)
{
    ubse::adapter_plugins::mmi::NodeMemDebtInfo memDebtInfo{};
    auto curNode = nodeController::UbseNodeController::GetInstance().GetCurrentNodeId();
    if (curNode.empty()) {
        UBSE_LOG_ERROR << "CurrentNodeId is empty";
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "CurrentNodeId is " << curNode;
    memDebtInfo = controller::GetNodeMemDebtInfoById(curNode);

    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> socketIdToChipDie{};
    auto res = GetCurNodeSocketInfo(socketIdToChipDie);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "GetCurNodeSocketInfo failed";
        return res;
    }
    GetHandleFromNumaDebtInfo(socketIdToChipDie, memDebtInfo, handleMap);
    return UBSE_OK;
}

void MemDecoderUtils::SetImportDecoderParam(decoder::utils::ImportDecoderParam &importParam)
{
    importParam.importType = UB_MEMORY_IMPORT_MEMORY;
    importParam.decoderIdx = 0;
    importParam.flag |= UB_MEMORY_IMPORT_ADDR_TR_ONCHIP;
    importParam.flag |= UB_MEMORY_IMPORT_SINGLE_PATH;
}

void MemDecoderUtils::SetImportDecoderParam(decoder::utils::ImportDecoderParam &importParam,
                                            const ubse::adapter_plugins::mmi::UbseMemPrivData &privData)
{
    importParam.importType = UB_MEMORY_IMPORT_MEMORY;
    privData.cacheableFlag == 1 ? importParam.decoderIdx = 0 : importParam.decoderIdx = 1;
    if (privData.adTrOchip == 1) {
        importParam.flag |= UB_MEMORY_IMPORT_ADDR_TR_ONCHIP;
    }
    importParam.flag |= UB_MEMORY_IMPORT_SINGLE_PATH;
    if (privData.so == 1) {
        importParam.flag |= UB_MEMORY_IMPORT_SO;
    }
    if (privData.reduceDelayComp == 1) {
        importParam.flag |= UB_MEMORY_IMPORT_REDUCE_DELAY_COMP;
    }
    if (privData.wrDelayComp == 1) {
        importParam.flag |= UB_MEMORY_IMPORT_WR_DELAY_COMP;
    }
    if (privData.cmoDelayComp == 1) {
        importParam.flag |= UB_MEMORY_IMPORT_CMO_DELAY_COMP;
    }
    importParam.flag |= UB_MEMORY_IMPORT_SHARE_TYPE; // 只有共享支持自定义配置
}

void MemDecoderUtils::SetImportDecoderParam(decoder::utils::ImportDecoderParam &importParam, uint16_t wrDelayComp)
{
    importParam.importType = UB_MEMORY_IMPORT_MEMORY;
    importParam.decoderIdx = 0;
    importParam.flag |= UB_MEMORY_IMPORT_ADDR_TR_ONCHIP;
    importParam.flag |= UB_MEMORY_IMPORT_SINGLE_PATH;
    if (wrDelayComp == 1) {
        importParam.flag |= UB_MEMORY_IMPORT_WR_DELAY_COMP;
    }
}

uint32_t MemDecoderUtils::PreImportDecoderEntry(const decoder::utils::PreImportDecoderParam &preImportDecoderParam,
                                                UbseMamiMemImportResult &outValue)
{
    UbseMamiMemImportInfo mamiImportInfo{};
    mamiImportInfo.ubpuId = preImportDecoderParam.ubpuId;
    mamiImportInfo.iouId = preImportDecoderParam.iouId;
    mamiImportInfo.importType = preImportDecoderParam.importType;
    mamiImportInfo.dstCNA = preImportDecoderParam.dstCNA;
    mamiImportInfo.marId = preImportDecoderParam.marId;
    mamiImportInfo.size = preImportDecoderParam.size;
    decoder::utils::DecoderEntryLoc loc{};
    loc.ubpuId = mamiImportInfo.ubpuId;
    loc.iouId = mamiImportInfo.iouId;
    loc.decoderId = mamiImportInfo.decoderId;
    loc.marId = mamiImportInfo.marId;
    mamiImportInfo.decoderId = 0; // 预引入只给numa借用使用，numa借用默认走cc表
    UbseMamiMemImportResult importResult{};
    auto res = adapter_plugins::mti::UbseMtiInterface::GetInstance().AddDecoderEntry(mamiImportInfo, importResult);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "preImport Failed";
        return res;
    }
    outValue = importResult;
    UbseMemPrehandleManager::GetInstance().CreatePreHandle(loc, importResult, mamiImportInfo.dstCNA,
                                                           mamiImportInfo.size);
    return UBSE_OK;
}

} // namespace ubse::mem::decoder::utils