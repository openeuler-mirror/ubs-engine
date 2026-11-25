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

#include "ubse_mem_cabinet_ledger.h"

#include <condition_variable>
#include <map>
#include <mutex>
#include <new>
#include <utility>

#include "ubse_com_base.h"
#include "ubse_com_module.h"
#include "ubse_com_op_code.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_cabinet_ledger_util.h"
#include "ubse_mem_util.h"
#include "api/ubse_mem_controller_helper.h"
#include "message/ubse_mem_cabinet_ledger_serial.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::com;
using namespace ubse::common::def;
using namespace ubse::context;
using namespace ubse::log;
using namespace ubse::mem::serial;
using namespace ubse::serial;

namespace {
std::mutex g_crossCabinetGateMutex;
std::condition_variable g_crossCabinetGateCv;
uint32_t g_crossCabinetInFlight = 0;

int64_t RemainingRoundMs(std::chrono::steady_clock::time_point deadline)
{
    const auto left =
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
    return left > 0 ? left : 0;
}

 /**
 * @brief 提取本 T 的外柜对端集合：master 账本与实际上报账本两份归并去重，按所属柜分组
 *        T 两份账本均空（冷启动）时返回空，不产生任何跨柜查询（设计文档 §5.1/§7.1.4）
 */
std::map<uint32_t, std::set<std::string>> ExtractCrossCabinetPeerGroups(const NodeMemDebtInfo& masterDebtInfo,
                                                                        const NodeMemDebtInfo& agentDebtInfo,
                                                                        uint32_t selfGroupId,
                                                                        std::set<std::string>& deferredPeers)
{
    std::map<uint32_t, std::set<std::string>> groups;
    auto peers = GetDebtPeerNodeIds(masterDebtInfo);
    const auto agentPeers = GetDebtPeerNodeIds(agentDebtInfo);
    peers.insert(agentPeers.begin(), agentPeers.end());
    for (const auto& peer : peers) {
        uint32_t groupId = 0;
        if (!GetCabinetGroupIdByNodeId(peer, groupId)) {
            // 拓扑无法定位对端柜号：无法发起聚合查询，按 SKIPPED 遮蔽（不得当空账本处理）
            UBSE_LOG_WARN << "[CLOS_CAB] action=extract_peer peer=" << peer << " result=group_unknown";
            deferredPeers.insert(peer);
            continue;
        }
        if (groupId != selfGroupId) {
            groups[groupId].insert(peer);
        }
    }
    return groups;
}

 /**
 * @brief 定位外柜柜主：以该柜任一对端为锚点走级联柜主定位（设计文档 §1.3）
 */
bool LocateCabinetMaster(const std::set<std::string>& peers, std::string& cabinetMasterNodeId)
{
    for (const auto& peer : peers) {
        std::string master;
        if (UbseGetCascadeMasterNodeIdByAgentNodeId(peer, master) == UBSE_OK && !master.empty()) {
            cabinetMasterNodeId = std::move(master);
            return true;
        }
    }
    return false;
}

 /**
 * @brief 发起 0x001A 柜级聚合查询（同步 RPC，超时沿用 com 层默认，设计文档 §4.1.1）
 */
bool QueryCabinetLedger(const std::string& cabinetMasterNodeId, uint32_t selfGroupId, const std::string& selfNodeId,
                        UbseCabinetLedgerQueryResp& response)
{
    UbseCabinetLedgerQueryReq request{};
    request.requesterMasterNodeId = selfNodeId;
    request.requesterGroupId = selfGroupId;
    UbseSerialization out;
    UbseCabinetLedgerQueryReqSerialization(out, request);
    if (!out.Check()) {
        UBSE_LOG_ERROR << "[CLOS_CAB] action=agg_query result=serialize_failed master=" << cabinetMasterNodeId;
        return false;
    }
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "[CLOS_CAB] action=agg_query result=com_module_null";
        return false;
    }
    const SendParam sendParam{cabinetMasterNodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY),
                              static_cast<uint32_t>(UbseMemQueryOpCode::UBSE_MEM_QUERY_CLOS_CABINET_LEDGER)};
    UbseComBaseBufferMessagePtr requestMsg = new (std::nothrow)
        UbseComBaseBufferMessage(out.GetBuffer(false), static_cast<uint32_t>(out.GetLength()));
    UbseComBaseBufferMessagePtr responseMsg = new (std::nothrow) UbseComBaseBufferMessage();
    if (requestMsg == nullptr || responseMsg == nullptr) {
        UBSE_LOG_ERROR << "[CLOS_CAB] action=agg_query result=alloc_failed";
        return false;
    }
    responseMsg->SetIsNeedFreeData(true);
    const auto ret = comModule->RpcSend(sendParam, requestMsg, responseMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[CLOS_CAB] action=agg_query master=" << cabinetMasterNodeId << " result=send_failed, "
                       << FormatRetCode(ret);
        return false;
    }
    if (responseMsg->GetData() == nullptr || responseMsg->GetDataLen() == 0) {
        UBSE_LOG_ERROR << "[CLOS_CAB] action=agg_query master=" << cabinetMasterNodeId << " result=empty_response";
        return false;
    }
    UbseDeSerialization in(responseMsg->GetData(), responseMsg->GetDataLen());
    if (!UbseCabinetLedgerQueryRespDeserialization(in, response)) {
        UBSE_LOG_ERROR << "[CLOS_CAB] action=agg_query master=" << cabinetMasterNodeId << " result=deserialize_failed";
        return false;
    }
    return true;
}

 /**
 * @brief 逐条目三态分类：仅 SUCCESS 写入 allDebtInfoMap（设计文档 §8.2 跳过≠空账本）
 */
struct CabinetItemStats {
    std::set<std::string> returnedNodeIds;
    uint32_t success{0};
    uint32_t skipped{0};
    uint32_t fault{0};
};

CabinetItemStats ClassifyCabinetItems(UbseCabinetLedgerQueryResp& resp,
                                      std::unordered_map<std::string, NodeMemDebtInfo>& allDebtInfoMap,
                                      std::set<std::string>& deferredPeers)
{
    CabinetItemStats stats;
    for (auto& item : resp.items) {
        if (item.nodeId.empty()) {
            continue;
        }
        stats.returnedNodeIds.insert(item.nodeId);
        if (item.status == UbseCollectStatus::SUCCESS) {
            allDebtInfoMap[item.nodeId] = std::move(item.debtInfo);
            ++stats.success;
        } else if (item.status == UbseCollectStatus::CONFIRMED_FAULT) {
            ++stats.fault;
        } else {
            deferredPeers.insert(item.nodeId);
            ++stats.skipped;
        }
    }
    return stats;
}

 /**
 * @brief 响应覆盖度校验：用 A 本地全网成员视图对比 items 清单，缺失节点按 SKIPPED 遮蔽
 *        防御 B 柜主刚倒换、镜像未消费完时不认识某节点导致该节点从 items 整体缺失（设计文档 §5.1）
 */
uint32_t VerifyItemsCoverage(uint32_t groupId, const std::set<std::string>& returnedNodeIds,
                             std::set<std::string>& deferredPeers)
{
    const auto members = GetCabinetMemberNodeIds(groupId);
    if (members.empty()) {
        UBSE_LOG_WARN << "[CLOS_CAB] action=verify_coverage cabinet=" << groupId << " result=topology_unknown";
        return 0;
    }
    uint32_t missing = 0;
    for (const auto& memberId : members) {
        if (returnedNodeIds.count(memberId) != 0) {
            continue;
        }
        ++missing;
        deferredPeers.insert(memberId);
        UBSE_LOG_WARN << "[CLOS_CAB] action=verify_coverage cabinet=" << groupId << " missingNode=" << memberId;
    }
    return missing;
}

 /**
 * @brief 将 SUCCESS 条目写入 allDebtInfoMap，下游三个 BorrowLedger 无感知复用（设计文档 §5.1）
 */
bool AssembleAllDebtInfoMap(std::unordered_map<std::string, NodeMemDebtInfo>& allDebtInfoMap,
                            UbseCabinetLedgerQueryResp& resp, uint32_t groupId, const std::set<std::string>& peers,
                            std::set<std::string>& deferredPeers)
{
    const auto stats = ClassifyCabinetItems(resp, allDebtInfoMap, deferredPeers);
    for (const auto& peer : peers) {
        if (stats.returnedNodeIds.count(peer) == 0) {
            deferredPeers.insert(peer);
            UBSE_LOG_WARN << "[CLOS_CAB] action=verify_coverage cabinet=" << groupId << " missingPeer=" << peer;
        }
    }
    const auto missing = VerifyItemsCoverage(groupId, stats.returnedNodeIds, deferredPeers);
    UBSE_LOG_INFO << "[CLOS_CAB] action=assemble cabinet=" << groupId << " items=" << resp.items.size()
                  << " success=" << stats.success << " skipped=" << stats.skipped << " fault=" << stats.fault
                  << " missing=" << missing;
    return stats.skipped == 0 && missing == 0;
}

bool CollectOneCabinet(uint32_t groupId, const std::set<std::string>& peers, uint32_t selfGroupId,
                       const std::string& selfNodeId, std::unordered_map<std::string, NodeMemDebtInfo>& allDebtInfoMap,
                       std::set<std::string>& deferredPeers)
{
    auto deferAllPeers = [&peers, &deferredPeers]() {
        deferredPeers.insert(peers.begin(), peers.end());
    };
    std::string expectedMaster;
    if (!LocateCabinetMaster(peers, expectedMaster)) {
        UBSE_LOG_WARN << "[CLOS_CAB] action=locate_master cabinet=" << groupId << " result=master_not_found";
        deferAllPeers();
        return false;
    }
    UbseCabinetLedgerQueryResp resp{};
    if (!QueryCabinetLedger(expectedMaster, selfGroupId, selfNodeId, resp)) {
        deferAllPeers();
        return false;
    }
    if (resp.ret == UBSE_MEMCONTROLLER_ERROR_CABINET_BUSY) {
        UBSE_LOG_WARN << "[CLOS_CAB] action=agg_query cabinet=" << groupId << " reason=busy result=skipped";
        deferAllPeers();
        return false;
    }
    if (resp.ret != UBSE_OK) {
        UBSE_LOG_WARN << "[CLOS_CAB] action=agg_query cabinet=" << groupId << " result=failed, "
                      << FormatRetCode(resp.ret);
        deferAllPeers();
        return false;
    }
    if (resp.cabinetMasterNodeId != expectedMaster) {
        UBSE_LOG_WARN << "[CLOS_CAB] action=agg_query cabinet=" << groupId << " expected=" << expectedMaster
                      << " actual=" << resp.cabinetMasterNodeId << " result=identity_mismatch";
        deferAllPeers();
        return false;
    }
    return AssembleAllDebtInfoMap(allDebtInfoMap, resp, groupId, peers, deferredPeers);
}
} // namespace

CrossCabinetRoundPermit::~CrossCabinetRoundPermit()
{
    Release();
}

bool CrossCabinetRoundPermit::Acquire(uint32_t waitMs)
{
    if (held_) {
        return true;
    }
    std::unique_lock<std::mutex> lock(g_crossCabinetGateMutex);
    const bool acquired = g_crossCabinetGateCv.wait_for(
        lock, std::chrono::milliseconds(waitMs), [] { return g_crossCabinetInFlight < kCrossCabinetConcurrency; });
    if (!acquired) {
        return false;
    }
    ++g_crossCabinetInFlight;
    held_ = true;
    return true;
}

void CrossCabinetRoundPermit::Release()
{
    if (!held_) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(g_crossCabinetGateMutex);
        if (g_crossCabinetInFlight > 0) {
            --g_crossCabinetInFlight;
        }
    }
    held_ = false;
    g_crossCabinetGateCv.notify_one();
}

void CollectCrossCabinetLedgers(std::unordered_map<std::string, NodeMemDebtInfo>& allDebtInfoMap,
                                const NodeMemDebtInfo& masterDebtInfo, const NodeMemDebtInfo& agentDebtInfo,
                                uint32_t selfGroupId, const std::string& selfNodeId, CrossCabinetRoundCtx& round,
                                std::set<std::string>& deferredPeers)
{
    const auto groups = ExtractCrossCabinetPeerGroups(masterDebtInfo, agentDebtInfo, selfGroupId, deferredPeers);
    if (groups.empty()) {
        // 无外柜对端（含 T 两份账本均空的冷启动场景）：本轮不发起任何外柜查询，仅做柜内/SHM 对账
        return;
    }
    const auto remainingMs = RemainingRoundMs(round.deadline);
    if (!round.permit.Acquire(static_cast<uint32_t>(remainingMs))) {
        UBSE_LOG_WARN << "[CLOS_CAB] action=gate requester=" << selfNodeId << " cabinets=" << groups.size()
                      << " result=timeout";
        for (const auto& item : groups) {
            deferredPeers.insert(item.second.begin(), item.second.end());
        }
        return;
    }
    uint32_t successCabinet = 0;
    uint32_t deferredCabinet = 0;
    const auto roundStart = std::chrono::steady_clock::now();
    for (const auto& [groupId, peers] : groups) {
        if (RemainingRoundMs(round.deadline) == 0) {
            UBSE_LOG_WARN << "[CLOS_CAB] action=collect cabinet=" << groupId << " result=round_timeout";
            deferredPeers.insert(peers.begin(), peers.end());
            ++deferredCabinet;
            continue;
        }
        if (CollectOneCabinet(groupId, peers, selfGroupId, selfNodeId, allDebtInfoMap, deferredPeers)) {
            ++successCabinet;
        } else {
            ++deferredCabinet;
        }
    }
    const auto costMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - roundStart);
    UBSE_LOG_INFO << "[CLOS_CAB] action=collect_round requester=" << selfNodeId << " cabinets=" << groups.size()
                  << " success=" << successCabinet << " deferred=" << deferredCabinet
                  << " costMs=" << costMs.count();
}

void ShadowFilterDeferredRelations(NodeMemDebtInfo& masterDebtInfo, NodeMemDebtInfo& agentDebtInfo,
                                   const std::set<std::string>& deferredPeers)
{
    if (deferredPeers.empty()) {
        return;
    }
    masterDebtInfo = FilterDebtInfoByPeerNodeIds(masterDebtInfo, deferredPeers, PeerFilterMode::REMOVE_IN_SET);
    agentDebtInfo = FilterDebtInfoByPeerNodeIds(agentDebtInfo, deferredPeers, PeerFilterMode::REMOVE_IN_SET);
    UBSE_LOG_INFO << "[CLOS_CAB] action=shadow_filter deferredPeers=" << deferredPeers.size();
}
} // namespace ubse::mem::controller
