/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#include "ubse_mem_strategy_helper.h"

#include "ubse_mem_resource.h"
#include "ubse_mgr_configuration.h"
#include "ubse_mem_algo_account.h"
#include "ubse_mem_functions.h"
#include "ubse_mem_meta_data.h"
#include "ubse_mem_topology_info_manager.h"
#include "ubse_node_topology.h"

namespace ubse::mem::strategy {
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_CONTROLLER_MID)
using namespace obj;
using namespace ubse::resource::mem;
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
        UBSE_LOG_ERROR << "The numaIndex is " << i << ", Memtotal=" << SizeByte2Mb(ubseStatus.numaStatus[i].memTotal)
                     << "MB, memUsed=" << SizeByte2Mb(ubseStatus.numaStatus[i].memUsed)
                     << "MB, memFree=" << SizeByte2Mb(ubseStatus.numaStatus[i].memFree) << "MB. "
                     << "memBorrowed=" << SizeByte2Mb(ubseStatus.numaLedgerStatus[i].memBorrowed)
                     << "MB, memLent=" << SizeByte2Mb(ubseStatus.numaLedgerStatus[i].memLent)
                     << "MB, memShared=" << SizeByte2Mb(ubseStatus.numaLedgerStatus[i].memShared) << "MB.";
    }
}

static UbseResult GetFdBorrowParam(const obj::UbseMemFdBorrowReq &req, tc::rs::mem::BorrowRequest &borrowRequest)
{
    UbseMemNumaLoc borrowNode{"", -1, -1};
    borrowRequest.requestSize = static_cast<int32_t>(req.size / ONE_M);
    borrowRequest.requestSize = ubse::mem::strategy::CeilToN(
        borrowRequest.requestSize, ubse::mem::strategy::MemMgrConfiguration::GetInstance().GetUnitSize());
    UBSE_LOG_DEBUG << "The request name is " << req.name << ", request_size=" << borrowRequest.requestSize;
    borrowRequest.urgentLevel = tc::rs::mem::RequestUrgentLevel::LEVEL0;
    borrowNode.nodeId = req.requestNodeId;
    borrowRequest.requestLoc.hostId = UbseMemTopologyInfoManager::GetInstance().NodeIdToIndex(borrowNode.nodeId);
    if (borrowRequest.requestLoc.hostId < 0 || borrowRequest.requestLoc.hostId >= tc::rs::mem::NUM_HOSTS) {
        UBSE_LOG_ERROR << "Failed to translate nodeId to index, nodeId = " << borrowNode.nodeId
                     << ", nodeIndex=" << borrowRequest.requestLoc.hostId;
        return E_CODE_MANAGER;
    }
    borrowRequest.requestLoc.socketId = -1;
    borrowRequest.requestLoc.numaId = -1;
    return UBSE_OK;
}

static UbseResult GetWaterBorrowParam(const obj::UbseMemNumaBorrowReq &req, tc::rs::mem::BorrowRequest &request)
{
#ifndef UB_ENVIRONMENT
    if (req.size > FOUR_G) {
        RM_LOG_DEBUG << "Size greater than 4G, invalid param " << request.requestSize;
        return E_CODE_INVALID_PAR;
    }
#endif
    if (req.size == 0) {
        UBSE_LOG_ERROR << "Size is invalid param " << request.requestSize;
        return E_CODE_INVALID_PAR;
    }
    UbseMemNumaLoc borrowNode{req.importNodeId, req.srcSocket, req.srcNuma};
    request.urgentLevel = tc::rs::mem::RequestUrgentLevel::LEVEL0;
    request.requestSize = static_cast<int32_t>(req.size / ONE_M);
    request.requestSize =
        CeilToN(request.requestSize, ubse::mem::strategy::MemMgrConfiguration::GetInstance().GetUnitSize());
    UBSE_LOG_DEBUG << "requestSize=" << request.requestSize << ", name=" << req.name;

    if (req.srcNuma != -1 && req.srcSocket != -1) {
        UBSE_LOG_DEBUG << "Not borrow Info";
        std::shared_ptr<ubse::mem::strategy::MemNumaInfo> memNumaInfo =
            UbseMemTopologyInfoManager::GetInstance().GetNumaInfo(req.importNodeId, req.srcNuma);
        if (memNumaInfo == nullptr) {
            UBSE_LOG_DEBUG << "IdLocToIndexLoc failed, nodeId=" << borrowNode.nodeId << "socketid="
                        << borrowNode.socketId << "numaId=" << borrowNode.numaId;
            return E_CODE_MANAGER;
        }
        if (memNumaInfo->mUbseMemNumaLoc.socketId != req.srcSocket) {
            UBSE_LOG_ERROR << "socketId " << memNumaInfo->mUbseMemNumaLoc.socketId << " != " << req.srcSocket;
            return E_CODE_INVALID_PAR;
        }
        UbseMemNumaIndexLoc memIndexLoc = memNumaInfo->mUbseMemNumaIndexLoc;
        request.requestLoc.hostId = static_cast<int16_t>(memIndexLoc.nodeIndex);
        request.requestLoc.socketId = static_cast<int8_t>(memIndexLoc.socketIndex);
        request.requestLoc.numaId = static_cast<int8_t>(memIndexLoc.numaIndex);
    } else {
        request.requestLoc.hostId = UbseMemTopologyInfoManager::GetInstance().NodeIdToIndex(borrowNode.nodeId);
        if (request.requestLoc.hostId < 0 || request.requestLoc.hostId >= tc::rs::mem::NUM_HOSTS) {
            UBSE_LOG_ERROR << "Failed to translate nodeId to index, nodeId = " << borrowNode.nodeId
                         << ", nodeIndex=" << request.requestLoc.hostId;
            return E_CODE_MANAGER;
        }
        request.requestLoc.socketId = -1;
        request.requestLoc.numaId = -1;
    }

    return UBSE_OK;
}

static UbseResult GetFdBorrowResultFromAlgoRes(const obj::UbseMemFdBorrowReq &req, obj::UbseMemAlgoResult &algoResult,
                                               const tc::rs::mem::BorrowResult &borrowResult)
{
    UbseMemNumaLoc memIdLocLend{};
    UbseMemNumaLoc memIdLocBorrow{};
    for (int i = 0; i < borrowResult.lenderLength; i++) {
        UbseMemNumaIndexLoc memIndexLocLend = {borrowResult.lenderLocs[i].hostId, borrowResult.lenderLocs[i].socketId,
                                               borrowResult.lenderLocs[i].numaId};
        if (!UbseMemTopologyInfoManager::GetInstance().ConvertNumaIndex(memIndexLocLend, memIdLocLend)) {
            UBSE_LOG_ERROR << "IndexLocToIdLoc error. nodeIndex is=" << memIndexLocLend.nodeIndex;
            return E_CODE_STRATEGY_ERROR;
        }
        UbseMemNumaIndexLoc memIndexLocBorrow = {borrowResult.borrowerLocs[i].hostId,
                                                 borrowResult.borrowerLocs[i].socketId,
                                                 borrowResult.borrowerLocs[i].numaId};
        if (!UbseMemTopologyInfoManager::GetInstance().ConvertNumaIndex(memIndexLocBorrow, memIdLocBorrow)) {
            UBSE_LOG_ERROR << "IndexLocToIdLoc error nodeIndex is=" << memIndexLocBorrow.nodeIndex;
            return E_CODE_MANAGER;
        }
        auto lenderSize = static_cast<uint64_t>(borrowResult.lenderSizes[i]) * ONE_M;
        UbseMemDebtNumaInfo exportDebtNumaInfo{memIdLocLend.nodeId, memIdLocLend.socketId, memIdLocLend.numaId,
                                               lenderSize};
        UbseMemDebtNumaInfo importDebtNumaInfo{memIdLocBorrow.nodeId, memIdLocBorrow.socketId, memIdLocBorrow.numaId,
                                               lenderSize};
        algoResult.exportNumaInfos.emplace_back(exportDebtNumaInfo);
        algoResult.importNumaInfos.emplace_back(importDebtNumaInfo);
        UbseMemStrategyHelper::GetInstance().AddNumaLendOutCountByMemIdLoc(
            memIdLocLend, static_cast<uint64_t>(borrowResult.lenderSizes[i]) * ONE_M);
    }
#ifdef UB_ENVIRONMENT
    SocketCnaTopoInfo socketCnaTopoInfo{};
    auto ret =
        UbseMemTopologyInfoManager::GetInstance().GetSocketCnaInfo(memIdLocBorrow, memIdLocLend, socketCnaTopoInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get socketCnaInfo failed, ret=" << ret;
        UbseMemStrategyHelper::GetInstance().RollBackPreNumaLendOut(algoResult);
        return ret;
    }
    ret = UbseMemStrategyHelper::GetInstance().AddPreSocketCnaSize(socketCnaTopoInfo.importNodeIdSocketId,
                                                                   socketCnaTopoInfo.exportNodeIdSocketId, req.size);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "add pre socket cna size failed, ret=" << ret
                     << ", importNodeIdSocketId=" << socketCnaTopoInfo.importNodeIdSocketId
                     << ", exportNodeIdSocketId=" << socketCnaTopoInfo.exportNodeIdSocketId;
        UbseMemStrategyHelper::GetInstance().RollBackPreNumaLendOut(algoResult);
        return ret;
    }
#endif
    return UBSE_OK;
}

static UbseResult ConstructNumaBorrowResultFromAlgoRes(const UbseMemNumaBorrowReq &req,
                                                       const tc::rs::mem::BorrowResult &algoRes,
                                                       UbseMemAlgoResult &algoResult)
{
    UbseMemNumaLoc memIdLocBorrow{};
    UbseMemNumaLoc memIdLocLend{};
    for (int i = 0; i < algoRes.lenderLength; i++) {
        UbseMemNumaIndexLoc memIndexLocLend = {algoRes.lenderLocs[i].hostId, algoRes.lenderLocs[i].socketId,
                                               algoRes.lenderLocs[i].numaId};
        if (!UbseMemTopologyInfoManager::GetInstance().ConvertNumaIndex(memIndexLocLend, memIdLocLend)) {
            UBSE_LOG_ERROR << "IndexLocToIdLoc error. nodeIndex=" << memIndexLocLend.nodeIndex;
            return E_CODE_STRATEGY_ERROR;
        }
        UbseMemNumaIndexLoc memIndexLocBorrow = {algoRes.borrowerLocs[i].hostId, algoRes.borrowerLocs[i].socketId,
                                                 algoRes.borrowerLocs[i].numaId};
        if (!UbseMemTopologyInfoManager::GetInstance().ConvertNumaIndex(memIndexLocBorrow, memIdLocBorrow)) {
            UBSE_LOG_ERROR << "IndexLocToIdLoc error nodeIndex is=" << memIndexLocBorrow.nodeIndex;
            return E_CODE_MANAGER;
        }
        auto lenderSize = static_cast<uint64_t>(algoRes.lenderSizes[i]) * ONE_M;
        UbseMemDebtNumaInfo exportDebtNumaInfo{memIdLocLend.nodeId, memIdLocLend.socketId, memIdLocLend.numaId,
                                               lenderSize};
        UbseMemDebtNumaInfo importDebtNumaInfo{memIdLocBorrow.nodeId, memIdLocBorrow.socketId, memIdLocBorrow.numaId,
                                               lenderSize};
        algoResult.exportNumaInfos.emplace_back(exportDebtNumaInfo);
        algoResult.importNumaInfos.emplace_back(importDebtNumaInfo);
        UbseMemStrategyHelper::GetInstance().AddNumaLendOutCountByMemIdLoc(
            memIdLocLend, static_cast<uint64_t>(algoRes.lenderSizes[i]) * ONE_M);
    }
    // 增加记录账本socket的账本部分，这部分不需要隔离
#ifdef UB_ENVIRONMENT
    SocketCnaTopoInfo socketCnaTopoInfo{};
    auto ret =
        UbseMemTopologyInfoManager::GetInstance().GetSocketCnaInfo(memIdLocBorrow, memIdLocLend, socketCnaTopoInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get socketCnaInfo failed, ret=" << ret;
        UbseMemStrategyHelper::GetInstance().RollBackPreNumaLendOut(algoResult);
        return ret;
    }
    ret = UbseMemStrategyHelper::GetInstance().AddPreSocketCnaSize(socketCnaTopoInfo.importNodeIdSocketId,
                                                                   socketCnaTopoInfo.exportNodeIdSocketId, req.size);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "add pre socket cna size failed, ret=" << ret
                     << ", importNodeIdSocketId=" << socketCnaTopoInfo.importNodeIdSocketId
                     << ", exportNodeIdSocketId=" << socketCnaTopoInfo.exportNodeIdSocketId;
        UbseMemStrategyHelper::GetInstance().RollBackPreNumaLendOut(algoResult);
        return ret;
    }
#endif
    return UBSE_OK;
}

static bool GetWatermarkDateFromRequestAttr(const UbseMemNumaBorrowReq &req)
{
    auto highWatermark = req.highWatermark;
    auto lowWatermark = req.lowWatermark;
    if (highWatermark <= 0 || lowWatermark <= 0) {
        UBSE_LOG_ERROR << "GetWatermarkDateFromRequestAttr error, name=" << req.name;
        return false;
    }
    if (highWatermark > 100u || lowWatermark > 100u) {
        UBSE_LOG_ERROR << "GetWatermarkDateFromRequestAttr error, name=" << req.name;
        return false;
    }
    MemWaterMarkHolder::GetInstance().SetUsedHigh(static_cast<int16_t>(highWatermark));
    MemWaterMarkHolder::GetInstance().SetUsedLow(static_cast<int16_t>(lowWatermark));
    auto initResult = UbseMemStrategyHelper::GetInstance().Init();
    if (initResult != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemResourceAllocateHandler InitFailed.";
        return false;
    }
    UBSE_LOG_DEBUG << "HighWatermark=" << highWatermark << ", lowWatermark=" << lowWatermark;
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

#if defined(COMPILE_WITHOUT_OBMM) && !defined(DEBUG_MEM_UT)
    importNodeIdSocketId = importNodeId + '-' + "0";
#else
    struct UbseNodeMemCnaInfoOutput cnaOutput;
    auto ret = UbseNodeMemGetTopologyCnaInfo(cnaInput, cnaOutput);
    if (ret != 0) {
        UBSE_LOG_ERROR << "failed to get cna, ubse manager return: " << ret << " exportNodeId: " << exportNodeId
                     << " exportSocketId: " << exportSocketId << " importNodeId: " << importNodeId;
        return E_CODE_STRATEGY_ERROR;
    }
    importNodeIdSocketId = importNodeId + '-' + cnaOutput.borrowSocketId;
#endif
    UBSE_LOG_DEBUG << "get import socketId, exportNodeId=" << exportNodeId << " exportSocketId=" << exportSocketId
                 << " importNodeId=" << importNodeId << " importSocketId=" << importNodeIdSocketId;
    return UBSE_OK;
}

void UbseMemStrategyHelper::AddNumaLendOutCountByMemIdLoc(const UbseMemNumaLoc &idLoc, uint64_t memLent)
{
    mNumaOutCountLock.Lock();
    const auto key = idLoc.nodeId + std::to_string(idLoc.numaId);
    const auto old = mNumaLendOutCount[key];
    mNumaLendOutCount[key] = old + memLent;
    mNumaOutCountLock.UnLock();
}

void UbseMemStrategyHelper::AddNumaShareOutCountByMemIdLoc(const UbseMemNumaLoc &idLoc, uint64_t memLent)
{
    mNumaOutCountLock.Lock();
    const auto key = idLoc.nodeId + std::to_string(idLoc.numaId);
    const auto old = mNumaShareOutCount[key];
    mNumaShareOutCount[key] = old + memLent;
    mNumaOutCountLock.UnLock();
}

void UbseMemStrategyHelper::SubNumaLendOutCountByMemIdLoc(const UbseMemNumaLoc &idLoc, uint64_t memLent)
{
    mNumaOutCountLock.Lock();
    const auto key = idLoc.nodeId + std::to_string(idLoc.numaId);
    const auto old = mNumaLendOutCount[key];
    if (old <= memLent) {
        mNumaLendOutCount[key] = 0;
    } else {
        mNumaLendOutCount[key] = old - memLent;
    }
    mNumaOutCountLock.UnLock();
}

void UbseMemStrategyHelper::SubNumaShareOutCountByMemIdLoc(const UbseMemNumaLoc &idLoc, uint64_t memLent)
{
    mNumaOutCountLock.Lock();
    const auto key = idLoc.nodeId + std::to_string(idLoc.numaId);
    const auto old = mNumaShareOutCount[key];
    if (old <= memLent) {
        mNumaShareOutCount[key] = 0;
    } else {
        mNumaShareOutCount[key] = old - memLent;
    }
    mNumaOutCountLock.UnLock();
}

uint64_t UbseMemStrategyHelper::GetNumaLendOutCountByMemIdLoc(const UbseMemNumaLoc &idLoc)
{
    mNumaOutCountLock.Lock();
    const auto key = idLoc.nodeId + std::to_string(idLoc.numaId);
    const auto old = mNumaLendOutCount[key];
    mNumaOutCountLock.UnLock();
    return static_cast<uint64_t>(old);
}

uint64_t UbseMemStrategyHelper::GetNumaShareOutCountByMemIdLoc(const UbseMemNumaLoc &idLoc)
{
    mNumaOutCountLock.Lock();
    const auto key = idLoc.nodeId + std::to_string(idLoc.numaId);
    const auto old = mNumaShareOutCount[key];
    mNumaOutCountLock.UnLock();
    return static_cast<uint64_t>(old);
}

void UbseMemStrategyHelper::RollBackPreNumaLendOut(const obj::UbseMemAlgoResult &algoResult)
{
    for (int i = 0; i < algoResult.exportNumaInfos.size(); ++i) {
        UbseMemNumaLoc lenderLocs{algoResult.exportNumaInfos[i].nodeId, algoResult.exportNumaInfos[i].socketId,
                                      algoResult.exportNumaInfos[i].numaId};
        UbseMemStrategyHelper::GetInstance().SubNumaLendOutCountByMemIdLoc(lenderLocs,
                                                                           algoResult.exportNumaInfos[i].size);
    }
}

UbseResult UbseMemStrategyHelper::AddPreSocketCnaSize(const std::string &importNodeIdSocketId,
                                                      const std::string &exportNodeIdSocketId, uint64_t preAddCnaSize)
{
    std::unique_lock<std::shared_mutex> lockGuard(preCnaCountLock);
    auto item = preSocketCnaSizeAccount.find(importNodeIdSocketId);
    if (item == preSocketCnaSizeAccount.end()) {
        std::unordered_map<std::string, uint64_t> cnaAccount{{exportNodeIdSocketId, preAddCnaSize}};
        preSocketCnaSizeAccount.emplace(importNodeIdSocketId, cnaAccount);
        return UBSE_OK;
    }
    if (item->second.find(exportNodeIdSocketId) == item->second.end()) {
        item->second.emplace(exportNodeIdSocketId, preAddCnaSize);
        return UBSE_OK;
    }
    auto oldPreCnaSize = item->second[exportNodeIdSocketId];
    uint64_t newPreCnaSize = 0;
    if (SafeAdd(oldPreCnaSize, preAddCnaSize, newPreCnaSize)) {
        UBSE_LOG_ERROR << "Add failed, would overflow, oldPreCnaSize=" << oldPreCnaSize
                     << ", preAddCnaSize=" << newPreCnaSize << ", type is uint64";
        return UBSE_OK;
    }
    item->second[exportNodeIdSocketId] = newPreCnaSize;
    return UBSE_OK;
}

UbseResult UbseMemStrategyHelper::SubPreSocketCnaSize(const std::string &importNodeIdSocketId,
                                                      const std::string &exportNodeIdSocketId, uint64_t subCnaSize)
{
    std::unique_lock<std::shared_mutex> lockGuard(preCnaCountLock);
    auto item = preSocketCnaSizeAccount.find(importNodeIdSocketId);
    // 有些操作不经过内部算法，直接进行内存借用，则在结果处不更新该部分
    if (item == preSocketCnaSizeAccount.end()) {
        UBSE_LOG_WARN << "Find importNodeIdSocketId(" << importNodeIdSocketId << ") failed.";
        return UBSE_OK;
    }
    if (item->second.find(exportNodeIdSocketId) == item->second.end()) {
        UBSE_LOG_WARN << "Find exportNodeIdSocketId(" << exportNodeIdSocketId << ") in importNodeIdSocketId"
                    << importNodeIdSocketId << " cna map failed.";
        return UBSE_OK;
    }
    auto oldPreCnaSize = item->second[exportNodeIdSocketId];
    uint64_t newPreCnaSize = 0;
    if (oldPreCnaSize < subCnaSize) {
        UBSE_LOG_WARN << "Sub failed, would overflow, oldPreCnaSize=" << oldPreCnaSize << ", subCnaSize=" << subCnaSize
                    << ", type=uint64, exportNodeIdSocketId=" << exportNodeIdSocketId
                    << " , importNodeIdSocketId=" << importNodeIdSocketId;
        item->second[exportNodeIdSocketId] = 0;
        return UBSE_OK;
    }
    item->second[exportNodeIdSocketId] = oldPreCnaSize - subCnaSize;
    return UBSE_OK;
}

void UbseMemStrategyHelper::GetSocketCnaAccount(
    std::unordered_map<std::string, std::unordered_map<std::string, uint64_t>> &socketCnaSizeCountOut)
{
    std::shared_lock<std::shared_mutex> lockGuard(socketCnaAccountLock);
    socketCnaSizeCountOut = socketCnaSizeCount;
}

void UbseMemStrategyHelper::GetSocketCnaSize(const std::string &importNodeIdSocketId,
                                             std::unordered_map<std::string, uint64_t> &socketCnaSize)
{
    std::shared_lock<std::shared_mutex> lockGuard(socketCnaAccountLock);
    auto item = socketCnaSizeCount.find(importNodeIdSocketId);
    if (item == socketCnaSizeCount.end()) {
        UBSE_LOG_INFO << "importNodeIdSocketId (" << importNodeIdSocketId << ") has not import mem.";
        socketCnaSize = {};
        return;
    }
    socketCnaSize = item->second;
}

void UbseMemStrategyHelper::GetPreSocketCnaSize(const std::string &importNodeIdSocketId,
                                                std::unordered_map<std::string, uint64_t> &socketCnaSize)
{
    std::shared_lock<std::shared_mutex> lockGuard(preCnaCountLock);
    auto item = preSocketCnaSizeAccount.find(importNodeIdSocketId);
    if (item == preSocketCnaSizeAccount.end()) {
        UBSE_LOG_INFO << "importNodeIdSocketId (" << importNodeIdSocketId << ") has not import mem.";
        socketCnaSize = {};
        return;
    }
    socketCnaSize = item->second;
}

UbseResult UbseMemStrategyHelper::AddSocketCnaSize(const std::string &importNodeIdSocketId,
                                                   const std::string &exportNodeIdSocketId, uint64_t AddCnaSize)
{
    std::unique_lock<std::shared_mutex> lockGuard(socketCnaAccountLock);
    auto item = socketCnaSizeCount.find(importNodeIdSocketId);
    if (item == socketCnaSizeCount.end()) {
        std::unordered_map<std::string, uint64_t> cnaAccount{{exportNodeIdSocketId, AddCnaSize}};
        socketCnaSizeCount.emplace(importNodeIdSocketId, cnaAccount);
        return UBSE_OK;
    }
    if (item->second.find(exportNodeIdSocketId) == item->second.end()) {
        item->second.emplace(exportNodeIdSocketId, AddCnaSize);
        return UBSE_OK;
    }
    auto oldCnaSize = item->second[exportNodeIdSocketId];
    uint64_t newPreCnaSize = 0;
    if (SafeAdd(oldCnaSize, AddCnaSize, newPreCnaSize)) {
        UBSE_LOG_ERROR << "Add failed, would overflow, oldCnaSize=" << oldCnaSize << ", addCnaSize=" << newPreCnaSize
                     << ", type is uint64";
        return E_CODE_INVALID_PAR;
    }
    item->second[exportNodeIdSocketId] = newPreCnaSize;
    return UBSE_OK;
}

UbseResult UbseMemStrategyHelper::SubSocketCnaSize(const std::string &importNodeIdSocketId,
                                                   const std::string &exportNodeIdSocketId, uint64_t subCnaSize)
{
    std::unique_lock<std::shared_mutex> lockGuard(socketCnaAccountLock);
    auto item = socketCnaSizeCount.find(importNodeIdSocketId);
    if (item == socketCnaSizeCount.end()) {
        UBSE_LOG_ERROR << "Find importNodeIdSocketId(" << importNodeIdSocketId << ") failed.";
        return E_CODE_MANAGER;
    }
    if (item->second.find(exportNodeIdSocketId) == item->second.end()) {
        UBSE_LOG_ERROR << "Find exportNodeIdSocketId(" << exportNodeIdSocketId << ") in importNodeIdSocketId"
                     << importNodeIdSocketId << " cna map failed.";
        return E_CODE_MANAGER;
    }
    auto oldCnaSize = item->second[exportNodeIdSocketId];
    uint64_t newCnaSize = 0;
    if (SafeSub(oldCnaSize, subCnaSize, newCnaSize)) {
        UBSE_LOG_ERROR << "Sub failed, would overflow, oldCnaSize=" << oldCnaSize << ", subCnaSize=" << newCnaSize
                     << ", type is uint64, exportNodeIdSocketId=" << exportNodeIdSocketId
                     << " , importNodeIdSocketId=" << importNodeIdSocketId;
        return E_CODE_INVALID_PAR;
    }
    item->second[exportNodeIdSocketId] = newCnaSize;
    return UBSE_OK;
}

UbseResult UbseMemStrategyHelper::GetSocketTotalLendOutByMemIdLoc(const UbseMemNumaLoc &idLoc,
                                                                  uint64_t &preSocketTotalExportMem)
{
    preSocketTotalExportMem = 0;
    auto numaList = UbseMemTopologyInfoManager::GetInstance().GetAllNumaInfo("");
    for (const auto &numa : numaList) {
        if (numa == nullptr) {
            UBSE_LOG_ERROR << "numa==nullptr";
            return UBSE_ERROR;
        }
        if (numa->mUbseMemNumaLoc.nodeId == idLoc.nodeId && numa->mUbseMemNumaLoc.socketId == idLoc.socketId) {
            bool isOverflow = false;
            auto numaLendOut = GetNumaLendOutCountByMemIdLoc(numa->mUbseMemNumaLoc);
        }
    }
    return UBSE_OK;
}

bool UbseMemStrategyHelper::CheckSocketExportOverLimit(const UbseMemNumaLoc &exportLoc, uint64_t curSize)
{
    uint64_t socketTotalExportMem = 0;
    NodeId exportNodeId = exportLoc.nodeId;
    SocketId exportSocketId = exportLoc.socketId;

    auto ret = UbseMemTopologyInfoManager::GetInstance().GetSocketTotalLentMem(exportNodeId, exportSocketId,
                                                                               socketTotalExportMem);
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "Get socket total export mem failed,"
                    << " nodeId: " << exportNodeId << " socketId: " << exportSocketId << ",ret " << ret;
        return true;
    }

    uint64_t preSocketTotalExportMem = 0; // 已借出但还没刷新的那部分内存 检查预占内存
    ret = GetSocketTotalLendOutByMemIdLoc(exportLoc, preSocketTotalExportMem);
    socketTotalExportMem += preSocketTotalExportMem + curSize;
    if ((ret != UBSE_OK) || (socketTotalExportMem > MAX_EXPORT_MEM_SIZE_PER_SOCKET)) {
        UBSE_LOG_INFO << "Socket total export mem " << socketTotalExportMem << ", max export mem per socket "
                    << MAX_EXPORT_MEM_SIZE_PER_SOCKET << ", exportNodeId: " << exportNodeId
                    << " exportSocketId: " << exportSocketId << ",ret " << ret;
        return true;
    }
    return false;
}

bool UbseMemStrategyHelper::CheckSocketImportOverLimit(const UbseMemNumaLoc &exportLoc, const NodeId &importNodeId,
                                                       uint64_t curSize)
{
    /* 共享内存的场景没有importNodeId，不做import超限检查 */
    if (importNodeId.empty() || exportLoc.nodeId == importNodeId) {
        UBSE_LOG_DEBUG << "The exportNodeId is " << exportLoc.nodeId << ", importNodeId is " << importNodeId;
        return false;
    }
    uint64_t socketTotalImportMem = 0;
    NodeId exportNodeId = exportLoc.nodeId;
    SocketId exportSocketId = exportLoc.socketId;
    std::string importNodeIdSocketId;
    auto ret = GetImportNodeIdSocketIdStr(exportNodeId, exportSocketId, importNodeId, importNodeIdSocketId);
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "Get GetImportNodeIdSocketIdStr failed,"
                    << " exportNodeId=" << exportNodeId << " exportSocketId=" << exportSocketId
                    << " importNodeId=" << importNodeId << ", ret=" << ret;
        return true;
    }

    std::stringstream ss;
    ss << exportNodeId << "-" << exportSocketId;
    std::string exportNodeIdSocketId = ss.str();
    std::unordered_map<std::string, uint64_t> socketCnaSize;
    GetSocketCnaSize(importNodeIdSocketId, socketCnaSize);
    if (socketCnaSize.find(exportNodeIdSocketId) != socketCnaSize.end()) {
        socketTotalImportMem = socketCnaSize[exportNodeIdSocketId];
    }
    std::unordered_map<std::string, uint64_t> preSocketCnaSize;
    GetPreSocketCnaSize(importNodeIdSocketId, preSocketCnaSize); // 已借入但还没刷新的那部分内存
    if (preSocketCnaSize.find(exportNodeIdSocketId) != preSocketCnaSize.end()) {
        socketTotalImportMem += preSocketCnaSize[exportNodeIdSocketId];
    }
    socketTotalImportMem += curSize;
    auto maxImportLimit = MemMgrConfiguration::GetInstance().GetMaxSocketImportSize();
    if (socketTotalImportMem > maxImportLimit) {
        UBSE_LOG_INFO << "socketTotalImportMem=" << socketTotalImportMem << ", maxImportLimit=" << maxImportLimit
                    << ", exportNodeId=" << exportNodeId << " exportSocketId=" << exportSocketId
                    << " importNodeId=" << importNodeId << ", ret=" << ret;
        return true;
    }
    return false;
}

void UbseMemStrategyHelper::FilterOutUnavailableNuma(tc::rs::mem::UbseStatus &ubseStatus, const NodeId &importNodeId,
                                                     uint64_t curSize, std::shared_ptr<MemNumaInfo> numa)
{
    if ((numa->mUbseMemNumaLoc.nodeId != importNodeId) && CheckProviderNodeHasBorrowed(numa->mUbseMemNumaLoc.nodeId)) {
        UBSE_LOG_WARN << "node(" << numa->mUbseMemNumaLoc.nodeId
                    << ") has borrowed before, not involved in algorithmic decision";
        ubseStatus.numaStatus[numa->mGlobalIndex].memFree = INVALID_VALUE64;
        ubseStatus.numaStatus[numa->mGlobalIndex].memTotal = INVALID_VALUE64;
        ubseStatus.numaStatus[numa->mGlobalIndex].memUsed = INVALID_VALUE64;
        return;
    }
#ifdef UB_ENVIRONMENT
    if (CheckSocketExportOverLimit(numa->mUbseMemNumaLoc, curSize) ||
        CheckSocketImportOverLimit(numa->mUbseMemNumaLoc, importNodeId, curSize)) {
        UBSE_LOG_WARN << "node(" << numa->mUbseMemNumaLoc.nodeId
                    << ") import or export over per socket limit, not involved in algorithmic decision";
        ubseStatus.numaStatus[numa->mGlobalIndex].memFree = INVALID_VALUE64;
        ubseStatus.numaStatus[numa->mGlobalIndex].memTotal = INVALID_VALUE64;
        ubseStatus.numaStatus[numa->mGlobalIndex].memUsed = INVALID_VALUE64;
        return;
    }
#endif
    return;
}

UbseResult UbseMemStrategyHelper::GetUbseStatus(tc::rs::mem::UbseStatus &ubseStatus, const NodeId &importNodeId,
                                                uint64_t curSize)
{
    // MEM_TRACE_DELAY_BEGIN(MANAGER_STRATEGY_SUB_GET_UBSE_STATUS) 计算耗时
    auto numaList = UbseMemTopologyInfoManager::GetInstance().GetAllNumaInfo("");
    for (const auto &numa : numaList) {
        if (numa == nullptr || numa->mGlobalIndex >= tc::rs::mem::NUM_TOTAL_NUMA) {
            UBSE_LOG_ERROR << "numa==nullptr||numa->mGlobalIndex>=NUM_TOTAL_NUMA";
            // MEM_TRACE_DELAY_END(MANAGER_STRATEGY_SUB_GET_UBSE_STATUS, -1)
            return UBSE_ERROR;
        }
        const tc::rs::mem::MemLoc loc{numa->mUbseMemNumaIndexLoc.nodeIndex,
                                      static_cast<int8_t>(numa->mUbseMemNumaIndexLoc.socketIndex),
                                      static_cast<int8_t>(numa->mUbseMemNumaIndexLoc.numaIndex)};
        ubseStatus.numaStatus[numa->mGlobalIndex].memFree = numa->mMemFree;
        ubseStatus.numaStatus[numa->mGlobalIndex].memTotal = numa->mMemTotal;
        ubseStatus.numaStatus[numa->mGlobalIndex].memUsed = numa->mMemUsed;
        ubseStatus.numaStatus[numa->mGlobalIndex].numa = loc;
        ubseStatus.numaLedgerStatus[numa->mGlobalIndex].numa = loc;

        FilterOutUnavailableNuma(ubseStatus, importNodeId, curSize, numa);

        if (numa->mActualMemTotal == INVALID_VALUE64) {
            ubseStatus.numaLedgerStatus[numa->mGlobalIndex].memBorrowed = INVALID_VALUE64;
            ubseStatus.numaLedgerStatus[numa->mGlobalIndex].memLent = INVALID_VALUE64;
            ubseStatus.numaLedgerStatus[numa->mGlobalIndex].memShared = INVALID_VALUE64;
        } else {
            ubseStatus.numaLedgerStatus[numa->mGlobalIndex].memBorrowed = numa->mMemBorrowed;
            ubseStatus.numaLedgerStatus[numa->mGlobalIndex].memLent =
                numa->mMemLent + GetNumaLendOutCountByMemIdLoc(numa->mUbseMemNumaLoc);
            ubseStatus.numaLedgerStatus[numa->mGlobalIndex].memShared =
                numa->mMemShared + GetNumaShareOutCountByMemIdLoc(numa->mUbseMemNumaLoc);
        }
    }

    mDebtDetailLock.Lock();
    ubseStatus.debtDetail = debtDetail;
    mDebtDetailLock.UnLock();
    return UBSE_OK;
}

UbseResult UbseMemStrategyHelper::NumaMemoryBorrow(const obj::UbseMemNumaBorrowReq &req,
                                                   obj::UbseMemAlgoResult &algoResult)
{
    UbseMemSpinLocker lockGuard(&mSpinLock);
    tc::rs::mem::UbseStatus ubseStatus;
    /* 检查请求方是否借出过内存，如果借出过，则不能借入 */
    if (CheckRequestNodeHasLentOut(req.importNodeId)) {
        UBSE_LOG_ERROR << "The request node has lent out memory, nodeId=" << req.importNodeId;
        return E_CODE_MANAGER;
    }
    auto ret = GetUbseStatus(ubseStatus, req.importNodeId, req.size);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "WaterMemoryBorrow get ubse status failed, ret=" << ret;
        return ret;
    }
#ifndef UB_ENVIRONMENT
    if (MemMgrConfiguration::GetInstance().GetManagerVmEnable()) {
        ret = ModifyStatusToFitOs(ubseStatus); // 虚拟化场景需要单独修正
        if (ret != UBSE_OK) {
            RM_LOG_ERROR << "WaterMemoryBorrow Modify numa status failed";
            return ret;
        }
    }
#endif
    if (!GetWatermarkDateFromRequestAttr(req)) {
        return E_CODE_STRATEGY_ERROR;
    }
    tc::rs::mem::BorrowRequest borrowRequest{};
    tc::rs::mem::BorrowResult res{};
    auto result = GetWaterBorrowParam(req, borrowRequest);
    if (result != UBSE_OK) {
        return result;
    }
    if (static_cast<PerfLevel>(req.distance) != ::PerfLevel::L0) {
        return E_CODE_INVALID_PAR;
    }
    UBSE_LOG_INFO << "request size=" << borrowRequest.requestSize;
    // 参数检查拦截
    ret = CheckBorrowSizeMeetLimit(ubseStatus, borrowRequest);
    if (ret != UBSE_OK) {
        return ret;
    }
    auto hr = tc::rs::mem::MemPoolStrategy::GetInstance().MemoryBorrow(borrowRequest, ubseStatus, res);
    if (ret != UBSE_OK) {
        PrintUbseStatus(ubseStatus);
        return E_CODE_STRATEGY_ERROR;
    }
    ret = ConstructNumaBorrowResultFromAlgoRes(req, res, algoResult);
    if (ret != UBSE_OK) {
        return ret;
    }
    UBSE_LOG_INFO << "Algo decision success, name=" << req.name;
    return UBSE_OK;
}

UbseResult UbseMemStrategyHelper::FdMemoryBorrow(const obj::UbseMemFdBorrowReq &req, obj::UbseMemAlgoResult &algoResult)
{
    tc::rs::mem::UbseStatus ubseStatus{};
    auto ret = GetUbseStatus(ubseStatus, req.requestNodeId, req.size);
    if (ret != UBSE_OK) {
        return ret;
    }
    tc::rs::mem::BorrowRequest borrowRequest{};
    tc::rs::mem::BorrowResult borrowResult{};
    if (MemWaterMarkHolder::GetInstance().SetUsedHigh(100u) || MemWaterMarkHolder::GetInstance().SetUsedLow(100u)) {
        auto initResult = UbseMemStrategyHelper::GetInstance().Init();
        if (initResult != UBSE_OK) {
            return initResult;
        }
    }
    ret = GetFdBorrowParam(req, borrowRequest);
    if (ret != UBSE_OK) {
        return ret;
    }
    ret = CheckBorrowSizeMeetLimit(ubseStatus, borrowRequest);
    if (ret != UBSE_OK) {
        return ret;
    }
    mSpinLock.Lock();
    if (CheckRequestNodeHasLentOut(req.requestNodeId)) {
        mSpinLock.UnLock();
        return E_CODE_MANAGER;
    }
    auto hr = tc::rs::mem::MemPoolStrategy::GetInstance().MemoryBorrow(borrowRequest, ubseStatus, borrowResult);
    if (hr != UBSE_OK || borrowResult.lenderLength <= 0) {
        mSpinLock.UnLock();
        PrintUbseStatus(ubseStatus);
        return E_CODE_STRATEGY_ERROR;
    }

    ret = GetFdBorrowResultFromAlgoRes(req, algoResult, borrowResult);
    if (ret != UBSE_OK) {
        mSpinLock.UnLock();
        return ret;
    }

    mSpinLock.UnLock();
    return UBSE_OK;
}

UbseResult UbseMemStrategyHelper::CheckBorrowSizeMeetLimit(tc::rs::mem::UbseStatus &ubseStatus,
                                                           tc::rs::mem::BorrowRequest &borrowRequest)
{
    // 通过获取一个host上所有numa的借入值，来比较大小
    auto requetNodeIndex = borrowRequest.requestLoc.hostId;
    uint64_t borrowedSize = 0;
    for (int i = 0; i < tc::rs::mem::NUM_TOTAL_NUMA; i++) {
        if (ubseStatus.numaLedgerStatus[i].numa.hostId == requetNodeIndex) {
            borrowedSize += SizeByte2Mb(ubseStatus.numaLedgerStatus[i].memBorrowed);
        }
    }
    auto nodeId = UbseMemTopologyInfoManager::GetInstance().NodeIndexToId(requetNodeIndex);
    uint64_t maxBorrowSize = 0;
    maxBorrowSize = MemMgrConfiguration::GetInstance().GetMaxBorrowSize();
    if (maxBorrowSize - borrowedSize < borrowRequest.requestSize) {
        UBSE_LOG_ERROR << "This borrow would exceed maxBorrow size, nodeId=" << nodeId;
        PrintUbseStatus(ubseStatus);
        return E_CODE_INVALID_PAR;
    }
    return UBSE_OK;
}

/* 指定进程借用场景，检查请求方之前是否已借出过内存，借出过，则不允许其借入，以免借用成环 */
bool UbseMemStrategyHelper::CheckRequestNodeHasLentOut(const NodeId &requestNodeId)
{
#if defined(UB_ENVIRONMENT) && !defined(COMPILE_WITHOUT_OBMM)
    auto meta = AlgoAccountManger::GetInstance().GetAllAlgoAccountByNode(requestNodeId);
    for (uint32_t i = 0; i < meta.size(); i++) {
        if (meta[i]->exportNumaLocs[0].nodeId == requestNodeId) {
            UBSE_LOG_ERROR << "Request node " << requestNodeId << " has lent out before, it can not borrow.";
            return true;
        }
    }
#endif
    UBSE_LOG_DEBUG << "Request node " << requestNodeId << " has not lent out before, it can borrow.";
    return false;
}

/* 检查提供方之前是否已借入过内存，借入过，则不允许其借出，以免借用成环 */
bool UbseMemStrategyHelper::CheckProviderNodeHasBorrowed(const NodeId &providerNodeId)
{
#if defined(UB_ENVIRONMENT) && !defined(DEBUG_MEM_UT)
    auto meta = AlgoAccountManger::GetInstance().GetAllAlgoAccountByNode(providerNodeId);
    for (uint32_t i = 0; i < meta.size(); i++) {
        if (meta[i]->importNumaLocs[0].nodeId == providerNodeId) {
            UBSE_LOG_ERROR << "Provider node " << providerNodeId << " has borrowed before, it can not lend.";
            for (const auto &ref : meta) {
                UBSE_LOG_DEBUG << ref->importNumaLocs[0].nodeId << "->" << ref->importNumaLocs[0].nodeId;
            }
            return true;
        }
    }
#endif
    return false;
}

UbseResult UbseMemStrategyHelper::Init()
{
    // MEM_TRACE_DELAY_BEGIN(MANAGER_STRATEGY_SUB_INIT) // 计算算法耗时时间
    std::lock_guard<std::mutex> lockGuard(mIniLock);
    mInited.store(false);
    static tc::rs::mem::StrategyParam strategyParam{};
    if (!UbseMemTopologyInfoManager::GetInstance().FillStrategyParam(strategyParam)) {
        UBSE_LOG_ERROR << "Strategy init failed, mem param check failed.";
        // MEM_TRACE_DELAY_END(MANAGER_STRATEGY_SUB_INIT, -1)
        return E_CODE_STRATEGY_ERROR;
    }
    static tc::rs::mem::MemPoolStrategy &strategy = tc::rs::mem::MemPoolStrategy::GetInstance();
    strategy.SetLogLevel(MemMgrConfiguration::GetInstance().GetAlgoLogLevel());  // 设置日志优先级
    strategy.SetLogFunction(Log);                                                // 设置算法日志
    const auto ret = strategy.Init(strategyParam);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Strategy init failed. ret=" << ret;
        // MEM_TRACE_DELAY_END(MANAGER_STRATEGY_SUB_INIT, -1) 计算耗时
        return ret;
    }
    UBSE_LOG_INFO << "MemPoolStrategy init successful, errCode=" << ret;
    mInited.store(true);
    // MEM_TRACE_DELAY_END(MANAGER_STRATEGY_SUB_INIT, 0)
    return UBSE_OK;
}

UbseResult UbseMemStrategyHelper::GetNumaDebtInfoFromNumaPair(const GlobalNumaIndex &brwNumaGlobalIdx,
                                                              const obj::UbseMemDebtNumaInfo &lend, bool add)
{
    /* 借出方 */
    UbseMemSpinLocker lockGuard(&mDebtDetailLock);
    NodeId nodeId = lend.nodeId;
    int64_t numaId = lend.numaId;
    auto numaInfo = UbseMemTopologyInfoManager::GetInstance().GetNumaInfo(nodeId, numaId);
#ifndef DEBUG_MEM_UT
    if (numaInfo == nullptr) {
        /* 节点如果down了，会被从mNodeIdMap中移除 */
        UBSE_LOG_WARN << "Export numa not exist, nodeId=" << nodeId << " numa=" << numaId;
        return UBSE_OK;
    }
#endif
    auto lntNumaGlobalIdx = numaInfo->mGlobalIndex;
    if (brwNumaGlobalIdx >= tc::rs::mem::NUM_TOTAL_NUMA || lntNumaGlobalIdx >= tc::rs::mem::NUM_TOTAL_NUMA) {
        UBSE_LOG_ERROR << "Global numa index valid, brwNumaGlobalIdx=" << brwNumaGlobalIdx
                     << " lntNumaGlobalIdx=" << lntNumaGlobalIdx;
        return E_CODE_INVALID_PAR;
    }
    std::map<int16_t, uint64_t> *numaDebt = &debtDetail.numaDebts[brwNumaGlobalIdx];
    if (add) {
        if (numaDebt->find(lntNumaGlobalIdx) != numaDebt->end()) {
            debtDetail.numaDebts[brwNumaGlobalIdx][lntNumaGlobalIdx] += lend.size;
        } else {
            debtDetail.numaDebts[brwNumaGlobalIdx][lntNumaGlobalIdx] = lend.size;
        }
    } else {
        if (numaDebt->find(lntNumaGlobalIdx) != numaDebt->end() &&
            debtDetail.numaDebts[brwNumaGlobalIdx][lntNumaGlobalIdx] >= lend.size) {
            debtDetail.numaDebts[brwNumaGlobalIdx][lntNumaGlobalIdx] -= lend.size;
        } else {
            debtDetail.numaDebts[brwNumaGlobalIdx][lntNumaGlobalIdx] = 0;
        }
    }
    return UBSE_OK;
}

} // namespace ubse::mem::strategy