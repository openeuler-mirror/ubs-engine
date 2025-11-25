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
Agent::Agent(RoleContext &ctx) : turnId(0), lastHeartTime()
{
    Node myself;
    UbseElectionNodeMgr::GetInstance().GetMyselfNode(myself);

    myselfID = myself.id;
    masterId = ctx.masterId;
    standbyId = ctx.standbyId;
    auto ret = GetBootTime(lastHeartTime);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] GetBootTime fail";
    }
    UBSE_LOG_INFO << "[ELECTION] Agent start ProcTimer: " << myselfID << ".";
}

void Agent::ProcTimer()
{
    // 从节点丢失主的心跳次数阈值，约定从节点丢失心跳次数阈值比备节点的多2次。
    uint32_t agentLostHbSwitchThreshold = (ElectionRole::GetHbLostTimes() + NO_2);
    if (IsAgentHeartBeatTimeout(agentLostHbSwitchThreshold)) {
        if (ForceElection(myselfID) == ELECTION_PKT_RESULT_ACCEPT) {
            RoleContext ctx;
            ctx.masterId = myselfID;
            ctx.standbyId = INVALID_NODE_ID;
            ctx.turnId = turnId;
            UBSE_LOG_INFO << "[ELECTION] Agent ProcTimer: switch Master";
            RoleMgr::GetInstance().SwitchRole(RoleType::MASTER, ctx);
        } else {
            RoleContext ctx;
            UBSE_LOG_INFO << "[ELECTION] Agent ProcTimer: switch Initializer";
            RoleMgr::GetInstance().SwitchRole(RoleType::INITIALIZER, ctx);
        }
    }
}

void Agent::HandleMasterChange(const ElectionPkt &rcvPkt, ElectionReplyPkt &reply)
{
    // 在节点还未触发转换角色前，丢失一定次数心跳，收到另一个主节点的心跳，则接收该节点为主。
    uint32_t acceptMasterAfterLossThreshold = (ElectionRole::GetHbLostTimes() - NO_1);
    if (IsAgentHeartBeatTimeout(acceptMasterAfterLossThreshold)) {
        masterId = rcvPkt.masterId;
        standbyId = rcvPkt.standbyId;
        turnId = rcvPkt.turnId;
        auto ret = GetBootTime(lastHeartTime);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "[ELECTION] GetBootTime fail";
        }
        reply.replyId = myselfID;
        reply.replyResult = ELECTION_PKT_RESULT_ACCEPT;
    } else {
        reply.replyId = myselfID;
        reply.replyResult = ELECTION_PKT_TYPE_REJECT_HAS_MASTER;
    }
}

uint32_t Agent::RecvPkt(UBSE_ID_TYPE srcID, const ElectionPkt rcvPkt, ElectionReplyPkt &reply)
{
    if (rcvPkt.type == ELECTION_PKT_TYPE_SELECT) {
        RecvPktForSelect(reply);
    } else if (rcvPkt.type == ELECTION_PKT_TYPE_HEART) {
        RecvPktForHeart(rcvPkt, reply);
    } else {
        UBSE_LOG_WARN << "[ELECTION] Agent rcvPkt.type: " << rcvPkt.type << ".";
    }

    return 0;
}

void Agent::RecvPktForHeart(const ElectionPkt &rcvPkt, ElectionReplyPkt &reply)
{
    masterStatus = rcvPkt.masterStatus;
    standbyStatus = rcvPkt.standbyStatus;
    if (rcvPkt.masterId == masterId) {
        auto ret = GetBootTime(lastHeartTime);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "[ELECTION] GetBootTime fail";
        }
        sequenceId = rcvPkt.sequenceId;
        masterId = rcvPkt.masterId;
        standbyId = rcvPkt.standbyId;
        if (turnId < rcvPkt.turnId) {
            turnId = rcvPkt.turnId;
        }

        reply.replyId = myselfID;
        reply.replyResult = ELECTION_PKT_RESULT_ACCEPT;
        agentIds = rcvPkt.agentIds;
        if (rcvPkt.broadcast == 0) {
            RoleMgr::GetInstance().RoleChangeNotifyAsync(UbseElectionEventType::MASTER_ONLINE_NOTIFICATION, masterId);
            UBSE_LOG_INFO <<"[ELECTION] The Master is online: " << masterId << ", turnId is: " << turnId;
            reply.broadcast = 1;
        }

        if (rcvPkt.standbyId == myselfID) {
            RoleContext ctx;
            ctx.masterId = rcvPkt.masterId;
            ctx.standbyId = rcvPkt.standbyId;
            ctx.turnId = rcvPkt.turnId;
            RoleMgr::GetInstance().SwitchRole(RoleType::STANDBY, ctx);
        }
    } else {
        // 如果主变了，脑裂合并
        HandleMasterChange(rcvPkt, reply);
    }
}

void Agent::RecvPktForSelect(ElectionReplyPkt &reply) const
{
    // 在节点还未触发转换角色前，丢失一定次数心跳，收到另一个节点的选主报文，回复同意。（需等到收到心跳，才会被收编）
    uint32_t acceptMasterAfterLossThreshold = (ElectionRole::GetHbLostTimes() - NO_1);
    if (IsAgentHeartBeatTimeout(acceptMasterAfterLossThreshold)) {
        reply.replyId = myselfID;
        reply.replyResult = ELECTION_PKT_RESULT_ACCEPT;
    } else {
        reply.replyId = myselfID;
        reply.masterId = masterId;
        reply.replyResult = ELECTION_PKT_TYPE_REJECT_HAS_MASTER;
    }
}

UBSE_ID_TYPE Agent::GetMasterNode()
{
    return masterId;
}

UBSE_ID_TYPE Agent::GetStandbyNode()
{
    return standbyId;
}

std::vector<UBSE_ID_TYPE> Agent::GetAgentNodes()
{
    return agentIds;
}

uint8_t Agent::GetMasterStatus()
{
    return masterStatus;
}

uint8_t Agent::GetStandbyStatus()
{
    return standbyStatus;
}

bool Agent::IsAgentHeartBeatTimeout(uint32_t heartbeatMultiplier) const
{
    uint64_t bootTime;
    auto ret = GetBootTime(bootTime);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] GetBootTime fail";
        return false;
    }
    if (bootTime < lastHeartTime) {
        UBSE_LOG_WARN << "[ELECTION] Current boot time is earlier than last heart time.";
        return false;
    }
    // 计算从上次收到心跳信号到现在的时间差（单位：毫秒）
    uint64_t timeSinceLastHeartbeat = bootTime - lastHeartTime;
    // 计算允许的最大心跳间隔时间 = 心跳丢失的倍数 * 心跳间隔时间（单位：毫秒）
    uint32_t maxAllowedHeartbeatInterval = heartbeatMultiplier * GetHeartTimeInterval();
    // 判断时间差是否超过了允许的最大间隔时间
    return timeSinceLastHeartbeat > maxAllowedHeartbeatInterval;
}
}