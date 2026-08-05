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

#include "ubse_election_role_agent.h"
#include "ubse_election_role_mgr.h"
namespace ubse::election {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::nodeController;
using namespace ::ubse::common::def;
Agent::Agent(RoleContext& ctx) : turnId_(0), lastHeartTime_()
{
    Node myself;
    UbseElectionNodeMgr::GetInstance().GetMyselfNode(myself);

    myselfID_ = myself.id;
    masterId_ = ctx.masterId;
    standbyId_ = ctx.standbyId;
    auto ret = GetBootTime(lastHeartTime_);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] GetBootTime fail";
    }
    UBSE_LOG_INFO << "[ELECTION] Agent start ProcTimer: " << myselfID_ << ".";
}

void Agent::ProcTimer()
{
    // 并发 SwitchRole 可能已把 currentRole_ 换成新对象，陈旧对象不应继续执行选举
    if (!IsCurrentRole()) {
        return;
    }
    // 从节点丢失主的心跳次数阈值，约定从节点丢失心跳次数阈值比备节点的多2次。
    uint32_t agentLostHbSwitchThreshold = (ElectionRole::GetHbLostTimes() + NO_2);
    bool hbTimeout = false;
    {
        std::lock_guard<std::mutex> lock(roleMutex_);
        hbTimeout = IsAgentHeartBeatTimeout(agentLostHbSwitchThreshold);
    }
    if (!hbTimeout) {
        return;
    }
    // 锁外：发起选举（ForceElection 内含阻塞的有界等待发送），持锁期间禁止网络等待
    bool candidate = GetElectionCandidate();
    uint32_t electionResult = ELECTION_PKT_TYPE_REJECT;
    if (candidate) {
        electionResult = ForceElection(myselfID_);
    }
    // 短临界区：校验本对象仍是当前角色（发送期间可能被并发收编），再决策
    std::lock_guard<std::mutex> lock(roleMutex_);
    if (!IsCurrentRole()) {
        // 选举发送期间角色已被并发收编（如收到 HEART 转 STANDBY/AGENT），放弃切换
        UBSE_LOG_INFO << "[ELECTION] Agent ProcTimer: role changed during election, skip switch";
        return;
    }
    if (electionResult == ELECTION_PKT_RESULT_ACCEPT) {
        RoleContext ctx;
        ctx.masterId = myselfID_;
        ctx.standbyId = INVALID_NODE_ID;
        ctx.turnId = turnId_;
        UBSE_LOG_INFO << "[ELECTION] Agent ProcTimer: switch Master";
        RoleMgr::GetInstance().SwitchRole(RoleType::MASTER, ctx);
    } else {
        RoleContext ctx;
        UBSE_LOG_INFO << "[ELECTION] Agent ProcTimer: switch Initializer";
        RoleMgr::GetInstance().SwitchRole(RoleType::INITIALIZER, ctx);
    }
}

void Agent::HandleMasterChange(const ElectionPkt& rcvPkt, ElectionReplyPkt& reply)
{
    // 在节点还未触发转换角色前，丢失一定次数心跳，收到另一个主节点的心跳，则接收该节点为主。
    uint32_t acceptMasterAfterLossThreshold = (ElectionRole::GetHbLostTimes() - NO_1);
    if (IsAgentHeartBeatTimeout(acceptMasterAfterLossThreshold)) {
        masterId_ = rcvPkt.masterId;
        standbyId_ = rcvPkt.standbyId;
        turnId_ = rcvPkt.turnId;
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

uint32_t Agent::RecvPkt(UBSE_ID_TYPE srcID, const ElectionPkt rcvPkt, ElectionReplyPkt& reply)
{
    std::lock_guard<std::mutex> lock(roleMutex_);
    // 陈旧角色对象（已被并发 SwitchRole 替换）不应再处理报文，避免按陈旧状态覆盖新角色
    if (!IsCurrentRole()) {
        UBSE_LOG_INFO << "[ELECTION] Agent: stale role object, skip RecvPkt from nodeId=" << srcID;
        reply.replyResult = ELECTION_PKT_TYPE_REJECT;
        return 0;
    }
    if ((rcvPkt.type == ELECTION_PKT_TYPE_HEART || rcvPkt.type == ELECTION_PKT_TYPE_SELECT) &&
        !IsAllowedMasterNode(rcvPkt.masterId)) {
        UBSE_LOG_DEBUG << "[ELECTION] Reject packet from non-candidate master: " << rcvPkt.masterId;
        reply.replyId = myselfID_;
        reply.masterId = masterId_;
        reply.replyResult = ELECTION_PKT_TYPE_REJECT_HAS_MASTER;
        return 0;
    }
    if (rcvPkt.type == ELECTION_PKT_TYPE_SELECT) {
        RecvPktForSelect(reply);
    } else if (rcvPkt.type == ELECTION_PKT_TYPE_HEART) {
        RecvPktForHeart(rcvPkt, reply);
    } else {
        UBSE_LOG_WARN << "[ELECTION] Agent rcvPkt.type: " << rcvPkt.type << ".";
    }

    return 0;
}

void Agent::RecvPktForHeart(const ElectionPkt& rcvPkt, ElectionReplyPkt& reply)
{
    masterStatus_ = rcvPkt.masterStatus;
    standbyStatus_ = rcvPkt.standbyStatus;
    if (rcvPkt.masterId == masterId_) {
        auto ret = GetBootTime(lastHeartTime_);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "[ELECTION] GetBootTime fail";
        }
        sequenceId_ = rcvPkt.sequenceId;
        masterId_ = rcvPkt.masterId;
        standbyId_ = rcvPkt.standbyId;
        if (turnId_ < rcvPkt.turnId) {
            turnId_ = rcvPkt.turnId;
        }

        reply.replyId = myselfID_;
        reply.replyResult = ELECTION_PKT_RESULT_ACCEPT;
        agentIds_ = rcvPkt.agentIds;
        reply.broadcast = static_cast<uint8_t>(NotifyStatus::BROADCAST);
        if (rcvPkt.standbyId == myselfID_) {
            RoleContext ctx;
            ctx.masterId = rcvPkt.masterId;
            ctx.standbyId = rcvPkt.standbyId;
            ctx.turnId = rcvPkt.turnId;
            RoleMgr::GetInstance().SwitchRole(RoleType::STANDBY, ctx);
        } else {
            DisconnectAgents(rcvPkt);
        }
    } else {
        // 如果主变了，脑裂合并
        HandleMasterChange(rcvPkt, reply);
    }
}

void Agent::DisconnectAgents(const ElectionPkt& rcvPkt)
{
    if (rcvPkt.standbyId.empty() || rcvPkt.agentCount <= NO_1) {
        return;
    }
    for (const auto& agent : rcvPkt.agentIds) {
        if (agent != myselfID_) {
            RoleMgr::GetInstance().GetCommMgr()->DisConnect(agent);
        }
    }
    std::vector<UbseNodeInfo> ubseNodeInfos = UbseNodeController::GetInstance().GetStaticNodeInfo();
    if (ubseNodeInfos.empty()) {
        UBSE_LOG_ERROR << "[ELECTION] LoadConfig get allNodes failed.";
        return;
    }
    for (const auto& node : ubseNodeInfos) {
        if (node.nodeId != masterId_ && node.nodeId != standbyId_ && node.nodeId != myselfID_ &&
            std::find(rcvPkt.agentIds.begin(), rcvPkt.agentIds.end(), node.nodeId) == rcvPkt.agentIds.end()) {
            RoleMgr::GetInstance().GetCommMgr()->DisConnect(node.nodeId);
        }
    }
}

void Agent::RecvPktForSelect(ElectionReplyPkt& reply) const
{
    // 在节点还未触发转换角色前，丢失一定次数心跳，收到另一个节点的选主报文，回复同意。（需等到收到心跳，才会被收编）
    uint32_t acceptMasterAfterLossThreshold = (ElectionRole::GetHbLostTimes() - NO_1);
    if (IsAgentHeartBeatTimeout(acceptMasterAfterLossThreshold)) {
        reply.replyId = myselfID_;
        reply.replyResult = ELECTION_PKT_RESULT_ACCEPT;
    } else {
        reply.replyId = myselfID_;
        reply.masterId = masterId_;
        reply.replyResult = ELECTION_PKT_TYPE_REJECT_HAS_MASTER;
    }
}

UBSE_ID_TYPE Agent::GetMasterNode()
{
    std::lock_guard<std::mutex> lock(roleMutex_);
    return masterId_;
}

UBSE_ID_TYPE Agent::GetStandbyNode()
{
    std::lock_guard<std::mutex> lock(roleMutex_);
    return standbyId_;
}

std::vector<UBSE_ID_TYPE> Agent::GetAgentNodes()
{
    std::lock_guard<std::mutex> lock(roleMutex_);
    return agentIds_;
}

uint8_t Agent::GetMasterStatus()
{
    std::lock_guard<std::mutex> lock(roleMutex_);
    return masterStatus_;
}

uint8_t Agent::GetStandbyStatus()
{
    std::lock_guard<std::mutex> lock(roleMutex_);
    return standbyStatus_;
}

bool Agent::IsAgentHeartBeatTimeout(uint32_t heartbeatMultiplier) const
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
} // namespace ubse::election