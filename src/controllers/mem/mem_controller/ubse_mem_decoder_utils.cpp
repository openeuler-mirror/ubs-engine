/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_mem_decoder_utils.h"

#include "ubse_error.h"
#include "ubse_lcne_decoder_handle.h"
#include "ubse_logger_inner.h"
#include "ubse_mem_controller_api.h"
#include "ubse_node_controller.h"

namespace ubse::mem::decoder::utils {
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_CONTROLLER_MID);
using namespace ubse::log;
using namespace lcne;
std::unordered_map<uint32_t, uint32_t> MemDecoderUtils::portToPortSet{
    {0, 0},
    {1, 0},
    {2, 0},
    {3, 0},
    {4, 3},
    {5, 3},
    {6, 3},
    {7, 3},
    {8, 4}
};

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
                UBSE_LOG_ERROR << "stoi throw a exception, chipId is " << cpuInfo.second.chipId << " cardId is " <<
                    cpuInfo.second.cardId;
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
        auto ret = UbseLcneDecoderHandle::GetInstance().GetAllMemHandles(queryInfo, tempHandleValues);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "[MTI_MEM] get handles of decoderId=, " << decoderId << " failed, "
                            << FormatRetCode(ret);
            return ret;
        }
        handleValues[loc] = tempHandleValues;
    }
    return UBSE_OK;
}

UbseResult GetAllHandlesFromAllMarId(UbseMamiMemHandleQueryInfo &queryInfo, DecoderLocTohandleValueMap &handleValues)
{
    std::vector<uint32_t> marIdSet{0, 3, 4}; // 0、3、4 是1650芯片的portSet
    for (const auto &marId : marIdSet) {
        queryInfo.marId = marId;
        auto ret = GetAllHandlesFromAllDecoderId(queryInfo, handleValues);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "[MTI_MEM] get handles of marid=, " << marId << " failed, "
                            << FormatRetCode(ret);
            return ret;
        }
    }
    return UBSE_OK;
}

UbseResult MemDecoderUtils::GetAllHandles(DecoderLocTohandleValueMap &handleValues)
{
    handleValues.clear();
    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> outSocketInfo;
    auto res = GetCurNodeSocketInfo(outSocketInfo);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI_MEM] no socketId found, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    UbseMamiMemHandleQueryInfo queryInfo{};
    queryInfo.type = UB_MEMORY_HANDLE_ALL_TYPE;
    for (const auto &[cpuLoc, valPair] : outSocketInfo) {
        queryInfo.ubpuId = valPair.first;
        queryInfo.iouId = valPair.second;
        auto ret = GetAllHandlesFromAllMarId(queryInfo, handleValues);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "[MTI_MEM] get handles of socketId=, " << valPair.first << " failed, "
                            << FormatRetCode(UBSE_ERROR);
            return ret;
        }
    }
    return UBSE_OK;
}

std::vector<uint32_t> MemDecoderUtils::GetAllChipId()
{
    std::vector<uint32_t> chipIdList;
    auto nodeInfo = nodeController::UbseNodeController::GetInstance().GetCurNode();
    for (const auto &cpuInfo : nodeInfo.cpuInfos) {
        chipIdList.emplace_back(stoi(cpuInfo.second.chipId));
    }
    return chipIdList;
}

UbseResult MemDecoderUtils::SetMarIdParam(uint32_t chipId, uint32_t remoteChipId, ImportDecoderParam &importParam)
{
    auto allLinkInfo = nodeController::UbseNodeController::GetInstance().UbseGetDirectConnectInfo();
    std::vector<ubse::nodeController::PhysicalLink> linkInfos{};
    for (const auto &[key, linkInfo] : allLinkInfo) {
        if (linkInfo.chipId == chipId && linkInfo.peerChipId == remoteChipId) {
            linkInfos.push_back(linkInfo);
        }
    }
    if (linkInfos.empty()) {
        UBSE_LOG_ERROR << "None linkInfo from chipId " << chipId << " to remoteChipId " << remoteChipId;
        return UBSE_ERROR;
    }
    if (portToPortSet.find(linkInfos[0].portId) == portToPortSet.end()) {
        UBSE_LOG_ERROR << "error portId is " << linkInfos[0].portId;
        return UBSE_ERROR;
    }
    importParam.portSet = portToPortSet[linkInfos[0].portId];
    UBSE_LOG_INFO << "portId is " << linkInfos[0].portId << "portSet is " << importParam.portSet;
    return UBSE_OK;
}

UbseResult MemDecoderUtils::GetCurNodeSocketInfo(
    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> &outSocketInfo)
{
    auto nodeInfo = nodeController::UbseNodeController::GetInstance().GetCurNode();
    for (const auto &[cpuLoc, socketInfo] : nodeInfo.cpuInfos) {
        try {
            outSocketInfo[cpuLoc.socketId] = std::make_pair(std::stoi(socketInfo.chipId), std::stoi(socketInfo.cardId));
        } catch (...) {
            UBSE_LOG_ERROR << "stoi throw one exception";
            return UBSE_ERROR;
        }
    }

    return UBSE_OK;
}

UbseResult MemDecoderUtils::GetHandleMapFromImportObj(DecoderLocTohandleMap &handleMap)
{
    resource::mem::NodeMemDebtInfo memDebtInfo{};
    auto res = controller::GetCurNodeDebtInfoMap(memDebtInfo);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "GetCurNodeDebtInfoMap failed";
        return res;
    }

    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> socketIdToChipDie{};
    res = GetCurNodeSocketInfo(socketIdToChipDie);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "GetCurNodeSocketInfo failed";
        return res;
    }

    for (const auto &[name, fdImportObj] : memDebtInfo.fdImportObjMap) {
        auto socketId = fdImportObj.algoResult.attachSocketId;
        auto decoderResult = fdImportObj.status.decoderResult;
        auto chipDie = socketIdToChipDie[socketId];
        uint8_t decoderId = 0; // 0是cc表
        for (const auto &importResult : decoderResult) {
            DecoderEntryLoc loc{.ubpuId = chipDie.first, .iouId = chipDie.second, .marId = importResult.marId,
                .decoderId = decoderId};
            handleMap[loc].insert(importResult.handle);
        }
    }

    for (const auto &[name, numaImportObj] : memDebtInfo.numaImportObjMap) {
        auto socketId = numaImportObj.algoResult.attachSocketId;
        auto decoderResult = numaImportObj.status.decoderResult;
        auto chipDie = socketIdToChipDie[socketId];
        uint8_t decoderId = 0; // 0是cc表
        for (const auto &importResult : decoderResult) {
            DecoderEntryLoc loc{.ubpuId = chipDie.first, .iouId = chipDie.second, .marId = importResult.marId,
                .decoderId = decoderId};
            handleMap[loc].insert(importResult.handle);
        }
    }
    return UBSE_OK;
}
}