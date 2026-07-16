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
#include <algorithm>

#include "ubse_mem_scheduler_impl.h"
#include "ubse_node_controller.h"

namespace ubse::mem::scheduler {

UBSE_DEFINE_THIS_MODULE("ubse_mem_scheduler");

using ubse::common::def::UbseResult;

namespace {

using NumaInfoPtr = SchedulerNumaInfo*;

void FillFromNumaList(const std::vector<NumaInfoPtr>& numaList, const ScoredNode& node, const SchedulerRequest& request,
                      adapter_plugins::mmi::UbseMemAlgoResult& algoResult, uint64_t& remaining)
{
    uint64_t blockSize = algoResult.blockSize * ONE_M;
    auto highWatermarkOpt = request.GetParamOpt<size_t>("highWatermark");
    uint64_t waterLine = static_cast<uint64_t>(highWatermarkOpt.value_or(MAX_PERCENT));
    for (const auto& numaInfo : numaList) {
        if (remaining == 0) {
            break;
        }
        uint64_t available = numaInfo->GetAvailableLendSize(waterLine, blockSize);
        if (available < blockSize) {
            continue;
        }
        uint64_t borrowSize = std::min(available, remaining);
        borrowSize = ((borrowSize + blockSize - 1) / blockSize) * blockSize;
        if (borrowSize > available) {
            borrowSize = (available / blockSize) * blockSize;
        }
        UBSE_LOG_INFO << "Add algoResult export numaInfo, nodeId=" << node.nodeId << ", socketId=" << node.socketId
                      << ", numaId=" << numaInfo->GetNumaId() << ", borrowSize=" << borrowSize;

        algoResult.exportNumaInfos.push_back({.nodeId = node.nodeId,
                                              .socketId = static_cast<int>(node.socketId),
                                              .numaId = static_cast<int64_t>(numaInfo->GetNumaId()),
                                              .size = borrowSize});

        if (request.requestMode_ == RequestMode::BORROW) {
            UBSE_LOG_INFO << "Add algoResult import numaInfo, nodeId=" << request.importNodeId_
                          << ", socketId=" << node.socketId << ", borrowSize=" << borrowSize;
            algoResult.importNumaInfos.push_back({.nodeId = request.importNodeId_,
                                                  .socketId = static_cast<int>(node.socketId),
                                                  .numaId = static_cast<int64_t>(numaInfo->GetNumaId()),
                                                  .size = borrowSize});
        }
        remaining = (remaining >= borrowSize) ? (remaining - borrowSize) : 0;
    }
}

std::vector<NumaInfoPtr> GetSortedNumaList(SchedulerNodeManager* nodeInfo, const NodeId& nodeId, SocketId socketId)
{
    auto socketInfo = nodeInfo->GetSocketInfo(nodeId, socketId);
    if (socketInfo == nullptr) {
        return {};
    }
    std::vector<NumaInfoPtr> numaList;
    for (const auto& numaEntry : socketInfo->GetAllNumaInfos()) {
        numaList.push_back(numaEntry.second.get());
    }
    std::sort(numaList.begin(), numaList.end(),
              [](NumaInfoPtr a, NumaInfoPtr b) { return a->GetMemFreeSize() > b->GetMemFreeSize(); });
    return numaList;
}
} // namespace

SchedulerImpl::SchedulerImpl()
    : nodeInfo_(std::make_unique<SchedulerNodeManager>()),
      account_(std::make_unique<SchedulerAccountManager>())
{
    nodeInfo_->SetAccountManager(account_.get());
    account_->SetNodeManager(nodeInfo_.get());
    filterManager_ = std::make_unique<SchedulerFilterManager>(nodeInfo_.get(), account_.get());
    scoreManager_ = std::make_unique<SchedulerScoreManager>(nodeInfo_.get(), account_.get());
}

SchedulerImpl::~SchedulerImpl() = default;

UbseResult SchedulerImpl::Init()
{
    {
        std::lock_guard<std::mutex> lock{lock_};
        if (initialized_) {
            UBSE_LOG_INFO << "Mem-scheduler already initialized";
            return UBSE_OK;
        }
    }
    using ubse::nodeController::UbseNodeController;
    // Lambda 延迟获取单例引用是安全的：GetInstance() 基于 magic static，
    // C++11 保证线程安全构造；每次回调时获取引用确保不会使用悬挂引用。
    auto ret = UbseNodeController::GetInstance().RegClusterStateNotifyHandler(
        [](const ubse::nodeController::UbseNodeInfo& nodeInfo) {
            return SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeInfo);
        });
    if (ret != UBSE_OK) {
        return ret;
    }
    std::lock_guard<std::mutex> lock{lock_};
    if (initialized_) {
        return UBSE_OK;
    }
    filterManager_->Init();
    scoreManager_->Init();
    nodeInfo_->InitPageSize();
    nodeInfo_->InitRadiusConfig();
    nodeInfo_->InitLenderBalance();
    initialized_ = true;
    UBSE_LOG_INFO << "Mem-scheduler inits successfully";
    return UBSE_OK;
}

void SchedulerImpl::ClearCache()
{
    std::lock_guard<std::mutex> lock{lock_};
    nodeInfo_->Clear();
    account_->Clear();
    UBSE_LOG_INFO << "Clear cache done";
}

UbseResult SchedulerImpl::NodeObjChangeHandler(const UbseNodeInfo& nodeInfo)
{
    std::lock_guard<std::mutex> lock{lock_};
    auto ret = nodeInfo_->UpdateNodeInfo(nodeInfo);
    return ret;
}

UbseResult SchedulerImpl::ScheduleBorrow(const SchedulerRequest& request,
                                         adapter_plugins::mmi::UbseMemAlgoResult& algoResult)
{
    UBSE_LOG_INFO << "Schedule borrow request begin, name=" << request.name_
                  << ", requestNodeId=" << request.requestNodeId_;
    auto nodeMap = nodeController::UbseNodeController::GetInstance().GetAllNodes();
    auto ret = nodeInfo_->UpdateAllNumaMemInfo(nodeMap);
    if (ret != UBSE_OK) {
        return ret;
    }
    ret = nodeInfo_->UpdateAllLinkInfo(nodeMap);
    if (ret != UBSE_OK) {
        return ret;
    }
    auto nodes = nodeInfo_->GetAllNodes();

    ret = filterManager_->FilterNodes(nodes, request);
    if (ret != UBSE_OK) {
        return ret;
    }
    if (nodes.empty()) {
        auto allNodes = nodeInfo_->GetAllNodes();
        for (const auto& node : allNodes) {
            auto nodePtr = nodeInfo_->GetNodeInfo(node.nodeId);
            if (nodePtr && nodePtr->GetClusterState() == UbseNodeClusterState::UBSE_NODE_SMOOTHING) {
                UBSE_LOG_INFO << "Node " << node.nodeId << " is in SMOOTHING state, will retry";
                return UBSE_SCHEDULER_ERROR_NODE_RECONCILE;
            }
        }
        UBSE_LOG_ERROR << "All nodes filtered out, can not schedule borrow request";
        return UBSE_SCHEDULER_ERROR_NO_NODE_CAN_LEND;
    }
    std::vector<ScoredNode> results;
    ret = scoreManager_->ScoreAndRank(nodes, request, results, 1);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Score and rank nodes failed, ret=" << ret;
        return ret;
    }
    if (results.empty()) {
        UBSE_LOG_ERROR << "Score and rank nodes failed, no valid nodes";
        return UBSE_ERROR;
    }
    return FillAlgoResult(request, results[0], algoResult);
}

UbseResult SchedulerImpl::SelectNumaForAddr(const SchedulerRequest& request,
                                            const adapter_plugins::mmi::UbseMemLenderInfo& lenderInfo,
                                            adapter_plugins::mmi::UbseMemAlgoResult& algoResult)
{
    const NodeId importNodeId = request.importNodeId_;

    // ADDR 借用：精确大小，不需要 blockSize 对齐；不需要检查内存容量
    uint64_t borrowSize = request.requestSize_;

    UBSE_LOG_INFO << "Add addr borrow algoResult export numaInfo, nodeId=" << lenderInfo.nodeId
                  << ", socketId=" << algoResult.attachSocketId << ", numaId=" << lenderInfo.numaId
                  << ", borrowSize=" << borrowSize;

    algoResult.exportNumaInfos.push_back({.nodeId = lenderInfo.nodeId,
                                          .socketId = static_cast<int>(algoResult.attachSocketId),
                                          .numaId = static_cast<int64_t>(lenderInfo.numaId),
                                          .size = borrowSize});
    if (request.requestMode_ == RequestMode::BORROW) {
        algoResult.importNumaInfos.push_back({.nodeId = importNodeId,
                                              .socketId = static_cast<int>(algoResult.attachSocketId),
                                              .numaId = static_cast<int64_t>(lenderInfo.numaId),
                                              .size = borrowSize});
    }
    return UBSE_OK;
}

UbseResult SchedulerImpl::SelectNumaByLenderInfo(const SchedulerRequest& request,
                                                 const adapter_plugins::mmi::UbseMemLenderInfo& lenderInfo,
                                                 adapter_plugins::mmi::UbseMemAlgoResult& algoResult)
{
    const NodeId importNodeId = request.importNodeId_;

    uint64_t blockSize = algoResult.blockSize * ONE_M;
    uint64_t borrowSize = ((lenderInfo.lender_size + blockSize - 1) / blockSize) * blockSize;
    UBSE_LOG_INFO << "Add algoResult export numaInfo, nodeId=" << lenderInfo.nodeId
                  << ", socketId=" << algoResult.attachSocketId << ", numaId=" << lenderInfo.numaId
                  << ", borrowSize=" << borrowSize;

    algoResult.exportNumaInfos.push_back({.nodeId = lenderInfo.nodeId,
                                          .socketId = static_cast<int>(algoResult.attachSocketId),
                                          .numaId = static_cast<int64_t>(lenderInfo.numaId),
                                          .size = borrowSize});
    if (request.requestMode_ == RequestMode::BORROW) {
        UBSE_LOG_INFO << "Add algoResult import numaInfo, nodeId=" << importNodeId
                      << ", socketId=" << algoResult.attachSocketId << ", borrowSize=" << borrowSize;
        algoResult.importNumaInfos.push_back({.nodeId = importNodeId,
                                              .socketId = static_cast<int>(algoResult.attachSocketId),
                                              .numaId = static_cast<int64_t>(lenderInfo.numaId),
                                              .size = borrowSize});
    }
    return UBSE_OK;
}

UbseResult SchedulerImpl::SelectNumaByFreeMemory(const SchedulerRequest& request, const ScoredNode& node,
                                                 adapter_plugins::mmi::UbseMemAlgoResult& algoResult)
{
    auto numaList = GetSortedNumaList(nodeInfo_.get(), node.nodeId, node.socketId);
    if (numaList.empty()) {
        return UBSE_ERROR;
    }

    uint64_t remaining = request.requestSize_;
    FillFromNumaList(numaList, node, request, algoResult, remaining);
    if (remaining > 0) {
        UBSE_LOG_ERROR << "SelectNumaByFreeMemory insufficient, remaining=" << remaining;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult SchedulerImpl::SelectNumaByReliable(const SchedulerRequest& request, const ScoredNode& node,
                                               adapter_plugins::mmi::UbseMemAlgoResult& algoResult)
{
    auto allNumas = GetSortedNumaList(nodeInfo_.get(), node.nodeId, node.socketId);
    if (allNumas.empty()) {
        return UBSE_ERROR;
    }

    auto borrowInfo = account_->GetNumaBorrowInfo(node.nodeId, node.socketId, request.importNodeId_);

    auto sortByBorrowerCount = [&](NumaInfoPtr a, NumaInfoPtr b) {
        auto aIt = borrowInfo.borrowerCount.find(a->GetNumaId());
        auto bIt = borrowInfo.borrowerCount.find(b->GetNumaId());
        uint32_t aCount = (aIt != borrowInfo.borrowerCount.end()) ? aIt->second : 0;
        uint32_t bCount = (bIt != borrowInfo.borrowerCount.end()) ? bIt->second : 0;
        if (aCount != bCount) {
            return aCount < bCount;
        }
        return a->GetMemFreeSize() > b->GetMemFreeSize();
    };

    std::vector<NumaInfoPtr> providedNumas;
    std::vector<NumaInfoPtr> notProvidedNumas;
    for (const auto& numa : allNumas) {
        if (borrowInfo.borrowedByRequestor.count(numa->GetNumaId())) {
            providedNumas.push_back(numa);
        } else {
            notProvidedNumas.push_back(numa);
        }
    }
    std::sort(providedNumas.begin(), providedNumas.end(), sortByBorrowerCount);
    std::sort(notProvidedNumas.begin(), notProvidedNumas.end(), sortByBorrowerCount);

    uint64_t remaining = request.requestSize_;
    FillFromNumaList(providedNumas, node, request, algoResult, remaining);
    FillFromNumaList(notProvidedNumas, node, request, algoResult, remaining);

    if (remaining > 0) {
        UBSE_LOG_ERROR << "SelectNumaByReliable failed, remaining=" << remaining;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult SchedulerImpl::FillAlgoResult(const SchedulerRequest& request, const ScoredNode& node,
                                         adapter_plugins::mmi::UbseMemAlgoResult& algoResult)
{
    auto nodeInfo = nodeInfo_->GetNodeInfo(node.nodeId);
    if (nodeInfo == nullptr) {
        return UBSE_ERROR;
    }
    algoResult.blockSize = nodeInfo->GetBlockSize();
    if (algoResult.blockSize == 0) {
        UBSE_LOG_ERROR << "Algo result block size is 0, nodeId=" << node.nodeId;
        return UBSE_ERROR;
    }
    algoResult.attachSocketId = node.socketId;
    UBSE_LOG_INFO << "Add algoResult, attachSocketId=" << algoResult.attachSocketId
                  << ", blockSize=" << algoResult.blockSize;

    auto lenderInfosOpt = request.GetParamOpt<std::vector<adapter_plugins::mmi::UbseMemLenderInfo>>("lenderInfos");
    if (lenderInfosOpt.has_value() && !lenderInfosOpt->empty() && (*lenderInfosOpt)[0].numaId != UINT32_MAX) {
        const auto& lenderInfos = *lenderInfosOpt;
        // ADDR 借用走专用路径：不检查内存容量，不走 blockSize 对齐
        if (request.GetParamOpt<bool>("isAddr").value_or(false)) {
            return SelectNumaForAddr(request, lenderInfos[0], algoResult);
        }
        for (const auto& li : lenderInfos) {
            if (li.numaId == UINT32_MAX) {
                continue;
            }
            auto ret = SelectNumaByLenderInfo(request, li, algoResult);
            if (ret != UBSE_OK) {
                return ret;
            }
        }
        return UBSE_OK;
    }

    auto lenderBalanceOpt = request.GetParamOpt<bool>("lenderBalance");
    if (lenderBalanceOpt.has_value() && lenderBalanceOpt.value() && request.requestMode_ == RequestMode::BORROW) {
        return SelectNumaByReliable(request, node, algoResult);
    }

    return SelectNumaByFreeMemory(request, node, algoResult);
}

} // namespace ubse::mem::scheduler
