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

#include "ubse_election_role_global_agent.h"
#include "ubse_election_role_mgr.h"
#include "ubse_timer.h"
namespace ubse::election {
const std::string UBSE_ELECTION_GLOBAL_AGENT_PROCTIMER = "UbseGlobalAgentProcTimer";
const std::string UBSE_ELECTION_GLOBAL_AGENT_COM = "UbseGlobalAgentComTimer";
const std::string UBSE_ELECTION_GLOBAL_AGENT_QUERY_LOCAL_MASTER = "UbseGlobalAgentQueryLocalMasterTimer";
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::nodeController;
using namespace ubse::context;
using namespace ubse::timer;
GlobalAgent::GlobalAgent(RoleContext &ctx) : globalTurnId_(0), lastHeartTime_()
{
    Node myself;
    UbseElectionNodeMgr::GetInstance().GetMyselfNode(myself);

    myselfID_ = myself.id;
    globalMasterId_ = ctx.masterId;
    globalStandbyId_ = ctx.standbyId;
    auto ret = UbseElectionNodeMgr::GetInstance().GetGroupId(groupId_);
    if (ret != UBSE_OK || groupId_.empty()) {
        UBSE_LOG_WARN << "[ELECTION] GetGroupId fail";
    }
    ret = GetBootTime(lastHeartTime_);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] GetBootTime fail";
    }
    UbseTimerHandlerRegister(
        UBSE_ELECTION_GLOBAL_AGENT_PROCTIMER,
        []() -> UbseResult {
            if (g_globalStop.load()) { return UBSE_OK; }
            RoleMgr::GetInstance().GlobalProcTimer();
            return UBSE_OK;
        }, UBSE_GLOBAL_PROC_INTERVAL);
    UbseTimerHandlerRegister(
        UBSE_ELECTION_GLOBAL_AGENT_COM,
        []() -> UbseResult {
            if (g_globalStop.load()) { return UBSE_OK; }
            UBSE_ID_TYPE groupId;
            auto ret = UbseElectionNodeMgr::GetInstance().GetGroupId(groupId);
            if (ret != UBSE_OK) {
                return UBSE_ERROR;
            }
            auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
            if (globalRole != nullptr && globalRole->GetGlobalRoleType() != GlobalRoleType::GLOBAL_AGENT
                && RoleMgr::GetInstance().IsManagingGroup(groupId)) {
                ConnectManagingMasters();
            }
            return UBSE_OK;
        }, UBSE_GLOBAL_COM_INTERVAL);
    UbseTimerHandlerRegister(
        UBSE_ELECTION_GLOBAL_AGENT_QUERY_LOCAL_MASTER,
        []() -> UbseResult {
            if (g_globalStop.load()) { return UBSE_OK; }
            auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
            if (globalRole != nullptr) {
                RoleMgr::GetInstance().QueryManagingMaster();
            }
            return UBSE_OK;
        }, UBSE_GLOBAL_QUERY_LOCAL_MASTER_INTERVAL);
    UBSE_LOG_INFO << "[ELECTION] Global Agent start ProcTimer: " << myselfID_ << ".";
}

GlobalAgent::~GlobalAgent()
{
    UbseTimerHandlerUnregister(UBSE_ELECTION_GLOBAL_AGENT_PROCTIMER);
    UbseTimerHandlerUnregister(UBSE_ELECTION_GLOBAL_AGENT_COM);
    UbseTimerHandlerUnregister(UBSE_ELECTION_GLOBAL_AGENT_QUERY_LOCAL_MASTER);
}

void GlobalAgent::ProcTimer()
{
    // 从节点丢失主的心跳次数阈值，约定从节点丢失心跳次数阈值比备节点的多2次。
    uint32_t agentLostHbSwitchThreshold = (ElectionRole::GetHbLostTimes() + NO_2);
    if (IsAgentHeartBeatTimeout(agentLostHbSwitchThreshold)) {
        std::vector<UBSE_ID_TYPE> allNodes = RoleMgr::GetInstance().GetCommMgr() -> GetConnectedMasterNodes();
        if (GetElectionCandidate() && ElectWhenLowest(myselfID_, allNodes) == ELECTION_PKT_RESULT_ACCEPT) {
            RoleContext ctx;
            ctx.masterId = myselfID_;
            ctx.standbyId = INVALID_NODE_ID;
            ctx.turnId = globalTurnId_;
            UBSE_LOG_INFO << "[ELECTION] Global Agent ProcTimer: switch global Master";
            RoleMgr::GetInstance().SwitchGlobalRole(GlobalRoleType::GLOBAL_MASTER, ctx);
        }
    }
}

void GlobalAgent::HandleMasterChange(const ElectionPkt &rcvPkt, ElectionReplyPkt &reply)
{
    // 在节点还未触发转换角色前，丢失一定次数心跳，收到另一个主节点的心跳，则接收该节点为主。
    uint32_t acceptMasterAfterLossThreshold = (ElectionRole::GetHbLostTimes() - NO_1);
    if (IsAgentHeartBeatTimeout(acceptMasterAfterLossThreshold)) {
        globalMasterId_ = rcvPkt.masterId;
        globalStandbyId_ = rcvPkt.standbyId;
        globalTurnId_ = rcvPkt.turnId;
        auto ret = GetBootTime(lastHeartTime_);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "[ELECTION] GetBootTime fail";
        }
        reply.replyId = myselfID_;
        reply.replyResult = ELECTION_PKT_RESULT_ACCEPT;
    } else {
        reply.replyId = myselfID_;
        reply.replyResult = ELECTION_PKT_TYPE_REJECT_HAS_MASTER;
    }
}

uint32_t GlobalAgent::RecvPkt(UBSE_ID_TYPE srcID, const ElectionPkt rcvPkt, ElectionReplyPkt &reply)
{
    if (rcvPkt.type == ELECTION_PKT_TYPE_GLOBAL_SELECT) {
        RecvPktForSelect(reply);
    } else if (rcvPkt.type == ELECTION_PKT_TYPE_GLOBAL_HEART) {
        RecvPktForHeart(rcvPkt, reply);
    } else {
        UBSE_LOG_WARN << "[ELECTION] Global Agent rcvPkt.type: " << rcvPkt.type << ".";
    }

    return 0;
}

void GlobalAgent::RecvPktForHeart(const ElectionPkt &rcvPkt, ElectionReplyPkt &reply)
{
    masterStatus_ = rcvPkt.masterStatus;
    standbyStatus_ = rcvPkt.standbyStatus;
    if (rcvPkt.masterId == globalMasterId_) {
        auto ret = GetBootTime(lastHeartTime_);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "[ELECTION] GetBootTime fail";
        }
        globalMasterId_ = rcvPkt.masterId;
        globalStandbyId_ = rcvPkt.standbyId;
        if (globalTurnId_ < rcvPkt.turnId) {
            globalTurnId_ = rcvPkt.turnId;
        }

        reply.replyId = myselfID_;
        reply.replyResult = ELECTION_PKT_RESULT_ACCEPT;
        globalAgentIds_ = rcvPkt.agentIds;
        reply.broadcast = static_cast<uint8_t>(NotifyStatus::BROADCAST);
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
        if (rcvPkt.standbyId == myselfID_) {
            RoleContext ctx;
            ctx.masterId = rcvPkt.masterId;
            ctx.standbyId = rcvPkt.standbyId;
            ctx.turnId = rcvPkt.turnId;
            RoleMgr::GetInstance().SwitchGlobalRole(GlobalRoleType::GLOBAL_STANDBY, ctx);
        } else {
            DisconnectAgents(rcvPkt);
        }
    } else {
        // 如果主变了，脑裂合并
        HandleMasterChange(rcvPkt, reply);
    }
}

void GlobalAgent::DisconnectAgents(const ElectionPkt &rcvPkt)
{
    if (rcvPkt.standbyId.empty() || rcvPkt.agentCount <= NO_1) {
        return;
    }
    for (const auto &agent : rcvPkt.agentIds) {
        if (agent != myselfID_) {
            RoleMgr::GetInstance().GetCommMgr()->DisConnect(agent);
        }
    }
    std::vector<UbseNodeInfo> ubseNodeInfos = UbseNodeController::GetInstance().GetStaticNodeInfo();
    if (ubseNodeInfos.empty()) {
        UBSE_LOG_ERROR << "[ELECTION] LoadConfig get allNodes failed.";
        return;
    }
    for (const auto &node : ubseNodeInfos) {
        if (node.nodeId != globalMasterId_ && node.nodeId != globalStandbyId_ && node.nodeId != myselfID_
            && std::find(rcvPkt.agentIds.begin(), rcvPkt.agentIds.end(), node.nodeId) == rcvPkt.agentIds.end()) {
            RoleMgr::GetInstance().GetCommMgr()->DisConnect(node.nodeId);
        }
    }
}

void GlobalAgent::RecvPktForSelect(ElectionReplyPkt &reply) const
{
    // 在节点还未触发转换角色前，丢失一定次数心跳，收到另一个节点的选主报文，回复同意。（需等到收到心跳，才会被收编）
    uint32_t acceptMasterAfterLossThreshold = (ElectionRole::GetHbLostTimes() - NO_1);
    if (IsAgentHeartBeatTimeout(acceptMasterAfterLossThreshold)) {
        reply.replyId = myselfID_;
        reply.replyResult = ELECTION_PKT_RESULT_ACCEPT;
    } else {
        reply.replyId = myselfID_;
        reply.masterId = globalMasterId_;
        reply.replyResult = ELECTION_PKT_TYPE_REJECT_HAS_MASTER;
    }
}

UBSE_ID_TYPE GlobalAgent::GetGlobalMasterNode()
{
    return globalMasterId_;
}

UBSE_ID_TYPE GlobalAgent::GetGlobalStandbyNode()
{
    return globalStandbyId_;
}

std::vector<UBSE_ID_TYPE> GlobalAgent::GetAgentNodes()
{
    return globalAgentIds_;
}

uint8_t GlobalAgent::GetMasterStatus()
{
    return masterStatus_;
}

uint8_t GlobalAgent::GetStandbyStatus()
{
    return standbyStatus_;
}

bool GlobalAgent::IsAgentHeartBeatTimeout(uint32_t heartbeatMultiplier) const
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

void GlobalAgent::RecvInterGroupInfo(const InterGroupInfo &rcvInfo, InterGroupInfo &replyInfo)
{
    if (rcvInfo.type == ELECTION_GROUP_INFO_TYPE_GLOBAL_CASCADE_REPORT) {
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
        }

        replyInfo.nodeId = myselfID_;
        replyInfo.groupMasterId = globalMasterId_;
        replyInfo.groupStandbyId = globalStandbyId_;
    }
}

InterGroupInfo GlobalAgent::GetCascadeGroupReport()
{
    return cascadeGroupReport_;
}
} // namespace ubse::election