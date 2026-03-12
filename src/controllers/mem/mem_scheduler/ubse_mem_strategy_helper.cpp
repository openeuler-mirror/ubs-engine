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
#include "ubse_mem_strategy_helper.h"

#include "ubse_logger.h"
#include "ubse_mem_algo_account.h"
#include "ubse_mem_configuration.h"
#include "ubse_mem_functions.h"
#include "ubse_mem_meta_data.h"
#include "ubse_mem_topology_info_manager.h"
#include "ubse_mem_validator.h"
#include "ubse_node.h"
#include "ubse_node_controller.h"

namespace ubse::mem::strategy {
UBSE_DEFINE_THIS_MODULE("ubse_mem_strategy");
using namespace ubse::nodeController;
using namespace ubse::nodeController;

static void PrintUbseStatus(tc::rs::mem::UbseStatus &ubseStatus)
{
    for (int i = 0; i < tc::rs::mem::NUM_TOTAL_NUMA; i++) {
        if (ubseStatus.numaStatus[i].memTotal <= 0 && ubseStatus.numaLedgerStatus[i].memLent <= 0 &&
            ubseStatus.numaLedgerStatus[i].memShared <= 0) {
            // 测试算法错误码时海量日志
            continue;
        }
        // 日志打印顺序错误
        UBSE_LOG_ERROR << "The numaIndex=" << i << ", Memtotal=" << SizeByte2Mb(ubseStatus.numaStatus[i].memTotal)
                       << "MB, memUsed=" << SizeByte2Mb(ubseStatus.numaStatus[i].memUsed)
                       << "MB, memFree=" << SizeByte2Mb(ubseStatus.numaStatus[i].memFree) << "MB. "
                       << "memBorrowed=" << SizeByte2Mb(ubseStatus.numaLedgerStatus[i].memBorrowed)
                       << "MB, memLent=" << SizeByte2Mb(ubseStatus.numaLedgerStatus[i].memLent)
                       << "MB, memShared=" << SizeByte2Mb(ubseStatus.numaLedgerStatus[i].memShared) << "MB.";
    }
}

static UbseResult GetFdBorrowParam(const ubse::adapter_plugins::mmi::UbseMemFdBorrowReq &req,
                                   tc::rs::mem::BorrowRequest &borrowRequest,
                                   ubse::adapter_plugins::mmi::UbseMemAlgoResult &algoResult)
{
    UbseMemNumaLoc borrowNode{"", -1, -1};
    if (req.size / ONE_M > UINT32_MAX) {
        UBSE_LOG_ERROR << "Req size is too large";
        return UBSE_ERROR;
    }
    borrowRequest.requestSize = static_cast<int32_t>(req.size / ONE_M);
    auto blockSize = UbseMemConfiguration::GetInstance().GetBlockSizeFromLenderNode();
    if (!blockSize.has_value()) {
        UBSE_LOG_ERROR << "Get block size failed";
        return UBSE_ERROR;
    }
    algoResult.blockSize = blockSize.value();
    borrowRequest.requestSize = ubse::mem::strategy::CeilToN(borrowRequest.requestSize, algoResult.blockSize);
    UBSE_LOG_INFO << "The request name= " << req.name << ", request_size= " << borrowRequest.requestSize;
    borrowRequest.urgentLevel = tc::rs::mem::RequestUrgentLevel::LEVEL0;
    borrowNode.nodeId = req.importNodeId;
    borrowRequest.requestLoc.hostId = UbseMemTopologyInfoManager::GetInstance().NodeIdToIndex(borrowNode.nodeId);
    if (borrowRequest.requestLoc.hostId < 0 || borrowRequest.requestLoc.hostId >= tc::rs::mem::NUM_HOSTS) {
        UBSE_LOG_ERROR << "Failed to translate nodeId to index, nodeId=" << borrowNode.nodeId
                       << ", nodeIndex=" << borrowRequest.requestLoc.hostId;
        return UBSE_ERROR_INVAL;
    }
    borrowRequest.requestLoc.socketId = -1;
    borrowRequest.requestLoc.numaId = -1;
    return UBSE_OK;
}

static UbseResult GetNumaBorrowParam(const ubse::adapter_plugins::mmi::UbseMemNumaBorrowReq &req,
                                     tc::rs::mem::BorrowRequest &request,
                                     ubse::adapter_plugins::mmi::UbseMemAlgoResult &algoResult)
{
    UbseMemNumaLoc borrowNode{req.importNodeId, req.srcSocket, req.srcNuma};
    request.urgentLevel = tc::rs::mem::RequestUrgentLevel::LEVEL0;
    request.requestSize = static_cast<int32_t>(req.size / ONE_M);
    auto blockSize = UbseMemConfiguration::GetInstance().GetBlockSizeFromLenderNode();
    if (!blockSize.has_value()) {
        UBSE_LOG_ERROR << "Get block size failed";
        return UBSE_ERROR;
    }
    algoResult.blockSize = blockSize.value();
    request.requestSize = CeilToN(request.requestSize, algoResult.blockSize);
    UBSE_LOG_INFO << "requestSize=" << request.requestSize << ", name=" << req.name;

    if (req.srcNuma != -1 && req.srcSocket != -1) {
        UBSE_LOG_INFO << "Not borrow Info";
        std::shared_ptr<ubse::mem::strategy::MemNumaInfo> memNumaInfo =
            UbseMemTopologyInfoManager::GetInstance().GetNumaInfo(req.importNodeId, req.srcNuma);
        if (memNumaInfo == nullptr) {
            UBSE_LOG_WARN << "IdLocToIndexLoc failed, nodeId=" << borrowNode.nodeId
                           << ", socketid=" << borrowNode.socketId << ", numaId=" << borrowNode.numaId;
            return UBSE_ERROR;
        }
        if (memNumaInfo->mUbseMemNumaLoc.socketId != req.srcSocket) {
            UBSE_LOG_ERROR << "socketId " << memNumaInfo->mUbseMemNumaLoc.socketId << " != " << req.srcSocket;
            return UBSE_ERROR;
        }
        UbseMemNumaIndexLoc memIndexLoc = memNumaInfo->mUbseMemNumaIndexLoc;
        request.requestLoc.hostId = static_cast<int16_t>(memIndexLoc.nodeIndex);
        request.requestLoc.socketId = static_cast<int8_t>(memIndexLoc.socketIndex);
        request.requestLoc.numaId = static_cast<int8_t>(memIndexLoc.numaIndex);
    } else {
        request.requestLoc.hostId = UbseMemTopologyInfoManager::GetInstance().NodeIdToIndex(borrowNode.nodeId);
        if (request.requestLoc.hostId < 0 || request.requestLoc.hostId >= tc::rs::mem::NUM_HOSTS) {
            UBSE_LOG_ERROR << "Failed to translate nodeId to index, nodeId=" << borrowNode.nodeId
                           << ", nodeIndex=" << request.requestLoc.hostId;
            return UBSE_ERROR;
        }
        request.requestLoc.socketId = -1;
        request.requestLoc.numaId = -1;
    }

    return UBSE_OK;
}

static UbseResult GetShareStrategyRequest(const ubse::adapter_plugins::mmi::UbseMemShareBorrowReq &req,
                                          tc::rs::mem::ShareRequest &request,
                                          ubse::adapter_plugins::mmi::UbseMemAlgoResult &algoResult)
{
    auto hostIndex = UbseMemTopologyInfoManager::GetInstance().NodeIdToIndex(req.requestNodeId);
    if (hostIndex < 0 || hostIndex >= tc::rs::mem::NUM_HOSTS) {
        UBSE_LOG_ERROR << "Neighbor node index is invalid, nodeId=" << req.requestNodeId << ", nodeIndex=" << hostIndex;
        return UBSE_ERROR_INVAL;
    }
    request.srcLoc = {hostIndex, -1, -1};
    request.requestSize = static_cast<int32_t>((req.size) / ONE_M);
    auto blockSize = UbseMemConfiguration::GetInstance().GetBlockSizeFromLenderNode();
    if (!blockSize.has_value()) {
        UBSE_LOG_ERROR << "Get block size failed";
        return UBSE_ERROR;
    }
    algoResult.blockSize = blockSize.value();
    request.requestSize = CeilToN(request.requestSize, algoResult.blockSize);
    UBSE_LOG_INFO << "requestSize=" << request.requestSize << ", name=" << req.name;
    request.region.type = tc::rs::mem::MemShmRegionType::ALL2ALL_SHARE;
    request.region.perfLevel = tc::rs::mem::PerfLevel::L0;
    if (req.shmRegion.nodeNum <= 0) {
        UBSE_LOG_ERROR << "Region num is less than or eq 0, regionNum=" << req.shmRegion.nodeNum;
        return UBSE_ERROR_INVAL;
    }
    int count{};
    for (int i = 0; i < req.shmRegion.nodeNum && i < TOPOLOGY_MAX_HOST_NUM; i++) {
        NodeIndex nodeIndex = UbseMemTopologyInfoManager::GetInstance().NodeIdToIndex(req.shmRegion.nodelist[i].nodeId);
        if (nodeIndex < 0 || nodeIndex >= tc::rs::mem::NUM_HOSTS) {
            UBSE_LOG_WARN << "Neighbor node index is invalid, nodeId=" << req.shmRegion.nodelist[i].nodeId
                          << ", nodeIndex=" << nodeIndex;
            continue;
        }
        request.region.nodeId[i] = nodeIndex;
        count++;
    }
    request.region.num = count;
    return UBSE_OK;
}

static UbseResult GetBorrowResultFromAlgoRes(ubse::adapter_plugins::mmi::UbseMemAlgoResult &algoResult,
                                             const tc::rs::mem::BorrowResult &borrowResult)
{
    UbseMemNumaLoc memIdLocLend{};
    UbseMemNumaLoc memIdLocBorrow{};
    for (int i = 0; i < borrowResult.lenderLength; ++i) {
        UbseMemNumaIndexLoc memIndexLocLend = {borrowResult.lenderLocs[i].hostId, borrowResult.lenderLocs[i].socketId,
                                               borrowResult.lenderLocs[i].numaId};
        if (!UbseMemTopologyInfoManager::GetInstance().ConvertNumaIndex(memIndexLocLend, memIdLocLend)) {
            UBSE_LOG_ERROR << "IndexLocToIdLoc error. nodeIndex=" << memIndexLocLend.nodeIndex;
            return UBSE_ERROR_INVAL;
        }
        UbseMemNumaIndexLoc memIndexLocBorrow = {borrowResult.borrowerLocs[i].hostId,
                                                 borrowResult.borrowerLocs[i].socketId,
                                                 borrowResult.borrowerLocs[i].numaId};
        if (!UbseMemTopologyInfoManager::GetInstance().ConvertNumaIndex(memIndexLocBorrow, memIdLocBorrow)) {
            UBSE_LOG_ERROR << "IndexLocToIdLoc error nodeIndex=" << memIndexLocBorrow.nodeIndex;
            return UBSE_ERROR_INVAL;
        }
        auto lenderSize = static_cast<uint64_t>(borrowResult.lenderSizes[i]) * ONE_M;
        UbseMemDebtNumaInfo exportDebtNumaInfo{memIdLocLend.nodeId, memIdLocLend.socketId, memIdLocLend.numaId,
                                               lenderSize};
        UbseMemDebtNumaInfo importDebtNumaInfo{memIdLocBorrow.nodeId, memIdLocBorrow.socketId, memIdLocBorrow.numaId,
                                               lenderSize};
        algoResult.exportNumaInfos.emplace_back(exportDebtNumaInfo);
        UBSE_LOG_INFO << "Algo result importNodeId=" << memIdLocBorrow.nodeId
                      << ", importSocket=" << memIdLocBorrow.socketId << ", exportNodeId=" << memIdLocLend.nodeId
                      << ", exportSocket=" << memIdLocLend.socketId << ", exportNumaId=" << memIdLocLend.numaId
                      << ", blockSize="<< algoResult.blockSize;
        algoResult.importNumaInfos.emplace_back(importDebtNumaInfo);
    }

    auto ret = UbseMemTopologyInfoManager::GetInstance().GetAttachNodeId(
        algoResult.importNumaInfos[0].nodeId, algoResult.exportNumaInfos[0].nodeId,
        algoResult.exportNumaInfos[0].socketId, algoResult.attachSocketId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get socketCnaInfo failed, ret=" << ret;
    }
    return ret;
}

static UbseResult ConstructNumaBorrowResultFromAlgoRes(const tc::rs::mem::BorrowResult &algoRes,
                                                       UbseMemAlgoResult &algoResult)
{
    UbseMemNumaLoc memIdLocBorrow{};
    UbseMemNumaLoc memIdLocLend{};
    for (int i = 0; i < algoRes.lenderLength; i++) {
        UbseMemNumaIndexLoc memIndexLocLend = {algoRes.lenderLocs[i].hostId, algoRes.lenderLocs[i].socketId,
                                               algoRes.lenderLocs[i].numaId};
        if (!UbseMemTopologyInfoManager::GetInstance().ConvertNumaIndex(memIndexLocLend, memIdLocLend)) {
            UBSE_LOG_ERROR << "IndexLocToIdLoc error. nodeIndex=" << memIndexLocLend.nodeIndex;
            return UBSE_ERROR_INVAL;
        }
        UbseMemNumaIndexLoc memIndexLocBorrow = {algoRes.borrowerLocs[i].hostId, algoRes.borrowerLocs[i].socketId,
                                                 algoRes.borrowerLocs[i].numaId};
        if (!UbseMemTopologyInfoManager::GetInstance().ConvertNumaIndex(memIndexLocBorrow, memIdLocBorrow)) {
            UBSE_LOG_ERROR << "IndexLocToIdLoc error nodeIndex=" << memIndexLocBorrow.nodeIndex;
            return UBSE_ERROR_INVAL;
        }
        auto lenderSize = static_cast<uint64_t>(algoRes.lenderSizes[i]) * ONE_M;
        UbseMemDebtNumaInfo exportDebtNumaInfo{memIdLocLend.nodeId, memIdLocLend.socketId, memIdLocLend.numaId,
                                               lenderSize};
        UbseMemDebtNumaInfo importDebtNumaInfo{memIdLocBorrow.nodeId, memIdLocBorrow.socketId, memIdLocBorrow.numaId,
                                               lenderSize};
        algoResult.exportNumaInfos.emplace_back(exportDebtNumaInfo);
        algoResult.importNumaInfos.emplace_back(importDebtNumaInfo);
    }

    SocketCnaTopoInfo socketCnaTopoInfo{};
    auto ret =
        UbseMemTopologyInfoManager::GetInstance().GetSocketCnaInfo(memIdLocBorrow, memIdLocLend, socketCnaTopoInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get socketCnaInfo failed, ret=" << ret;
        return ret;
    }
    try {
        algoResult.attachSocketId = static_cast<uint32_t>(stoi(socketCnaTopoInfo.importNodeIdSocketId));
    } catch (...) {
        UBSE_LOG_ERROR << "Get socketCnaInfo failed, catch one Error";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

static UbseResult ConstructShareBorrowResultFromAlgoRes(const UbseMemShareBorrowReq &req,
                                                        const tc::rs::mem::ShareResult &algoRes,
                                                        UbseMemAlgoResult &algoResult)
{
    for (int i = 0; i < algoRes.numShareLocs; ++i) {
        UbseMemNumaIndexLoc memIndexLoc = {algoRes.sharerLocs[i].hostId, algoRes.sharerLocs[i].socketId,
                                           algoRes.sharerLocs[i].numaId};
        UbseMemNumaLoc memIdLoc{};
        if (!UbseMemTopologyInfoManager::GetInstance().ConvertNumaIndex(memIndexLoc, memIdLoc)) {
            UBSE_LOG_ERROR << "IndexLocToIdLoc error. nodeIndex=" << memIndexLoc.nodeIndex;
            return UBSE_ERROR_INVAL;
        }
        UbseMemDebtNumaInfo exportDebtNumaInfo{memIdLoc.nodeId, memIdLoc.socketId, memIdLoc.numaId,
                                               static_cast<uint64_t>(algoRes.shareSizes[i]) * ONE_M};
        algoResult.exportNumaInfos.push_back(exportDebtNumaInfo);
        UBSE_LOG_INFO << "Algo result exportNodeId=" << exportDebtNumaInfo.nodeId
                      << ", exportSocket=" << exportDebtNumaInfo.socketId
                      << ", exportNumaId=" << exportDebtNumaInfo.numaId
                      << ", blockSize=" << algoResult.blockSize;
    }

    return UBSE_OK;
}

static UbseResult ConstructAddrBorrowResultFromAlgoRes(const UbseMemAddrBorrowReq &req, UbseMemAlgoResult &algoResult)
{
    uint64_t size = 0;
    for (const auto &valist : req.exportAddrList) {
        size += valist.size;
    }
    UbseMemDebtNumaInfo exportDebtNumaInfo{req.exportNodeId, req.dstSocket, req.dstNuma, size};
    UbseMemDebtNumaInfo importDebtNumaInfo{req.importNodeId, req.srcSocket, req.dstNuma, size};
    algoResult.exportNumaInfos.push_back(exportDebtNumaInfo);
    algoResult.importNumaInfos.push_back(importDebtNumaInfo);
    auto ret = UbseMemTopologyInfoManager::GetInstance().GetAttachNodeId(
        algoResult.importNumaInfos[0].nodeId, algoResult.exportNumaInfos[0].nodeId,
        algoResult.exportNumaInfos[0].socketId, algoResult.attachSocketId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get socketCnaInfo failed, ret=" << ret;
    }
    return ret;
}

static bool GetWatermarkDateFromRequestAttr(const UbseMemNumaBorrowReq &req)
{
    auto highWatermark = req.highWatermark;
    auto lowWatermark = req.lowWatermark;
    MemWaterMarkHolder::GetInstance().SetUsedHigh(static_cast<int16_t>(highWatermark));
    MemWaterMarkHolder::GetInstance().SetUsedLow(static_cast<int16_t>(lowWatermark));
    auto initResult = UbseMemStrategyHelper::GetInstance().Init();
    if (initResult != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemResourceAllocateHandler InitFailed.";
        return false;
    }
    UBSE_LOG_INFO << "HighWatermark=" << highWatermark << ", lowWatermark=" << lowWatermark;
    return true;
}

static UbseResult GetImportNodeIdSocketIdStr(const NodeId &exportNodeId, const SocketId &exportSocketId,
                                             const NodeId &importNodeId, std::string &importNodeIdSocketId)
{
    importNodeIdSocketId = "";
    UbseNodeMemCnaInfoInput cnaInput;
    cnaInput.borrowNodeId = importNodeId;
    cnaInput.exportNodeId = exportNodeId;
    cnaInput.exportSocketId = std::to_string(exportSocketId);
    UbseNodeMemCnaInfoOutput cnaOutput;
    auto ret = UbseNodeMemGetTopologyCnaInfo(cnaInput, cnaOutput);
    if (ret != 0) {
        UBSE_LOG_ERROR << "failed to get cna, ubse manager return=" << ret << ", exportNodeId=" << exportNodeId
                       << ", exportSocketId=" << exportSocketId << ", importNodeId=" << importNodeId;
        return UBSE_ERROR;
    }
    importNodeIdSocketId = importNodeId + '-' + cnaOutput.borrowSocketId;
    UBSE_LOG_INFO << "get import socketId, exportNodeId=" << exportNodeId << ", exportSocketId=" << exportSocketId
                   << ", importNodeId=" << importNodeId << ", importSocketId=" << importNodeIdSocketId;
    return UBSE_OK;
}

void UbseMemStrategyHelper::GetSocketCnaSize(const std::string &importNodeIdSocketId,
                                             std::unordered_map<std::string, uint64_t> &socketCnaSize)
{
    auto item = socketCnaSizeCount_.find(importNodeIdSocketId);
    if (item == socketCnaSizeCount_.end()) {
        UBSE_LOG_INFO << "importNodeIdSocketId=" << importNodeIdSocketId << " has not imported mem.";
        socketCnaSize = {};
        return;
    }
    socketCnaSize = item->second;
}

void UbseMemStrategyHelper::AddSocketCnaSize(const std::string &importNodeIdSocketId,
                                             const std::string &exportNodeIdSocketId, uint64_t AddCnaSize)
{
    auto item = socketCnaSizeCount_.find(importNodeIdSocketId);
    if (item == socketCnaSizeCount_.end()) {
        std::unordered_map<std::string, uint64_t> cnaAccount{{exportNodeIdSocketId, AddCnaSize}};
        socketCnaSizeCount_.emplace(importNodeIdSocketId, cnaAccount);
        return;
    }
    if (item->second.find(exportNodeIdSocketId) == item->second.end()) {
        item->second.emplace(exportNodeIdSocketId, AddCnaSize);
        return;
    }
    auto oldCnaSize = item->second[exportNodeIdSocketId];
    uint64_t newPreCnaSize = 0;
    if (SafeAdd(oldCnaSize, AddCnaSize, newPreCnaSize)) {
        UBSE_LOG_ERROR << "Add failed, would overflow, oldCnaSize=" << oldCnaSize << ", addCnaSize=" << newPreCnaSize
                       << " type is uint64";
        return;
    }
    item->second[exportNodeIdSocketId] = newPreCnaSize;
    return;
}

void UbseMemStrategyHelper::SubSocketCnaSize(const std::string &importNodeIdSocketId,
                                             const std::string &exportNodeIdSocketId, uint64_t subCnaSize)
{
    auto item = socketCnaSizeCount_.find(importNodeIdSocketId);
    if (item == socketCnaSizeCount_.end()) {
        UBSE_LOG_ERROR << "Find importNodeIdSocketId=(" << importNodeIdSocketId << ") failed.";
        return;
    }
    if (item->second.find(exportNodeIdSocketId) == item->second.end()) {
        UBSE_LOG_ERROR << "Find exportNodeIdSocketId=(" << exportNodeIdSocketId << ") in importNodeIdSocketId"
                       << importNodeIdSocketId << " cna map failed.";
        return;
    }
    auto oldCnaSize = item->second[exportNodeIdSocketId];
    uint64_t newCnaSize = 0;
    if (SafeSub(oldCnaSize, subCnaSize, newCnaSize)) {
        UBSE_LOG_ERROR << "Sub failed, would overflow, oldCnaSize=" << oldCnaSize << ", subCnaSize=" << newCnaSize
                       << ", type is uint64, exportNodeIdSocketId=" << exportNodeIdSocketId
                       << ", importNodeIdSocketId=" << importNodeIdSocketId;
        return;
    }
    item->second[exportNodeIdSocketId] = newCnaSize;
    return;
}

UbseResult UbseMemStrategyHelper::NumaMemoryBorrow(const ubse::adapter_plugins::mmi::UbseMemNumaBorrowReq &req,
                                                   ubse::adapter_plugins::mmi::UbseMemAlgoResult &algoResult,
                                                   uint64_t checkMaskCode)
{
    UbseMemValidator validator{};
    validator.srcSocket_ = req.srcSocket;
    validator.linkInfo_ = req.linkInfo;
    validator.srcNuma_ = req.srcNuma;
    validator.importNodeId_ = req.importNodeId;
    validator.requestSize_ = req.size;
    validator.candidateNodeList_ = req.candidateNodeList;
    UbseResult ret = UBSE_OK;
    if (validator.CheckAndFilterParam(checkMaskCode) != UBSE_OK) {
        UBSE_LOG_ERROR << "CheckAndFilterParam failed";
        return UBSE_SCHEDULER_ERROR_INVAL;
    }
    auto ubseStatus = validator.GetUbseStatus();
    if (!GetWatermarkDateFromRequestAttr(req)) {
        return UBSE_ERROR;
    }
    tc::rs::mem::BorrowRequest borrowRequest{};
    tc::rs::mem::BorrowResult res{};
    auto result = GetNumaBorrowParam(req, borrowRequest, algoResult);
    if (result != UBSE_OK) {
        return result;
    }
    if (static_cast<PerfLevel>(req.distance) != ::PerfLevel::L0) {
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "request size=" << borrowRequest.requestSize << ", name=" << req.name;
    auto hr = tc::rs::mem::MemPoolStrategy::GetInstance().MemoryBorrow(borrowRequest, ubseStatus, res);
    if (hr != UBSE_OK || res.lenderLength <= 0) {
        PrintUbseStatus(ubseStatus);
        return UBSE_SCHEDULER_ERROR_NO_NODE_CAN_LEND;
    }
    ret = GetBorrowResultFromAlgoRes(algoResult, res);
    if (req.srcSocket != -1) {
        UBSE_LOG_INFO << "Req srcSocket is valid, srcSocket=" << req.srcSocket;
        for (auto &importNumaInfo : algoResult.importNumaInfos) {
            importNumaInfo.socketId = req.srcSocket;
        }
    }
    if (ret != UBSE_OK) {
        return ret;
    }
    UBSE_LOG_INFO << "Algo decision success, name=" << req.name;
    return UBSE_OK;
}

UbseResult UbseMemStrategyHelper::FdMemoryBorrow(const ubse::adapter_plugins::mmi::UbseMemFdBorrowReq &req,
                                                 ubse::adapter_plugins::mmi::UbseMemAlgoResult &algoResult,
                                                 uint64_t checkMaskCode)
{
    UbseMemValidator validator{};
    validator.importNodeId_ = req.importNodeId;
    validator.requestSize_ = req.size;
    validator.candidateNodeList_ = req.candidateNodeList;
    UbseResult ret = UBSE_OK;
    if (validator.CheckAndFilterParam(checkMaskCode) != UBSE_OK) {
        UBSE_LOG_ERROR << "CheckAndFilterParam failed";
        return UBSE_SCHEDULER_ERROR_INVAL;
    }
    auto ubseStatus = validator.GetUbseStatus();
    tc::rs::mem::BorrowRequest borrowRequest{};
    tc::rs::mem::BorrowResult borrowResult{};
    MemWaterMarkHolder::GetInstance().SetUsedHigh(100u);
    auto initResult = UbseMemStrategyHelper::GetInstance().Init();
    if (initResult != UBSE_OK) {
        return initResult;
    }
    ret = GetFdBorrowParam(req, borrowRequest, algoResult);
    if (ret != UBSE_OK) {
        return ret;
    }
    auto hr = tc::rs::mem::MemPoolStrategy::GetInstance().MemoryBorrow(borrowRequest, ubseStatus, borrowResult);
    if (hr != UBSE_OK || borrowResult.lenderLength <= 0) {
        PrintUbseStatus(ubseStatus);
        return UBSE_SCHEDULER_ERROR_NO_NODE_CAN_LEND;
    }

    ret = GetBorrowResultFromAlgoRes(algoResult, borrowResult);
    if (ret != UBSE_OK) {
        return ret;
    }
    return UBSE_OK;
}

UbseResult UbseMemStrategyHelper::ShareMemoryBorrow(const ubse::adapter_plugins::mmi::UbseMemShareBorrowReq &req,
                                                    ubse::adapter_plugins::mmi::UbseMemAlgoResult &algoResult,
                                                    uint64_t checkMaskCode)
{
    UbseMemValidator validator{};
    validator.requestSize_ = req.size;
    validator.providerList_ = req.providerList;
    validator.srcSocket_ = req.withAffinity.affinitySocketId;
    validator.importNodeId_ = req.withAffinity.createReqNodeId;
    validator.lenderInfo_ = req.lenderInfo;
    if (validator.CheckAndFilterParam(checkMaskCode) != UBSE_OK) {
        UBSE_LOG_ERROR << "CheckAndFilterParam failed";
        return UBSE_SCHEDULER_ERROR_INVAL;
    }
    auto ubseStatus = validator.GetUbseStatus();
    tc::rs::mem::ShareRequest shareRequest{};
    auto ret = GetShareStrategyRequest(req, shareRequest, algoResult);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetShareStrategyRequest fail. ret=" << ret;
        return ret;
    }
    MemWaterMarkHolder::GetInstance().SetUsedHigh(100u);
    MemWaterMarkHolder::GetInstance().SetUsedLow(100u);
    auto initResult = UbseMemStrategyHelper::GetInstance().Init();
    if (initResult != UBSE_OK) {
        UBSE_LOG_ERROR << "Init fail";
        return initResult;
    }
    tc::rs::mem::ShareResult shareResult{};
    ret = tc::rs::mem::MemPoolStrategy::GetInstance().MemoryShare(shareRequest, ubseStatus, shareResult);
    if (ret != UBSE_OK || shareResult.numShareLocs == 0) {
        UBSE_LOG_ERROR << "MemoryShare failed";
        return UBSE_SCHEDULER_ERROR_NO_NODE_CAN_LEND;
    }
    ret = ConstructShareBorrowResultFromAlgoRes(req, shareResult, algoResult);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get strategy result, requestSize=" << req.size << ", ret=" << ret;
    }
    return ret;
}

UbseResult UbseMemStrategyHelper::AddrMemoryBorrow(const ubse::adapter_plugins::mmi::UbseMemAddrBorrowReq &req,
                                                   ubse::adapter_plugins::mmi::UbseMemAlgoResult &algoResult,
                                                   uint64_t checkMaskCode)
{
    size_t totalSize = 0;
    for (auto valist : req.exportAddrList) {
        totalSize += valist.size;
    }
    UbseMemValidator validator{};
    validator.importNodeId_ = req.importNodeId;
    validator.requestSize_ = totalSize;
    validator.exportNodeId_ = req.exportNodeId;
    ubse::adapter_plugins::mmi::UbseNumaLocation lenderloc{req.exportNodeId, static_cast<uint32_t>(req.dstNuma)};
    validator.lenderLocs_.push_back(lenderloc);
    validator.lenderSizes_.push_back(totalSize);
    UbseResult ret = UBSE_OK;
    if (validator.CheckAndFilterParam(checkMaskCode) != UBSE_OK) {
        UBSE_LOG_ERROR << "CheckAndFilterParam failed";
        return UBSE_SCHEDULER_ERROR_INVAL;
    }
    ret = ConstructAddrBorrowResultFromAlgoRes(req, algoResult);
    return ret;
}

UbseResult UbseMemStrategyHelper::Init()
{
    static tc::rs::mem::StrategyParam strategyParam{};
    if (!UbseMemTopologyInfoManager::GetInstance().FillStrategyParam(strategyParam)) {
        UBSE_LOG_ERROR << "Strategy init failed, mem param check failed.";
        return UBSE_ERROR;
    }
    static tc::rs::mem::MemPoolStrategy &strategy = tc::rs::mem::MemPoolStrategy::GetInstance();
    strategy.SetLogFunction(Log);                                                // 设置算法日志
    const auto ret = strategy.Init(strategyParam);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Strategy init failed. ret=" << ret;
        return ret;
    }
    UBSE_LOG_INFO << "MemPoolStrategy init successful, errCode=" << ret;
    return UBSE_OK;
}

UbseResult UbseMemStrategyHelper::GetNumaDebtInfoFromNumaPair(
    const GlobalNumaIndex &brwNumaGlobalIdx, const ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo &lend, bool add)
{
    /* 借出方 */
    NodeId nodeId = lend.nodeId;
    int64_t numaId = lend.numaId;
    auto numaInfo = UbseMemTopologyInfoManager::GetInstance().GetNumaInfo(nodeId, numaId);
    if (numaInfo == nullptr) {
        UBSE_LOG_WARN << "Export numa not exist, nodeId=" << nodeId << ", numa=" << numaId;
        return UBSE_OK;
    }
    auto lntNumaGlobalIdx = numaInfo->mGlobalIndex;
    if (brwNumaGlobalIdx >= tc::rs::mem::NUM_TOTAL_NUMA || lntNumaGlobalIdx >= tc::rs::mem::NUM_TOTAL_NUMA) {
        UBSE_LOG_ERROR << "Global numa index valid, brwNumaGlobalIdx=" << brwNumaGlobalIdx
                       << ", lntNumaGlobalIdx=" << lntNumaGlobalIdx;
        return UBSE_ERROR;
    }
    std::map<int16_t, uint64_t> *numaDebt = &debtDetail_.numaDebts[brwNumaGlobalIdx];
    if (add) {
        if (numaDebt->find(lntNumaGlobalIdx) != numaDebt->end()) {
            debtDetail_.numaDebts[brwNumaGlobalIdx][lntNumaGlobalIdx] += lend.size;
        } else {
            debtDetail_.numaDebts[brwNumaGlobalIdx][lntNumaGlobalIdx] = lend.size;
        }
    } else {
        if (numaDebt->find(lntNumaGlobalIdx) != numaDebt->end() &&
            debtDetail_.numaDebts[brwNumaGlobalIdx][lntNumaGlobalIdx] >= lend.size) {
            debtDetail_.numaDebts[brwNumaGlobalIdx][lntNumaGlobalIdx] -= lend.size;
        } else {
            debtDetail_.numaDebts[brwNumaGlobalIdx][lntNumaGlobalIdx] = 0;
        }
    }
    return UBSE_OK;
}

void UbseMemStrategyHelper::Clear()
{
    for (int i = 0; i < tc::rs::mem::NUM_TOTAL_NUMA; i++) {
        debtDetail_.numaDebts[i].clear();
    }
    socketCnaSizeCount_.clear();
}

} // namespace ubse::mem::strategy