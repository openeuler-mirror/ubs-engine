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

#include "ubse_election_role_initializer.h"
#include <chrono>
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_election_node_mgr.h"
#include "ubse_election_role_mgr.h"
#include "ubse_node_controller.h"

namespace ubse::election {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::config;
using namespace ubse::nodeController;
Initializer::Initializer() : lastTimeMs_(0)
{
    auto ret = GetBootTime(startTimeMs_);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] GetBootTime fail";
    }
    lastTimeMs_ = startTimeMs_;
    Node myself;
    UbseElectionNodeMgr::GetInstance().GetMyselfNode(myself);
    myselfID_ = myself.id;
    UBSE_LOG_INFO << "[ELECTION] Initializer: " << myselfID_ << ".";
}
Initializer::~Initializer() {}

void Initializer::CheckAndSwitchMaster(const Node &myself, const std::vector<Node> &allNodes, RoleContext ctx)
{
    if ((lastTimeMs_ - startTimeMs_) <= GetHeartTimeInterval() * NO_2) {
        if (IsSmallestNode(myself, allNodes)) {
            // 阶段1内 最小ID的发起选组，没有拒绝就宣布为主
            if (SendElectionPkt(myselfID_) == ELECTION_PKT_RESULT_ACCEPT) {
                UBSE_LOG_INFO << "[ELECTION] Initializer: ProcTimer switch master - 1";
                RoleMgr::GetInstance().SwitchRole(RoleType::MASTER, ctx);
            }
        }
    } else if ((lastTimeMs_ - startTimeMs_) > GetHeartTimeInterval() * NO_2 &&
               (lastTimeMs_ - startTimeMs_) <= GetHeartTimeInterval() * NO_4) {
        if (IsSecondSmallestNode(myself, allNodes)) {
            // 阶段2内 最小ID的发起选组，没有拒绝就宣布为主
            if (SendElectionPkt(myselfID_) == ELECTION_PKT_RESULT_ACCEPT) {
                UBSE_LOG_INFO << "[ELECTION] Initializer: ProcTimer switch master - 2";
                RoleMgr::GetInstance().SwitchRole(RoleType::MASTER, ctx);
            }
        }
    } else {
        // 和接收互斥
        if (SendElectionPkt(myselfID_) == ELECTION_PKT_RESULT_ACCEPT) {
            UBSE_LOG_INFO << "[ELECTION] Initializer: ProcTimer switch master - 3";
            RoleMgr::GetInstance().SwitchRole(RoleType::MASTER, ctx);
        }
    }
}

void Initializer::ProcRoleSwitch()
{
    RoleContext ctx;
    Node myself;
    std::vector<Node> allNodes;
    ctx.masterId = myselfID_;
    ctx.turnId = 0;
    UbseElectionNodeMgr::GetInstance().GetMyselfNode(myself);
    UbseElectionNodeMgr::GetInstance().GetAllNode(allNodes);
    auto ret = GetBootTime(lastTimeMs_);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] GetBootTime fail";
    }
    if (GetElectionCandidate() && GetElectionWait()) {
        CheckAndSwitchMaster(myself, allNodes, ctx);
    } else if (GetElectionCandidate() && !GetElectionWait()) {
        if (SendElectionPkt(myselfID_) == ELECTION_PKT_RESULT_ACCEPT) {
            UBSE_LOG_INFO << "[ELECTION] Initializer: ProcTimer switch master - 4";
            RoleMgr::GetInstance().SwitchRole(RoleType::MASTER, ctx);
        }
    }
}

void Initializer::ProcTimer()
{
    UbseNodeLocalState localState = UbseElectionNodeMgr::GetInstance().GetLocalNodeState();
    if (localState == UbseNodeLocalState::UBSE_NODE_READY) {
        if (!isStartTimeSet_) {
            auto ret = GetBootTime(startTimeMs_);
            if (ret != UBSE_OK) {
                UBSE_LOG_WARN << "[ELECTION] GetBootTime fail";
            }
            isStartTimeSet_ = true;
        }
        ProcRoleSwitch();
    }
}

uint32_t Initializer::RecvPkt(UBSE_ID_TYPE srcID, const ElectionPkt rcvPkt, ElectionReplyPkt &reply)
{
    UbseNodeLocalState localState = UbseElectionNodeMgr::GetInstance().GetLocalNodeState();
    if (localState == UbseNodeLocalState::UBSE_NODE_RESTORE) {
        if (rcvPkt.type == ELECTION_PKT_TYPE_SELECT) {
            reply.replyResult = ELECTION_PKT_RESULT_ACCEPT;
        } else if (rcvPkt.type == ELECTION_PKT_TYPE_HEART) {
            reply.replyResult = ELECTION_PKT_TYPE_REJECT;
        }
        reply.replyId = myselfID_;
    } else if (localState == UbseNodeLocalState::UBSE_NODE_READY) {
        if (rcvPkt.type == ELECTION_PKT_TYPE_SELECT && GetElectionCandidate()) {
            if (myselfID_ < rcvPkt.masterId) {
                reply.replyResult = ELECTION_PKT_TYPE_REJECT;
            } else {
                reply.replyResult = ELECTION_PKT_RESULT_ACCEPT;
            }
            reply.replyId = myselfID_;
        } else if (rcvPkt.type == ELECTION_PKT_TYPE_SELECT && !GetElectionCandidate()) {
            reply.replyResult = ELECTION_PKT_RESULT_ACCEPT;
            reply.replyId = myselfID_;
        } else if (rcvPkt.type == ELECTION_PKT_TYPE_HEART) {
            masterId_ = rcvPkt.masterId;
            turnId_ = rcvPkt.turnId;
            RoleContext ctx;

            ctx.masterId = masterId_;
            ctx.standbyId = rcvPkt.standbyId;
            ctx.turnId = turnId_;

            if (rcvPkt.standbyId == myselfID_) {
                RoleMgr::GetInstance().SwitchRole(RoleType::STANDBY, ctx);
            } else {
                RoleMgr::GetInstance().SwitchRole(RoleType::AGENT, ctx);
            }
            reply.replyId = myselfID_;
            reply.replyResult = 0;
        } else {
            UBSE_LOG_WARN << "[ELECTION] Initializer rcvPkt.type: " << rcvPkt.type << ".";
        }
    }
    return UBSE_OK;
}

UBSE_ID_TYPE Initializer::GetMasterNode()
{
    return INVALID_NODE_ID;
}

UBSE_ID_TYPE Initializer::GetStandbyNode()
{
    return INVALID_NODE_ID;
}

std::vector<UBSE_ID_TYPE> Initializer::GetAgentNodes()
{
    return {};
}

uint8_t Initializer::GetMasterStatus()
{
    return masterStatus_;
}

uint8_t Initializer::GetStandbyStatus()
{
    return standbyStatus_;
}
} // namespace ubse::election