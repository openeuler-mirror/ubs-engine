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
using namespace ubse::config;
using namespace ubse::nodeController;
Initializer::Initializer() : lastTimeMs(0)
{
    auto ret = GetBootTime(startTimeMs);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] GetBootTime fail";
    }
    lastTimeMs = startTimeMs;
    Node myself;
    UbseElectionNodeMgr::GetInstance().GetMyselfNode(myself);
    myselfID = myself.id;
    UBSE_LOG_INFO << "[ELECTION] Initializer: " << myselfID << ".";
}
Initializer::~Initializer() {}

void Initializer::ProcRoleSwitch()
{
    RoleContext ctx;
    Node myself;
    std::vector<Node> allNodes;
    ctx.masterId = myselfID;
    ctx.turnId = 0;
    UbseElectionNodeMgr::GetInstance().GetMyselfNode(myself);
    UbseElectionNodeMgr::GetInstance().GetAllNode(allNodes);
    auto ret = GetBootTime(lastTimeMs);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] GetBootTime fail";
    }
    if ((lastTimeMs - startTimeMs) <= GetHeartTimeInterval() * NO_2) {
        if (IsSmallestNode(myself, allNodes)) {
            // 阶段1内 最小ID的发起选组，没有拒绝就宣布为主
            if (SendElectionPkt(myselfID) == ELECTION_PKT_RESULT_ACCEPT) {
                UBSE_LOG_INFO << "[ELECTION] Initializer: ProcTimer switch master - 1";
                RoleMgr::GetInstance().SwitchRole(RoleType::MASTER, ctx);
            }
        }
    } else if ((lastTimeMs - startTimeMs) > GetHeartTimeInterval() * NO_2 &&
               (lastTimeMs - startTimeMs) <= GetHeartTimeInterval() * NO_4) {
        if (IsSecondSmallestNode(myself, allNodes)) {
            // 阶段2内 最小ID的发起选组，没有拒绝就宣布为主
            if (SendElectionPkt(myselfID) == ELECTION_PKT_RESULT_ACCEPT) {
                UBSE_LOG_INFO << "[ELECTION] Initializer: ProcTimer switch master - 2";
                RoleMgr::GetInstance().SwitchRole(RoleType::MASTER, ctx);
            }
        }
    } else {
        // 和接收互斥
        if (ForceElection(myselfID) == ELECTION_PKT_RESULT_ACCEPT) {
            UBSE_LOG_INFO << "[ELECTION] Initializer: ProcTimer switch master - 3";
            RoleMgr::GetInstance().SwitchRole(RoleType::MASTER, ctx);
        }
    }
}

void Initializer::ProcTimer()
{
    UbseNodeLocalState localState = UbseElectionNodeMgr::GetInstance().GetLocalNodeState();
    if (localState == UbseNodeLocalState::UBSE_NODE_READY) {
        if (!isStartTimeSet) {
            auto ret = GetBootTime(startTimeMs);
            if (ret != UBSE_OK) {
                UBSE_LOG_WARN << "[ELECTION] GetBootTime fail";
            }
            isStartTimeSet = true;
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
        reply.replyId = myselfID;
    } else if (localState == UbseNodeLocalState::UBSE_NODE_READY) {
        if (rcvPkt.type == ELECTION_PKT_TYPE_SELECT) {
            if (myselfID < rcvPkt.masterId) {
                reply.replyResult = ELECTION_PKT_TYPE_REJECT;
            } else {
                reply.replyResult = ELECTION_PKT_RESULT_ACCEPT;
            }
            reply.replyId = myselfID;
        } else if (rcvPkt.type == ELECTION_PKT_TYPE_HEART) {
            masterId = rcvPkt.masterId;
            turnId = rcvPkt.turnId;
            RoleContext ctx;

            ctx.masterId = masterId;
            ctx.standbyId = rcvPkt.standbyId;
            ctx.turnId = turnId;

            if (rcvPkt.standbyId == myselfID) {
                RoleMgr::GetInstance().SwitchRole(RoleType::STANDBY, ctx);
            } else {
                RoleMgr::GetInstance().SwitchRole(RoleType::AGENT, ctx);
            }
            reply.replyId = myselfID;
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
    return masterStatus;
}

uint8_t Initializer::GetStandbyStatus()
{
    return standbyStatus;
}
} // namespace ubse::election