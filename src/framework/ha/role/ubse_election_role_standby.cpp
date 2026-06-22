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

#include "ubse_election_role_standby.h"
#include "ubse_context.h"
#include "ubse_election_role_mgr.h"
#include "ubse_logger_audit.h"
namespace ubse::election {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::context;
Standby::Standby(RoleContext &ctx) : turnId_(0), lastHeartTime_()
{
    Node myself;
    if (UBSE_ERROR == UbseElectionNodeMgr::GetInstance().GetMyselfNode(myself)) {
        UBSE_LOG_ERROR << "[ELECTION] Master GetMyselfNode: no node found.";
    }

    standbyId_ = myself.id;
    masterId_ = ctx.masterId;
    turnId_ = ctx.turnId;
    auto ret = UbseElectionNodeMgr::GetInstance().GetGroupId(groupId_);
    if (ret != UBSE_OK || groupId_.empty()) {
        UBSE_LOG_ERROR << "[ELECTION] GetGroupId failed";
    }
    auto result = GetBootTime(lastHeartTime_);
    if (result != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] GetBootTime fail";
    }
    UBSE_LOG_INFO << "[ELECTION] Standby start ProcTimer: " << standbyId_ << ".";
}

void Standby::ProcTimer()
{
    // 备节点丢失心跳次数阈值
    uint32_t standbyLostHbSwitchThreshold = ElectionRole::GetHbLostTimes();
    if (IsStandbyHeartBeatTimeout(standbyLostHbSwitchThreshold) && GetElectionCandidate()) {
        UBSE_LOG_INFO << "[ELECTION] Standby ProcTimer: switch Master";
        UBSE_AUDIT_RUNTIME_ALLOC << "Current node switched from standby to master, node ID: " << standbyId_;
        SwitchMaster();
    } else if (IsStandbyHeartBeatTimeout(standbyLostHbSwitchThreshold) && !GetElectionCandidate()) {
        RoleContext ctx;
        UBSE_LOG_INFO << "[ELECTION] Standby ProcTimer: switch Initializer";
        RoleMgr::GetInstance().SwitchRole(RoleType::INITIALIZER, ctx);
    }
}
void Standby::SwitchMaster()
{
    RoleContext ctx;
    ctx.masterId = standbyId_;
    ctx.standbyId = INVALID_NODE_ID;
    ctx.turnId = turnId_ + 1;
    UbseContext::GetInstance().SetWorkReadiness(NOT_READY);
    RoleMgr::GetInstance().SwitchRole(RoleType::MASTER, ctx);
    RoleMgr::GetInstance().RoleChangeNotifyAsync(UbseElectionEventType::STANDBY_CHANGE_TO_MASTER, ctx.masterId);
}

void HandleMasterOnlineNotification(const ElectionPkt &rcvPkt, ElectionReplyPkt &reply)
{
    if (rcvPkt.broadcast == 0) {
        RoleMgr::GetInstance().RoleChangeNotifyAsync(UbseElectionEventType::MASTER_ONLINE_NOTIFICATION,
                                                     rcvPkt.masterId);
        UBSE_LOG_INFO << "[ELECTION] The Master is online: " << rcvPkt.masterId << ", turnId is: " << rcvPkt.turnId;
        reply.broadcast = 1;
    }
}

uint32_t Standby::RecvPkt(UBSE_ID_TYPE srcID, const ElectionPkt rcvPkt, ElectionReplyPkt &reply)
{
    if (rcvPkt.type == ELECTION_PKT_TYPE_SELECT) {
        // 备节点拒绝所有选主报文，不管主如何，备优先会成为主
        reply.replyId = standbyId_;
        reply.masterId = masterId_;
        reply.replyResult = ELECTION_PKT_TYPE_REJECT_HAS_MASTER;
    } else if (rcvPkt.type == ELECTION_PKT_TYPE_HEART) {
        RecvPktForHeart(rcvPkt, reply);
    } else if (rcvPkt.type == ELECTION_PKT_TYPE_QUERY_LOCAL_MASTER) {
        reply.replyId = standbyId_;
        reply.groupId = groupId_;
        reply.masterId = masterId_;
        reply.standbyId = standbyId_;
    }
    return 0;
}
void Standby::RecvPktForHeart(const ElectionPkt &rcvPkt, ElectionReplyPkt &reply)
{
    if (rcvPkt.masterId == masterId_) {
        if (rcvPkt.standbyId == standbyId_) {
            turnId_ = rcvPkt.turnId;
            sequenceId_ = rcvPkt.sequenceId;
            auto ret = GetBootTime(lastHeartTime_);
            if (ret != UBSE_OK) {
                UBSE_LOG_WARN << "[ELECTION] GetBootTime fail";
            }
            masterStatus_ = rcvPkt.masterStatus;
            globalMasterId_ = rcvPkt.globalMasterId;
            globalStandbyId_ = rcvPkt.globalStandbyId;
            auto currentStatus = UbseContext::GetInstance().GetWorkReadiness();
            reply.standbyStatus = currentStatus;
            reply.replyId = standbyId_;
            reply.replyResult = ELECTION_PKT_RESULT_ACCEPT;
            agentIds_ = rcvPkt.agentIds;
            HandleMasterOnlineNotification(rcvPkt, reply);
        } else {
            RoleContext ctx;
            ctx.masterId = rcvPkt.masterId;
            ctx.standbyId = rcvPkt.standbyId;
            ctx.turnId = rcvPkt.turnId;
            RoleMgr::GetInstance().SwitchRole(RoleType::AGENT, ctx);
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
            RoleMgr::GetInstance().SwitchRole(RoleType::AGENT, ctx);
        } else {
            reply.replyId = standbyId_;
            reply.replyResult = ELECTION_PKT_TYPE_REJECT_HAS_MASTER;
        }
    }
}

UBSE_ID_TYPE Standby::GetMasterNode()
{
    return masterId_;
}

UBSE_ID_TYPE Standby::GetStandbyNode()
{
    return standbyId_;
}

UBSE_ID_TYPE Standby::GetGlobalMasterNode()
{
    return globalMasterId_;
}

UBSE_ID_TYPE Standby::GetGlobalStandbyNode()
{
    return globalStandbyId_;
}

std::vector<UBSE_ID_TYPE> Standby::GetAgentNodes()
{
    return agentIds_;
}

uint8_t Standby::GetMasterStatus()
{
    return masterStatus_;
}

uint8_t Standby::GetStandbyStatus()
{
    auto currentStatus = UbseContext::GetInstance().GetWorkReadiness();
    return currentStatus;
}

bool Standby::IsStandbyHeartBeatTimeout(uint32_t heartbeatMultiplier) const
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