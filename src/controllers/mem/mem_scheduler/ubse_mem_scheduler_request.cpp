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
#include "ubse_mem_scheduler_request.h"

#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_logger.h"
#include "ubse_math_util.h"

namespace ubse::mem::scheduler {
UBSE_DEFINE_THIS_MODULE("ubse_mem_scheduler");

namespace {

constexpr char CONF_MEM_SECTION[] = "ubse.memory";
constexpr char CONF_LENDER_BALANCE[] = "lender.balance";

bool IsLenderBalanceEnabled()
{
    auto confModule = context::UbseContext::GetInstance().GetModule<config::UbseConfModule>();
    if (confModule == nullptr) {
        return false;
    }
    bool lenderBalance = false;
    confModule->GetConf<bool>(CONF_MEM_SECTION, CONF_LENDER_BALANCE, lenderBalance);
    return lenderBalance;
}

} // namespace

SchedulerRequest SchedulerRequest::FromFdBorrowReq(const adapter_plugins::mmi::UbseMemFdBorrowReq& req)
{
    SchedulerRequest schedulerReq;
    schedulerReq.name_ = req.name;
    schedulerReq.requestNodeId_ = req.requestNodeId;
    schedulerReq.importNodeId_ = req.importNodeId;
    schedulerReq.requestSize_ = req.size;

    schedulerReq.filterNames_ = {
        "ConfigConsistencyFilter", "RoleConflictFilter",     "LenderRoleFilter",   "GroupFilter",
        "ProviderFilter",          "NodeStateFilter",        "RadiusBorrowFilter", "RadiusLenderFilter",
        "LendCountFilter",         "TopoReachabilityFilter", "MaxLentSizeFilter",  "FreeMemoryFilter"};
    if (!req.candidateNodeList.empty()) {
        schedulerReq.provideNodes_ = req.candidateNodeList;
        schedulerReq.filterNames_.emplace_back("RequestedProvidersFilter");
    }

    if (!req.lenderLocs.empty()) {
        std::vector<adapter_plugins::mmi::UbseMemLenderInfo> lenderInfos;
        for (size_t i = 0; i < req.lenderLocs.size(); ++i) {
            adapter_plugins::mmi::UbseMemLenderInfo info{};
            info.nodeId = req.lenderLocs[i].nodeId;
            info.numaId = req.lenderLocs[i].numaId;
            if (i >= req.lenderSizes.size()) {
                UBSE_LOG_WARN << "lenderSizes.size()=" << req.lenderSizes.size()
                              << " < lenderLocs.size()=" << req.lenderLocs.size() << ", lender[" << i
                              << "] has no size, defaulting to 0";
            }
            info.lender_size = (i < req.lenderSizes.size()) ? req.lenderSizes[i] : 0;
            lenderInfos.push_back(info);
        }
        schedulerReq.params_["lenderInfos"] = lenderInfos;
        schedulerReq.filterNames_.emplace_back("SpecifiedLenderFilter");
    }

    schedulerReq.scoreNames_ = {
        "LatencyScore", "RegionBalanceScore", "BalanceScore", "BorrowReliabilityScore", "DivideNumaScore",
    };
    if (IsLenderBalanceEnabled()) {
        schedulerReq.scoreNames_[3] = "ReliabilityBalanceScore";
        schedulerReq.weights_ = ScoreWeights::ForLenderBalance();
    } else {
        schedulerReq.weights_ = ScoreWeights::ForBorrow();
    }
    schedulerReq.requestMode_ = RequestMode::BORROW;

    return schedulerReq;
}

SchedulerRequest SchedulerRequest::FromNumaBorrowReq(const adapter_plugins::mmi::UbseMemNumaBorrowReq& req)
{
    SchedulerRequest schedulerReq;
    schedulerReq.name_ = req.name;
    schedulerReq.importNodeId_ = req.importNodeId;
    schedulerReq.requestNodeId_ = req.requestNodeId;
    schedulerReq.requestSize_ = req.size;
    size_t highWatermark = req.highWatermark;
    if (highWatermark > MAX_PERCENT) {
        UBSE_LOG_WARN << "highWatermark=" << highWatermark << " exceeds MAX_PERCENT, capping to " << MAX_PERCENT;
        highWatermark = MAX_PERCENT;
    }
    schedulerReq.params_["highWatermark"] = highWatermark;

    schedulerReq.filterNames_ = {
        "ConfigConsistencyFilter", "RoleConflictFilter",     "LenderRoleFilter",   "GroupFilter",
        "ProviderFilter",          "NodeStateFilter",        "RadiusBorrowFilter", "RadiusLenderFilter",
        "LendCountFilter",         "TopoReachabilityFilter", "MaxLentSizeFilter",  "FreeMemoryFilter"};

    if (req.linkInfo.lenderSocketId != -1) {
        schedulerReq.params_["linkInfo"] = req.linkInfo;
        schedulerReq.filterNames_.emplace_back("SpecifiedLinkFilter");
    } else if (!req.lenderLocs.empty()) {
        std::vector<adapter_plugins::mmi::UbseMemLenderInfo> lenderInfos;
        for (size_t i = 0; i < req.lenderLocs.size(); ++i) {
            adapter_plugins::mmi::UbseMemLenderInfo info{};
            info.nodeId = req.lenderLocs[i].nodeId;
            info.numaId = req.lenderLocs[i].numaId;
            if (i >= req.lenderSizes.size()) {
                UBSE_LOG_WARN << "lenderSizes.size()=" << req.lenderSizes.size()
                              << " < lenderLocs.size()=" << req.lenderLocs.size() << ", lender[" << i
                              << "] has no size, defaulting to 0";
            }
            info.lender_size = (i < req.lenderSizes.size()) ? req.lenderSizes[i] : 0;
            lenderInfos.push_back(info);
        }
        schedulerReq.params_["lenderInfos"] = lenderInfos;
        schedulerReq.filterNames_.emplace_back("SpecifiedLenderFilter");
    }
    if (!req.candidateNodeList.empty()) {
        schedulerReq.provideNodes_ = req.candidateNodeList;
        schedulerReq.filterNames_.emplace_back("RequestedProvidersFilter");
    }
    if (req.srcSocket != -1) {
        schedulerReq.filterNames_.emplace_back("SocketAffinityFilter");
        schedulerReq.params_["affinitySocketId"] = req.srcSocket;
    }

    schedulerReq.scoreNames_ = {
        "LatencyScore", "RegionBalanceScore", "BalanceScore", "BorrowReliabilityScore", "DivideNumaScore",
    };
    if (IsLenderBalanceEnabled()) {
        schedulerReq.scoreNames_[3] = "ReliabilityBalanceScore";
        schedulerReq.weights_ = ScoreWeights::ForLenderBalance();
    } else {
        schedulerReq.weights_ = ScoreWeights::ForBorrow();
    }
    schedulerReq.requestMode_ = RequestMode::BORROW;

    return schedulerReq;
}

SchedulerRequest SchedulerRequest::FromAddrBorrowReq(const adapter_plugins::mmi::UbseMemAddrBorrowReq& req)
{
    SchedulerRequest schedulerReq;
    schedulerReq.name_ = req.name;
    schedulerReq.requestNodeId_ = req.requestNodeId;
    schedulerReq.importNodeId_ = req.importNodeId;
    schedulerReq.requestSize_ = 0;
    for (const auto& addr : req.exportAddrList) {
        if (!ubse::utils::SafeAdd(schedulerReq.requestSize_, addr.size, schedulerReq.requestSize_)) {
            UBSE_LOG_ERROR << "requestSize overflow when summing exportAddrList";
            schedulerReq.requestSize_ = UINT64_MAX / 2;
            break;
        }
    }

    schedulerReq.filterNames_ = {"LenderRoleFilter", "NodeStateFilter", "RoleConflictFilter"};

    if (req.dstSocket != -1 && req.dstNuma != -1 && !req.exportAddrList.empty()) {
        std::vector<adapter_plugins::mmi::UbseMemLenderInfo> lenderInfos;
        adapter_plugins::mmi::UbseMemLenderInfo info{};
        info.nodeId = req.exportNodeId;
        info.socketId = static_cast<uint32_t>(req.dstSocket);
        info.numaId = static_cast<uint32_t>(req.dstNuma);
        lenderInfos.push_back(info);
        schedulerReq.params_["lenderInfos"] = lenderInfos;
        schedulerReq.filterNames_.emplace_back("SpecifiedLenderFilter");
    }

    schedulerReq.scoreNames_ = {
        "LatencyScore", "RegionBalanceScore", "BalanceScore", "BorrowReliabilityScore", "DivideNumaScore",
    };
    schedulerReq.weights_ = ScoreWeights::ForBorrow();
    schedulerReq.requestMode_ = RequestMode::BORROW;
    schedulerReq.params_["isAddr"] = true;

    return schedulerReq;
}

SchedulerRequest SchedulerRequest::FromShareBorrowReq(const adapter_plugins::mmi::UbseMemShareBorrowReq& req)
{
    SchedulerRequest schedulerReq;
    schedulerReq.name_ = req.name;
    schedulerReq.requestNodeId_ = req.requestNodeId;
    schedulerReq.requestSize_ = req.size;
    schedulerReq.provideNodes_ = req.providerList;
    schedulerReq.params_["shmRegion"] = req.shmRegion;

    if (req.withAffinity.enableCreateWithAffinity) {
        schedulerReq.params_["affinitySocketId"] = static_cast<int>(req.withAffinity.affinitySocketId);
    }

    schedulerReq.filterNames_ = {"ConfigConsistencyFilter", "LenderRoleFilter",  "ProviderFilter",
                                 "NodeStateFilter",         "RegionFilter",      "LendCountFilter",
                                 "TopoReachabilityFilter",  "MaxLentSizeFilter", "FreeMemoryFilter"};
    if (!req.providerList.empty()) {
        schedulerReq.filterNames_.emplace_back("RequestedProvidersFilter");
    }
    if (req.withAffinity.enableCreateWithAffinity) {
        schedulerReq.filterNames_.emplace_back("SocketAffinityFilter");
    }
    if (req.lenderInfo.lender_size != 0) {
        std::vector<adapter_plugins::mmi::UbseMemLenderInfo> lenderInfos;
        lenderInfos.push_back(req.lenderInfo);
        schedulerReq.params_["lenderInfos"] = lenderInfos;
        schedulerReq.filterNames_.emplace_back("SpecifiedLenderFilter");
    }

    schedulerReq.scoreNames_ = {
        "LatencyScore", "RegionBalanceScore", "BalanceScore", "ShareReliabilityScore", "DivideNumaScore",
    };
    schedulerReq.requestMode_ = RequestMode::SHARE;
    schedulerReq.weights_ = ScoreWeights::ForShare();

    return schedulerReq;
}

} // namespace ubse::mem::scheduler
