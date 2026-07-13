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

#include "ubse_election_role_global_initializer.h"
#include "ubse_election_group_info_simpo.h"
#include "ubse_election_role_mgr.h"
#include "ubse_timer.h"

namespace ubse::election {
const std::string UBSE_ELECTION_GLOBAL_INIT_TIMER = "UbseGlobalInitTimer";
const std::string UBSE_ELECTION_GLOBAL_INIT_COM = "UbseGlobalInitComTimer";
const std::string UBSE_ELECTION_GLOBAL_INIT_QUERY_TIMER = "UbseGlobalInitQueryTimer";
const std::string UBSE_ELECTION_GLOBAL_INIT_QUERY_COM = "UbseGlobalInitQueryComTimer";
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::timer;
using namespace ubse::context;
using namespace ubse::nodeController;
GlobalInitializer::GlobalInitializer() : lastTimeMs_(0)
{
    auto ret = GetBootTime(startTimeMs_);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] GetBootTime fail";
    }
    lastTimeMs_ = startTimeMs_;
    Node myself;
    UbseElectionNodeMgr::GetInstance().GetMyselfNode(myself);
    myselfID_ = myself.id;
    ret = UbseElectionNodeMgr::GetInstance().GetGroupId(groupId_);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] GetGroupId fail";
        return;
    }
    RegisterTimers();
    UBSE_LOG_INFO << "[ELECTION] Global Initializer: " << myselfID_ << ".";
}

void GlobalInitializer::RegisterTimers()
{
    UbseTimerHandlerRegister(
        UBSE_ELECTION_GLOBAL_INIT_TIMER,
        []() -> UbseResult {
            if (g_globalStop.load()) { return UBSE_OK; }
            RoleMgr::GetInstance().GlobalProcTimer();
            return UBSE_OK;
        }, UBSE_GLOBAL_PROC_INTERVAL);

    UbseTimerHandlerRegister(
        UBSE_ELECTION_GLOBAL_INIT_COM,
        []() -> UbseResult {
            if (g_globalStop.load()) { return UBSE_OK; }
            UBSE_ID_TYPE groupId;
            auto ret = UbseElectionNodeMgr::GetInstance().GetGroupId(groupId);
            if (ret != UBSE_OK) { return UBSE_ERROR; }
            auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
            if (globalRole != nullptr && globalRole->GetGlobalRoleType() != GlobalRoleType::GLOBAL_AGENT) {
                ConnectManagingMasters();
            }
            return UBSE_OK;
        }, UBSE_GLOBAL_COM_INTERVAL);

    UbseTimerHandlerRegister(
        UBSE_ELECTION_GLOBAL_INIT_QUERY_TIMER,
        [this]() -> UbseResult {
            if (g_globalStop.load()) { return UBSE_OK; }
            auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
            if (globalRole != nullptr) {
                RoleMgr::GetInstance().QueryManagingMaster();
                ReportCascadeGroupToManagingGroup();
            }
            return UBSE_OK;
        }, UBSE_GLOBAL_QUERY_LOCAL_MASTER_INTERVAL);

    UbseTimerHandlerRegister(
        UBSE_ELECTION_GLOBAL_INIT_QUERY_COM,
        []() -> UbseResult {
            if (g_globalStop.load()) { return UBSE_OK; }
            auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
            if (globalRole != nullptr) { RoleMgr::GetInstance().ConnectInterManagingGroup(); }
            return UBSE_OK;
        }, UBSE_GLOBAL_QUERY_COM_INTERVAL);
}

void GlobalInitializer::CheckAndSwitchMaster(const Node &myself, const std::vector<Node> &masterIds, RoleContext ctx)
{
    if ((lastTimeMs_ - startTimeMs_) <= GetHeartTimeInterval() * NO_2) {
        if (IsSmallestNode(myself, masterIds)) {
            // 阶段1内 最小ID的发起选组，没有拒绝就宣布为主
            if (SendGlobalElectionPkt(myselfID_) == ELECTION_PKT_RESULT_ACCEPT) {
                UBSE_LOG_INFO << "[ELECTION] Initializer: ProcTimer switch global master - 1";
                RoleMgr::GetInstance().SwitchGlobalRole(GlobalRoleType::GLOBAL_MASTER, ctx);
            }
        }
    } else if ((lastTimeMs_ - startTimeMs_) > GetHeartTimeInterval() * NO_2 &&
               (lastTimeMs_ - startTimeMs_) <= GetHeartTimeInterval() * NO_4) {
        if (IsSecondSmallestNode(myself, masterIds)) {
            // 阶段2内 最小ID的发起选组，没有拒绝就宣布为主
            if (SendGlobalElectionPkt(myselfID_) == ELECTION_PKT_RESULT_ACCEPT) {
                UBSE_LOG_INFO << "[ELECTION] Initializer: ProcTimer switch global master - 2";
                RoleMgr::GetInstance().SwitchGlobalRole(GlobalRoleType::GLOBAL_MASTER, ctx);
            }
        }
    } else {
        // 和接收互斥
        if (SendGlobalElectionPkt(myselfID_) == ELECTION_PKT_RESULT_ACCEPT) {
            UBSE_LOG_INFO << "[ELECTION] Initializer: ProcTimer switch global master - 3";
            RoleMgr::GetInstance().SwitchGlobalRole(GlobalRoleType::GLOBAL_MASTER, ctx);
        }
    }
}

void GlobalInitializer::ProcRoleSwitch(const std::vector<Node> &masterIds)
{
    RoleContext ctx;
    ctx.masterId = myselfID_;
    ctx.turnId = 0;
    auto ret = GetBootTime(lastTimeMs_);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] GetBootTime fail";
    }
    if (GetElectionCandidate() && GetElectionWait()) {
        CheckAndSwitchMaster({myselfID_}, masterIds, ctx);
    } else if (GetElectionCandidate() && !GetElectionWait()) {
        if (SendGlobalElectionPkt(myselfID_) == ELECTION_PKT_RESULT_ACCEPT) {
            UBSE_LOG_INFO << "[ELECTION] Initializer: ProcTimer switch master - 4";
            RoleMgr::GetInstance().SwitchGlobalRole(GlobalRoleType::GLOBAL_MASTER, ctx);
        }
    }
}

void GlobalInitializer::ProcTimer()
{
    // 挂载组不参与全局选主
    if (!RoleMgr::GetInstance().IsManagingGroup(groupId_)) {
        UBSE_LOG_INFO << "[ELECTION] The group do not participate in global election. The groupId is " << groupId_;
        return;
    }

    std::vector<UBSE_ID_TYPE> pdMasterIds = RoleMgr::GetInstance().GetManagingGroupMasterIds();
    UbseNodeLocalState localState = UbseElectionNodeMgr::GetInstance().GetLocalNodeState();
    if (localState == UbseNodeLocalState::UBSE_NODE_READY) {
        std::vector<Node> masterIds{};
        for (const auto& item : pdMasterIds) {
            masterIds.push_back({item});
            UBSE_LOG_INFO << "[ELECTION] query Managing Group masterId = " << item;
        }
        masterIds.push_back({myselfID_});
        if (!isStartTimeSet_) {
            auto ret = GetBootTime(startTimeMs_);
            if (ret != UBSE_OK) {
                UBSE_LOG_WARN << "[ELECTION] GetBootTime fail";
            }
            isStartTimeSet_ = true;
        }
        ProcRoleSwitch(masterIds);
    }
}

uint32_t GlobalInitializer::RecvPkt(UBSE_ID_TYPE srcID, const ElectionPkt rcvPkt, ElectionReplyPkt &reply)
{
    UbseNodeLocalState localState = UbseElectionNodeMgr::GetInstance().GetLocalNodeState();
    if (localState == UbseNodeLocalState::UBSE_NODE_RESTORE) {
        if (rcvPkt.type == ELECTION_PKT_TYPE_GLOBAL_SELECT) {
            reply.replyResult = ELECTION_PKT_RESULT_ACCEPT;
        } else if (rcvPkt.type == ELECTION_PKT_TYPE_GLOBAL_HEART) {
            reply.replyResult = ELECTION_PKT_TYPE_REJECT;
        }
        reply.replyId = myselfID_;
    } else if (localState == UbseNodeLocalState::UBSE_NODE_READY) {
        if (rcvPkt.type == ELECTION_PKT_TYPE_GLOBAL_SELECT && GetElectionCandidate()) {
            if (myselfID_ < rcvPkt.masterId) {
                reply.replyResult = ELECTION_PKT_TYPE_REJECT;
            } else {
                reply.replyResult = ELECTION_PKT_RESULT_ACCEPT;
            }
            reply.replyId = myselfID_;
        } else if (rcvPkt.type == ELECTION_PKT_TYPE_GLOBAL_SELECT && !GetElectionCandidate()) {
            reply.replyResult = ELECTION_PKT_RESULT_ACCEPT;
            reply.replyId = myselfID_;
        } else if (rcvPkt.type == ELECTION_PKT_TYPE_GLOBAL_HEART) {
            globalTurnId_ = rcvPkt.turnId;
            RoleContext ctx;

            ctx.masterId = rcvPkt.masterId;
            ctx.standbyId = rcvPkt.standbyId;
            ctx.turnId = globalTurnId_;

            if (rcvPkt.standbyId == myselfID_) {
                RoleMgr::GetInstance().SwitchGlobalRole(GlobalRoleType::GLOBAL_STANDBY, ctx);
            } else {
                RoleMgr::GetInstance().SwitchGlobalRole(GlobalRoleType::GLOBAL_AGENT, ctx);
            }
            reply.replyId = myselfID_;
            reply.replyResult = 0;
        } else {
            UBSE_LOG_WARN << "[ELECTION] Global Initializer rcvPkt.type: " << rcvPkt.type << ".";
        }
    }
    return UBSE_OK;
}

std::vector<UBSE_ID_TYPE> GlobalInitializer::GetAgentNodes()
{
    return {};
}

uint8_t GlobalInitializer::GetMasterStatus()
{
    return masterStatus_;
}

uint8_t GlobalInitializer::GetStandbyStatus()
{
    return standbyStatus_;
}

void AddUpstreamRouteToCom(const UBSE_ID_TYPE &nextHopNodeId)
{
    if (nextHopNodeId.empty()) {
        UBSE_LOG_WARN << "[ELECTION] AddUpstreamRouteToCom: nextHopNodeId is empty.";
        return;
    }
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_WARN << "[ELECTION] AddUpstreamRouteToCom: Getting ComModule failed.";
        return;
    }
    RouteEntry entry;
    entry.dstNodeId = "0";
    entry.capacity = UINT32_MAX;
    entry.priority = 127;
    entry.nextHopNodeId = nextHopNodeId;
    if (comModule->AddRoute(entry) != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] AddUpstreamRouteToCom: AddRoute fail.";
    }
}

void DeleteUpstreamRouteToCom()
{
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_WARN << "[ELECTION] DeleteUpstreamRouteToCom: Getting ComModule failed.";
        return;
    }
    comModule->DelRoute("0");
}

void AsyncDealCascadeReply(void* ctx, void* recv, uint32_t len, int32_t result)
{
    auto* context = static_cast<CallbackCascadeCtx*>(ctx);
    if (context == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] Received null context in callback";
        return;
    }
    const auto& nodeId = context->destId;
    auto& globalMasterId = *context->globalMasterId;
    auto& globalStandbyId = *context->globalStandbyId;
    auto& cascadeMtx = *context->mtx;
    auto& managingGroupInfo = *context->managingGroupInfo;

    if (result != UBSE_OK) {
        UBSE_LOG_INFO << "[ELECTION] Failed to query information.";
        return;
    }
    UbseBaseMessagePtr respMsg = new (std::nothrow) message::UbseElectionGroupInfoSimpo();
    if (respMsg == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] Failed to allocate response message.";
        return;
    }
    auto ret = respMsg->SetInputRawData(static_cast<uint8_t*>(recv), len);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] SendQueryInformation failed." << FormatRetCode(ret);
        return;
    }
    ret = respMsg->Deserialize();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] Failed to deserialize response message." << FormatRetCode(ret);
        return;
    }
    auto* replyMsg = dynamic_cast<message::UbseElectionGroupInfoSimpo *>(respMsg.Get());
    if (!replyMsg) {
        UBSE_LOG_ERROR << "[ELECTION] cast to UbseElectionGroupInfoSimpo failed";
        return;
    }
    InterGroupInfo reply = replyMsg->GetInterGroupInfo();
    if (reply.type == ELECTION_PKT_REPLY_GLOBAL_STOP) {
        UBSE_LOG_DEBUG << "[ELECTION] node = " << nodeId <<"stopped.";
        return;
    }
    if (reply.nodeId.empty() || reply.groupMasterId.empty()) {
        UBSE_LOG_WARN << "[ELECTION] Cascade reply fields empty, skip.";
        SafeDelete(context);
        return;
    }
    {
        std::lock_guard<std::mutex> lck(cascadeMtx);
        globalStandbyId = reply.groupStandbyId;
        globalMasterId = reply.groupMasterId;
        managingGroupInfo.groupId = reply.groupId;
        managingGroupInfo.nodeId = reply.nodeId;
        managingGroupInfo.groupMasterId = reply.groupMasterId;
        managingGroupInfo.groupStandbyId = reply.groupStandbyId;
        managingGroupInfo.groupNodeIds = reply.groupNodeIds;
        auto& upstreamNextHopId = *context->upstreamNextHopId;
        if (reply.nodeId == globalMasterId) {
            // 当前节点为global_master时，删除上行路由
            if (!upstreamNextHopId.empty()) {
                DeleteUpstreamRouteToCom();
                upstreamNextHopId.clear();
            }
        } else if (reply.nodeId != upstreamNextHopId) {
            // 当前节点为global_agent时，查询到的master_id与上行路由不同时，删除并添加上行路由
            DeleteUpstreamRouteToCom();
            AddUpstreamRouteToCom(reply.nodeId);
            upstreamNextHopId = reply.nodeId;
        }
    }
    SafeDelete(context);
}

UbseResult GlobalInitializer::SendCascadeInformation(UBSE_ID_TYPE destId, const InterGroupInfo &cascadeRepo)
{
    UbseContext &ubseContext = UbseContext::GetInstance();
    auto rackComModule = ubseContext.GetModule<UbseComModule>();
    if (rackComModule == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] get rackComModule failed";
        return UBSE_ERROR;
    }
    InterGroupInfo electionCascade{ cascadeRepo };
    UbseBaseMessagePtr electionSimpoPtr = new (std::nothrow) message::UbseElectionGroupInfoSimpo(electionCascade);
    if (electionSimpoPtr == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] Newing UbseElectionGroupInfoSimpo failed.";
        return UBSE_ERROR;
    }
    ubse::com::SendParam sendParam(destId, static_cast<uint16_t>(UbseModuleCode::ELECTION),
                                   static_cast<uint16_t>(UbseElectionOpCode::ELECTION_INTER_GROUP_INFO),
                                   UbseChannelType::NORMAL);
    auto context = new (std::nothrow) CallbackCascadeCtx;
    if (context == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] New context failed.";
        return UBSE_ERROR_NULLPTR;
    }

    std::unique_lock<std::mutex> lock(cascadeMtx_);
    context->destId = destId;
    context->globalMasterId = &globalMasterId_;
    context->globalStandbyId = &globalStandbyId_;
    context->upstreamNextHopId = &upstreamNextHopId_;
    context->mtx = &cascadeMtx_;
    context->managingGroupInfo = &managingGroupInfo_;
    ubse::com::UbseComCallback callback;
    callback.cb = AsyncDealCascadeReply;
    callback.cbCtx = reinterpret_cast<void *>(context);
    lock.unlock();
    auto retCode = rackComModule->RpcAsyncSend(sendParam, electionSimpoPtr, callback);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] Send dispatch failed : " << destId;
        SafeDelete(context);
        return retCode;
    }
    return UBSE_OK;
}

void GlobalInitializer::ReportCascadeGroupToManagingGroup()
{
    if (RoleMgr::GetInstance().IsManagingGroup(groupId_)) {
        return;
    }
    InterGroupInfo cascadeGroupReport{};
    cascadeGroupReport.type = ELECTION_GROUP_INFO_TYPE_GLOBAL_CASCADE_REPORT;
    cascadeGroupReport.nodeId = myselfID_; // 上报当前节点ID = groupMasterId
    auto ret = UbseElectionNodeMgr::GetInstance().GetGroupId(cascadeGroupReport.groupId);
    if (ret != UBSE_OK || cascadeGroupReport.groupId.empty()) {
        UBSE_LOG_ERROR << "[ELECTION] GetGroupId fail";
        return;
    }
    auto role = RoleMgr::GetInstance().GetRole();
    cascadeGroupReport.groupMasterId = role->GetMasterNode();
    cascadeGroupReport.groupStandbyId = role->GetStandbyNode();
    cascadeGroupReport.groupNodeIds.push_back(cascadeGroupReport.groupMasterId);
    cascadeGroupReport.groupNodeIds.push_back(cascadeGroupReport.groupStandbyId);
    for (const auto &agentNode : role->GetAgentNodes()) {
        cascadeGroupReport.groupNodeIds.push_back(agentNode);
    }

    std::unordered_map<UBSE_ID_TYPE, UBSE_ID_TYPE> discoveryNodes =
        RoleMgr::GetInstance().GetCommMgr()->GetInterManagementGroupLinkMap();
    for (const auto &item : discoveryNodes) {
        auto sendRet = SendCascadeInformation(item.second, cascadeGroupReport);
        if (sendRet != UBSE_OK) {
            UBSE_LOG_WARN << "[ELECTION] SendCascadeInformation failed." << item.second;
        }
    }
}

UBSE_ID_TYPE GlobalInitializer::GetGlobalMasterNode()
{
    std::lock_guard<std::mutex> lock(cascadeMtx_);
    return globalMasterId_;
}

UBSE_ID_TYPE GlobalInitializer::GetGlobalStandbyNode()
{
    std::lock_guard<std::mutex> lock(cascadeMtx_);
    return globalStandbyId_;
}

GlobalInitializer::~GlobalInitializer()
{
    UbseTimerHandlerUnregister(UBSE_ELECTION_GLOBAL_INIT_TIMER);
    UbseTimerHandlerUnregister(UBSE_ELECTION_GLOBAL_INIT_COM);
    UbseTimerHandlerUnregister(UBSE_ELECTION_GLOBAL_INIT_QUERY_TIMER);
    UbseTimerHandlerUnregister(UBSE_ELECTION_GLOBAL_INIT_QUERY_COM);
}

void GlobalInitializer::CleanupRoutes()
{
    DeleteUpstreamRouteToCom();
}

std::vector<GroupTopology> GlobalInitializer::GetManagingGroupNodeIds()
{
    std::lock_guard<std::mutex> lock(cascadeMtx_);
    GroupTopology managingGroup{
        managingGroupInfo_.groupId,
        true,
        managingGroupInfo_.groupMasterId,
        managingGroupInfo_.groupStandbyId,
        managingGroupInfo_.groupNodeIds
    };
    return { managingGroup };
}
}