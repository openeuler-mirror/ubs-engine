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

#include "ubse_election_role_global_standby.h"
#include "ubse_context.h"
#include "ubse_election_role_mgr.h"
#include "ubse_logger_audit.h"
#include "ubse_timer.h"

namespace ubse::election {
const std::string UBSE_ELECTION_GLOBAL_STANDBY_PROCTIMER = "UbseGlobalStandbyProcTimer";
const std::string UBSE_ELECTION_STANDBY_DISCOVERY_TIMER = "UbseStandbyDiscoveryTimer";
const std::string UBSE_ELECTION_GLOBAL_STANDBY_COM = "UbseGlobalStandbyComTimer";
const std::string UBSE_ELECTION_GLOBAL_STANDBY_QUERY_LOCAL_MASTER = "UbseGlobalStandbyQueryLocalMasterTimer";
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::timer;
using namespace ubse::context;
GlobalStandby::GlobalStandby(RoleContext &ctx) : globalTurnId_(0), lastHeartTime_()
{
    Node myself;
    if (UBSE_ERROR == UbseElectionNodeMgr::GetInstance().GetMyselfNode(myself)) {
        UBSE_LOG_ERROR << "[ELECTION] Master GetMyselfNode: no node found.";
        return;
    }

    globalStandbyId_ = myself.id;
    globalMasterId_ = ctx.masterId;
    globalTurnId_ = ctx.turnId;
    auto ret = UbseElectionNodeMgr::GetInstance().GetGroupId(groupId_);
    if (ret != UBSE_OK || groupId_.empty()) {
        UBSE_LOG_WARN << "[ELECTION] GetGroupId fail";
    }
    auto result = GetBootTime(lastHeartTime_);
    if (result != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] GetBootTime fail";
    }
    ret = GetBootTime(lastCascadeReportTime_);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] GetBootTime lastCascadeReportTime_ fail";
    }
    RegisterTimers();
    UBSE_LOG_INFO << "[ELECTION] Global Standby start ProcTimer: " << globalStandbyId_ << ".";
}

GlobalStandby::~GlobalStandby()
{
    UbseTimerHandlerUnregister(UBSE_ELECTION_GLOBAL_STANDBY_PROCTIMER);
    UbseTimerHandlerUnregister(UBSE_ELECTION_STANDBY_DISCOVERY_TIMER);
    UbseTimerHandlerUnregister(UBSE_ELECTION_GLOBAL_STANDBY_COM);
    UbseTimerHandlerUnregister(UBSE_ELECTION_GLOBAL_STANDBY_QUERY_LOCAL_MASTER);
}

void GlobalStandby::CleanupRoutes()
{
    DeleteDownstreamGroupRoute();
}
void GlobalStandby::RegisterTimers()
{
    UbseTimerHandlerRegister(
        UBSE_ELECTION_GLOBAL_STANDBY_PROCTIMER,
        []() -> UbseResult {
            if (g_globalStop.load()) { return UBSE_OK; }
            RoleMgr::GetInstance().GlobalProcTimer();
            return UBSE_OK;
        }, UBSE_GLOBAL_PROC_INTERVAL);
    UbseTimerHandlerRegister(
        UBSE_ELECTION_STANDBY_DISCOVERY_TIMER,
        []() -> UbseResult {
            if (g_globalStop.load()) { return UBSE_OK; }
            auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
            if (globalRole != nullptr) { RoleMgr::GetInstance().ConnectInterManagingGroup(); }
            return UBSE_OK;
        }, UBSE_GLOBAL_QUERY_COM_INTERVAL);
    UbseTimerHandlerRegister(
        UBSE_ELECTION_GLOBAL_STANDBY_COM,
        []() -> UbseResult {
            if (g_globalStop.load()) { return UBSE_OK; }
            UBSE_ID_TYPE groupId;
            auto ret = UbseElectionNodeMgr::GetInstance().GetGroupId(groupId);
            if (ret != UBSE_OK) { return UBSE_ERROR; }
            auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
            if (globalRole != nullptr && globalRole->GetGlobalRoleType() != GlobalRoleType::GLOBAL_AGENT &&
                RoleMgr::GetInstance().IsManagingGroup(groupId)) {
                ConnectManagingMasters();
            }
            return UBSE_OK;
        }, UBSE_GLOBAL_COM_INTERVAL);
    UbseTimerHandlerRegister(
        UBSE_ELECTION_GLOBAL_STANDBY_QUERY_LOCAL_MASTER,
        []() -> UbseResult {
            if (g_globalStop.load()) { return UBSE_OK; }
            auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
            if (globalRole != nullptr) { RoleMgr::GetInstance().QueryManagingMaster(); }
            return UBSE_OK;
        }, UBSE_GLOBAL_QUERY_LOCAL_MASTER_INTERVAL);
}

void GlobalStandby::ProcTimer()
{
    // 备节点丢失心跳次数阈值
    uint32_t standbyLostHbSwitchThreshold = ElectionRole::GetHbLostTimes();
    if (IsStandbyHeartBeatTimeout(standbyLostHbSwitchThreshold) && GetElectionCandidate()) {
        UBSE_LOG_INFO << "[ELECTION] Standby ProcTimer: switch Master";
        UBSE_AUDIT_RUNTIME_ALLOC << "Current node switched from standby to master, node ID: " << globalStandbyId_;
        RoleContext ctx;
        ctx.masterId = globalStandbyId_;
        ctx.standbyId = INVALID_NODE_ID;
        ctx.turnId = globalTurnId_ + 1;
        UbseContext::GetInstance().SetWorkReadiness(NOT_READY);
        RoleMgr::GetInstance().SwitchGlobalRole(GlobalRoleType::GLOBAL_MASTER, ctx);
    }
    DetectCascadeGroupTimeout();
}

void GlobalStandby::DetectCascadeGroupTimeout()
{
    if (cascadeGroupReport_.groupMasterId.empty()) {
        return;
    }
    uint64_t bootTime;
    if (GetBootTime(bootTime) != UBSE_OK) {
        return;
    }
    uint32_t timeoutThreshold = UBSE_GLOBAL_QUERY_LOCAL_MASTER_INTERVAL * NO_10 * NO_1000; // 10s
    if (bootTime - lastCascadeReportTime_ > timeoutThreshold) {
        UBSE_LOG_ERROR << "[ELECTION] Cascade group report timeout, masterId="
                       << cascadeGroupReport_.groupMasterId;
        if (!g_globalStop.load()) {
            RoleMgr::GetInstance().RoleChangeNotifyAsync(
                UbseElectionEventType::GLOBAL_CASCADE_NODE_DOWN, cascadeGroupReport_.groupMasterId);
        }
        DeleteDownstreamGroupRoute();
        cascadeGroupReport_ = {};
    }
}


uint32_t GlobalStandby::RecvPkt(UBSE_ID_TYPE srcID, const ElectionPkt rcvPkt, ElectionReplyPkt &reply)
{
    if (rcvPkt.type == ELECTION_PKT_TYPE_GLOBAL_SELECT) {
        // 备节点拒绝所有选主报文，不管主如何，备优先会成为主
        reply.replyId = globalStandbyId_;
        reply.masterId = globalMasterId_;
        reply.replyResult = ELECTION_PKT_TYPE_REJECT_HAS_MASTER;
    } else if (rcvPkt.type == ELECTION_PKT_TYPE_GLOBAL_HEART) {
        RecvPktForHeart(rcvPkt, reply);
    }
    return 0;
}

void GlobalStandby::RecvPktForHeart(const ElectionPkt &rcvPkt, ElectionReplyPkt &reply)
{
    if (rcvPkt.masterId == globalMasterId_) {
        if (rcvPkt.standbyId == globalStandbyId_) {
            globalTurnId_ = rcvPkt.turnId;
            auto ret = GetBootTime(lastHeartTime_);
            if (ret != UBSE_OK) {
                UBSE_LOG_WARN << "[ELECTION] GetBootTime fail";
            }
            globalMasterStatus_ = rcvPkt.masterStatus;
            auto currentStatus = UbseContext::GetInstance().GetWorkReadiness();
            reply.standbyStatus = currentStatus;
            reply.replyId = globalStandbyId_;
            reply.replyResult = ELECTION_PKT_RESULT_ACCEPT;
            globalAgentIds_ = rcvPkt.agentIds;
            reply.groupId = groupId_;
            auto groupRole = RoleMgr::GetInstance().GetRole();
            if (groupRole != nullptr) {
                std::vector<UBSE_ID_TYPE> agentNodes = groupRole->GetAgentNodes();
                reply.standbyId = groupRole->GetStandbyNode();
                reply.masterId = groupRole->GetMasterNode();
                if (reply.standbyId != INVALID_NODE_ID) {
                    reply.managingGroupNodeIds.push_back(reply.standbyId);
                }
                if (reply.masterId != INVALID_NODE_ID) {
                    reply.managingGroupNodeIds.push_back(reply.masterId);
                }
                for (auto &node : agentNodes) {
                    if (node != reply.masterId && node != reply.standbyId) {
                        reply.managingGroupNodeIds.push_back(node);
                    }
                }
            }
            reply.cascadeGroupId = cascadeGroupReport_.groupId;
            reply.cascadeMasterId = cascadeGroupReport_.groupMasterId;
            reply.cascadeStandbyId = cascadeGroupReport_.groupStandbyId;
            reply.cascadeGroupNodeIds = cascadeGroupReport_.groupNodeIds;
            HandleGlobalMasterOnlineNotification(rcvPkt, reply);
        } else {
            RoleContext ctx;
            ctx.masterId = rcvPkt.masterId;
            ctx.standbyId = rcvPkt.standbyId;
            ctx.turnId = rcvPkt.turnId;
            RoleMgr::GetInstance().SwitchGlobalRole(GlobalRoleType::GLOBAL_AGENT, ctx);
            reply.replyResult = ELECTION_PKT_RESULT_ACCEPT;
        }
    } else {
        // 在节点还未触发转换角色前，丢失一定次数收到另一个主节点的心跳，接收该节点为主。
        uint32_t acceptMasterAfterLossThreshold = (ElectionRole::GetHbLostTimes() - NO_1);
        // 如果主变了，脑裂合并
        if (IsStandbyHeartBeatTimeout(acceptMasterAfterLossThreshold)) {
            reply.replyResult = ELECTION_PKT_RESULT_ACCEPT;
            RoleContext ctx;
            ctx.masterId = rcvPkt.masterId;
            ctx.standbyId = rcvPkt.standbyId;
            ctx.turnId = rcvPkt.turnId;
            RoleMgr::GetInstance().SwitchGlobalRole(GlobalRoleType::GLOBAL_AGENT, ctx);
        } else {
            reply.replyId = globalStandbyId_;
            reply.replyResult = ELECTION_PKT_TYPE_REJECT_HAS_MASTER;
        }
    }
}

UBSE_ID_TYPE GlobalStandby::GetGlobalMasterNode()
{
    return globalMasterId_;
}

UBSE_ID_TYPE GlobalStandby::GetGlobalStandbyNode()
{
    return globalStandbyId_;
}

std::vector<UBSE_ID_TYPE> GlobalStandby::GetAgentNodes()
{
    return globalAgentIds_;
}

uint8_t GlobalStandby::GetMasterStatus()
{
    return globalMasterStatus_;
}

uint8_t GlobalStandby::GetStandbyStatus()
{
    auto currentStatus = UbseContext::GetInstance().GetWorkReadiness();
    return currentStatus;
}

bool GlobalStandby::IsStandbyHeartBeatTimeout(uint32_t heartbeatMultiplier) const
{
    uint64_t bootTime;
    auto ret = GetBootTime(bootTime);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] GetBootTime fail";
        return false;
    }
    if (bootTime < lastHeartTime_) {
        UBSE_LOG_WARN << "[ELECTION] Current boot time is earlier than last heart time.";
        return false;
    }
    // 计算从上次收到心跳信号到现在的时间差（单位：毫秒）
    uint64_t timeSinceLastHeartbeat = bootTime - lastHeartTime_;
    // 计算允许的最大心跳间隔时间 = 心跳丢失的倍数 * 心跳间隔时间（单位：毫秒）
    uint32_t maxAllowedHeartbeatInterval = heartbeatMultiplier * GetHeartTimeInterval();
    // 判断时间差是否超过了允许的最大间隔时间
    return timeSinceLastHeartbeat > maxAllowedHeartbeatInterval;
}

void GlobalStandby::RecvInterGroupInfo(const InterGroupInfo &rcvInfo, InterGroupInfo &replyInfo)
{
    if (rcvInfo.type == ELECTION_GROUP_INFO_TYPE_GLOBAL_CASCADE_REPORT) {
        GetBootTime(lastCascadeReportTime_);
        UBSE_ID_TYPE previousCascadeMasterId = cascadeGroupReport_.groupMasterId;
        cascadeGroupReport_ = rcvInfo;
        UBSE_ID_TYPE currentCascadeMasterId = cascadeGroupReport_.groupMasterId;

        if (previousCascadeMasterId != currentCascadeMasterId) {
            if (!previousCascadeMasterId.empty()) {
                UBSE_LOG_INFO << "[ELECTION] Cascade group master offline: previousCascadeMasterId="
                              << previousCascadeMasterId
                              << ", currentCascadeMasterId=" << currentCascadeMasterId;
                if (!g_globalStop.load()) {
                    RoleMgr::GetInstance().RoleChangeNotifyAsync(
                        UbseElectionEventType::GLOBAL_CASCADE_NODE_DOWN, previousCascadeMasterId);
                }
            }
            DeleteDownstreamGroupRoute();
            AddDownstreamGroupRoute(rcvInfo);
        }
        // 回复全局主备
        replyInfo.nodeId = globalStandbyId_;
        replyInfo.globalMasterId = globalMasterId_;
        replyInfo.globalStandbyId = globalStandbyId_;
        replyInfo.groupId = groupId_;
        auto groupRole = RoleMgr::GetInstance().GetRole();
        if (groupRole != nullptr) {
            std::vector<UBSE_ID_TYPE> agentNodes = groupRole->GetAgentNodes();
            replyInfo.groupStandbyId = groupRole->GetStandbyNode();
            replyInfo.groupMasterId = groupRole->GetMasterNode();
            if (replyInfo.groupMasterId != INVALID_NODE_ID) {
                replyInfo.groupNodeIds.push_back(replyInfo.groupMasterId);
            }
            if (replyInfo.groupStandbyId != INVALID_NODE_ID) {
                replyInfo.groupNodeIds.push_back(replyInfo.groupStandbyId);
            }
            for (auto &node : agentNodes) {
                if (node != replyInfo.groupMasterId && node != replyInfo.groupStandbyId) {
                    replyInfo.groupNodeIds.push_back(node);
                }
            }
        }


    }
}

InterGroupInfo GlobalStandby::GetCascadeGroupReport()
{
    return cascadeGroupReport_;
}

void GlobalStandby::AddDownstreamGroupRoute(const InterGroupInfo &cascadeInfo)
{
    if (cascadeInfo.nodeId.empty() || cascadeInfo.nodeId != cascadeInfo.groupMasterId) {
        return;
    }
    uint32_t capability = UbseElectionNodeMgr::GetInstance().GetCapability();
    RouteEntry entry;
    entry.dstNodeId = cascadeInfo.groupMasterId;
    entry.capacity = capability;
    entry.priority = 64;
    entry.nextHopNodeId = cascadeInfo.groupMasterId;
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] AddDownstreamGroupRoute: Getting ComModule failed.";
        return;
    }
    if (comModule->AddRoute(entry) == UBSE_OK) {
        downstreamRouteEntry_ = entry;
        UBSE_LOG_INFO << "[ELECTION] AddDownstreamGroupRoute: dstNodeId=" << entry.dstNodeId
                      << ", capacity=" << entry.capacity
                      << ", nextHopNodeId=" << entry.nextHopNodeId;
    } else {
        UBSE_LOG_WARN << "[ELECTION] AddDownstreamGroupRoute: AddRoute fail.";
    }
}

void GlobalStandby::DeleteDownstreamGroupRoute()
{
    if (downstreamRouteEntry_.dstNodeId.empty()) {
        return;
    }
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] DeleteDownstreamGroupRoute: Getting ComModule failed.";
        return;
    }
    UBSE_LOG_INFO << "[ELECTION] DeleteDownstreamGroupRoute: dstNodeId=" << downstreamRouteEntry_.dstNodeId;
    comModule->DelRoute(downstreamRouteEntry_.dstNodeId);
    downstreamRouteEntry_ = {};
}
} // namespace ubse::election